#include "EntityBars.hpp"
#include "utility/Module.hpp"
#include "utility/Memory.hpp"
#include "utility/Log.hpp"
#include "ConfigManager.hpp"
#include <format>
#include <limits>

EntityBars::EntityBars() : context(), bIsInitialized(false), pLockOnSystemStaticPtr(nullptr), target(), fnGetMaxDurability(nullptr)
{
	context.bShowWindow = true;
}

EntityBars::~EntityBars()
{

}

bool EntityBars::OnInitialize()
{
	LOG_INFO("Initializing EntityBars...");

	context.bCustomPosition = ConfigManager::GetInstance().Get<int>("IsCustomPosition").value_or(0);

	HMODULE hExec = utility::GetExecutable();
	if (hExec == NULL)
	{
		LOG_ERROR("Failed to get executable address");
		return false;
	}

	std::optional<size_t> sizeExec = utility::GetModuleSize(hExec);
	if (!sizeExec.has_value())
	{
		LOG_ERROR("Failed to get executable size");
		return false;
	}

	char* ptr = utility::PatternScan(GET_LOCKONSYSTEM_FN_SIG, sizeof(GET_LOCKONSYSTEM_FN_SIG), hExec, sizeExec.value_or(0));
	if (ptr == nullptr)
	{
		LOG_ERROR("Failed to find GetLockOnSystem function signature");
		return false;
	}

	ptr = ptr + 0x20; // Move ptr to the call parameter
	int offset = *(int*)ptr; // Read the function offset
	ptr = ptr + sizeof(int); // Move ptr to the next instruction (after call)

	//ptr = ptr + offset; // Move ptr to the function address (in this case it's a vtable pointer)
	//if (*ptr != (char)0xE9) // This must be a jmp instruction, we are looking for a vtable reference here
	//{
	//	LOG_ERROR("Failed to locate GetLockOnSystem function address");
	//	return false;
	//}
	//offset = *(int*)(++ptr); // Inc ptr and read the function offset
	//ptr = ptr + sizeof(int); // Move ptr to the next instruction
	//fix 1.5
	ptr = ptr + offset; // Move ptr to the begining of the function
	if (*ptr != (char)0x48 && *(ptr+1) != (char)0x83 && *(ptr + 4) != (char)0x48 && *(ptr + 5) != (char)0x8B && *(ptr + 6) != (char)0xB9) // This must be a jmp instruction, we are looking for a vtable reference here
	{
		LOG_ERROR("Failed to locate GetLockOnSystem function address");
		return false;
	}
	ptr = ptr + 7; // Move ptr to the offset of the static pointer
	offset = *(int*)ptr; // Read the offset of the static pointer
	ptr = ptr + sizeof(int); // Move ptr to the next instruction (after mov)
	
	pLockOnSystemStaticPtr = ptr + offset; // Read the static address

	if (utility::IsBadReadPtr(pLockOnSystemStaticPtr))
	{
		LOG_ERROR("LockOnSystem static pointer is bad read");
		return false;
	}

	ptr = utility::PatternScan(GET_MAX_DURABILITY_FN_SIG, sizeof(GET_MAX_DURABILITY_FN_SIG), hExec, sizeExec.value_or(0));
	if (ptr == nullptr)
	{
		LOG_ERROR("Failed to find GetMaxDurability function signature");
		return false;
	}

	fnGetMaxDurability = (GetMaxDurability)ptr;

	bIsInitialized = true;

	//pThreadUpdateEntity = std::make_unique<std::jthread>(std::bind_front(&EntityBars::UpdateEntityData, this));

	LOG_INFO("EntityBars initialized!");

	return true;
}

void EntityBars::OnReset()
{
	if (!bIsInitialized)
		return;

	std::scoped_lock _{ context.mutex };
	context.bEnableDrag = false;
}

void EntityBars::ShowBasicStats(int iValues[], float fValues[], const ImVec2& progressBarSize)
{
	// Read all buff values
	static int iBuffValues[4]; // Max health, Max stamina, Max toughness, Max Stagger

	memset(iBuffValues, 0, sizeof(int) * 4);

	char* ptr;
	unsigned char t;
	for (int i = 0; i < target.buffList.iSize; ++i)
	{
		ptr = target.buffList.pList + (i * 0x18);
		t = *(unsigned char*)(ptr + 0x8);
		switch (t)
		{
			case ENTITY_STATS::S_HEALTH_POINT_MAX:
				iBuffValues[0] = *(int*)(ptr + 0xC);
				break;
			case ENTITY_STATS::S_STAMINA_POINT_MAX:
				iBuffValues[1] = *(int*)(ptr + 0xC);
				break;
			case ENTITY_STATS::S_TOUGH_POINT_MAX:
				iBuffValues[2] = *(int*)(ptr + 0xC);
				break;
			case ENTITY_STATS::S_GROGGY_POINT_MAX:
				iBuffValues[3] = *(int*)(ptr + 0xC);
				break;
		}
	}

	static float fMulStatValues[4];

	memset(fMulStatValues, 0, sizeof(float) * 4);

	for (int i = 0; i < target.maxStatMulList.iSize; ++i)
	{
		ptr = target.maxStatMulList.pList + (i * 0x18);
		t = *(unsigned char*)(ptr + 0x8);
		switch (t)
		{
			case ENTITY_STATS::S_HEALTH_POINT_MAX:
				fMulStatValues[0] = *(int*)(ptr + 0xC) / 10000.0f;
				break;
			case ENTITY_STATS::S_STAMINA_POINT_MAX:
				fMulStatValues[1] = *(int*)(ptr + 0xC) / 10000.0f;
				break;
			case ENTITY_STATS::S_TOUGH_POINT_MAX:
				fMulStatValues[2] = *(int*)(ptr + 0xC) / 10000.0f;
				break;
			case ENTITY_STATS::S_GROGGY_POINT_MAX:
				fMulStatValues[3] = *(int*)(ptr + 0xC) / 10000.0f;
				break;
		}
	}
	if (ConfigManager::GetInstance().Get<int>("ShowSeparator").value_or(1) != 0)
		ImGui::SeparatorText("Stats");

	// Health
	//*(int*)(target.statList.pList + 0xC) = 1;
	iValues[0] = *(int*)(target.statList.pList + 0xC);
	iValues[1] = *(int*)(target.statList.pList + 0xD2C) + iBuffValues[0];
	if (fMulStatValues[0] > 0.0f)
		iValues[1] *= fMulStatValues[0];
	std::string sText = std::format("Health ({}/{})", iValues[0], iValues[1]);
	if (ConfigManager::GetInstance().Get<int>("ShowHealth").value_or(1) != 0)
		ImGui::ProgressBar((float)iValues[0] / (float)iValues[1], progressBarSize, sText.c_str(), ImVec4(0.4f, 0.0f, 0.0f, 1.0f));

	// Stamina
	iValues[0] = *(int*)(target.statList.pList + 0x3C);
	iValues[1] = *(int*)(target.statList.pList + 0xD5C) + iBuffValues[1];
	if (fMulStatValues[1] > 0.0f)
		iValues[1] *= fMulStatValues[1];
	sText = std::format("Stamina ({}/{})", iValues[0], iValues[1]);
	if (ConfigManager::GetInstance().Get<int>("ShowStamina").value_or(1) != 0)
		ImGui::ProgressBar((float)iValues[0] / (float)iValues[1], progressBarSize, sText.c_str(), ImVec4(0.0f, 0.4f, 0.0f, 1.0f));

	// Tough
	iValues[0] = *(int*)(target.statList.pList + 0x18C);
	iValues[1] = *(int*)(target.statList.pList + 0xE1C) + iBuffValues[2];
	if (fMulStatValues[2] > 0.0f)
		iValues[1] *= fMulStatValues[2];
	sText = std::format("Posture ({}/{})", iValues[0], iValues[1]);
	if (ConfigManager::GetInstance().Get<int>("ShowTough").value_or(1) != 0)
		ImGui::ProgressBar((float)iValues[0] / (float)iValues[1], progressBarSize, sText.c_str(), ImVec4(0.5f, 0.0f, 0.5f, 1.0f));

	// Stagger
	iValues[0] = *(int*)(target.statList.pList + 0x8DC); // Stagger point
	iValues[1] = *(int*)(target.statList.pList + 0xE7C) + iBuffValues[3]; // Max stagger point
	fValues[1] = *(float*)(target.pBase + 0xF60); // Stagger max duration
	if (iValues[0] == 0 && fValues[1] > 1.0f)
	{
		fValues[0] = *(float*)(target.pBase + 0xF68); // Stagger retain/duration
		sText = std::vformat("Stagger {:.2f}s"sv, std::make_format_args(fValues[0]));
		if (ConfigManager::GetInstance().Get<int>("ShowStagger").value_or(1) != 0)
			ImGui::ProgressBar(fValues[0] / fValues[1], progressBarSize, sText.c_str(), ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
	}
	else if (iValues[1] > 0)
	{
		if (fMulStatValues[3] > 0.0f)
			iValues[1] *= fMulStatValues[3];
		sText = std::format("Stagger ({}/{})", iValues[0], iValues[1]);
		if (ConfigManager::GetInstance().Get<int>("ShowStagger").value_or(1) != 0)
			ImGui::ProgressBar((float)iValues[0] / (float)iValues[1], progressBarSize, sText.c_str(), ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
	}
}

void EntityBars::ShowElementalBuildup(const ImVec2& progressBarSize)
{
	size_t end = (target.abnormalStatList.iSize * 0x10);
	uintptr_t itemAddr;
	unsigned char e;
	bool bIsDebuffActive;

	memset(E_BAR_LIST, 0, sizeof(E_BAR) * E_TYPE::COUNT);

	for (size_t i = 0; i < end; i += 0x10)
	{
		itemAddr = *(uintptr_t*)(target.abnormalStatList.pList + i);

		e = *(char*)(*(uintptr_t*)(itemAddr + 0x8) + 0x84); // Element type

		if (e < E_TYPE::FIRE || e > E_TYPE::COUNT - 1) // Skip unsupported types
			continue;

		E_BAR_LIST[e].fValues[0] = *(float*)(itemAddr + 0x258); // Current buildup
		E_BAR_LIST[e].fValues[1] = *(float*)(itemAddr + 0x254); // Max buildup
		//E_BAR_LIST[e].fValues[1] = (float)*(int*)(target.statList.pList + E_RESIST_OFFSET[e]); // Max buildup
		//bIsDebuffActive = (E_BAR_LIST[e].fValues[0] == E_BAR_LIST[e].fValues[1]); // Check if debuff is active
		bIsDebuffActive = (E_BAR_LIST[e].fValues[1] == 0.0f); // Check if debuff is active

		// Get Retain values if debuff is active
		if (bIsDebuffActive)
		{
			E_BAR_LIST[e].fValues[0] = *(int*)(itemAddr + 0x74) / 1000.0f; // Buildup retain in seconds
			E_BAR_LIST[e].fValues[1] = *(int*)(itemAddr + 0x78) / 1000.0f; // Buildup max retain in seconds
			if (E_BAR_LIST[e].fValues[0] < 0.0f)
				E_BAR_LIST[e].fValues[0] = 0.0f;
		}

		// Set the progress bar text
		E_BAR_LIST[e].sBarText = bIsDebuffActive ?
			std::format("{} {:.2f}s"sv, E_NAME[e], E_BAR_LIST[e].fValues[0]) :
			std::format("{} ({}/{})"sv, E_NAME[e], (int)E_BAR_LIST[e].fValues[0], (int)E_BAR_LIST[e].fValues[1]);
	}
	if (ConfigManager::GetInstance().Get<int>("ShowSeparator").value_or(1) != 0)
		ImGui::SeparatorText("Buildup");

	for (char i = E_TYPE::FIRE; i < E_TYPE::COUNT; ++i)
	{
		if (E_BAR_LIST[i].fValues[1] <= 0.0f)
		{
			E_BAR_LIST[i].fValues[1] = (float)*(int*)(target.statList.pList + E_RESIST_OFFSET[i]); // Max buildup
			E_BAR_LIST[i].sBarText = std::format("{} ({}/{})"sv, E_NAME[i], 0.0f, (int)E_BAR_LIST[i].fValues[1]);
		}
		if (ConfigManager::GetInstance().Get<int>("ShowEmptBuildup").value_or(1) == 0 && E_BAR_LIST[i].fValues[0] <= 0)
			continue;
		ImGui::ProgressBar(E_BAR_LIST[i].fValues[0] / E_BAR_LIST[i].fValues[1], progressBarSize, E_BAR_LIST[i].sBarText.c_str(), E_COLOR[i]);
	}

}

void EntityBars::ShowWeaponsDurability(int iValues[], const ImVec2& progressBarSize)
{
	char* pWeapon;
	bool bShowHeader = true;

	for (int i = 0; i < target.weaponList.iSize; ++i)
	{
		pWeapon = (char*)*(uintptr_t*)(target.weaponList.pList + 0x30 + (i * 0x40));
		iValues[0] = *(int*)(pWeapon + 0x308);

		if (iValues[0] < 0)
			continue;

		if (bShowHeader)
		{
			bShowHeader = false;
			if (ConfigManager::GetInstance().Get<int>("ShowSeparator").value_or(1) != 0)
				ImGui::SeparatorText("Weapons");
		}

		iValues[1] = fnGetMaxDurability(pWeapon);
		std::string sText = std::format("Durability ({}/{})", iValues[0], iValues[1]);
		ImGui::ProgressBar((float)iValues[0] / (float)iValues[1], progressBarSize, sText.c_str(), ImVec4(0.9f, 0.7f, 0.0f, 1.0f));
	}
}

//FMatrix GetViewProjectionMatrix(const POV& viewInfo)
//{
//	auto viewport = ImGui::GetMainViewport();
//
//	int x = 0;
//	int y = 0;
//
//	int sizeX = viewport->Size.x;
//	int sizeY = viewport->Size.y;
//
//	FIntRect unconstrainedRect = FIntRect(x, y, x + sizeX, y + sizeY);
//
//	FVector viewOrigin = viewInfo.location;
//	FMatrix viewRotationMatrix = FInverseRotationMatrix(viewInfo.rotation) * FMatrix(
//		FPlane(0, 0, 1, 0),
//		FPlane(1, 0, 0, 0),
//		FPlane(0, 1, 0, 0),
//		FPlane(0, 0, 0, 1)
//	);
//
//	float XAxisMultiplier;
//	float YAxisMultiplier;
//	float MatrixHalfFOV;
//
//	//if (sizeX > sizeY)
//	//{
//	//	XAxisMultiplier = 1.0f;
//	//	YAxisMultiplier = sizeX / (float)sizeY;
//	//	const float HalfXFOV = FMath::DegreesToRadians(MAX(0.001f, viewInfo.fov) / 2.f);
//	//	const float HalfYFOV = std::atan(std::tan(HalfXFOV) / viewInfo.aspectRatio);
//	//	MatrixHalfFOV = HalfYFOV;
//	//}
//	//else
//	{
//		XAxisMultiplier = sizeY / (float)sizeX;
//		YAxisMultiplier = 1.0f;
//		//MatrixHalfFOV = MAX(0.001f, viewInfo.fov) * (float)PI / 360.0f;
//		const float HalfXFOV = FMath::DegreesToRadians(MAX(0.001f, viewInfo.fov) / 2.f);
//		const float HalfYFOV = std::atan(std::tan(HalfXFOV) / viewInfo.aspectRatio);
//		MatrixHalfFOV = HalfYFOV;
//	}
//
//	FMatrix projectionMatrix = FReversedZPerspectiveMatrix(
//		MatrixHalfFOV,
//		MatrixHalfFOV,
//		XAxisMultiplier,
//		YAxisMultiplier,
//		viewInfo.orthoNearClipPlane,
//		viewInfo.orthoFarClipPlane
//	);
//
//	FMatrix viewProjectionMatrix = FTranslationMatrix(-viewOrigin) * viewRotationMatrix * projectionMatrix;
//	return viewProjectionMatrix;
//}
//
//bool WorldToScreen(const FVector& worldPos, FVector2D& screenPos)
//{
//	char* pLocalPlayer = (char*)*(uintptr_t*)((char*)utility::GetExecutable() + 0x7357830);
//	char* pPlayerController = (char*)*(uintptr_t*)(pLocalPlayer + 0x30);
//	char* pCameraManager = (char*)*(uintptr_t*)(pPlayerController + 0x278);
//	POV pov = *(POV*)(pCameraManager + 0xF00);
//
//	FMatrix viewProjectionMatrix = GetViewProjectionMatrix(pov);
//	FPlane result = viewProjectionMatrix.TransformFVector4(FVector4(worldPos, 1.0f));
//
//	if (result.W <= 0.0f)
//		return false;
//
//	const float RHW = 1.0f / result.W;
//
//	FPlane PosInScreenSpace = FPlane(result.X * RHW, result.Y * RHW, result.Z * RHW, result.W);
//
//	const float NormalizedX = (PosInScreenSpace.X / 2.f) + 0.5f;
//	const float NormalizedY = 1.f - (PosInScreenSpace.Y / 2.f) - 0.5f;
//
//	auto viewport = ImGui::GetMainViewport();
//
//	FVector2D RayStartViewRectSpace(
//		(NormalizedX * viewport->Size.x),
//		(NormalizedY * viewport->Size.y)
//	);
//
//	screenPos.X = RayStartViewRectSpace.X;
//	screenPos.Y = RayStartViewRectSpace.Y;
//
//	return true;
//}
//
//void EntityBars::ShowTestWindow(const FVector& headTagPos)
//{
//	static FVector2D screenPos;
//	//static FVector2D lastScreenPos;
//
//	FVector roundedHeadTagPos;
//	roundedHeadTagPos.X = FMath::RoundToFloat(headTagPos.X);
//	roundedHeadTagPos.Y = FMath::RoundToFloat(headTagPos.Y);
//	roundedHeadTagPos.Z = FMath::RoundToFloat(headTagPos.Z);
//
//	if (!WorldToScreen(roundedHeadTagPos, screenPos))
//		return;
//
//	//if (lastScreenPos.Y - screenPos.Y > 20.0f)
//	//{
//	//	int i = 234;
//	//}
//	//lastScreenPos = screenPos;
//
//	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration;
//
//	ImVec2 progressBarSize = ImVec2(-1.0f, ImGui::GetFontSize() - 4.0f);
//
//	auto& style = ImGui::GetStyle();
//	style.WindowRounding = 0.0f;
//	style.FrameRounding = 0.0f;
//	style.FrameBorderSize = 1.0f;
//	style.WindowBorderSize = 0.0f;
//	style.ItemSpacing = ImVec2(4.0f, 1.0f);
//
//	ImGui::SetNextWindowBgAlpha(0.0f);
//	ImGui::SetNextWindowPos(ImVec2(screenPos.X, screenPos.Y - 3.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
//	ImGui::SetNextWindowSize(ImVec2(106.0f, 0.0f));
//	if (ImGui::Begin("TestWindow", nullptr, windowFlags))
//	{
//		ImGui::ProgressBar(100 / 100, progressBarSize, "", ImVec4(0.5f, 0.0f, 0.5f, 1.0f));
//		ImGui::ProgressBar(100 / 100, progressBarSize, "", ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
//	}
//	ImGui::End();
//
//	style.WindowRounding = 4.0f;
//	style.FrameRounding = 4.0f;
//	style.WindowBorderSize = 1.0f;
//	style.ItemSpacing = ImVec2(8.0f, 4.0f);
//}

void EntityBars::OnDraw()
{
	if (!bIsInitialized)
		return;

	static WND_CONTEXT tmpContext;
	context.GetData(tmpContext);

	auto pLockOnSystem = (char*)*(uintptr_t*)pLockOnSystemStaticPtr;

	if (!tmpContext.bShowWindow || pLockOnSystem == nullptr)
		return;

	auto pLockOnSystemData = (char*)*(uintptr_t*)(pLockOnSystem + 0x98);
	if (pLockOnSystemData == nullptr)
		return;

	target.pBase = (char*)*(uintptr_t*)(pLockOnSystemData + 0x200);
	if (target.pBase == nullptr)
		return;

	// Get only objects that have their instigator reference as themselves
	if ((void*)*(uintptr_t*)(target.pBase + 0xD8) != target.pBase)
		return;

	target.bFaction = *(target.pBase + 0x760);

	// Quick offset fix for 1.2.0
	//target.pBase -= 0x10;

	// Get entity's StatComponent
	uintptr_t ptr = *(uintptr_t*)(target.pBase + 0x848);

	// Filter out incomplete StatLists
	target.statList = *(LIST_DATA*)(*(uintptr_t*)(ptr + 0xE0) + 0x28);
	if (target.statList.iSize < 130)
		return;

	target.buffList = *(LIST_DATA*)(*(uintptr_t*)(ptr + 0xE0) + 0x38);

	target.maxStatMulList = *(LIST_DATA*)(*(uintptr_t*)(ptr + 0xE0) + 0x58);

	// Get entity's AbnormalComponent
	ptr = *(uintptr_t*)(target.pBase + 0x850);
	target.abnormalStatList = *(LIST_DATA*)(ptr + 0xD0);

	// Get entity's EquipmentComponent
	ptr = *(uintptr_t*)(target.pBase + 0x858);
	target.weaponList = *(LIST_DATA*)(ptr + 0xF8); // 0xF8 for 1.3.0 and 0xF0 for 1.2.0

	//FVector headTag = *(FVector*)(target.pBase + 0x149C);
	//FVector entityPos = *(FVector*)(*(uintptr_t*)(target.pBase + 0xF0) + 0x10C);
	//ShowTestWindow(headTag);

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

	if (!tmpContext.bEnableDrag)
		windowFlags = windowFlags | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration;
	else
		ImGui::SetNextWindowFocus();

	auto& io = ImGui::GetIO();

	io.MouseDrawCursor = tmpContext.bEnableDrag;

	if (!tmpContext.bCustomPosition)
	{
		ImVec2 windowPos, windowPosPivot;
		auto viewport = ImGui::GetMainViewport();
		windowPos.x = viewport->WorkPos.x + viewport->WorkSize.x - 10.0f;
		windowPos.y = viewport->GetCenter().y;
		windowPosPivot.x = 1.0f;
		windowPosPivot.y = 0.5f;
		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, windowPosPivot);
	}

	float SizeRatio = ConfigManager::GetInstance().Get<float>("SizeRatio").value_or(1);
	
	auto& style = ImGui::GetStyle();
	//style.WindowRounding = 0.0f;
	//style.FrameRounding = 0.0f;
	style.FrameBorderSize = 0.0f;
	style.WindowBorderSize = 0.0f;

	ImGui::SetNextWindowSize(ImVec2(220.0f * SizeRatio, 0.0f));
	ImGui::SetNextWindowBgAlpha(0.0f);

	if (ImGui::Begin("Entity Bars", nullptr, windowFlags))
	{
		ImGui::SetWindowFontScale(SizeRatio);
		static ImVec2 progressBarSize = ImVec2(-1.0f, ImGui::GetFontSize() + 7.0f );

		static int iValues[2];
		static float fValues[2];
		if (ConfigManager::GetInstance().Get<int>("ShowBasicStats").value_or(1) != 0)
			ShowBasicStats(iValues, fValues, progressBarSize);

		if (ConfigManager::GetInstance().Get<int>("ShowElementalBuildup").value_or(1) != 0 && target.abnormalStatList.iSize > -1)
			ShowElementalBuildup(progressBarSize);

		if (ConfigManager::GetInstance().Get<int>("ShowWeaponsDurability").value_or(1) != 0 && target.weaponList.iSize > 0)
			ShowWeaponsDurability(iValues, progressBarSize);
	}
	ImGui::End();
}

bool EntityBars::OnMessage(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	if (!bIsInitialized)
		return true;

	switch (iMsg)
	{
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
			if (context.bEnableDrag)
				return false;
			break;
		case WM_KEYDOWN:
		{
			switch (wParam)
			{
				case VK_HOME: // Toggle overlay
				{
					std::scoped_lock _{ context.mutex };
					context.bShowWindow = !context.bShowWindow;
				}
				return false;
				case VK_PRIOR: // Reset window position
					if (context.bShowWindow)
					{
						std::scoped_lock _{ context.mutex };
						context.bEnableDrag = false;
						context.bCustomPosition = false;
						ConfigManager::GetInstance().Set<int>("IsCustomPosition", 0);
					}
					return false;
				case VK_NEXT: // Toggle window drag and resize 
					if (context.bShowWindow)
					{
						std::scoped_lock _{ context.mutex };
						context.bEnableDrag = !context.bEnableDrag;
						context.bCustomPosition = true;
						if (!context.bEnableDrag)
							ConfigManager::GetInstance().Set<int>("IsCustomPosition", 1);
					}
					return false;
			}
		}
		break;
	}
	return true;
}
