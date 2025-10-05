#include "Grounded2Minimal.hpp"
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

// Main console input loop control
static bool g_bRunning = true;
// Debug console visibility status
bool ShowDebugConsole = true;
// Main thread handle
GLOBALHANDLE g_hThread = nullptr;
// Global console handle
HWND g_hConsole = nullptr;

// Original ProcessEvent function pointer
ProcessEvent_t OriginalProcessEvent = nullptr;
// Original ChatBoxWidget ProcessEvent function pointer
ProcessEvent_t OriginalChatBoxWidgetProcessEvent = nullptr;

// Version information
VersionInfo GroundedMinimalVersionInfo = { 0 };

// Game options
GameOptions g_GameOptions;

/////////////////////////////////////////////////////////////
// Cached data

// Cached player list
std::vector<SDK::APlayerState*> g_vPlayers;
// Cached local player ID
int32_t g_iLocalPlayerId = -1;
// Cached world instance
SDK::UWorld* g_lpWorld = nullptr;

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


///// ProcessEvent hooks
void __fastcall _HookedProcessEvent(
    SDK::UObject *lpObject,
    SDK::UFunction *lpFunction,
    void *lpParams
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
    
    if (!ItemSpawner::GlobalCheatMode) {
        return;
    }

    if (
        !lpObject->IsA(SDK::UUI_ChatLog_C::StaticClass()) 
        || 
        !lpObject->IsA(SDK::UChatBoxWidget::StaticClass())
    ) {
        return;
    }

    if (!lpFunction->GetName().contains("HandleChatMessageReceived")) {
        return;
    }

    SDK::FChatBoxMessage *lpMessage = static_cast<SDK::FChatBoxMessage *>(lpParams);
    if (nullptr == lpMessage || nullptr == lpMessage->SenderPlayerState) {
        return;
    }

    ItemSpawner::SafeChatMessageData* lpMessageDataCopy = new ItemSpawner::SafeChatMessageData{
        .iSenderId = lpMessage->SenderPlayerState->PlayerId,
        .szMessage = lpMessage->Message.ToString(),
        .szSenderName = lpMessage->SenderPlayerState->GetPlayerName().ToString(),
        .Color = lpMessage->Color,
        .Type = lpMessage->Type
    };

    std::thread([lpMessageDataCopy]() {
        ItemSpawner::EvaluateChatSpawnRequestSafe(lpMessageDataCopy);
        delete lpMessageDataCopy;
    }).detach();
}

///// Native function hooks
NativeHooker::NativeFunc_t fnOriginalUpdateCollisionStateChange = nullptr;

static void __stdcall Hook_UpdateCollisionStateChange(
    SDK::UObject* lpObj,
    void* lpFFrame,
    void* lpResult
) {
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
    if (nullptr != fnOriginalUpdateCollisionStateChange) {
        fnOriginalUpdateCollisionStateChange(lpObj, lpFFrame, lpResult);
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

    NativeHooker::HookEntry *lpEntry = nullptr;

    AllocConsole();
    freopen_s(&lpStdout, "CONOUT$", "w", stdout);
    freopen_s(&lpStderr, "CONOUT$", "w", stderr);
    freopen_s(&lpStdin, "CONIN$", "r", stdin);
    
    g_hConsole = GetConsoleWindow();

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

    LogMessage("Init", "Starting hooks initialization...");
    LogMessage("Init", "Initializing APawn ProcessEvent hook...", true);
    if (!HookManager::InstallHook(
        UnrealUtils::GetLocalPawn(),
        _HookedProcessEvent,
        &OriginalProcessEvent
    )) {
        LogError(
            "Init", 
            "Failed to hook LocalPawn ProcessEvent"
        );
        return EXIT_FAILURE;
    }

    LogMessage("Init", "Initializing ChatBoxWidget ProcessEvent hook...", true);

    if (!HookManager::InstallHook(
        SDK::UChatBoxWidget::GetDefaultObj(),
        _HookedChatBoxProcessEvent,
        &OriginalChatBoxWidgetProcessEvent
    )) {
        LogError("Init", "Failed to hook ChatBoxWidget ProcessEvent");
        goto _RYUJI; // lol
    }

    lpEntry = NativeHooker::HookNativeFunction(
        "UpdateCollisionStateChange",
        &Hook_UpdateCollisionStateChange,
        &fnOriginalUpdateCollisionStateChange
    );

    if (nullptr == lpEntry) {
        LogError(
            "Init", 
            "Failed to hook Building::UpdateCollisionStateChange"
        );
        goto _RYUJI;
    }


    LogMessage("Init", "Hooks initialized");

    LogMessage("Init", "Initializing CheatManager..");
    if (!CheatManager::ManualInitialize()) {
        LogError("Init", "Failed to initialize CheatManager");
        goto _RYUJI;
    }
    LogMessage("Init", "CheatManager initialized successfully");

    // GUI initialization
    LogMessage("Init", "Grounded2Minimal: Launching GUI thread...");

    if (!WinGUI::Initialize()) {
        LogError("Init", "Grounded2Minimal: Failed to initialize GUI");
        goto _RYUJI;
    }
    LogMessage("Init", "Grounded2Minimal: GUI thread launched successfully");

    while (g_bRunning) {
        Command::WaitForCommandBufferReady();

        std::string szInput;
        Interpreter::ReadInterpreterInput("$: ", szInput);

        if (szInput == "quit" || szInput == "exit") {
            LogMessage("Exit", "Exiting GroundedInternal...");
            g_bRunning = false;

            break;
        }

        if (!Interpreter::IsCommandAvailable(szInput)) {
            UnrealUtils::FindSpawnedItemByType(szInput);
        }
    }

    iRet = EXIT_SUCCESS;

    WinGUI::Stop();

_RYUJI:
    LogMessage("Exit", "GroundedMinimal2: Unhooking and cleaning up...");

    LogMessage(
        "Exit", "Restoring ProcessEvent hooks..."
    );
    HookManager::RestoreHooks();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    LogMessage(
        "Exit", "Restoring native function hooks..."
    );
    NativeHooker::RestoreAll();
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
            g_bRunning = FALSE;
            break;
        }
    }
    return TRUE;
}
