// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#ifndef _GROUNDED_MINIMAL_HPP
#define _GROUNDED_MINIMAL_HPP

#include <Windows.h>

//#include "SDK.hpp"
#include "SDK/Basic.hpp"
#include "SDK/Maine_structs.hpp"
#include "SDK/Maine_classes.hpp"
#include "SDK/Maine_parameters.hpp"
#include "SDK/MaineCommon_classes.hpp"
#include "SDK/MaineCommon_parameters.hpp"
#include "SDK/Engine_structs.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/Engine_parameters.hpp"
#include "SDK/BP_SurvivalGameMode_classes.hpp"
#include "SDK/BP_SurvivalGameMode_parameters.hpp"
#include "SDK/BP_SurvivalPlayerCharacter_classes.hpp"
#include "SDK/BP_SurvivalPlayerCharacter_parameters.hpp"
#include "SDK/BP_WorldItem_classes.hpp"
#include "SDK/UI_ChatLog_classes.hpp"
#include "SDK/UI_ChatLog_parameters.hpp"

#define DLLEXPORT __declspec(dllexport)
#define GameThread  // Can only be executed from the game thread

struct VersionInfo {
    DWORD major;
    DWORD minor;
    DWORD patch;
    DWORD build;
};

typedef void (*ProcessEvent_t)(const SDK::UObject *, SDK::UFunction *, LPVOID);

typedef BOOL (WINAPI *MiniDumpReadDumpStream_t)(
    PVOID,
    ULONG,
    LPVOID,
    PVOID,
    ULONG
);

typedef BOOL (WINAPI *MiniDumpWriteDump_t)(
    HANDLE,
    DWORD,
    HANDLE,
    UINT32,
    LPVOID,
    LPVOID,
    LPVOID
);

extern bool ShowDebugConsole;
extern bool GlobalOutputEnabled;
extern VersionInfo GroundedMinimalVersionInfo;
extern std::vector<SDK::APlayerState*> g_vPlayers;

////////////////////////////////////
/// Functions

bool WINAPI SideLoadInit(
    void
);

bool StringContainsCaseInsensitive(
    const std::string& szHaystack,
    const std::string& szNeedle
);

void HideConsole(
    void
);

void ShowConsole(
    void
);

bool IsDebugModeEnabled(void);
void EnableGlobalOutput(void);
void DisableGlobalOutput(void);

// NULL check with error logging
bool CheckNullAndLog(const void* lpPtr, const std::string& szPtrName, const std::string& szContext = "");

// Console output with prefix
void LogChar(const char cChar, bool bOnlyDebug = false);
void LogMessage(const std::string& szPrefix, const std::string& szMessage, bool bOnlyDebug = false);
void LogMessage(const std::wstring& wszPrefix, const std::wstring& wszMessage, bool bOnlyDebug = false);
void LogError(const std::string& szPrefix, const std::string& szMessage, bool bOnlyDebug = false);

#endif // _GROUNDED_MINIMAL_HPP