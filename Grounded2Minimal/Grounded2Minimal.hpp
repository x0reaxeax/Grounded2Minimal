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

///////////////////////////////////////////////////////////////
/// Globals

// Debug console visibility status [enabled/disabled]
extern bool ShowDebugConsole;
// Version information
extern VersionInfo GroundedMinimalVersionInfo;
// Cached player list
extern std::vector<SDK::APlayerState*> g_vPlayers;
// Cached local player ID
extern int32_t g_iLocalPlayerId;
// Cached world instance
extern SDK::UWorld* g_lpWorld;


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