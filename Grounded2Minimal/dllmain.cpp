#include "Grounded2Minimal.hpp"
#include "PlayerCache.hpp"
#include "CheatManager.hpp"
#include "Interpreter.hpp"
#include "ItemSpawner.hpp"
#include "HookManager.hpp"
#include "UnrealUtils.hpp"
#include "CoreUtils.hpp"
#include "Command.hpp"
#include "Logging.hpp"
#include "WinGUI.hpp"

#include <thread>

#include "SDK/UI_ChatLog_classes.hpp"
#include "SDK/Maine_parameters.hpp"

#define _RELEASE

// Tool options
G2MOptions g_G2MOptions{};

// Main thread handle
GLOBALHANDLE g_hThread = nullptr;
// Global console handle
HWND g_hConsole = nullptr;

// Version information
VersionInfo GroundedMinimalVersionInfo = { 0 };

// Game options
GameOptions g_GameOptions;

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

///////////////////////////////////////////////////////////////
// Hooked functions

void ProcessDebugFilter(
    HookManager::ProcessEventHooker::HookData *lpHookData,
    ProcessEventParams *lpParams
) {
    if (nullptr == lpHookData) {
        return;
    }
    
    if (lpHookData->szDebugFilter.empty()) {
        return;
    }

    if (nullptr == lpParams) {
        return;
    }

    if (nullptr == lpParams->lpObject || nullptr == lpParams->lpFunction) {
        return;
    }

    if (
        CoreUtils::IsStringWildcard(lpHookData->szDebugFilter)
        ||
        CoreUtils::StringContainsCaseInsensitive(
            lpParams->lpFunction->GetFullName(),
            lpHookData->szDebugFilter
        )
    ) {
        LogMessage(
            lpHookData->szHookName,
            "Function: " + lpParams->lpFunction->GetName() + " ['" + lpParams->lpFunction->GetFullName() + "']",
            true
        );

        LogMessage(
            lpHookData->szHookName,
            "Object: " + lpParams->lpObject->GetName() + " ['" + lpParams->lpObject->GetFullName() + "']",
            true
        );
    }
}

void ProcessDebugFilter(
    HookManager::NativeHooker::HookEntry* lpHookData,
    NativeProcessEventParams* lpParams
) {
    if (nullptr == lpHookData) {
        return;
    }
    if (lpHookData->szDebugFilter.empty()) {
        return;
    }
    if (nullptr == lpParams) {
        return;
    }
    if (nullptr == lpParams->lpObject) {
        return;
    }
    if (
        CoreUtils::IsStringWildcard(lpHookData->szDebugFilter) 
        ||
        CoreUtils::StringContainsCaseInsensitive(
            lpParams->lpObject->GetFullName(),
            lpHookData->szDebugFilter
        )
    ) {
        LogMessage(
            lpHookData->szHookName,
            "Object: " + lpParams->lpObject->GetName() + " ['" + lpParams->lpObject->GetFullName() + "']",
            true
        );
    }
}

///// ProcessEvent hooks
// BP_SurvivalPlayerCharacter
PROCESSEVENTHOOK _HookedSPCProcessEvent(
    SDK::UObject *lpObject,
    SDK::UFunction *lpFunction,
    void *lpParams
) {
    using namespace HookManager;

    ProcessEventHooker::InFlightGuard inFlight;

    if (ProcessEventHooker::IsRestoring()) {
        return;
    }

    ProcessEventHooker::HookData* lpHookData = ProcessEventHooker::GetHookByHookedFunction(
        _HookedSPCProcessEvent
    );
    if (nullptr == lpHookData) {
        // catastrophic cataclysmic shit
        //throw std::runtime_error("SPCProcessEvent: Hook data not found");
        return;
    }
    // Re-entrancy guard for command processing on the same thread
    static thread_local bool s_InProcessCommands = false;

    // Check for Engine.PlayerController:ClientRestart
    if (lpFunction->GetName().contains("ClientRestart")) {
        // Invalidate player cache on client restart, as the player state will be recreated
        // as of right now, this is completely useless, cuz all hooks will still be present
        PlayerCache::InvalidateCache();
    } else { // Discard request cuz it might hold invalid pointers
        // Process pending commands before calling the original, to avoid
        // nested ProcessEvent re-entry while the flag is still set.
        if (!s_InProcessCommands && Command::CommandBufferCookedForExecution.load()) {
            s_InProcessCommands = true;
            Command::ProcessCommands();
            s_InProcessCommands = false;
        }
    }

    // Call the original ProcessEvent
    lpHookData->OriginalFn(lpObject, lpFunction, lpParams);

    if (lpHookData->bDebugFilterEnabled.load()) {
        ProcessEventParams funcParams{
            lpObject,
            lpFunction,
            lpParams
        };
        ProcessDebugFilter(lpHookData, &funcParams);
    }
    // Do NOT process here anymore to avoid re-entrancy while commands run
}

PROCESSEVENTHOOK _HookedChatBoxProcessEvent(
    SDK::UObject* lpObject,
    SDK::UFunction* lpFunction,
    LPVOID lpParams
) {
    using namespace HookManager;

    HookManager::ProcessEventHooker::InFlightGuard inFlight;

    if (ProcessEventHooker::IsRestoring()) {
        return;
    }

    ProcessEventHooker::HookData* lpHookData = ProcessEventHooker::GetHookByHookedFunction(
        _HookedChatBoxProcessEvent
    );
    if (nullptr == lpHookData) {
        // catastrophic cataclysmic shit
        //throw std::runtime_error("ChatBoxProcessEvent: Hook data not found");
        return;
    }
    SDK::FChatBoxMessage* lpMessage = nullptr;
    ItemSpawner::SafeChatMessageData* lpMessageDataCopy = nullptr;
    
    // Fast-path: if cheat mode is off, just pass through.
    if (!ItemSpawner::GlobalCheatMode.load()) {
        goto _RYUJI;
    }

    if (nullptr == lpObject || nullptr == lpFunction) {
        goto _RYUJI;
    }

    // Require expected types - the check logic is INTENTIONAL
    if (
        !lpObject->IsA(SDK::UUI_ChatLog_C::StaticClass())
        || 
        !lpObject->IsA(SDK::UChatBoxWidget::StaticClass())
    ) {
        goto _RYUJI;
    }

    // Only handle incoming chat events
    if (!lpFunction->GetName().contains("HandleChatMessageReceived")) {
        goto _RYUJI;
    }

    if (nullptr == lpParams) {
        goto _RYUJI;
    }
    
    lpMessage = static_cast<SDK::FChatBoxMessage*>(lpParams);
    if (lpMessage && lpMessage->SenderPlayerState) {
        lpMessageDataCopy = new ItemSpawner::SafeChatMessageData{
            lpMessage->SenderPlayerState->PlayerId,
            lpMessage->Message.ToString(),
            lpMessage->SenderPlayerState->GetPlayerName().ToString(),
            lpMessage->Color,
            lpMessage->Type
        };
    }

_RYUJI:
    // Call the original event handler
    lpHookData->OriginalFn(lpObject, lpFunction, lpParams);

    // Launch async evaluation if we captured a message
    if (nullptr != lpMessageDataCopy) {
        try {
            std::thread([lpMessageDataCopy]() {
                ItemSpawner::EvaluateChatSpawnRequestSafe(lpMessageDataCopy);
                delete lpMessageDataCopy;
            }).detach();
        } catch (const std::exception& e) {
            LogError(
                "ChatBoxProcessEvent",
                "Exception launching chat evaluation thread: " + std::string(e.what())
            );
            delete lpMessageDataCopy;
        } catch (...) {
            LogError(
                "ChatBoxProcessEvent",
                "Unknown exception launching chat evaluation thread"
            );
            delete lpMessageDataCopy;
        }
    }

    if (lpHookData->bDebugFilterEnabled.load()) {
        ProcessEventParams funcParams{
            lpObject,
            lpFunction,
            lpParams
        };
        ProcessDebugFilter(lpHookData, &funcParams);
    }
}

PROCESSEVENTHOOK _HookedGameModeBaseProcessEvent(
    SDK::UObject* lpObject,
    SDK::UFunction* lpFunction,
    void* lpParams
) {
    using namespace HookManager;
    
    HookManager::ProcessEventHooker::InFlightGuard inFlight;

    if (ProcessEventHooker::IsRestoring()) {
        return;
    }

    ProcessEventHooker::HookData* lpHookData = ProcessEventHooker::GetHookByHookedFunction(
        _HookedGameModeBaseProcessEvent
    );
    if (nullptr == lpHookData) {
        //throw std::runtime_error("GameModeBaseProcessEvent: Hook data not found");
        return;
    }
    lpHookData->OriginalFn(lpObject, lpFunction, lpParams);
    if (nullptr == lpObject || nullptr == lpFunction) {
        return;
    }

    if (nullptr != lpFunction) {
        if (lpFunction->GetName().contains("K2_PostLogin")) {
            SDK::Params::GameModeBase_K2_PostLogin* lpFuncParams =
                static_cast<SDK::Params::GameModeBase_K2_PostLogin*>(lpParams);
            if (nullptr != lpFuncParams) {
                SDK::APlayerController* lpNewPlayer = lpFuncParams->NewPlayer;
                if (nullptr != lpNewPlayer) {
                    LogMessage(
                        "K2_PostLogin",
                        "Updating player cache..",
                        true
                    );
                    PlayerCache::AttachCachedPlayerData(lpNewPlayer->PlayerState);
                }
            }
        } else if (lpFunction->GetName().contains("K2_Logout")) {
            SDK::Params::GameModeBase_K2_OnLogout* lpFuncParams =
                static_cast<SDK::Params::GameModeBase_K2_OnLogout*>(lpParams);
            if (nullptr != lpFuncParams) {
                SDK::AController* lpExitingController = lpFuncParams->ExitingController;
                if (nullptr != lpExitingController) {
                    LogMessage(
                        "K2_Logout",
                        "Updating player cache..",
                        true
                    );
                    PlayerCache::RemoveCachedPlayer(lpExitingController->PlayerState);
                }
            }
        }
    }

    if (lpHookData->bDebugFilterEnabled.load()) {
        ProcessEventParams funcParams{
            lpObject,
            lpFunction,
            lpParams
        };
        ProcessDebugFilter(lpHookData, &funcParams);
    }
}

///// Native function hooks
NATIVEHOOK _HookedUpdateCollisionStateChange(
    SDK::UObject* lpObj,
    void* lpFFrame,
    void* lpResult
) {
    using namespace HookManager;

    NativeHooker::InFlightGuard inFlight;

    // If teardown is happening, don't do extra work.
    if (NativeHooker::IsRestoring()) {
        return;
    }

    NativeHooker::HookEntry* lpHookData = NativeHooker::GetHookByHookedFunction(
        &_HookedUpdateCollisionStateChange
    );

    if (nullptr == lpHookData) {
        // catastrophic cataclysmic shit
        //throw std::runtime_error("UpdateCollisionStateChange: Hook data not found");
        return;
    }

    SDK::ABuilding* lpBuilding = nullptr;
    if (!g_GameOptions.BuildAnywhere.load()) {
        goto _RYUJI;
    }

    if (nullptr == lpObj) {
        goto _RYUJI;
    }

    if (!lpObj->IsA(SDK::ABuilding::StaticClass())) {
        goto _RYUJI;
    }

    lpBuilding = static_cast<SDK::ABuilding*>(lpObj);
    if (nullptr == lpBuilding) {
        goto _RYUJI;
    }

    if (
        SDK::EBuildingGridSurfaceType::None == lpBuilding->AnchoredSurface
        ||
        SDK::EBuildingGridSurfaceType::Invalid == lpBuilding->AnchoredSurface
    ) {
        lpBuilding->AnchoredSurface = SDK::EBuildingGridSurfaceType::Default;
    }

    if (SDK::EBuildingState::BeingPlacedInvalid == lpBuilding->BuildingState) {
        lpBuilding->BuildingState = SDK::EBuildingState::BeingPlaced;
    }
    
_RYUJI:
    lpHookData->OriginalFn(lpObj, lpFFrame, lpResult);

    if (lpHookData->bDebugFilterEnabled.load()) {
        NativeProcessEventParams funcParams{
            lpObj,
            lpFFrame,
            lpResult
        };
        ProcessDebugFilter(lpHookData, &funcParams);
    }
}

NATIVEHOOK _HookedGetPlacementValid(
    SDK::UObject* lpObj,
    void* lpFFrame,
    void* lpResult
) {
    using namespace HookManager;
    NativeHooker::InFlightGuard inFlight;
    if (NativeHooker::IsRestoring()) {
        return;
    }
    NativeHooker::HookEntry* lpHookData = NativeHooker::GetHookByHookedFunction(
        (NativeHooker::NativeFunc_t) &_HookedGetPlacementValid
    );

    if (nullptr == lpHookData) {
        // catastrophic cataclysmic 
        return;
    }

    if (g_GameOptions.BuildAnywhere.load()) {
        LPBYTE lpPlacementValidBool = reinterpret_cast<LPBYTE>(lpResult);
        if (nullptr != lpPlacementValidBool) {
            *lpPlacementValidBool = (BYTE) 1;
        }
    } else {
        lpHookData->OriginalFn(lpObj, lpFFrame, lpResult);
    }

    if (lpHookData->bDebugFilterEnabled.load()) {
        NativeProcessEventParams funcParams{
            lpObj,
            lpFFrame,
            lpResult
        };
        ProcessDebugFilter(lpHookData, &funcParams);
    }
}

/////////////////////////////////////////////////////////////

DWORD WINAPI ThreadEntry(
    LPVOID lpParam
) { 
#ifndef _RELEASE
    EnableDebugOutput();
#endif // _RELEASE
    EnableGlobalOutput();

    INT iRet = EXIT_FAILURE;
    HMODULE hLocalModule = static_cast<HMODULE>(lpParam);
    FILE *lpStdout = nullptr, *lpStderr = nullptr, *lpStdin = nullptr;

    AllocConsole();
    freopen_s(&lpStdout, "CONOUT$", "w", stdout);
    freopen_s(&lpStderr, "CONOUT$", "w", stderr);
    freopen_s(&lpStdin, "CONIN$", "r", stdin);
    
    g_hConsole = GetConsoleWindow();

    // Init log file
    g_G2MOptions.hLogFile = InitalizeLogFile();
    if (nullptr == g_G2MOptions.hLogFile) {
        LogError("Init", "Failed to initialize log file");
        // we still continue
    }

    if (!CoreUtils::GetVersionFromResource(
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

    CoreUtils::GetCurrentWorkingDirectory(g_G2MOptions.szCurrentDirectory);
    LogMessage(
        "Init",
        "Current working directory: " + g_G2MOptions.szCurrentDirectory,
        true
    );

    while (nullptr == UnrealUtils::GetLocalPawn()) {
        LogMessage("Init", "Waiting for LocalPawn to be available...");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    LogMessage(
        "Init",
        "Local APawn @ " + CoreUtils::HexConvert(
            reinterpret_cast<uint64_t>(UnrealUtils::GetLocalPawn())
        ),
        true
     );

    // Host authority check
    LogMessage(
        "Init",
        "Checking client host authority"
    );

    g_G2MOptions.bIsClientHost = UnrealUtils::IsPlayerHostAuthority(
        UnrealUtils::GetLocalPlayerId()
    );

    if (!g_G2MOptions.bIsClientHost) {
        LogMessage("Init", "Client is NOT host authority, tool will have limited functionality");
    }

    LogMessage("Init", "Starting hooks initialization...");
    
    // Don't hook SPC and ChatBox events if client is not host
    if (g_G2MOptions.bIsClientHost) {
        LogMessage("Init", "Initializing SurvivalPlayerController ProcessEvent hook...", true);
        if (!HookManager::ProcessEventHooker::InstallHook(
            UnrealUtils::GetLocalSurvivalPlayerController(),
            _HookedSPCProcessEvent,    // ASurvivalPlayerController
            "SPC_ProcessEvent"
        )) {
            LogError(
                "Init",
                "Failed to hook SurvivalPlayerController ProcessEvent"
            );
            return EXIT_FAILURE;
        }

        LogMessage("Init", "Initializing ChatBoxWidget ProcessEvent hook...", true);

        if (!HookManager::ProcessEventHooker::InstallHook(
            SDK::UChatBoxWidget::GetDefaultObj(),
            _HookedChatBoxProcessEvent,
            "ChatBoxWidget_ProcessEvent"
        )) {
            LogError("Init", "Failed to hook ChatBoxWidget ProcessEvent");
            goto _RYUJI; // lol
        }
    } else {
        LogMessage("Init", "Skipping SurvivalPlayerController and ChatBoxWidget ProcessEvent hooks - no host authority");
    }

    // Idk if GameModeBase is doable for non-host clients
    LogMessage("Init", "Initializing GameModeBase ProcessEvent hook...", true);
    if (!HookManager::ProcessEventHooker::InstallHook(
        SDK::AGameModeBase::StaticClass(),
        _HookedGameModeBaseProcessEvent,
        "GameModeBase_ProcessEvent"
    )) {
        LogError(
            "Init",
            "Failed to hook GameModeBase ProcessEvent"
        );
        goto _RYUJI;
    }

    LogMessage("Init", "Initializing Building::UpdateCollisionStateChange native hook...", true);

    if (nullptr == HookManager::NativeHooker::HookNativeFunction(
        "UpdateCollisionStateChange",
        &_HookedUpdateCollisionStateChange,
        "UpdateCollisionStateChange_Native"
    )) {
        LogError(
            "Init", 
            "Failed to hook Building::UpdateCollisionStateChange"
        );
        goto _RYUJI;
    }

    LogMessage("Init", "Initializing IPlaceable::GetPlacementValid native hook...", true);
    if (nullptr == HookManager::NativeHooker::HookNativeFunction(
        "GetPlacementValid",
        &_HookedGetPlacementValid,
        "GetPlacementValid_Native"
    )) {
        LogError(
            "Init",
            "Failed to hook IPlaceable::GetPlacementValid"
        );
        goto _RYUJI;
    }

    LogMessage("Init", "Hooks initialized");

    if (g_G2MOptions.bIsClientHost) {
        LogMessage("Init", "Initializing CheatManager..");
        if (!CheatManager::ManualInitialize()) {
            LogError("Init", "Failed to initialize CheatManager");
            goto _RYUJI;
        }
        LogMessage("Init", "CheatManager initialized successfully");
    }

    // Cache initialization

    // GUI initialization
    LogMessage("Init", "Grounded2Minimal: Launching GUI thread...");

    if (!WinGUI::Initialize()) {
        LogError("Init", "Grounded2Minimal: Failed to initialize GUI");
        goto _RYUJI;
    }
    LogMessage("Init", "Grounded2Minimal: GUI thread launched successfully");

    LogMessage("Init", "Starting keybind thread...");
    // Keybinds initialization
    Interpreter::KeyBinds::Initialize();

    // ready to process commands
    g_G2MOptions.bRunning.store(true);
    while (g_G2MOptions.bRunning.load()) {
        Command::WaitForCommandBufferReady();

        std::string szInput;
        Interpreter::ReadInterpreterInput("$: ", szInput);

        if (szInput == "quit" || szInput == "exit") {
            LogMessage("Exit", "Exiting GroundedInternal...");
            g_G2MOptions.bRunning.store(false);

            break;
        }

        if (!Interpreter::IsCommandAvailable(szInput)) {
            UnrealUtils::FindSpawnedItemByType(szInput);
        }
    }

    iRet = EXIT_SUCCESS;

    WinGUI::Stop();
    Interpreter::KeyBinds::Shutdown();

    /////// Cleanup ///////
_RYUJI:
    LogMessage("Exit", "GroundedMinimal2: Unhooking and cleaning up...");

    if (g_G2MOptions.bIsClientHost) {
        LogMessage("Exit", "Cleaning up CheatManager instance...");
        CheatManager::Destroy();
    }

    LogMessage(
        "Exit", "Restoring ProcessEvent hooks..."
    );
    HookManager::ProcessEventHooker::RestoreHooks();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    LogMessage(
        "Exit", "Restoring native function hooks..."
    );
    HookManager::NativeHooker::RestoreAll();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (EXIT_SUCCESS != iRet) {
        LogError("Exit", "GroundedMinimal2: Exiting due to errors");
        system("pause");
    }

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

    CloseHandle(g_G2MOptions.hLogFile);

    FreeConsole();

    FreeLibraryAndExitThread(hLocalModule, EXIT_SUCCESS);

    return iRet;
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
            g_G2MOptions.bRunning.store(false);
            break;
        }
    }
    return TRUE;
}
