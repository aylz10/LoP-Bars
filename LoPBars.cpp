#include "LoPBars.hpp"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include "utility/Log.hpp"
#include "EntityBars.hpp"
#include "Splash.hpp"
#include "ConfigManager.hpp"

LoPBars::LoPBars() : iD3D12(), hWnd(0), bIsInitialized(false), initAttemptCount(0)
{
	LOG_INFO("LoP Bars is initializing...");

	ConfigManager::GetInstance().LoadData();

	imGuiWindows.push_back(std::make_unique<Splash>());
	imGuiWindows.push_back(std::make_unique<EntityBars>());

	for (auto& wnd : imGuiWindows)
	{
		if (!wnd->OnInitialize())
		{
			LOG_ERROR("Failed to initialize");
			throw std::exception("Failed to initialize due to an incompatible version of the game. Check for an update.");
		}
	}

	if (!HookD3D12())
	{
		LOG_ERROR("Failed to hook D3D12. Retrying in 3 seconds...");
		Sleep(3000);
		if (!HookD3D12())
		{
			LOG_ERROR("Failed to hook D3D12.");
			throw std::exception("Failed to hook D3D12. This could be a conflict with another active overlay.");
		}
	}

	LOG_INFO("D3D12 hooked!");
}

LoPBars::~LoPBars()
{
	LOG_INFO("Destroying LoP Bars...");

	Cleanup();
}

bool LoPBars::HookD3D12()
{
	auto &hkD3D12 = D3D12Hook::GetInstance();

	if (hkD3D12.IsHooked())
	{
		LOG_WARNING("D3D12 already hooked!");
		return false;
	}

	hkD3D12.SetOnPresent([this]() { OnPresentD3D12(); });
	//hkD3D12.on_post_present([this]() { onPostPresentD3D12(); });
	hkD3D12.SetOnResizeBuffers([this]() { OnDeviceReset(); });
	hkD3D12.SetOnResizeTarget([this]() { OnDeviceReset(); });

	return hkD3D12.Hook();
}

bool LoPBars::HookWindowsMsg()
{
	if (hWnd == 0)
	{
		LOG_ERROR("Invalid window handle!");
		return false;
	}

	auto &msgHook = WindowsMessageHook::GetInstance();

	if (msgHook.IsHookIntact())
	{
		LOG_WARNING("Windows message already hooked!");
		return true;
	}

	if (!msgHook.Hook(hWnd))
	{
		LOG_ERROR("Failed to hook windows message handler");
		return false;
	}

	msgHook.SetOnMessage(
		[this](HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			return OnWindowsMsg(wnd, msg, wParam, lParam);
		}
	);

	return true;
}

bool LoPBars::Initialize()
{
	if (bIsInitialized)
		return true;

	auto& hkD3D12 = D3D12Hook::GetInstance();

	if (!hkD3D12.IsHooked())
	{
		HookD3D12();
		return false;
	}

	auto device = hkD3D12.GetDevice();
	auto swapChain = hkD3D12.GetSwapChain();

	LOG_INFO("Device: " << std::hex << (uintptr_t)device);
	LOG_INFO("SwapChain: " << std::hex << (uintptr_t)swapChain);

	if (device == nullptr || swapChain == nullptr)
	{
		LOG_ERROR("Device or SwapChain null. Retrying hook...");
		hkD3D12.Unhook();
		hkD3D12.Hook();
		return false;
	}

	if (hkD3D12.GetCommandQueue() == nullptr)
	{
		LOG_ERROR("Failed to retrieve command queue. Retrying hook...");
		hkD3D12.Unhook();
		hkD3D12.Hook();
		return false;
	}

	DXGI_SWAP_CHAIN_DESC swapDesc{};
	swapChain->GetDesc(&swapDesc);
	hWnd = swapDesc.OutputWindow;

	iD3D12.frameContexts.clear();
	for (UINT i = 0; i < swapDesc.BufferCount; ++i)
		iD3D12.frameContexts.push_back(D3D12_FRAME_CONTEXT());

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ConfigImGui();

	if (!ImGui_ImplWin32_Init(hWnd))
	{
		LOG_ERROR("Failed to initialize ImGui ImplWin32.");
		return false;
	}

	if (!InitializeD3D12())
	{
		LOG_ERROR("Failed to initialize D3D12.");
		return false;
	}

	if (!HookWindowsMsg())
	{
		LOG_ERROR("Failed to hook Windows message handler.");
		return false;
	}

	return true;
}

void LoPBars::Cleanup()
{
	if (!ConfigManager::GetInstance().Get<int>("ShowBasicStats")
		|| !ConfigManager::GetInstance().Get<int>("ShowElementalBuildup")
		|| !ConfigManager::GetInstance().Get<int>("ShowWeaponsDurability")
		|| !ConfigManager::GetInstance().Get<int>("ShowHealth")
		|| !ConfigManager::GetInstance().Get<int>("ShowStamina")
		|| !ConfigManager::GetInstance().Get<int>("ShowTough")
		|| !ConfigManager::GetInstance().Get<int>("ShowStagger")
		|| !ConfigManager::GetInstance().Get<int>("ShowEmptBuildup")
		|| !ConfigManager::GetInstance().Get<int>("ShowSeparator")
		|| !ConfigManager::GetInstance().Get<float>("SizeRatio"))
	{

		ConfigManager::GetInstance().Del("ShowBasicStats");
		ConfigManager::GetInstance().Del("ShowElementalBuildup");
		ConfigManager::GetInstance().Del("ShowWeaponsDurability");
		ConfigManager::GetInstance().Del("ShowHealth");
		ConfigManager::GetInstance().Del("ShowStamina");
		ConfigManager::GetInstance().Del("ShowTough");
		ConfigManager::GetInstance().Del("ShowStagger");
		ConfigManager::GetInstance().Del("ShowEmptBuildup");
		ConfigManager::GetInstance().Del("ShowSeparator");
		ConfigManager::GetInstance().Del("SizeRatio");


		ConfigManager::GetInstance().Set<int>("ShowBasicStats", 1);
		ConfigManager::GetInstance().Set<int>("ShowElementalBuildup", 1);
		ConfigManager::GetInstance().Set<int>("ShowWeaponsDurability", 1);
		ConfigManager::GetInstance().Set<int>("ShowHealth", 1);
		ConfigManager::GetInstance().Set<int>("ShowStamina", 1);
		ConfigManager::GetInstance().Set<int>("ShowTough", 1);
		ConfigManager::GetInstance().Set<int>("ShowStagger", 1);
		ConfigManager::GetInstance().Set<int>("ShowEmptBuildup", 1);
		ConfigManager::GetInstance().Set<int>("ShowSeparator", 1);
		ConfigManager::GetInstance().Set<float>("SizeRatio", 1.0f);
	}

	ConfigManager::GetInstance().SaveData();

	if (D3D12Hook::GetInstance().IsHooked())
		D3D12Hook::GetInstance().Unhook();

	if (!WindowsMessageHook::GetInstance().IsHookIntact())
		WindowsMessageHook::GetInstance().Unhook();

	if (ImGui::GetCurrentContext() != nullptr)
	{
		if (ImGui::GetIO().BackendRendererUserData != nullptr)
			ImGui_ImplDX12_Shutdown();
		if (ImGui::GetIO().BackendPlatformUserData != nullptr)
			ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	iD3D12.frameContexts.clear();
	iD3D12.pCmdAllocator = nullptr;
	iD3D12.pCmdList = nullptr;
	iD3D12.pRTVHeapDesc = nullptr;
	iD3D12.pSRVHeapDesc = nullptr;

	imGuiWindows.clear();
}

void LoPBars::ConfigImGui()
{
	ImGui::StyleColorsDark();

	auto& style = ImGui::GetStyle();
	style.WindowRounding = 4.0f;
	style.ChildRounding = 4.0f;
	style.PopupRounding = 4.0f;
	style.FrameRounding = 4.0f;
	style.ScrollbarRounding = 9.0f;
	style.FrameBorderSize = 1.0f;

	style.SelectableTextAlign = ImVec2(0.5f, 0.5f);
	style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
	style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
	style.SeparatorTextAlign = ImVec2(0.5f, 0.5f);

	style.WindowMenuButtonPosition = ImGuiDir_None;

	auto& colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Border] = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.155f, 0.155f, 0.155f, 0.5f);

	auto& io = ImGui::GetIO();
	io.IniFilename = "lop_bars.ini";
	io.LogFilename = nullptr;
}

bool LoPBars::InitializeD3D12()
{
	auto& hkD3D12 = D3D12Hook::GetInstance();

	auto swapChain = hkD3D12.GetSwapChain();
	auto device = hkD3D12.GetDevice();
	UINT bufferCount = (UINT)iD3D12.frameContexts.size();

	LOG_INFO("Creating RTV heap descriptor...");
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = { };

		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		heapDesc.NumDescriptors = bufferCount;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		heapDesc.NodeMask = 1;

		if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&iD3D12.pRTVHeapDesc))))
		{
			LOG_ERROR("Failed to create RTV heap descriptor");
			return false;
		}
	}

	LOG_INFO("Creating SRV heap descriptor...");
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = { };

		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.NumDescriptors = bufferCount;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&iD3D12.pSRVHeapDesc))))
		{
			LOG_ERROR("Failed to create SRV heap descriptor");
			return false;
		}
	}

	LOG_INFO("Creating command allocator...");
	if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&iD3D12.pCmdAllocator))))
	{
		LOG_ERROR("Failed to create command allocator");
		return false;
	}

	LOG_INFO("Creating command list...");
	if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, iD3D12.pCmdAllocator.Get(), nullptr, IID_PPV_ARGS(&iD3D12.pCmdList))))
	{
		LOG_ERROR("Failed to create command list");
		return false;
	}

	if (FAILED(iD3D12.pCmdList->Close()))
	{
		LOG_ERROR("Failed to close command list");
		return false;
	}

	LOG_INFO("Creating render targets...");

	const auto iRTVDescSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	D3D12_CPU_DESCRIPTOR_HANDLE hCPUDesc = iD3D12.pRTVHeapDesc->GetCPUDescriptorHandleForHeapStart();

	for (size_t i = 0; i < iD3D12.frameContexts.size(); ++i)
	{
		iD3D12.frameContexts[i].hCPUDesc = hCPUDesc;
		swapChain->GetBuffer((UINT)i, IID_PPV_ARGS(&iD3D12.frameContexts[i].pRTV));
		device->CreateRenderTargetView(iD3D12.frameContexts[i].pRTV.Get(), nullptr, hCPUDesc);
		hCPUDesc.ptr += iRTVDescSize;
	}

	D3D12_RESOURCE_DESC desc = iD3D12.frameContexts[0].pRTV->GetDesc();

	if (!ImGui_ImplDX12_Init(
		device,
		(int)bufferCount,
		desc.Format,
		iD3D12.pSRVHeapDesc.Get(),
		iD3D12.pSRVHeapDesc->GetCPUDescriptorHandleForHeapStart(),
		iD3D12.pSRVHeapDesc->GetGPUDescriptorHandleForHeapStart()
	))
	{
		LOG_ERROR("Failed to initialize ImGui");
		return false;
	}

	ImGui_ImplDX12_CreateDeviceObjects();

	LOG_INFO("D3D12 initialization completed!");

	return true;
}

void LoPBars::OnPresentD3D12()
{
	if (!bIsInitialized)
	{
		if (!Initialize())
		{
			LOG_ERROR("Failed to initialize! Attempt " << std::dec << initAttemptCount + 1 << "/" << MAX_INIT_ATTEMPTS);

			if (++initAttemptCount >= MAX_INIT_ATTEMPTS)
			{
				MessageBox(0, L"Failed to initialize D3D12.", L"LoP Bars", MB_OK | MB_ICONERROR);
				Cleanup();
			}
			return;
		}

		bIsInitialized = true;
		initAttemptCount = 0;
		LOG_INFO("LopBars initialized!");
	}

	auto& hkD3D12Hook = D3D12Hook::GetInstance();
	auto swapChain = hkD3D12Hook.GetSwapChain();

	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

	for (auto &wnd : imGuiWindows)
		wnd->OnDraw();
	ImGui::EndFrame();

	auto& frameContext = iD3D12.frameContexts[swapChain->GetCurrentBackBufferIndex()];
	iD3D12.pCmdAllocator->Reset();

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = frameContext.pRTV.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	iD3D12.pCmdList->Reset(iD3D12.pCmdAllocator.Get(), NULL);
	iD3D12.pCmdList->ResourceBarrier(1, &barrier);
	iD3D12.pCmdList->OMSetRenderTargets(1, &frameContext.hCPUDesc, FALSE, NULL);
	iD3D12.pCmdList->SetDescriptorHeaps(1, iD3D12.pSRVHeapDesc.GetAddressOf());

	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), iD3D12.pCmdList.Get());

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

	iD3D12.pCmdList->ResourceBarrier(1, &barrier);
	iD3D12.pCmdList->Close();
	ID3D12CommandList* const pCmdList[] = { iD3D12.pCmdList.Get() };
	hkD3D12Hook.GetCommandQueue()->ExecuteCommandLists(1, pCmdList);
}

void LoPBars::OnPostPresentD3D12()
{

}

void LoPBars::OnDeviceReset()
{
	bIsInitialized = false;

	if (ImGui::GetCurrentContext() != nullptr)
	{
		if (ImGui::GetIO().BackendRendererUserData != nullptr)
			ImGui_ImplDX12_Shutdown();
		if (ImGui::GetIO().BackendPlatformUserData != nullptr)
			ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	iD3D12.frameContexts.clear();
	iD3D12.pCmdAllocator = nullptr;
	iD3D12.pCmdList = nullptr;
	iD3D12.pRTVHeapDesc = nullptr;
	iD3D12.pSRVHeapDesc = nullptr;

	for (auto &wnd : imGuiWindows)
		wnd->OnReset();
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool LoPBars::OnWindowsMsg(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	if (!bIsInitialized)
		return true;

	ImGui_ImplWin32_WndProcHandler(hwnd, iMsg, wParam, lParam);

	bool anyFalse = false;
	for (auto &wnd : imGuiWindows)
	{
		if (!wnd->OnMessage(hwnd, iMsg, wParam, lParam))
			anyFalse = true;
	}

	return !anyFalse;
}
