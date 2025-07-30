// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

/// @file   dllmain.cpp
/// @brief  Main entry point for the GroundedMinimal DLL and friends.
///         This code is pure meme shit, i hate this language,
///         but shit somehow works, so let's just take the UD win for now
/// 
/// @author x0reaxeax
///         https://github.com/x0reaxeax/GroundedMinimal
/// 
/// @credits
///         https://github.com/Encryqed/Dumper-7
/// 
/// 
/// C++17 and higher is required.
/// 
/// You need a valid SDK dump to compile this tool.
/// Dump it via Dumper-7, then paste it inside $(ProjectDir):
/// GroundedMinimal [$(ProjectDir)]:
/// |   dllmain.cpp
/// |   blah blah blah cpp/hpp
/// |   NameCollisions.inl
/// |   PropertyFixup.hpp
/// |   SDK.hpp
/// |   UnrealContainers.hpp
/// |   UtfN.hpp
/// +---SDK/
/// |       sdk gen cpp/hpp
/// |       blah blah blah
/// 
/// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
/// !!! PLEASE BACKUP YOUR SAVE FILE BEFORE USING THIS PROGRAM !!!
/// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

#include "GroundedMinimal.hpp"
#include "CheatManager.hpp"
#include "HookManager.hpp"
#include "UnrealUtils.hpp"
#include "ItemSpawner.hpp"
#include "C2Cycle.hpp"
#include "Command.hpp"
#include "Summon.hpp"
#include "WinGUI.hpp"

#include <vector>
#include <mutex>

#include <algorithm>
#include <cctype>

#pragma comment (lib, "Version.lib")

struct ConsoleCommand {
    std::string szCommand;
    std::string szDescription;
    void (*fnHandler)();
};

VersionInfo GroundedMinimalVersionInfo = { 0 };

// Global bs (boolean settings, of course)
bool ShowDebugConsole = true;
bool GlobalOutputEnabled = true;

HWND g_hConsole = nullptr;
GLOBALHANDLE g_hThread = nullptr;
ProcessEvent_t OriginalProcessEvent = nullptr;
ProcessEvent_t OriginalChatBoxWidgetProcessEvent = nullptr;

static bool g_bRunning = true;
static bool g_bDebug = false;

/*
inline volatile bool CheckGlobalOutputEnabled(void) {
    volatile bool bForceRead = *((volatile bool *) &GlobalOutputEnabled);
    return bForceRead;
}
*/
// no way bro tried to prevent compiler optimizations that were not even there.
// now we have to use this macro, because replacing 5 instances of this function is not worth it.
// this should be a clear indicator that this code is definitely god-tier.
#define CheckGlobalOutputEnabled() (GlobalOutputEnabled)


///////////////////////////////////////////////////////
/// Some helper stuff

// yes these are wrapped, deal with it.

bool GetVersionFromResource(
    VersionInfo& versionInfo
) {
    HMODULE hModule = GetModuleHandleA("Grounded2Minimal.dll");

    HRSRC hResource = FindResource(
        hModule,
        MAKEINTRESOURCE(VS_VERSION_INFO),
        RT_VERSION
    );
    if (nullptr == hResource) {
        return false;
    }

    HGLOBAL hGlobal = LoadResource(hModule, hResource);
    if (nullptr == hGlobal) {
        return false;
    }

    LPVOID pVersionInfo = LockResource(hGlobal);
    if (!pVersionInfo) {
        return false;
    }

    VS_FIXEDFILEINFO* pFileInfo = nullptr;
    UINT uiLen;

    if (VerQueryValueW(
        pVersionInfo,
        L"\\",
        (LPVOID*) &pFileInfo,
        &uiLen
    )) {
        versionInfo.major = HIWORD(pFileInfo->dwFileVersionMS);
        versionInfo.minor = LOWORD(pFileInfo->dwFileVersionMS);
        versionInfo.patch = HIWORD(pFileInfo->dwFileVersionLS);
        versionInfo.build = LOWORD(pFileInfo->dwFileVersionLS);
        return true;
    }

    return false;
}

void EnableGlobalOutput(void) {
    GlobalOutputEnabled = true;
}

void DisableGlobalOutput(void) {
    GlobalOutputEnabled = false;
}

bool IsDebugModeEnabled(void) {
    return g_bDebug;
}

bool StringContainsCaseInsensitive(
    const std::string& szHaystack,
    const std::string& szNeedle
) {
    std::string szHaystackLower = szHaystack;
    std::string szNeedleLower = szNeedle;

    std::transform(
        szHaystackLower.begin(), 
        szHaystackLower.end(), 
        szHaystackLower.begin(),
        [](unsigned char c) { 
            return std::tolower(c); 
        }
    );

    std::transform(
        szNeedleLower.begin(), 
        szNeedleLower.end(), 
        szNeedleLower.begin(),
        [](unsigned char c) { 
            return std::tolower(c); 
        }
    );

    return szHaystackLower.find(szNeedleLower) != std::string::npos;
}

///////////////////////////////////////////////////////
/// Logging n stuff

bool CheckNullAndLog(
    const void* lpcPtr, 
    const std::string& szPtrName, 
    const std::string& szContext
) {
    if (nullptr == lpcPtr) {
        std::string szFullContext = szContext.empty() ? "General" : szContext;
        if (CheckGlobalOutputEnabled()) {
            std::cout << "[" << szFullContext << "] " << szPtrName << " is NULL." << std::endl;
        }
        return true;
    }
    return false;
}

void LogChar(
    const char cChar,
    bool bOnlyDebug
) {
    if (!CheckGlobalOutputEnabled() && !g_bDebug) {
        return;
    }
    
    if (bOnlyDebug && !g_bDebug) {
        return; // Skip logging if debug mode is off
    }
    
    std::cout << cChar << std::endl;
}

void LogMessage(
    const std::string& szPrefix, 
    const std::string& szMessage,
    bool bOnlyDebug
) {
    if (!CheckGlobalOutputEnabled() && !g_bDebug) {
        return;
    }
    
    if (bOnlyDebug && !g_bDebug) {
        return; // Skip logging if debug mode is off
    }
    std::cout << "[" << szPrefix << "] " << szMessage << std::endl;
}

void LogMessage(
    const std::wstring& wszPrefix,
    const std::wstring& wszMessage,
    bool bOnlyDebug
) {
    if (!CheckGlobalOutputEnabled() && !g_bDebug) {
        return;
    }
    if (bOnlyDebug && !g_bDebug) {
        return; // Skip logging if debug mode is off
    }
    
    std::wcout << L"[" << wszPrefix << L"] " << wszMessage << std::endl;
}

void LogError(const std::string& szPrefix, const std::string& szMessage, bool bOnlyDebug) {
    if (!CheckGlobalOutputEnabled() && !g_bDebug) {
        return;
    }

    if (bOnlyDebug && !g_bDebug) {
        return; // Skip logging if debug mode is off
    }
    std::cout << "[" << szPrefix << "] ERROR: " << szMessage << std::endl;
}

///////////////////////////////////////////////////////
/// Hooked game functions

void __fastcall _HookedProcessEvent(
    SDK::UObject* lpObject,
    SDK::UFunction* lpFunction,
    LPVOID lpParams
) {
    OriginalProcessEvent(lpObject, lpFunction, lpParams);

    if (Command::CommandBufferCookedForExecution.load()) {
        Command::ProcessCommands();
    }
}

void __fastcall _HookedChatBoxProcessEvent(
    SDK::UObject* lpObject,
    SDK::UFunction* lpFunction,
    LPVOID lpParams
) {
    OriginalChatBoxWidgetProcessEvent(lpObject, lpFunction, lpParams);

    if (!lpObject->IsA(SDK::UUI_ChatLog_C::StaticClass()) || !lpObject->IsA(SDK::UChatBoxWidget::StaticClass())) {
        return;
    }

    if (lpFunction->GetName().contains("HandleChatMessageReceived")) {
        SDK::FChatBoxMessage *lpMessage = static_cast<SDK::FChatBoxMessage *>(lpParams);
        if (nullptr == lpMessage || nullptr == lpMessage->SenderPlayerState) {
            return;
        }

        if (!lpMessage->Message.ToString().empty()) {
            // dirty check if it's c2
            if (StringContainsCaseInsensitive(
                lpMessage->Message.ToString(),
                "/c2cycle"
            )) {
                if (
                    C2Cycle::GlobalC2Authority 
                    || 
                    lpMessage->SenderPlayerState->HasAuthority()    // Host can trigger this, why not
                ) {
                    /*DisableGlobalOutput();
                    C2Cycle::C2Cycle();
                    EnableGlobalOutput();*/
                    Command::SubmitTypedCommand<void>(
                        Command::CommandId::CmdIdC2Cycle,
                        nullptr
                    );
                }
                return;
            }
        }

        if (!ItemSpawner::GlobalCheatMode) {
            // get out
            return;
        }

        // create a new copy of the hoe
        auto* lpSafeCopy = new ItemSpawner::SafeChatMessageData{
            .iSenderId = lpMessage->SenderPlayerState->PlayerId,
            .szMessage = lpMessage->Message.ToString(),
            .szSenderName = lpMessage->SenderPlayerState->GetPlayerName().ToString(),
            .Color = lpMessage->Color,
            .Type = lpMessage->Type
        };

        std::thread([lpSafeCopy]() {
            ItemSpawner::EvaluateChatSpawnRequestSafe(lpSafeCopy);
            delete lpSafeCopy; // clean up that copy boi
        }).detach();
    }
}

///////////////////////////////////////////////////////
/// Input related sheit

static int32_t ResolvePlayerId(
    const std::string& szInput
) {
    int32_t iPlayerId = -1;
    int32_t iLocalPlayerId = UnrealUtils::GetLocalPlayerId();
    
    if (!szInput.empty()) {
        try {
            iPlayerId = std::stoi(szInput);
        } catch (const std::exception& e) {
            // Fall back to local player ID if available
            if (iLocalPlayerId == -1) {
                LogError("PlayerResolver", "Unable to determine local player ID");
                return -1;
            }
            LogMessage("PlayerResolver", "Using local player ID (" + std::to_string(iLocalPlayerId) + ")");
            return iLocalPlayerId;
        }
    } else {
        if (iLocalPlayerId == -1) {
            LogError("PlayerResolver", "Unable to determine player ID");
            return -1;
        }
        LogMessage("PlayerResolver", "Using local player ID (" + std::to_string(iLocalPlayerId) + ")");
        return iLocalPlayerId; // use local player ID if input is empty
    }
    
    return iPlayerId;
}

static bool ReadInterpreterInput(
    const std::string& szPrompt,
    std::string& szOutInput
) {
    char szInput[512] = { 0 };

    std::cout << szPrompt << std::flush;

    if (nullptr == fgets(
        szInput,
        sizeof(szInput) - 1, 
        stdin
    )) {
        LogError("Input", "Failed to read input.");
        return false;
    }

    // strip newline bih
    szInput[strcspn(szInput, "\n")] = '\0';

    std::string s(szInput);

    // trim sucka front
    size_t start = s.find_first_not_of(" \t\r\n");
    // and trim sucka end
    size_t end = s.find_last_not_of(" \t\r\n");

    if (start == std::string::npos || end == std::string::npos) {
        szOutInput = "";
    } else {
        szOutInput = s.substr(start, end - start + 1);
    }

    return true;
}

static int32_t ReadIntegerInput(
    const std::string& szPrompt, 
    int32_t iDefaultValue = -1
) {
    std::string szInput;
    if (!ReadInterpreterInput(szPrompt, szInput)) {
        LogError("Input", "No input provided", true);
        return iDefaultValue;
    }

    try {
        return std::stoi(szInput);
    } catch (const std::exception& e) {
        LogError("Input", "Invalid number format", true);
        return iDefaultValue;
    }
}

///////////////////////////////////////////////////////
/// Supa command handlerz

void HandleCullItem(void) {
    int32_t iItemIndex = ReadIntegerInput("[Cull] Enter item index to cull: ");
    if (iItemIndex < 0) {
        LogError("Cull", "Invalid item index");
        return;
    }
    C2Cycle::CullItemByItemIndex(iItemIndex);
}

void HandleCullItemType(void) {
    std::string szItemName;
    if (!ReadInterpreterInput("[Cull] Enter item name to cull: ", szItemName)) {
        LogError("Cull", "Invalid input, please provide an item name");
        return;
    }
    C2Cycle::CullAllItemInstances(szItemName);
}

void HandleSpawnItem(void) {
    std::string szItemName, szDataTableName;
    
    if (
        !ReadInterpreterInput("[ItemSpawner] Enter item to spawn: ", szItemName) 
        ||
        !ReadInterpreterInput("[ItemSpawner] Enter DataTable name: ", szDataTableName
    )) {
        LogError("ItemSpawner", "Invalid input, please provide all fields");
        return;
    }

    int32_t iPlayerId = ReadIntegerInput(
        "[ItemSpawner] Enter player ID (empty for local): ", 
        UnrealUtils::GetLocalPlayerId()
    );
    int32_t iItemCount = ReadIntegerInput(
        "[ItemSpawner] Enter item count (default 1): ", 
        1
    );

    auto* lpParams = new ItemSpawner::BufferParamsSpawnItem{
        .iPlayerId = iPlayerId,
        .szItemName = szItemName,
        .szDataTableName = szDataTableName,
        .iCount = iItemCount
    };
    
    Command::SubmitTypedCommand(Command::CommandId::CmdIdSpawnItem, lpParams);
}

void HandleDataTableSearch(void) {
    std::string szTableName;
    if (!ReadInterpreterInput(
        "[DataTableSearch] Enter DataTable name to search: ", 
        szTableName
    )) {
        LogError("DataTableDump", "Invalid input, please provide a table name");
        return;
    }
    UnrealUtils::FindDataTableByName(szTableName);
}

void HandleItemDump(void) {
    std::string szTableName;
    if (!ReadInterpreterInput(
        "[DataTableItemDump] Enter DataTable name to dump items: ", 
        szTableName
    )) {
        LogError("DataTableItemDump", "Invalid input, please provide a table name");
        return;
    }
    
    SDK::UDataTable* lpDataTable = UnrealUtils::GetDataTablePointer(szTableName);
    if (nullptr == lpDataTable) {
        LogError("DataTableItemDump", "DataTable not found: " + szTableName);
    } else {
        UnrealUtils::ParseRowMapManually(lpDataTable);
    }
}

void HandleFindItemTable(void) {
    std::string szItemName;
    if (!ReadInterpreterInput(
        "[DataTableItemSearch] Enter item name to find DataTable for: ", 
        szItemName
    )) {
        LogError("DataTableItemSearch", "Invalid input, please provide an item name");
        return;
    }
    UnrealUtils::FindMatchingDataTableForItemName(szItemName);
}

void HandleFunctionDump(void) {
    std::string szFunctionName;
    if (!ReadInterpreterInput(
        "[FunctionDump] Enter function name to search: ", 
        szFunctionName
    )) {
        LogError("FunctionDump", "Invalid input, please provide a function name");
        return;
    }
    UnrealUtils::DumpFunctions(szFunctionName);
}

void HandleC2Cycle(void) {
    Command::SubmitTypedCommand<void>(
        Command::CommandId::CmdIdC2Cycle,
        nullptr
    );
}

void HandleSummon(void) {
    std::string szClassName;
    if (!ReadInterpreterInput(
        "[Summon] Enter class name to summon: ", 
        szClassName
    )) {
        LogError("Summon", "Invalid input, please provide a class name");
        return;
    }

    if (szClassName.empty()) {
        LogError("Summon", "Class name cannot be empty");
        return;
    }

    Summon::SummonClass(
        szClassName
    );
}

void HandleClassDump(void) {
    std::string szClassName;
    if (!ReadInterpreterInput(
        "[ClassDump] Enter class name needle to search: ", 
        szClassName
    )) {
        LogError("ClassDump", "Invalid input, please provide a class name");
        return;
    }

    UnrealUtils::DumpClasses(nullptr, szClassName);
}

void HandleGetAuthority(void) {
    if (UnrealUtils::IsPlayerHostAuthority()) {
        LogMessage("Authority", "You have authority as the host");
    } else {
        LogError("Authority", "You do not have authority as the host");
    }
}

// Super command table oh my god
ConsoleCommand g_Commands[] = {
    //{"c2", "Run C2 cleanup cycle", []() { C2Cycle::C2Cycle(); }},
    {"c2", "Run C2 cleanup cycle", HandleC2Cycle },
    {"C_CullItem", "Cull item by index", HandleCullItem },
    {"C_CullItemType", "Cull all items of type", HandleCullItemType },
    {"F_DataTableNeedle", "Search for DataTable", HandleDataTableSearch },
    {"F_ItemDump", "Dump DataTable items", HandleItemDump },
    {"F_FindItemTable", "Find DataTable for item", HandleFindItemTable },
    {"F_FunctionDump", "Dump functions", HandleFunctionDump },
    {"F_ClassDump", "Dump classes by name", HandleClassDump },
    {"H_GetAuthority", "Check host authority", HandleGetAuthority },
    {"I_SpawnItem", "Spawn item", HandleSpawnItem },
    {"P_ShowPlayers", "Show connected players", []() { UnrealUtils::DumpConnectedPlayers(); }},
    {"S_SummonClass", "Summon an internal class", HandleSummon },
    {
        "X_GlobalCheatMode", "Toggle global cheat mode", []() { 
            ItemSpawner::GlobalCheatMode = !ItemSpawner::GlobalCheatMode;
            LogMessage(
                "Cheat", 
                "Global cheat mode " + std::string(ItemSpawner::GlobalCheatMode ? "enabled" : "disabled")
            );
        }
    },
    {
        "X_DebugToggle", "Toggle debug mode", []() { 
            g_bDebug = !g_bDebug;
            LogMessage(
                "Debug", 
                "Debug mode " + std::string(g_bDebug ? "enabled" : "disabled")
            );
        }
    },
    {
        "X_UnlockCheats", "Patch EnableCheats to skip multiplayer checks", []() { 
            LogMessage(
                "CheatManager", 
                "Unlocking Multiplayer Cheat Manager..."
            );
            CheatManager::UnlockMultiplayerCheatManager();
        }
    },
    {
        "X_InitCheatManager", "Initialize SurvivalCheatManager", []() {
            LogMessage(
                "CheatManager", 
                "Initializing Survival Cheat Manager..."
            );
            CheatManager::Initialize();
        }
    }
};

// bruh
void HideConsole(
    void
) {
    if (nullptr != g_hConsole) {
        ShowWindow(static_cast<HWND>(g_hConsole), SW_HIDE);
    }
}

void ShowConsole(
    void
) {
    if (nullptr != g_hConsole) {
        ShowWindow(static_cast<HWND>(g_hConsole), SW_SHOW);
    }
}

///////////////////////////////////////////////////////
/// Entry

DWORD WINAPI ThreadEntry(
    LPVOID lpParam
) {
    HMODULE hLocalModule = static_cast<HMODULE>(lpParam);
    FILE *lpStdout = nullptr, *lpStderr = nullptr, *lpStdin = nullptr;
    AllocConsole();
    freopen_s(&lpStdout, "CONOUT$", "w", stdout);
    freopen_s(&lpStderr, "CONOUT$", "w", stderr);
    freopen_s(&lpStdin, "CONIN$", "r", stdin);

    if (!GetVersionFromResource(
        GroundedMinimalVersionInfo
    )) {
        LogError("Init", "Failed to retrieve version information from resources");
    }

    LogMessage(
        "Init",
        "Grounded2Minimal: Version " +
        std::to_string(GroundedMinimalVersionInfo.major) + "." +
        std::to_string(GroundedMinimalVersionInfo.minor) + "." +
        std::to_string(GroundedMinimalVersionInfo.patch) + "." +
        std::to_string(GroundedMinimalVersionInfo.build)
    );

    LogMessage("Init", "Grounded2Minimal: Starting hooks initialization...");
    while (nullptr == UnrealUtils::GetLocalPawn()) {
        LogMessage("Init", "Grounded2Minimal: Waiting for LocalPawn to be available...");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    if (!HookManager::InstallHook(
        UnrealUtils::GetLocalPawn(), 
        _HookedProcessEvent, 
        &OriginalProcessEvent
    )) {
        LogError("Init", "Grounded2Minimal: Failed to hook LocalPawn ProcessEvent");
        return EXIT_FAILURE;
    }

    if (!HookManager::InstallHook(
        SDK::UChatBoxWidget::GetDefaultObj(), 
        _HookedChatBoxProcessEvent, 
        &OriginalChatBoxWidgetProcessEvent
    )) {
        LogError("Init", "Grounded2Minimal: Failed to hook ChatBoxWidget ProcessEvent");
        goto _RYUJI; // lol
    }

    LogMessage("Init", "Grounded2Minimal: Hooks initialized");
    LogMessage("Init", "Grounded2Minimal: Launching GUI thread...");

    g_hConsole = GetConsoleWindow();
    if (!WinGUI::Initialize()) {
        LogError("Init", "Grounded2Minimal: Failed to initialize GUI");
        goto _RYUJI;
    }
    LogMessage("Init", "Grounded2Minimal: GUI thread launched successfully");

    while (g_bRunning) {
        Command::WaitForCommandBufferReady();

        std::string szInput;
        ReadInterpreterInput("$: ", szInput);

        if (szInput == "quit" || szInput == "exit") {
            LogMessage("Exit", "Exiting Grounded2Minimal...");
            g_bRunning = false;

            break;
        }

        // Check predefined commands
        bool bCommandFound = false;
        const int iCommandCount = sizeof(g_Commands) / sizeof(g_Commands[0]);
        
        for (int i = 0; i < iCommandCount; i++) {
            if (szInput == g_Commands[i].szCommand) {
                g_Commands[i].fnHandler();
                bCommandFound = true;
                break;
            }
        }

        // If no command found, hunt for items
        if (!bCommandFound) {
            UnrealUtils::FindSpawnedItemByType(szInput);
        }
    }

    WinGUI::Stop();

_RYUJI:
    HookManager::RestoreHooks();

    // Give time for any pending operations to complete, very safe approach, trust me bro
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Close all console streams
    if (lpStdin) {
        fclose(lpStdin);
        lpStdin = nullptr;
    }
    if (lpStdout) {
        fclose(lpStdout);
        lpStdout = nullptr;
    }
    if (lpStderr) {
        fclose(lpStderr);
        lpStderr = nullptr;
    }

    FreeConsole();

    FreeLibraryAndExitThread(hLocalModule, EXIT_SUCCESS);

    return EXIT_SUCCESS;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    switch (dwReason) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(hModule);
            //SideLoadInit();

            g_hThread = CreateThread(
                nullptr, 
                0, 
                ThreadEntry,
                (LPVOID) hModule, 
                0, 
                nullptr
            );
            break;
        }
        case DLL_PROCESS_DETACH: {
            g_bRunning = FALSE;
            break;
        }
    }
    return TRUE;
}
