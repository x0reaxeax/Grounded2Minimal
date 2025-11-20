/// @file CheatManager.cpp

#include "CheatManager.hpp"
#include "UnrealUtils.hpp"
#include "CoreUtils.hpp"
#include "Command.hpp"

namespace CheatManager {
    namespace StaticCheats {
        bool SetMaxActiveMutations(
            uint32_t uMaxMutations
        ) {
            SDK::ASurvivalPlayerState *lpSurvivalPlayerState = UnrealUtils::GetLocalSurvivalPlayerState();
            if (nullptr == lpSurvivalPlayerState) {
                LogError("CheatManager", "Failed to get local SurvivalPlayerState");
                return false;
            }
            lpSurvivalPlayerState->PerkComponent->MaxEquippedPerks = uMaxMutations;
            return true;
        }
    }

    namespace InvokedCheats {
        void SetPlayerCollision(
            bool bNewCollisionState
        ) {
            Command::Params::SetCollision *scParams = new Command::Params::SetCollision {
                .lpPlayerState = UnrealUtils::GetLocalSurvivalPlayerState(),
                .bNewCollisionState = bNewCollisionState
            };

            Command::SubmitTypedCommand(
                Command::CommandId::CmdIdSetCollision,
                scParams
            );
        }
    }

    namespace Summon {
        void SummonClass(
        const std::string& szClassName
        ) {
            if (szClassName.empty()) {
                LogError("Summon", "Class name is empty");
                return;
            }

            SDK::UWorld *lpWorld = UnrealUtils::GetWorld(true);
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
            for (int32_t i = 0; i < lpOwningGameInstance->LocalPlayers.Num(); i++) {
                SDK::ULocalPlayer *lpLocalPlayer = lpOwningGameInstance->LocalPlayers[i];
                if (nullptr == lpLocalPlayer) {
                    continue;
                }

                SDK::APlayerController *lpLocalPlayerController = lpLocalPlayer->PlayerController;
                if (nullptr == lpLocalPlayerController) {
                    continue;
                }

                SDK::APlayerState *lpPlayerState = lpLocalPlayerController->PlayerState;
                if (nullptr == lpPlayerState) {
                    continue;
                }

                if (iLocalPlayerId != lpPlayerState->PlayerId) {
                    continue;
                }

                if (!lpPlayerState->HasAuthority()) {
                    LogError("Summon", "Local player does not have authority to summon classes");
                    return;
                }

                std::wstring wszClassName;
                if (!CoreUtils::Utf8ToWideString(
                    szClassName,
                    wszClassName
                )) {
                    LogError("Summon", "Failed to convert class name to wide string");
                    return;
                }

                if (wszClassName.size() > 256) {
                    LogError(
                        "Summon", 
                        "Class name is too long, aborting due to probable corruption.."
                    );
                    return;
                }

                SDK::FString fszTargetClassName(wszClassName.c_str());

                auto* lpParams = new (std::nothrow) BufferParamsSummon{
                    .iPlayerId = iLocalPlayerId,
                    .fszClassName = fszTargetClassName,
                    .lpLocalPlayerController = lpLocalPlayerController
                };

                if (nullptr == lpParams) {
                    LogError("Summon", "Failed to allocate memory for BufferParamsSummon");
                    return;
                }

                Command::SubmitTypedCommand(
                    Command::CommandId::CmdIdSummon,
                    lpParams
                );
            }
        }
    }

    static SDK::USurvivalCheatManager *SurvivalCheatManager = nullptr;

    bool IsSurvivalCheatManagerInitialized(void) {
        if (nullptr == SurvivalCheatManager) {
            LogError(
                "CheatManager", 
                "SurvivalCheatManager is not initialized", 
                true
            );
            return false;
        }
        return true;
    }

    void Destroy(void) {
        if (nullptr != SurvivalCheatManager) {
            SDK::APlayerController *lpPlayerController = 
                static_cast<SDK::APlayerController*>(SurvivalCheatManager->Outer);

            if (
                nullptr != lpPlayerController 
                && 
                lpPlayerController->CheatManager == SurvivalCheatManager
            ) {
                LogMessage(
                    "CheatManager",
                    "Destroying SurvivalCheatManager instance: "
                    + CoreUtils::HexConvert(
                        reinterpret_cast<uintptr_t>(SurvivalCheatManager)
                    ),
                    true
                );
                lpPlayerController->CheatManager = nullptr;

                if (lpPlayerController->CheatClass == SDK::USurvivalCheatManager::StaticClass()) {
                    LogMessage(
                        "CheatManager",
                        "Clearing CheatClass for player controller: "
                        + CoreUtils::HexConvert(
                            reinterpret_cast<uintptr_t>(lpPlayerController)
                        ),
                        true
                    );
                    lpPlayerController->CheatClass = nullptr;
                }
            }

            LogMessage(
                "CheatManager",
                "Cleaning up SurvivalCheatManager pointers...",
                true
            );

            SurvivalCheatManager->Outer = nullptr;
            SurvivalCheatManager = nullptr;

            LogMessage(
                "CheatManager",
                "Cleanup successful",
                true
            );
        }
    }

    bool ManualInitialize(void) {
        if (!g_G2MOptions.bIsClientHost) {
            LogError(
                "CheatManager",
                "Cannot initialize CheatManager without host authority"
            );
            return false;
        }

        SDK::APlayerController *lpLocalPlayerController = UnrealUtils::GetLocalPlayerController();
        if (nullptr == lpLocalPlayerController) {
            LogError("CheatManager", "Failed to get local player controller");
            return false;
        }

        if (
            nullptr != lpLocalPlayerController->CheatManager 
            && 
            lpLocalPlayerController->CheatManager->IsA(
                SDK::USurvivalCheatManager::StaticClass()
            )
        ) {
            LogMessage(
                "CheatManagerInit",
                "CheatManager already initialized for local player controller: "
                + CoreUtils::HexConvert(
                    reinterpret_cast<uintptr_t>(lpLocalPlayerController->CheatManager)
                )
            );
            SurvivalCheatManager = static_cast<SDK::USurvivalCheatManager*>(lpLocalPlayerController->CheatManager);
            return true;
        }

        SDK::UWorld *lpWorld = UnrealUtils::GetWorld();
        if (nullptr == lpWorld) {
            LogError("ItemSpawner", "UWorld is NULL");
            return false;
        }

        if (nullptr == lpWorld->AuthorityGameMode) {
            LogError("ItemSpawner", "Client has no host authority");
            return false;
        }

        LogMessage(
            "CheatManagerInit",
            "CheatManager is NULL for local player controller, initializing..."
        );

        lpLocalPlayerController->CheatClass = SDK::USurvivalCheatManager::StaticClass();

        LogMessage(
            "CheatManagerInit",
            "Creating new SurvivalCheatManager instance for local player controller: "
            + CoreUtils::HexConvert(
                reinterpret_cast<uintptr_t>(lpLocalPlayerController)
            )
        );

        SDK::USurvivalCheatManager* lpNewCheatManager = static_cast<SDK::USurvivalCheatManager*>(
            SDK::UGameplayStatics::SpawnObject(
                SDK::USurvivalCheatManager::StaticClass(),
                lpLocalPlayerController
            )
        );

        if (nullptr == lpNewCheatManager) {
            LogError("CheatManagerInit", "Failed to create CheatManager instance");
            return false;
        }

        lpLocalPlayerController->CheatManager = lpNewCheatManager;
        LogMessage(
            "CheatManagerInit",
            "CheatManager initialized successfully for local player controller: "
            + CoreUtils::HexConvert(
                reinterpret_cast<uintptr_t>(lpLocalPlayerController->CheatManager)
            )
        );

        if (nullptr == lpLocalPlayerController->CheatManager->Outer) {
            lpLocalPlayerController->CheatManager->Outer = lpLocalPlayerController;
        }

        SurvivalCheatManager = static_cast<SDK::USurvivalCheatManager*>(lpLocalPlayerController->CheatManager);

        LogMessage(
            "CheatManagerInit",
            "SurvivalCheatManager assigned: "
            + CoreUtils::HexConvert(
                reinterpret_cast<uintptr_t>(SurvivalCheatManager)
            ),
            true
        );

        LogMessage(
            "CheatManagerInit",
            "CheatManager successfully initialized"
        );

        return true;
    }

    // unused, but ManualInitialize() will accept target player ID in the future, so keeping it here
    // goal will be to assign guest multiplayer players their own CheatManager instances
    // its outdated af tho
    SDK::UCheatManager *GetPlayersCheatManager(
        int32_t iPlayerId
    ) {
        if (iPlayerId <= 0) {
            LogError("CheatManager", "Invalid player ID: " + std::to_string(iPlayerId));
            return nullptr;
        }

        SDK::UWorld *lpWorld = UnrealUtils::GetWorld();
        if (nullptr == lpWorld) {
            LogError("CheatManager", "Failed to get UWorld instance");
            return nullptr;
        }

        
        return nullptr;
    }

    void __gamethread CheatManagerEnableCheats(
        const CheatManagerEnableParams *lpParams
    ) {
        if (nullptr == lpParams) {
            LogError("CheatManager", "Params are NULL");
            return;
        }
        SDK::APlayerController *lpLocalPlayerController = lpParams->lpLocalPlayerController;
        if (nullptr == lpLocalPlayerController) {
            LogError("CheatManager", "Local player controller is NULL");
            return;
        }
        LogMessage(
            "CheatManager",
            "Enabling cheats for local player controller: "
            + std::to_string(reinterpret_cast<uintptr_t>(lpLocalPlayerController)),
            true
        );
        lpLocalPlayerController->EnableCheats();
        LogMessage(
            "CheatManager",
            "Cheats enabled for local player controller: "
            + std::to_string(reinterpret_cast<uintptr_t>(lpLocalPlayerController)),
            true
        );
    }

    void __gamethread CheatManagerExecute(
        const CheatManagerParams *lpParams
    ) {
        if (nullptr == SurvivalCheatManager) {
            LogError(
                "CheatManager",
                "SurvivalCheatManager is NULL, cannot execute cheat"
            );
            return;
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
            if (nullptr == SurvivalCheatManager) {
                LogError("CheatManager", "SurvivalCheatManager is NULL, cannot execute cheat");
                return;
            }
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

            case CheatManagerFunctionId::UnlockMutations: {
                SurvivalCheatManager->UnlockAllPerks();
                break;
            }

            case CheatManagerFunctionId::UnlockScabs: {
                SurvivalCheatManager->UnlockAllColorThemes();
                break;
            }
            case CheatManagerFunctionId::ToggleGod: {
                SurvivalCheatManager->God();
                break;
            }

            case CheatManagerFunctionId::ToggleAnalyzer: {
                SurvivalCheatManager->ResearchAllItems();   // Crash here, sometimes works though
                break;
            }

            case CheatManagerFunctionId::ToggleStamina: {
                if (nullptr == alpqwParams) {
                    LogError(
                        "CheatManager", 
                        "Params are NULL for ToggleStamina"
                    );
                    return;
                }
                bool bInfiniteStaminaEnabled = (bool) alpqwParams[0];
                SurvivalCheatManager->Stamina(bInfiniteStaminaEnabled);
                break;
            }

            case CheatManagerFunctionId::UnlockOmniTool: {
                SurvivalCheatManager->SetOmniToolTier(5);
                break;
            }
            /**
              * TODO:
              *  - CompleteQuest
              *  - CompleteActiveDefensePoint
            */
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