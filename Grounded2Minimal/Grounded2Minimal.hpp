// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#ifndef _GROUNDED2_MINIMAL_HPP
#define _GROUNDED2_MINIMAL_HPP

#include <Windows.h>
#include <unordered_map>
#include <atomic>

#include "Logging.hpp"

#include "SDK/Basic.hpp"
#include "SDK/Maine_structs.hpp"
#include "SDK/Maine_classes.hpp"
#include "SDK/Maine_parameters.hpp"
#include "SDK/MaineCommon_classes.hpp"
#include "SDK/MaineCommon_parameters.hpp"
#include "SDK/Engine_structs.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/Engine_parameters.hpp"

#define __gamethread
#define __unused

struct VersionInfo {
    DWORD major;
    DWORD minor;
    DWORD patch;
    DWORD build;
};

typedef uint8_t ubool;
typedef void (*ProcessEvent_t)(const SDK::UObject *, SDK::UFunction *, void *);

constexpr int32_t INVALID_PLAYER_ID = -1;

struct GameOptions {
    std::atomic<ubool> BuildAnywhere{ false };
    std::atomic<ubool> GodMode{ false };
    std::atomic<ubool> InfiniteStamina{ false };
    struct _GameStatics {
        std::atomic<ubool> HandyGnatForceEnable{ false };
        std::atomic<ubool> AutoCompleteBuildings{ false };
        std::atomic<ubool> BuildingIntegrity{ false };
        std::atomic<ubool> FreeCrafting{ false };
        std::atomic<ubool> InvinciblePets{ false };
        std::atomic<float> PlayerDamageMultiplier{ 1.0f };
    } GameStatics;
    std::atomic<SDK::ABuilding *> CurrentlyAdjustedBuilding = nullptr;
};

struct G2MOptions {
    std::atomic<ubool> bRunning{ false };               // Main console input loop control
    std::atomic<ubool> bShowDebugConsole{ true };       // Debug console visibility status
    std::atomic<ubool> bHideAutoPlayerDbgInfo{ true };  // Automatic player debug info control flag
    std::atomic<bool> bIsGamePaused{ false };           // Game paused state
    bool bIsClientHost{ false };                        // Client is host flag
    GLOBALHANDLE hLogFile = nullptr;                    // Log file handle
    std::atomic<ubool> bSuperSecretDebugFlag{ false };  // Experimental debug flag for internal testing
    std::string szCurrentDirectory;                     // Current working directory, used for resolving relative paths
};

struct ProcessEventParams {
    SDK::UObject* lpObject = nullptr;
    SDK::UFunction* lpFunction = nullptr;
    void* lpParams = nullptr;
};

struct NativeProcessEventParams {
    SDK::UObject* lpObject = nullptr;
    void* lpFFrame = nullptr;
    void* lpResult = nullptr;
};

///////////////////////////////////////////////////////////////
/// Globals

// Tool options
extern G2MOptions g_G2MOptions;

// Version information
extern VersionInfo GroundedMinimalVersionInfo;

// Game options
extern GameOptions g_GameOptions;

// Console handle
extern HWND g_hConsole;

//////////////////////////////////////////////////////////////////
/// Function declarations and inline implementations

// Hide debug console
void HideConsole(
    void
);

// Show debug console
void ShowConsole(
    void
);

#endif // _GROUNDED2_MINIMAL_HPP