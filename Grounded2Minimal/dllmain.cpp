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
#include "SDK/BP_SurvivalPlayerController_Augusta_parameters.hpp"

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

enum class ReloadState : uint8_t {
    Idle,
    Requested,
    Reloading,
    Failed
};

std::atomic<ReloadState> g_ReloadState{ ReloadState::Idle };
std::atomic<bool> g_ReloadWorkerRunning{ false };
std::atomic<SDK::UObject*> g_EndingController{ nullptr };
std::thread g_ReloadWorker;

static void RequestReload(
    SDK::UObject* lpEndingController,
    SDK::EEndPlayReason eEndPlayReason
) {
    g_EndingController.store(lpEndingController, std::memory_order_release);

    ReloadState eExpected = ReloadState::Idle;
    if (!g_ReloadState.compare_exchange_strong(
        eExpected,
        ReloadState::Requested,
        std::memory_order_acq_rel
    )) {
        return;
    }

    LogMessage(
        "Reload",
        "Local SurvivalPlayerController received ReceiveEndPlay (reason "
        + std::to_string(static_cast<uint8_t>(eEndPlayReason))
        + "); scheduling hook reload"
    );
}

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
            "ProcessEvent called for hook ID " + std::to_string(lpHookData->iUniqueId) + " (" + lpHookData->szHookName + ")",
            true
        );

        LogMessage(
            lpHookData->szHookName,
            " * Function: " + lpParams->lpFunction->GetName() 
            + " ['" + lpParams->lpFunction->GetFullName() 
            + "'] @ " 
            + CoreUtils::HexConvert(reinterpret_cast<uintptr_t>(lpParams->lpFunction)),
            true
        );

        LogMessage(
            lpHookData->szHookName,
            " * Object: " + lpParams->lpObject->GetName() 
            + " ['" + lpParams->lpObject->GetFullName() 
            + "'] @ " 
            + CoreUtils::HexConvert(reinterpret_cast<uintptr_t>(lpParams->lpObject)),
            true
        );
    }
}

static bool CheckGameCompat(void) {
#if (TARGET_PLATFORM == TARGET_PLATFORM_STEAM)
    constexpr uintptr_t GameVersionStringOffset = 0x08187424;
#elif (TARGET_PLATFORM == TARGET_PLATFORM_XGP)
    constexpr uintptr_t GameVersionStringOffset = 0x07A43C24;
#endif

    HMODULE hBaseAddress = GetModuleHandleW(nullptr);
    if (nullptr == hBaseAddress) {
        LogError(
            "CheckGameCompat",
            "Failed to get base address of the game module"
        );
        return false;
    }

    MEMORY_BASIC_INFORMATION mbInfo{};

    DWORD32 dwRdataRVA = CoreUtils::GetSectionRVAOffset(
        reinterpret_cast<LPCBYTE>(hBaseAddress), 
        ".rdata"
    );

    LPCBYTE lpcTargetVersionAddress = reinterpret_cast<LPCBYTE>(hBaseAddress) + dwRdataRVA + GameVersionStringOffset;

    if (0 == VirtualQuery(
        lpcTargetVersionAddress,
        &mbInfo,
        sizeof(mbInfo)
    )) {
        LogError(
            "CheckGameCompat",
            "Failed to query memory information for the game module"
        );
        return false;
    }
    
    if (
        mbInfo.State != MEM_COMMIT || mbInfo.Protect == PAGE_NOACCESS
    ) {
        LogError(
            "CheckGameCompat",
            "Game module memory is not committed or is inaccessible"
        );
        return false;
    }

    CONST BYTE abGameVersion[] = { 0x30, 0x00, 0x2E, 0x00, 0x35, 0x00, 0x2E, 0x00, 0x30, 0x00, 0x2E, 0x00, 0x33 };
    if (0 != memcmp(
        reinterpret_cast<const void*>(lpcTargetVersionAddress),
        abGameVersion,
        sizeof(abGameVersion)
    )) {
        LogMessage(
            "CheckGameCompat",
            "Incompatibility report:\n"
            " * BaseAddress: " + CoreUtils::HexConvert(reinterpret_cast<uintptr_t>(hBaseAddress)) + "\n"
            " * TargetVersionAddress: " + CoreUtils::HexConvert(reinterpret_cast<uintptr_t>(lpcTargetVersionAddress)) + "\n"
            " * .rdata RVA: " + CoreUtils::HexConvert(dwRdataRVA) + "\n",
            true
        );

        LogError(
            "CheckGameCompat",
            "Game version string does not match expected value, the game may have been updated and is incompatible with this mod"
        );
        return false;
    }

    return true;
}

void ProcessDebugFilter(
    HookManager::NativeHooker::HookEntry* lpHookData,
    NativeProcessEventParams* lpParams
) {
    if (nullptr == lpHookData) {
        LogMessage(
            "ProcessDebugFilter",
            "Invalid hook data for native debug filter, skipping debug output",
            true
        );
        return;
    }
    if (lpHookData->szDebugFilter.empty()) {
        LogMessage(
            "ProcessDebugFilter",
            "Debug filter is empty for hook '" + lpHookData->szHookName + "', skipping debug output",
            true
        );
        return;
    }
    if (nullptr == lpParams) {
        LogMessage(
            "ProcessDebugFilter",
            "Invalid parameters for native debug filter, skipping debug output",
            true
        );
        return;
    }
    if (nullptr == lpParams->lpObject) {
        LogMessage(
            "ProcessDebugFilter",
            "Object parameter is null for native debug filter, skipping debug output",
            true
        );
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
            "Native hook triggered for hook ID " + std::to_string(lpHookData->iUniqueId) + " (" + lpHookData->szHookName + ")",
            true
        );
        LogMessage(
            lpHookData->szHookName,
            " * Object: " 
            + lpParams->lpObject->GetName() 
            + " ['" + lpParams->lpObject->GetFullName() 
            + "'] @ " 
            + CoreUtils::HexConvert(reinterpret_cast<uintptr_t>(lpParams->lpObject)),
            true
        );

        LogMessage(
            lpHookData->szHookName,
            " * FFrame: " + CoreUtils::HexConvert(reinterpret_cast<uintptr_t>(lpParams->lpFFrame)),
            true
        );

        LogMessage(
            lpHookData->szHookName,
            " * Result: " + CoreUtils::HexConvert(reinterpret_cast<uintptr_t>(lpParams->lpResult)),
            true
        );
    } else {
        LogMessage(
            lpHookData->szHookName,
            "Native hook triggered but did not match debug filter for hook ID " + std::to_string(lpHookData->iUniqueId) + " (" + lpHookData->szHookName + ")",
            true
        );

        // print debug filter
        LogMessage(
            lpHookData->szHookName,
            " * Debug filter: '" + lpHookData->szDebugFilter + "'",
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

    ProcessEventHooker::HookData* lpHookData = ProcessEventHooker::GetHookByHookedFunction(
        _HookedSPCProcessEvent
    );
    if (nullptr == lpHookData) {
        // catastrophic cataclysmic shit
        //throw std::runtime_error("SPCProcessEvent: Hook data not found");
        return;
    }

    if (ProcessEventHooker::IsRestoring()) {
        lpHookData->OriginalFn(lpObject, lpFunction, lpParams);
        return;
    }

    // Re-entrancy guard for command processing on the same thread
    static thread_local bool s_InProcessCommands = false;

    // Check for Engine.PlayerController:ClientRestart
    if (lpFunction->GetName().contains("ClientRestart")) {
        // Invalidate player cache on client restart, as the player state will be recreated
        // as of right now, this is completely useless, cuz all hooks will still be present
        PlayerCache::InvalidateCache();
    } else if (
        lpFunction->GetName().contains("ReceiveEndPlay")
        && ProcessEventHooker::GetHookByObject(lpObject) == lpHookData
    ) {
        SDK::Params::BP_SurvivalPlayerController_Augusta_C_ReceiveEndPlay* lpEndPlayParams =
            static_cast<SDK::Params::BP_SurvivalPlayerController_Augusta_C_ReceiveEndPlay*>(lpParams);
        if (nullptr != lpEndPlayParams) {
            RequestReload(lpObject, lpEndPlayParams->EndPlayReason);
        }
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

    ProcessEventHooker::HookData* lpHookData = ProcessEventHooker::GetHookByHookedFunction(
        _HookedChatBoxProcessEvent
    );
    if (nullptr == lpHookData) {
        // catastrophic cataclysmic shit
        //throw std::runtime_error("ChatBoxProcessEvent: Hook data not found");
        return;
    }

    if (ProcessEventHooker::IsRestoring()) {
        lpHookData->OriginalFn(lpObject, lpFunction, lpParams);
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

    ProcessEventHooker::HookData* lpHookData = ProcessEventHooker::GetHookByHookedFunction(
        _HookedGameModeBaseProcessEvent
    );
    if (nullptr == lpHookData) {
        //throw std::runtime_error("GameModeBaseProcessEvent: Hook data not found");
        return;
    }

    if (ProcessEventHooker::IsRestoring()) {
        lpHookData->OriginalFn(lpObject, lpFunction, lpParams);
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

    NativeHooker::HookEntry* lpHookData = NativeHooker::GetHookByHookedFunction(
        &_HookedUpdateCollisionStateChange
    );

    if (nullptr == lpHookData) {
        // catastrophic cataclysmic shit
        //throw std::runtime_error("UpdateCollisionStateChange: Hook data not found");
        return;
    }

    if (NativeHooker::IsRestoring()) {
        lpHookData->OriginalFn(lpObj, lpFFrame, lpResult);
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
    NativeHooker::HookEntry* lpHookData = NativeHooker::GetHookByHookedFunction(
        (NativeHooker::NativeFunc_t) &_HookedGetPlacementValid
    );

    if (nullptr == lpHookData) {
        // catastrophic cataclysmic 
        return;
    }

    if (NativeHooker::IsRestoring()) {
        lpHookData->OriginalFn(lpObj, lpFFrame, lpResult);
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

NATIVEHOOK Maine_HealthComponent_OnRep_CurrentDamage(
    SDK::UObject* lpObj,
    void* lpFFrame,
    void* lpResult
) {
    using namespace HookManager;
    NativeHooker::InFlightGuard inFlight;
    NativeHooker::HookEntry* lpHookData = NativeHooker::GetHookByHookedFunction(
        (NativeHooker::NativeFunc_t) &Maine_HealthComponent_OnRep_CurrentDamage
    );

    if (nullptr == lpHookData) {
        LogMessage(
            "OnRep_CurrentDamageHook",
            "Hook data not found, skipping damage modification",
            true
        );
        // catastrophic cataclysmic 
        return;
    }

    if (NativeHooker::IsRestoring()) {
        lpHookData->OriginalFn(lpObj, lpFFrame, lpResult);
        return;
    }

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

//NATIVEHOOK Maine_SurvivalPlayerController_OnDamaged(
//    SDK::UObject* lpObj,
//    void* lpFFrame,
//    void* lpResult
//) {
//    using namespace HookManager;
//    NativeHooker::InFlightGuard inFlight;
//    if (NativeHooker::IsRestoring()) {
//        return;
//    }
//    NativeHooker::HookEntry* lpHookData = NativeHooker::GetHookByHookedFunction(
//        (NativeHooker::NativeFunc_t) &Maine_SurvivalPlayerController_OnDamaged
//    );
//    if (nullptr == lpHookData) {
//        LogMessage(
//            "SPC_OnDamaged_Hook",
//            "Hook data not found, skipping damage modification",
//            true
//        );
//        // catastrophic cataclysmic 
//        return;
//    }
//
//    //UnrealUtils::FrameWalker::FFrame* lpStack = reinterpret_cast<UnrealUtils::FrameWalker::FFrame*>(lpFFrame);
//
//    lpHookData->OriginalFn(lpObj, lpFFrame, lpResult);
//
//    if (g_GameOptions.GodMode.load()) {
//        SDK::ASurvivalPlayerCharacter* lpPlayerCharacter = UnrealUtils::GetSurvivalPlayerCharacterById();
//        if (nullptr == lpPlayerCharacter) {
//            LogMessage(
//                "SPC_OnDamaged_Hook",
//                "Failed to get local player character, cannot apply god mode damage modification",
//                true
//            );
//        } else {
//            lpPlayerCharacter->HealthComponent->CurrentDamage = 0.0f;
//        }
//    }
//
//    if (lpHookData->bDebugFilterEnabled.load()) {
//        NativeProcessEventParams funcParams{
//            lpObj,
//            lpFFrame,
//            lpResult
//        };
//        ProcessDebugFilter(lpHookData, &funcParams);
//    }
//}

/////////////////////////////////////////////////////////////

void InitializeGameStatics(void) {
    g_GameOptions.GameStatics.HandyGnatForceEnable.store(
        UnrealUtils::GameStatics::IsHandyGnatEnabled(),
        std::memory_order_release
    );

    g_GameOptions.GameStatics.BuildingIntegrity.store(
        UnrealUtils::GameStatics::IsBuildingIntegrityEnabled(),
        std::memory_order_release
    );

    g_GameOptions.GameStatics.AutoCompleteBuildings.store(
        UnrealUtils::GameStatics::IsAutoCompleteBuildingsEnabled(),
        std::memory_order_release
    );

    g_GameOptions.GameStatics.FreeCrafting.store(
        UnrealUtils::GameStatics::IsFreeCraftingEnabled(),
        std::memory_order_release
    );

    g_GameOptions.GameStatics.InvinciblePets.store(
        UnrealUtils::GameStatics::IsPetInvincibilityEnabled(),
        std::memory_order_release
    );

    g_GameOptions.GameStatics.PlayerDamageMultiplier.store(
        UnrealUtils::GameStatics::GetPlayerDamageMultiplier(),
        std::memory_order_release
    );

    g_GameOptions.CurrentlyAdjustedBuilding.store(
        nullptr,
        std::memory_order_release
    );

    LogMessage(
        "Init - GameStatics",
        "Initial game options: \n" +
        std::string(" * HandyGnatForceEnable   =") + (g_GameOptions.GameStatics.HandyGnatForceEnable.load() ? "true" : "false") + "\n" +
        std::string(" * BuildingIntegrity      =") + (g_GameOptions.GameStatics.BuildingIntegrity.load() ? "true" : "false") + "\n" +
        std::string(" * AutoCompleteBuildings  =") + (g_GameOptions.GameStatics.AutoCompleteBuildings.load() ? "true" : "false") + "\n" +
        std::string(" * FreeCrafting           =") + (g_GameOptions.GameStatics.FreeCrafting.load() ? "true" : "false") + "\n" +
        std::string(" * InvinciblePets         =") + (g_GameOptions.GameStatics.InvinciblePets.load() ? "true" : "false") + "\n" +
        std::string(" * PlayerDamageMultiplier =") + std::to_string(g_GameOptions.GameStatics.PlayerDamageMultiplier.load()) + "\n",
        true
    );
}

static bool InitializeRuntimeState(void) {
    g_G2MOptions.bIsClientHost = UnrealUtils::IsPlayerHostAuthority(
        UnrealUtils::GetLocalPlayerId()
    );

    if (g_G2MOptions.bIsClientHost) {
        SDK::ASurvivalPlayerController* lpLocalSPC = UnrealUtils::GetLocalSurvivalPlayerControllerFast();
        if (nullptr == lpLocalSPC) {
            lpLocalSPC = UnrealUtils::GetLocalSurvivalPlayerController();
        }

        if (nullptr == lpLocalSPC) {
            LogError("Init", "Failed to get local SurvivalPlayerController for hooking");
            return false;
        }

        if (!HookManager::ProcessEventHooker::InstallHook(
            lpLocalSPC,
            _HookedSPCProcessEvent,
            "SPC_ProcessEvent"
        )) {
            LogError("Init", "Failed to hook SurvivalPlayerController ProcessEvent");
            return false;
        }

        if (!HookManager::ProcessEventHooker::InstallHook(
            SDK::UChatBoxWidget::GetDefaultObj(),
            _HookedChatBoxProcessEvent,
            "ChatBoxWidget_ProcessEvent"
        )) {
            LogError("Init", "Failed to hook ChatBoxWidget ProcessEvent");
            return false;
        }
    }

    if (!HookManager::ProcessEventHooker::InstallHook(
        SDK::AGameModeBase::StaticClass(),
        _HookedGameModeBaseProcessEvent,
        "GameModeBase_ProcessEvent"
    )) {
        LogError("Init", "Failed to hook GameModeBase ProcessEvent");
        return false;
    }

    if (nullptr == HookManager::NativeHooker::HookNativeFunction(
        "UpdateCollisionStateChange",
        &_HookedUpdateCollisionStateChange,
        "UpdateCollisionStateChange_Native"
    )) {
        LogError("Init", "Failed to hook Building::UpdateCollisionStateChange");
        return false;
    }

    if (nullptr == HookManager::NativeHooker::HookNativeFunction(
        "GetPlacementValid",
        &_HookedGetPlacementValid,
        "GetPlacementValid_Native"
    )) {
        LogError("Init", "Failed to hook IPlaceable::GetPlacementValid");
        return false;
    }

    if (g_G2MOptions.bIsClientHost && !CheatManager::ManualInitialize()) {
        LogError("Init", "Failed to initialize CheatManager");
        return false;
    }

    InitializeGameStatics();
    return true;
}

static bool WaitForReplacementWorld(SDK::UObject* lpEndingController) {
    constexpr int32_t MaxAttempts = 120;

    for (
        int32_t iAttempt = 0;
        iAttempt < MaxAttempts && g_ReloadWorkerRunning.load(std::memory_order_acquire);
        ++iAttempt
    ) {
        SDK::ASurvivalPlayerController* lpLocalSPC = UnrealUtils::GetLocalSurvivalPlayerControllerFast();
        if (
            nullptr != lpLocalSPC
            && lpLocalSPC != lpEndingController
            && nullptr != UnrealUtils::GetLocalPawn()
        ) {
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return false;
}

static void ReloadRuntimeState(void) {
    LogMessage("Reload", "Restoring hooks for world reload");

    if (g_G2MOptions.bIsClientHost) {
        CheatManager::Destroy();
    }

    PlayerCache::InvalidateCache();
    HookManager::ProcessEventHooker::RestoreHooks();
    HookManager::NativeHooker::RestoreAll();

    SDK::UObject* lpEndingController = g_EndingController.load(std::memory_order_acquire);
    if (!WaitForReplacementWorld(lpEndingController)) {
        LogError(
            "Reload", 
            "Timed out waiting for a replacement playable world; hooks remain restored"
        );
        g_ReloadState.store(ReloadState::Failed, std::memory_order_release);
        return;
    }

    if (
        !HookManager::ProcessEventHooker::CompleteRestore()
        || 
        !HookManager::NativeHooker::CompleteRestore()
    ) {
        LogError(
            "Reload", 
            "Failed to complete hook restoration for reinitialization"
        );
        g_ReloadState.store(ReloadState::Failed, std::memory_order_release);
        return;
    }

    if (!InitializeRuntimeState()) {
        LogError(
            "Reload", 
            "Failed to initialize hooks for the replacement world"
        );
        g_ReloadState.store(ReloadState::Failed, std::memory_order_release);
        return;
    }

    g_EndingController.store(nullptr, std::memory_order_release);
    g_ReloadState.store(ReloadState::Idle, std::memory_order_release);
    LogMessage("Reload", "Hook reload completed");
}

static void ReloadWorkerLoop(void) {
    while (g_ReloadWorkerRunning.load(std::memory_order_acquire)) {
        ReloadState eExpected = ReloadState::Requested;
        if (g_ReloadState.compare_exchange_strong(
            eExpected,
            ReloadState::Reloading,
            std::memory_order_acq_rel
        )) {
            ReloadRuntimeState();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

static bool IsDebugFilePresent(void) {
    LPCSTR cszDebugFileName = ".g2m_debug";

    DWORD dwAttrib = GetFileAttributesA(cszDebugFileName);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

DWORD WINAPI ThreadEntry(
    LPVOID lpParam
) { 
#ifndef _RELEASE
    EnableDebugOutput();
#else 
    if (IsDebugFilePresent()) {
        EnableDebugOutput();
        LogMessage(
            "Init",
            "Debug output enabled via presence of .g2m_debug file in working directory",
            true
        );
    }
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

    LogMessage(
        "Init",
        "Checking game version compatibility..."
    );
    if (!CheckGameCompat()) {
        LogError(
            "Init",
            "FATAL ERROR\n"
            "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
            "! Incompatible game version detected!     !\n"
            "! Check for Grounded2Minimal updates.     !\n"
            "!                                         !\n"
            "! Press ENTER to continue...              !\n"
            "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
        );
        iRet = EXIT_FAILURE;
        goto _CLEAN_EXIT;
    }

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

    LogMessage("Init", "Starting runtime initialization...");
    if (!InitializeRuntimeState()) {
        goto _RYUJI;
    }

    g_ReloadWorkerRunning.store(true, std::memory_order_release);
    g_ReloadWorker = std::thread(ReloadWorkerLoop);

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

    g_ReloadWorkerRunning.store(false, std::memory_order_release);
    if (g_ReloadWorker.joinable()) {
        g_ReloadWorker.join();
    }

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

_CLEAN_EXIT:
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
