// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#ifndef _GROUNDED2_MINIMAL_HPP
#define _GROUNDED2_MINIMAL_HPP

#include <Windows.h>
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

struct VersionInfo {
    DWORD major;
    DWORD minor;
    DWORD patch;
    DWORD build;
};

typedef uint8_t ubool;
typedef void (*ProcessEvent_t)(const SDK::UObject *, SDK::UFunction *, void *);

struct GameOptions {
    std::atomic<ubool> BuildAnywhere{ false };
    std::atomic<ubool> GodMode{ false };
    std::atomic<ubool> InfiniteStamina{ false };
};

struct G2MOptions {
    std::atomic<ubool> bRunning{ true };                // Main console input loop control
    std::atomic<ubool> bShowDebugConsole{ true };       // Debug console visibility status
    std::atomic<ubool> bHideAutoPlayerDbgInfo{ true };  // Automatic player debug info control flag
    std::atomic<bool> bIsGamePaused{ false };           // Game paused state
};

struct ProcessEventParams {
    SDK::UObject* lpObject = nullptr;
    SDK::UFunction* lpFunction = nullptr;
    void* lpParams = nullptr;
};

struct UnrealCache {
    SDK::UWorld* lpWorld = nullptr;
    int32_t iLocalPlayerId = -1;
    std::vector<SDK::APlayerState*> vPlayers = {};
}; // TODO: use this

///////////////////////////////////////////////////////////////
/// Globals


extern G2MOptions g_G2MOptions;

// Version information
extern VersionInfo GroundedMinimalVersionInfo;
// Cached player list
extern std::vector<SDK::APlayerState*> g_vPlayers;
// Cached local player ID
extern int32_t g_iLocalPlayerId;
// Cached world instance
extern SDK::UWorld* g_lpWorld;

// Game options
extern GameOptions g_GameOptions;

//////////////////////////////////////////////////////////////////
/// Function declarations

// Hide debug console
void HideConsole(
    void
);

// Show debug console
void ShowConsole(
    void
);

#endif // _GROUNDED2_MINIMAL_HPP