/// @file CheatManager.cpp

#include "CheatManager.hpp"
#include "UnrealUtils.hpp"
#include "CoreUtils.hpp"
#include "Command.hpp"

namespace CheatManager {
    // Static cheats can be used on any player, as they don't rely on a SurvivalCheatManager instance
    namespace StaticCheats {
        
        // Helpers for int32_t values
        using GetIntStateFn = int32_t(*)(SDK::ASurvivalPlayerState*);
        using SetIntStateFn = void(*)(SDK::ASurvivalPlayerState*, int32_t);
        
        static int32_t HandleIntStateCheat(
            EStaticCheatOp eOperation,
            int32_t iTargetPlayerId,
            int32_t iNewValue,
            const std::string &szCheatName,
            GetIntStateFn fnGet,
            SetIntStateFn fnSet
        ) {
            SDK::ASurvivalPlayerState* lpSurvivalPlayerState =
                UnrealUtils::GetSurvivalPlayerStateById(iTargetPlayerId);
            if (nullptr == lpSurvivalPlayerState) {
                LogError(
                    "StaticCheatManager",
                    "Failed to get SurvivalPlayerState for '" + szCheatName + "'"
                );
                return -static_cast<int32_t>(EStaticCheatOp::_Debug);
            }
#pragma warning (push)
#pragma warning (disable: 26813) // bitwise op on enum
            if (EStaticCheatOp::Set == eOperation) {
                fnSet(lpSurvivalPlayerState, iNewValue);
                int32_t current = fnGet(lpSurvivalPlayerState);
                LogMessage(
                    "StaticCheatManager",
                    "Set '" + szCheatName + "' to '" 
                    + std::to_string(current) + "' for player ID " 
                    + std::to_string(iTargetPlayerId),
                    true
                );
                return current;

            } else if (EStaticCheatOp::Get == eOperation) {
                int32_t current = fnGet(lpSurvivalPlayerState);
                LogMessage(
                    "CheatManager",
                    "Current '" + szCheatName + "' value: '" 
                    + std::to_string(current) + "' for player ID " 
                    + std::to_string(iTargetPlayerId),
                    true
                );
                return current;
            }
#pragma warning (pop)

            return -static_cast<int32_t>(EStaticCheatOp::_Debug);
        }

        // Helpers for float values
        using GetFloatCharFn = float(*)(SDK::ASurvivalPlayerCharacter*);
        using SetFloatCharFn = void(*)(SDK::ASurvivalPlayerCharacter*, float);

        static float HandleFloatCharCheat(
            EStaticCheatOp eOperation,
            int32_t iTargetPlayerId,
            float fNewValue,
            const std::string &szCheatName,
            GetFloatCharFn fnGet,
            SetFloatCharFn fnSet
        ) {
            SDK::ASurvivalPlayerCharacter* lpSurvivalPlayerCharacter =
                UnrealUtils::GetSurvivalPlayerCharacterById(iTargetPlayerId);
            if (nullptr == lpSurvivalPlayerCharacter) {
                LogError(
                    "CheatManager",
                    "Failed to get SurvivalPlayerCharacter for '" + szCheatName + "'"
                );
                return -1.0f;
            }

#pragma warning (push)
#pragma warning (disable: 26813) // bitwise op on enum
            if (EStaticCheatOp::Set == eOperation) {
                fnSet(lpSurvivalPlayerCharacter, fNewValue);
                float current = fnGet(lpSurvivalPlayerCharacter);
                LogMessage(
                    "CheatManager",
                    "Set '" + szCheatName + "' to '" 
                    + std::to_string(current) + "' for player ID " 
                    + std::to_string(iTargetPlayerId),
                    true
                );
                return current;

            } else if (EStaticCheatOp::Get == eOperation) {
                float current = fnGet(lpSurvivalPlayerCharacter);
                LogMessage(
                    "CheatManager",
                    "Current '" + szCheatName + "' value: '" 
                    + std::to_string(current) + "' for player ID " 
                    + std::to_string(iTargetPlayerId),
                    true
                );
                return current;
            }
#pragma warning (pop)

            return -1.0f;
        }
        
        int32_t MaxActiveMutations(
            EStaticCheatOp eOperation,
            int32_t iTargetPlayerId,
            int32_t iMaxMutations  // desired total, inc. base 2
        ) {
            auto getter = [](SDK::ASurvivalPlayerState* ps) -> int32_t {
                return ps->PerkComponent->MaxEquippedPerks + 2;
            };

            auto setter = [](SDK::ASurvivalPlayerState* ps, int32_t desiredTotal) {
                if (desiredTotal < 2) {
                    desiredTotal = 2;
                }
                ps->PerkComponent->MaxEquippedPerks = (desiredTotal - 2);
            };

            return HandleIntStateCheat(
                eOperation,
                iTargetPlayerId,
                iMaxMutations,
                "MaxActiveMutations",
                getter,
                setter
            );
        }

        int32_t MaxCozinessLevelAchieved(
            EStaticCheatOp eOperation,
            int32_t iTargetPlayerId,
            int32_t iMaxCozinessLevel
        ) {
            auto getter = [](SDK::ASurvivalPlayerState* ps) -> int32_t {
                return ps->MaxCozinessLevelAchieved;
            };

            auto setter = [](SDK::ASurvivalPlayerState* ps, int32_t value) {
                ps->MaxCozinessLevelAchieved = value;
            };
            
            return HandleIntStateCheat(
                eOperation,
                iTargetPlayerId,
                iMaxCozinessLevel,
                "MaxCozinessLevelAchieved",
                getter,
                setter
            );
        }

        float StaminaRegenRate(
            EStaticCheatOp eOperation,
            int32_t iTargetPlayerId,
            float fNewRegenRate
        ) {
            auto getter = [](SDK::ASurvivalPlayerCharacter* pc) -> float {
                return pc->StaminaComponent->RegenRate;
            };
            auto setter = [](SDK::ASurvivalPlayerCharacter* pc, float value) {
                pc->StaminaComponent->RegenRate = value;
            };
            return HandleFloatCharCheat(
                eOperation,
                iTargetPlayerId,
                fNewRegenRate,
                "StaminaRegenRate",
                getter,
                setter
            );
        }

        float StaminaRegenDelay(
            EStaticCheatOp eOperation,
            int32_t iTargetPlayerId,
            float fNewRegenDelay
        ) {
            auto getter = [](SDK::ASurvivalPlayerCharacter* pc) -> float {
                return pc->StaminaComponent->RegenDelay;
            };
            auto setter = [](SDK::ASurvivalPlayerCharacter* pc, float value) {
                pc->StaminaComponent->RegenDelay = value;
            };
            return HandleFloatCharCheat(
                eOperation,
                iTargetPlayerId,
                fNewRegenDelay,
                "StaminaRegenDelay",
                getter,
                setter
            );
        }

        float NearbyStorageRadius(
            EStaticCheatOp eOperation,
            int32_t iTargetPlayerId,
            float fNewRadius
        ) {
            auto getter = [](SDK::ASurvivalPlayerCharacter* pc) -> float {
                return pc->ProximityInventoryComponent->StorageRadius;
            };

            auto setter = [](SDK::ASurvivalPlayerCharacter* pc, float value) {
                pc->ProximityInventoryComponent->StorageRadius = value;
            };

            return HandleFloatCharCheat(
                eOperation,
                iTargetPlayerId,
                fNewRadius,
                "NearbyStorageRadius",
                getter,
                setter
            );
        }

        float ChillRateMultiplier(
            EStaticCheatOp eOperation,
            int32_t iTargetPlayerId,
            float fNewMultiplier
        ) {
            auto getter = [](SDK::ASurvivalPlayerCharacter* pc) -> float {
                return pc->BodyTemperatureComponent->ChillRateMultiplier;
            };

            auto setter = [](SDK::ASurvivalPlayerCharacter* pc, float value) {
                pc->BodyTemperatureComponent->ChillRateMultiplier = value;
            };

            return HandleFloatCharCheat(
                eOperation,
                iTargetPlayerId,
                fNewMultiplier,
                "ChillRateMultiplier",
                getter,
                setter
            );
        }

        float SizzleRateMultiplier(
            EStaticCheatOp eOperation,
            int32_t iTargetPlayerId,
            float fNewMultiplier
        ) {
            auto getter = [](SDK::ASurvivalPlayerCharacter* pc) -> float {
                return pc->BodyTemperatureComponent->SizzleRateMultiplier;
            };

            auto setter = [](SDK::ASurvivalPlayerCharacter* pc, float value) {
                pc->BodyTemperatureComponent->SizzleRateMultiplier = value;
            };

            return HandleFloatCharCheat(
                eOperation,
                iTargetPlayerId,
                fNewMultiplier,
                "SizzleRateMultiplier",
                getter,
                setter
            );
        }

        float PerfectBlockWindow(
            EStaticCheatOp eOperation,
            int32_t iTargetPlayerId,
            float fNewWindow
        ) {
            auto getter = [](SDK::ASurvivalPlayerCharacter* pc) -> float {
                return pc->BlockComponent->PerfectBlockWindow;
            };

            auto setter = [](SDK::ASurvivalPlayerCharacter* pc, float value) {
                pc->BlockComponent->PerfectBlockWindow = value;
            };

            return HandleFloatCharCheat(
                eOperation,
                iTargetPlayerId,
                fNewWindow,
                "PerfectBlockWindow",
                getter,
                setter
            );
        }

        float DodgeDistance(
            EStaticCheatOp eOperation,
            int32_t iTargetPlayerId,
            float fNewDistance
        ) {
            auto getter = [](SDK::ASurvivalPlayerCharacter* pc) -> float {
                return pc->DodgeComponent->DodgeDistance;
            };

            auto setter = [](SDK::ASurvivalPlayerCharacter* pc, float value) {
                pc->DodgeComponent->DodgeDistance = value;
            };

            return HandleFloatCharCheat(
                eOperation,
                iTargetPlayerId,
                fNewDistance,
                "DodgeDistance",
                getter,
                setter
            );
        }

        // Hunger
        float CurrentFoodLevel(
            EStaticCheatOp eOperation,
            int32_t iTargetPlayerId,
            float fNewFoodLevel
        ) {
            auto getter = [](SDK::ASurvivalPlayerCharacter* pc) -> float {
                return pc->SurvivalComponent->CurrentFood;
            };

            auto setter = [](SDK::ASurvivalPlayerCharacter* pc, float value) {
                pc->SurvivalComponent->CurrentFood = value;
            };

            return HandleFloatCharCheat(
                eOperation,
                iTargetPlayerId,
                fNewFoodLevel,
                "CurrentFoodLevel",
                getter,
                setter
            );
        }

        // Thirst
        float CurrentWaterLevel(
            EStaticCheatOp eOperation,
            int32_t iTargetPlayerId,
            float fNewThirstLevel
        ) {
            auto getter = [](SDK::ASurvivalPlayerCharacter* pc) -> float {
                return pc->SurvivalComponent->CurrentWater;
            };

            auto setter = [](SDK::ASurvivalPlayerCharacter* pc, float value) {
                pc->SurvivalComponent->CurrentWater = value;
            };
            
            return HandleFloatCharCheat(
                eOperation,
                iTargetPlayerId,
                fNewThirstLevel,
                "CurrentWaterLevel",
                getter,
                setter
            );
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

    namespace Culling {
        void __gamethread CullItemInstance(
            SDK::ASpawnedItem *lpItemToCull
        ) {
            if (nullptr == lpItemToCull) {
                return;
            }

            if (lpItemToCull->IsActorBeingDestroyed() || lpItemToCull->bActorIsBeingDestroyed) {
                return;
            }

            lpItemToCull->SetLifeSpan(0.01f);
        }

        void CullItemByItemIndex(
            int32_t iItemIndex
        ) {
            SDK::ASpawnedItem *lpItemToCull = UnrealUtils::GetSpawnedItemByIndex(iItemIndex);
            if (nullptr == lpItemToCull) {
                LogError("Cull", "Invalid item index: " + std::to_string(iItemIndex));
                return;
            }

            if (lpItemToCull->bActorIsBeingDestroyed) {
                LogError("Cull", "Item is already being destroyed: " + lpItemToCull->GetName());
                return; // Item is already being destroyed
            }

            BufferParamsCullItemInstance *lpParams = new BufferParamsCullItemInstance{
                .lpItemInstance = lpItemToCull
            };

            Command::SubmitTypedCommand(
                Command::CommandId::CmdIdCullItemInstance,
                lpParams
            );
        }

        void CullAllItemInstances(
            std::string &szTargetItemTypeName,
            bool bSilent
        ) {
            if (szTargetItemTypeName.empty()) {
                LogError("Culling", "Target object name is empty");
                return;
            }

            int64_t iTotalCulled = 0;

            for (int32_t i = 0; i < SDK::ASpawnedItem::GObjects->Num(); i++) {
                SDK::UObject *lpObj = SDK::ASpawnedItem::GObjects->GetByIndex(i);
                if (nullptr == lpObj) {
                    continue;
                }

                if (!lpObj->IsA(SDK::ASpawnedItem::StaticClass())) {
                    continue;
                }

                SDK::ASpawnedItem *lpSpawnedItem = static_cast<SDK::ASpawnedItem*>(lpObj);

                std::string szFullObjectName = lpSpawnedItem->GetFullName();
                if (!CoreUtils::StringContainsCaseInsensitive(
                    szFullObjectName,
                    szTargetItemTypeName
                )) {
                    continue;
                }

                // Don't cull if item FVector coordinates are all 0
                /*SDK::FVector vItemLocation = lpSpawnedItem->K2_GetActorLocation();
                if (vItemLocation.X == 0.0f && vItemLocation.Y == 0.0f && vItemLocation.Z == 0.0f) {
                    continue;
                }*/

                //CullItemInstance(static_cast<SDK::ASpawnedItem*>(lpObj));
                BufferParamsCullItemInstance *lpParams = new BufferParamsCullItemInstance{
                    .lpItemInstance = lpSpawnedItem
                };

                Command::SubmitTypedCommand(
                    Command::CommandId::CmdIdCullItemInstance,
                    lpParams
                );

                iTotalCulled++;

                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }

            if (!bSilent) {
                LogMessage(
                    "Culling",
                    "Culled a total of " + std::to_string(iTotalCulled)
                    + " item instances matching '" + szTargetItemTypeName + "'"
                );
            }
        }
    }

    // Local Player's SurvivalCheatManager instance
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

        if (nullptr == SurvivalCheatManager->Outer) {
            LogError(
                "CheatManager", 
                "SurvivalCheatManager Outer is NULL", 
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
                    ) + " for player id: " + std::to_string(
                        lpPlayerController->PlayerState->PlayerId
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
        } else {
            LogMessage(
                "CheatManager",
                "No SurvivalCheatManager instance to destroy",
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

    SDK::UCheatManager *GetPlayersCheatManager(
        int32_t iPlayerId
    ) {
        if (INVALID_PLAYER_ID == iPlayerId) {
            iPlayerId = UnrealUtils::GetLocalPlayerId(true);
        }

        SDK::APlayerController *lpPlayerController = 
            UnrealUtils::GetPlayerControllerById(iPlayerId);

        if (nullptr != lpPlayerController) {
            return lpPlayerController->CheatManager;
        }
        
        return nullptr;
    }

    SDK::USurvivalCheatManager *GetPlayersSurvivalCheatManager(
        int32_t iPlayerId
    ) {
        SDK::UCheatManager *lpCheatManager = GetPlayersCheatManager(iPlayerId);
        if (nullptr == lpCheatManager) {
            LogError(
                "CheatManager",
                "Failed to get CheatManager for player id: " + std::to_string(iPlayerId)
            );
            return nullptr;
        }
        if (!lpCheatManager->IsA(SDK::USurvivalCheatManager::StaticClass())) {
            LogError(
                "CheatManager",
                "CheatManager is not a SurvivalCheatManager for player id: "
                + std::to_string(iPlayerId)
            );
            return nullptr;
        }
        return static_cast<SDK::USurvivalCheatManager*>(lpCheatManager);
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
                "Target SurvivalCheatManager is NULL, cannot execute cheat"
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
                SurvivalCheatManager->SetOmniToolTier(6);
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