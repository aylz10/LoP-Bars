#pragma once

#define NOMINMAX

#include <mutex>
#include <thread>
#include <chrono>
#include "imgui.h"
#include "ImGuiWindow.hpp"
#include "utility/FunctionHook.hpp"
//#include "Game/Matrix.h"
//#include "Game/Vector2D.h"
//#include "Game/Vector.h"
//#include "Game/Rotator.h"
//#include "Game/IntRect.h"

using namespace std::string_view_literals;

struct WND_CONTEXT
{
	bool bShowWindow;
	bool bEnableDrag;
	bool bCustomPosition;
};

struct MUTEX_WND_CONTEXT : WND_CONTEXT
{
	std::mutex mutex;

	inline void GetData(WND_CONTEXT& context)
	{
		std::scoped_lock _{ mutex };
		memcpy(&context, this, sizeof(WND_CONTEXT));
	}
};

// --------------------------------ELEMENTAL BUILDUP---------------------------------------
enum E_TYPE : unsigned char
{
	UNKNOWN = 0,
	FIRE,
	ELETRIC,
	ACID,
	COUNT,
};

// Elements Max resist offset from StatList
static const unsigned int E_RESIST_OFFSET[] =
{
	0xFFFFFFFF, // Unknown
	0x16BC, // Fire
	0x16EC, // Eletric
	0x171C, // Acid
};

static const ImVec4 E_COLOR[] =
{
	ImVec4(0.0f, 0.0f, 0.0f, 0.0f), // Unknown
	ImVec4(0.8f, 0.4f, 0.0f, 1.0f), // Fire
	ImVec4(0.0f, 0.3f, 0.8f, 1.0f), // Eletric
	ImVec4(0.0f, 0.5f, 0.4f, 1.0f), // Acid
};

static const std::string_view E_NAME[] =
{
	"Unknown"sv,
	"Fire"sv,
	"Eletric"sv,
	"Acid"sv,
};

struct E_BAR
{
	float fValues[2];
	std::string sBarText;
};

static E_BAR E_BAR_LIST[E_TYPE::COUNT] = { 0 };
// ----------------------------------------------------------------------------------------

enum ENTITY_FACTION : unsigned char
{
	F_NONE = 0,
	F_PLAYER,
	F_MONSTER,
	F_ALLENEMY,
	F_NEUTRAL,

	F_PLAYER_HITENABLE,
	F_MONSTER_PUPPET,
	F_MONSTER_CARCASS,
	F_MONSTER_REBORNER,
	F_MONSTER_STALKER,
	F_ENEMY_TOONLYPLAYER,
	F_MONSTER_CARCASSNPUPPET,

	F_MAX,
	F_DEFAULT,
};

enum ENTITY_STATS : unsigned char
{
	S_NONE = 0,

	S_HEALTH_POINT_CURRENT = 1,
	S_STAMINA_POINT_CURRENT = 2,
	S_TOUGH_POINT_CURRENT = 9,
	S_GROGGY_POINT_CURRENT = 48,

	S_HEALTH_POINT_MAX = 71,
	S_STAMINA_POINT_MAX = 72,
	S_TOUGH_POINT_MAX = 76,
	S_GROGGY_POINT_MAX = 78,

	S_MAX = 190,
	S_DEFAULT = 191,
};

struct LIST_DATA
{
	char* pList;
	int iSize;
};

struct ENTITY_PTRS
{
	unsigned char bFaction;
	char* pBase;
	LIST_DATA statList;
	LIST_DATA buffList;
	LIST_DATA maxStatMulList;
	LIST_DATA abnormalStatList;
	LIST_DATA weaponList;
};

//struct POV
//{
//	FVector location;
//	FRotator rotation;
//	float fov;
//	float desiredFOV;
//	float orthoWidth;
//	float orthoNearClipPlane;
//	float orthoFarClipPlane;
//	float aspectRatio;
//};

static const short GET_LOCKONSYSTEM_FN_SIG[] =
{
	0xC1, 0xE8, 0x1D,
	0xA8, 0x01,
	0x0F, 0x85, -1, -1, -1, -1,
	0x41, 0x80, 0xFD, 0x0D,
	0x0F, 0x84, -1, -1, -1, -1,
	0x40, 0xB7, 0x01,
	0xE9, -1, -1, -1, -1,
	0xB3, 0x01,
	0xE8, -1, -1, -1, -1,
	0x45, 0x0F, 0xB6, 0x8E, -1, -1, -1, -1,
	0x49, 0x8D, 0x4F, 0x08,
};

static const size_t GET_LOCKONSYSTEM_FN_SIG_OFFSET = 0x0;

static const short GET_MAX_DURABILITY_FN_SIG[] =
{
	0x40, 0x57,
	0x48, 0x83, 0xEC, 0x20,
	0x48, 0x8B, 0xB9, 0x38, 0x03, 0x00, 0x00,
	0x48, 0x85, 0xFF,
	0x0F, 0x84, -1, -1, -1, -1,
	0x48, 0x89, 0x5C, 0x24, 0x30,
	0x48, 0x63, 0x5F, 0x50,
	0xE8, -1, -1, -1, -1,
	0x8B, 0x97, 0xD0, 0x00, 0x00, 0x00,
	// fix 1.5
	//0x48, 0x89, 0xC1,
	//0xE8, -1, -1, -1, -1,
	//0x48, 0x85, 0xC0,
	//0x74,
};

static const size_t GET_MAX_DURABILITY_OFFSET = 0x0;

typedef int(__stdcall* GetMaxDurability)(void* pWeapon);

class EntityBars : public ImGuiWindow
{
public:
	EntityBars();
	~EntityBars() override;
	void OnDraw() override;
	bool OnInitialize() override;
	void OnReset() override;
	bool OnMessage(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam) override;
private:
	bool bIsInitialized;
	MUTEX_WND_CONTEXT context;

	char* pLockOnSystemStaticPtr;
	ENTITY_PTRS target;
	GetMaxDurability fnGetMaxDurability;

	//inline void ShowTestWindow(const FVector& headTagPos);
	inline void ShowBasicStats(int iValues[], float fValues[], const ImVec2& progressBarSize);
	inline void ShowElementalBuildup(const ImVec2& progressBarSize);
	inline void ShowWeaponsDurability(int iValues[], const ImVec2& progressBarSize);
};
