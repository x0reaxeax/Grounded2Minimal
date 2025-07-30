#include "CheatManager.hpp"
#include "UnrealUtils.hpp"
#include "CoreUtils.hpp"

namespace CheatManager {
    static SDK::USurvivalCheatManager *SurvivalCheatManager = nullptr;

    void UnlockMultiplayerCheatManager(void) {
        const uintptr_t qwPatchSkipTargetOffset = 0x2F;         // Patch conditional jump with unconditional jump
        const byte abySkipTargetCheck[] = {
            0x48, 0x83, 0xBF, 0x70, 0x03, 0x00, 0x00, 0x00      // cmp qword ptr [rdi+370h], 0
        };
        const byte abyPatchSkipTarget[sizeof(abySkipTargetCheck)] = {
            0xE9, 0x72, 0x00, 0x00, 0x00,                       // jmp 0x72 bytes forward
            0x90, 0x90, 0x90                                    // 3x NOP
        };

        SDK::UWorld *lpWorld = SDK::UWorld::GetWorld();
        if (nullptr == lpWorld) {
            LogError("Summon", "Failed to get UWorld instance");
            return;
        }

        SDK::UGameInstance *lpOwningGameInstance = lpWorld->OwningGameInstance;
        if (nullptr == lpOwningGameInstance) {
            LogError("Summon", "Failed to get UGameInstance instance");
            return;
        }

        int32_t iLocalPlayerId = UnrealUtils::GetLocalPlayerId();
        SDK::APlayerController *lpLocalPlayerController = nullptr;
        for (int32_t i = 0; i < lpOwningGameInstance->LocalPlayers.Num(); i++) {
            SDK::ULocalPlayer *lpLocalPlayer = lpOwningGameInstance->LocalPlayers[i];
            if (nullptr == lpLocalPlayer) {
                continue;
            }

            lpLocalPlayerController = lpLocalPlayer->PlayerController;
            if (nullptr != lpLocalPlayerController) {
                break;
            }
        }

        if (nullptr == lpLocalPlayerController) {
            LogError("Summon", "Failed to get local player controller");
            return;
        }

        LogMessage("EnableCheats", "Local player controller found, attempting to enable cheats...");

        uintptr_t* lpvfTable = *(uintptr_t**) lpLocalPlayerController;

        /*
        ; APlayerController::execEnableCheats(void)
        _ZN17APlayerController16execEnableCheatsEv proc near
            mov     rax, [rdx+20h]
            xor     r8d, r8d
            test    rax, rax
            setnz   r8b
            add     r8, rax
            mov     [rdx+20h], r8
            mov     rax, [rcx]
            jmp     qword ptr [rax+0BA0h]
        _ZN17APlayerController16execEnableCheatsEv endp

        ; 0BA0h / sizeof(voidptr) = 174h


        */

        uintptr_t qwLevel1 = lpvfTable[0x174];                  // = vtable[0x174] = trampoline

        LogMessage(
            "EnableCheats",
            "Found Level1 trampoline at "
            + CoreUtils::HexConvert(qwLevel1)
        );

        /*
         * trampoline:
            mov rax,[rcx]
            xor edx,edx
            jmp qword ptr [rax+00000E78]
        */

        uintptr_t fnEnableCheatsImpl = lpvfTable[0xE78 / 8];
        if (0 == fnEnableCheatsImpl) {
            LogError("EnableCheats", "Failed to resolve EnableCheats function");
            return;
        }

        LogMessage(
            "EnableCheats",
            "Found EnableCheatsImpl at "
            + CoreUtils::HexConvert(fnEnableCheatsImpl)
        );

        if (nullptr == lpLocalPlayerController->CheatClass) {
            LogMessage(
                "EnableCheats",
                "CheatClass is NULL, attempting to assign it..."
            );

            SDK::UClass *lpCheatManagerClass = SDK::UCheatManager::StaticClass();
            if (nullptr == lpCheatManagerClass) {
                LogError("EnableCheats", "Failed to get CheatManager class");
                return;
            }

            LogMessage(
                "EnableCheats",
                "Patching CheatClass to "
                + std::to_string(reinterpret_cast<uintptr_t>(lpCheatManagerClass))
            );

            lpLocalPlayerController->CheatClass = lpCheatManagerClass;

            LogMessage(
                "EnableCheats",
                "CheatClass assigned successfully"
            );
        }

        LogMessage(
            "EnableCheats",
            "Attempting to patch EnableCheatsImpl..."
        );

        // Patch the EnableCheatsImpl function to skip the check
        void *lpEnableCheatsImpl = reinterpret_cast<void*>(fnEnableCheatsImpl);
        if (nullptr == lpEnableCheatsImpl) {
            LogError("EnableCheats", "Failed to resolve EnableCheatsImpl function");
            return;
        }

        void *lpSkipCheckTarget =
            reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(lpEnableCheatsImpl) + qwPatchSkipTargetOffset);

        // compare the target bytes to the expected bytes
        if (EXIT_SUCCESS != memcmp(
            lpSkipCheckTarget,
            abySkipTargetCheck,
            sizeof(abySkipTargetCheck)
        )) {
            LogError("EnableCheats", "Unexpected bytes at skip check target");
            return;
        }

        DWORD dwOldProtect;
        if (!VirtualProtect(
            lpEnableCheatsImpl,
            qwPatchSkipTargetOffset + sizeof(abySkipTargetCheck),
            PAGE_EXECUTE_READWRITE,
            &dwOldProtect
        )) {
            LogError(
                "EnableCheats",
                "Failed to change memory protection for EnableCheatsImpl"
            );
            return;
        }

        // Patch the target bytes with unconditional jump
        memcpy(
            lpSkipCheckTarget,
            abyPatchSkipTarget,
            sizeof(abyPatchSkipTarget)
        );

        // Restore the original memory protection
        VirtualProtect(
            lpEnableCheatsImpl,
            qwPatchSkipTargetOffset + sizeof(abySkipTargetCheck),
            dwOldProtect,
            &dwOldProtect
        );


        LogMessage(
            "EnableCheats",
            "EnableCheatsImpl patched successfully, EnableCheats() can now be called."
        );
    }

    bool GetSurvivalCheatManager(void) {
        SDK::UWorld *lpWorld = SDK::UWorld::GetWorld();
        if (nullptr == lpWorld) {
            LogError("CheatManager", "Failed to get UWorld instance");
            return false;
        }

        SDK::APlayerController *lpLocalPlayerController = UnrealUtils::GetLocalPlayerController();
        if (nullptr == lpLocalPlayerController) {
            LogError("CheatManager", "Failed to get local player controller");
            return false;
        }

        SDK::UGameInstance *lpOwningGameInstance = lpWorld->OwningGameInstance;
        if (nullptr == lpOwningGameInstance) {
            LogError("CheatManager", "Failed to get UGameInstance instance");
            return false;
        }

        int32_t iLocalPlayerId = UnrealUtils::GetLocalPlayerId();
        SDK::USurvivalCheatManager *lpSurvivalCheatManager = nullptr;

        int32_t iGObjectCount = SDK::USurvivalCheatManager::GObjects->Num();
        for (int32_t i = 0; i < iGObjectCount; i++) {
            SDK::UObject *lpObj = SDK::USurvivalCheatManager::GObjects->GetByIndex(i);
            if (nullptr == lpObj) {
                continue;
            }

            if (!lpObj->IsA(SDK::USurvivalCheatManager::StaticClass())) {
                continue;
            }

            SDK::USurvivalCheatManager *lpCurrentSurvivalCheatManager = 
                static_cast<SDK::USurvivalCheatManager*>(lpObj);
        
            LogMessage(
                "CheatManager",
                "Found SurvivalCheatManager at index " + std::to_string(i) 
                + " [" + std::to_string(i + 1) + "/" + std::to_string(iGObjectCount) + "]",
                true
            );

            SDK::UCheatManager* lpCheatManager = static_cast<SDK::UCheatManager*>(lpCurrentSurvivalCheatManager);
            if (nullptr == lpCheatManager) {
                LogError("CheatManager", "Failed to cast to UCheatManager", true);
                continue;
            }

            SDK::UObject *lpOuter = lpCheatManager->Outer;
            if (nullptr == lpOuter) {
                LogError("CheatManager", "Outer is NULL", true);
                continue;
            }
        
            if (!lpOuter->IsA(SDK::APlayerController::StaticClass())) {
                LogError("CheatManager", "Outer is not a APlayerController", true);
                continue;
            }
        
            SDK::APlayerController* lpOwningPlayerController = static_cast<SDK::APlayerController*>(lpOuter);
            if (nullptr == lpOwningPlayerController) {
                LogError("CheatManager", "Owning PlayerController is NULL", true);
                continue;
            }
        
            SDK::APlayerState *lpOwningPlayerState = lpOwningPlayerController->PlayerState;
            if (nullptr == lpOwningPlayerState) {
                LogError("CheatManager", "Owning PlayerState is NULL", true);
                continue;
            }
            if (lpOwningPlayerState->PlayerId != iLocalPlayerId) {
                LogError(
                    "CheatManager",
                    "Found SurvivalCheatManager for a different player ID: "
                    + std::to_string(lpOwningPlayerController->PlayerState->PlayerId)
                );
                continue; // Not the local player
            }
            
            LogMessage(
                "CheatManager",
                "Found SurvivalCheatManager for local player ID: "
                + std::to_string(lpOwningPlayerState->PlayerId)
            );

            lpSurvivalCheatManager = lpCurrentSurvivalCheatManager;
            break; // Found the SurvivalCheatManager for the local player
        }

        if (nullptr == lpSurvivalCheatManager) {
            LogError("CheatManager", "Failed to find SurvivalCheatManager instance");
            return false;
        }

        SurvivalCheatManager = lpSurvivalCheatManager;
        LogMessage("CheatManager", "SurvivalCheatManager found successfully");
        return true; // Found the SurvivalCheatManager
    }

    bool Initialize(void) {
        LogMessage("CheatManager", "Initializing CheatManager...");
        if (!GetSurvivalCheatManager()) {
            LogError("CheatManager", "Failed to get SurvivalCheatManager");
            return false;
        }
        LogMessage("CheatManager", "CheatManager initialized successfully");
        return true;
    }

    void GameThread CheatManagerExecute(
        const CheatManagerParams *lpParams
    ) {
        if (nullptr == SurvivalCheatManager) {
            if (!Initialize()) {
                LogError(
                    "CheatManager", 
                    "Failed to initialize SurvivalCheatManager, cannot execute cheat"
                );
                return;
            }

            if (nullptr == SurvivalCheatManager) {
                LogError(
                    "CheatManager",
                    "SurvivalCheatManager is NULL, cannot execute cheat"
                );
                return;
            }
        }

        if (nullptr == lpParams) {
            LogError("CheatManager", "Params are NULL");
            return;
        }

        CheatManagerFunctionId fdwFunctionId = lpParams->FunctionId;
        LogMessage(
            "CheatManager",
            "Executing cheat function ID: " + std::to_string(static_cast<uint32_t>(fdwFunctionId))
        );

        const uint64_t *alpqwParams = lpParams->FunctionParams;
        switch (fdwFunctionId) {
            case CheatManagerFunctionId::AddGoldMolars: {
                if (nullptr == alpqwParams) {
                    LogError("CheatManager", "Params are NULL for AddGoldMolars");
                    return;
                }

                int32_t iAmount = static_cast<int32_t>(alpqwParams[0]);

                SurvivalCheatManager->AddPartyUpgradePoints(iAmount);
                
                break;
            }

            case CheatManagerFunctionId::AddWhiteMolars: {
                if (nullptr == alpqwParams) {
                    LogError("CheatManager", "Params are NULL for AddWhiteMolars");
                    return;
                }

                int32_t iAmount = static_cast<int32_t>(alpqwParams[0]);
                SurvivalCheatManager->AddPersonalUpgradePoints(iAmount);

                break;
            }

            case CheatManagerFunctionId::AddRawScience: {
                if (nullptr == alpqwParams) {
                    LogError("CheatManager", "Params are NULL for AddRawScience");
                    return;
                }

                int32_t iAmount = static_cast<int32_t>(alpqwParams[0]);
                SurvivalCheatManager->AddScience(iAmount);

                break;
            }

            case CheatManagerFunctionId::CompleteActiveDefensePoint: {
                SurvivalCheatManager->CompleteActiveDefensePoint();
                break;
            }

            case CheatManagerFunctionId::ToggleHud: {
                SurvivalCheatManager->ToggleHUD();
                break;
            }

            case CheatManagerFunctionId::UnlockAllRecipes: {
                SurvivalCheatManager->UnlockAllRecipes(SDK::ERecipeUnlockMode::IncludeHidden);
                break;
            }

            case CheatManagerFunctionId::UnlockAllLandmarks: {
                SurvivalCheatManager->UnlockAllPOIs();
                break;
            }

            case CheatManagerFunctionId::UnlockMonthlyTechTrees: {
                SurvivalCheatManager->UnlockMonthlyTechTrees();
                break;
            }

            case CheatManagerFunctionId::UnlockMutations: {
                SurvivalCheatManager->UnlockAllPerks();
                break;
            }

            case CheatManagerFunctionId::UnlockScabs: {
                SurvivalCheatManager->UnlockAllColorThemes();
                break;
            }

            default: {
                LogError(
                    "CheatManager", 
                    "Unknown CheatManagerFunctionId: " + std::to_string(static_cast<int32_t>(fdwFunctionId))
                );
                return;
            }
        }
    }
} // namespace CheatManager