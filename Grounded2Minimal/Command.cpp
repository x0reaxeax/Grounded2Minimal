// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#include "CheatManager.hpp"
#include "UnrealUtils.hpp"
#include "ItemSpawner.hpp"
#include "PlayerCache.hpp"
#include "CoreUtils.hpp"
#include "Command.hpp"

#include <chrono>
#include <iostream>

namespace Command {
    std::mutex CommandBufferMutex = {};
    std::condition_variable CommandBufferCondition = {};
    std::atomic<bool> CommandBufferCookedForExecution{ false };

    CommandBuffer GameCommandBuffer = {
        .Id = CommandId::CmdIdNone,
        .Params = nullptr
    };

    void WaitForCommandBufferReady(void) {
        std::unique_lock<std::mutex> lock(CommandBufferMutex);
        CommandBufferCondition.wait(lock, []() {
            return !CommandBufferCookedForExecution.load() || IsReloadInProgress();
        });
    }

    bool WaitForCommandBufferReady(uint32_t dwTimeoutMilliseconds) {
        std::unique_lock<std::mutex> lock(CommandBufferMutex);
        return CommandBufferCondition.wait_for(
            lock,
            std::chrono::milliseconds(dwTimeoutMilliseconds),
            []() {
                return !CommandBufferCookedForExecution.load() || IsReloadInProgress();
            }
        );
    }

    void DiscardPendingCommand(void) {
        std::unique_lock<std::mutex> lock(CommandBufferMutex);

        // fuck it off
        if (nullptr != GameCommandBuffer.Deleter && nullptr != GameCommandBuffer.Params) {
            GameCommandBuffer.Deleter(GameCommandBuffer.Params);
        }

        GameCommandBuffer.Id = CommandId::CmdIdNone;
        GameCommandBuffer.Params = nullptr;
        GameCommandBuffer.Deleter = nullptr;
        CommandBufferCookedForExecution.store(false);

        lock.unlock();
        CommandBufferCondition.notify_all();
    }

    void __gamethread ProcessCommands(void) {
        std::unique_lock<std::mutex> lockUnique(CommandBufferMutex);

        if (!CommandBufferCookedForExecution.load()) {
            return;
        }

        // Copy command data
        CommandBuffer localBuffer = GameCommandBuffer;

        // Reset the buffer immediately while holding lock
        GameCommandBuffer.Id = CommandId::CmdIdNone;
        GameCommandBuffer.Params = nullptr;
        GameCommandBuffer.Deleter = nullptr;

        // Process command without holding lock
        lockUnique.unlock();

        switch (localBuffer.Id) {
            case CommandId::CmdIdSpawnItem: {
                LogMessage("ProcessEvent", "Command: Spawn Item");

                if (nullptr == localBuffer.Params) {
                    LogError("ProcessEvent", "CmdIdSpawnItem: Params are null");
                    break;
                }

                auto* lpParams = static_cast<ItemSpawner::BufferParamsSpawnItem*>(localBuffer.Params);
                bool bRet = ItemSpawner::GiveItemToPlayer(
                    lpParams->iPlayerId,
                    lpParams->szItemName,
                    lpParams->szDataTableName,
                    lpParams->iCount
                );

                // ig this is more valid than just smacking "success (source: trust me bro)"
                LogMessage(
                    "ProcessEvent",
                    "ItemSpawn - " + std::string(bRet ? "Dispatched" : "Rejected")
                );
                break;
            }

            case CommandId::CmdIdEnumPlayers: {
                if (nullptr == localBuffer.Params) {
                    LogError("ProcessEvent", "CmdIdEnumPlayers: Params are null");
                    break;
                }

                Params::EnumPlayers* lpParams =
                    static_cast<Params::EnumPlayers*>(localBuffer.Params);
                
                if (nullptr == lpParams->Results) {
                    LogError("ProcessEvent", "CmdIdEnumPlayers: Result storage is null");
                    break;
                }

                std::vector<SDK::APlayerState*> vPlayerStates;
                UnrealUtils::DumpConnectedPlayers(&vPlayerStates, lpParams->bHideOutput);
                PlayerCache::BuildPlayerCache(&vPlayerStates);

                lpParams->Results->clear();
                lpParams->Results->reserve(vPlayerStates.size());
                for (SDK::APlayerState* lpPlayerState : vPlayerStates) {
                    if (!UnrealUtils::IsValidUObject(lpPlayerState)) {
                        continue;
                    }

                    lpParams->Results->push_back(Params::PlayerListEntry{
                        .iPlayerId = lpPlayerState->PlayerId,
                        .wszPlayerName = lpPlayerState->GetPlayerName().ToWString(),
                        .bHostAuthority = lpPlayerState->HasAuthority()
                    });
                }
                break;
            }

            case CommandId::CmdIdSummon: {
                LogMessage("ProcessEvent", "Command: Summon Item");

                if (nullptr == localBuffer.Params) {
                    LogError("ProcessEvent", "CmdIdSummon: Params are null");
                    break;
                }

                CheatManager::Summon::BufferParamsSummon* lpParams = 
                    static_cast<CheatManager::Summon::BufferParamsSummon*>(localBuffer.Params);

                if (lpParams->wszClassName.empty()) {
                    LogError("ProcessEvent", "CmdIdSummon: Class name is empty");
                    break;
                }

                SDK::ASurvivalPlayerController* lpLocalPlayerController =
                    UnrealUtils::GetLocalSurvivalPlayerControllerFast();
                if (
                    nullptr == lpLocalPlayerController
                    || !UnrealUtils::IsValidUObject(lpLocalPlayerController)
                ) {
                    LogError("ProcessEvent", "CmdIdSummon: LocalPlayerController is unavailable");
                    break;
                }

                SDK::APlayerState* lpPlayerState = lpLocalPlayerController->PlayerState;
                if (
                    nullptr == lpPlayerState
                    || !UnrealUtils::IsValidUObject(lpPlayerState)
                    || !lpPlayerState->HasAuthority()
                ) {
                    LogError("ProcessEvent", "CmdIdSummon: Local player has no summon authority");
                    break;
                }

                lpLocalPlayerController->EnableCheats();

                SDK::UCheatManager *lpCheatManager = lpLocalPlayerController->CheatManager;
                if (
                    nullptr == lpCheatManager
                    || !UnrealUtils::IsValidUObject(lpCheatManager)
                ) {
                    LogError(
                        "ProcessEvent",
                        "CmdIdSummon: CheatManager is not initialized, aborting.."
                    );
                    break;
                }
                
                std::string szClassName;
                if (!CoreUtils::WideStringToUtf8(lpParams->wszClassName, szClassName)) {
                    LogError("ProcessEvent", "CmdIdSummon: Failed to encode class name for logging");
                    break;
                }

                LogMessage(
                    "ProcessEvent",
                    "Summon - Player ID: " + std::to_string(lpPlayerState->PlayerId) +
                    ", Class: " + szClassName
                );

                SDK::FString fszClassName(lpParams->wszClassName.c_str());
                lpCheatManager->Summon(fszClassName);

                break;
            }

            case CommandId::CmdIdCullItemInstance: {
                // no output for this one to cut spam and save performance
                //LogMessage("ProcessEvent", "Command: Cull Item");
                
                CheatManager::Culling::BufferParamsCullItemInstance *lpParams =
                    reinterpret_cast<CheatManager::Culling::BufferParamsCullItemInstance*>(localBuffer.Params);

                if (nullptr == lpParams->lpItemInstance) {
                    LogError("ProcessEvent", "CmdIdCullItemInstance: ItemInstance is null");
                    break;
                }

                CheatManager::Culling::CullItemInstance(
                    lpParams->lpItemInstance
                );

                break;

            }

            case CommandId::CmdIdSetCulledItemOwner: {
                LogMessage("ProcessEvent", "Command: Set Culled Item Owner");
                if (nullptr == localBuffer.Params) {
                    LogError("ProcessEvent", "CmdIdSetCulledItemOwner: Params are null");
                    break;
                }
                
                CheatManager::Culling::BufferParamsSetItemOwner *lpParams = 
                    reinterpret_cast<CheatManager::Culling::BufferParamsSetItemOwner*>(localBuffer.Params);

                if (nullptr == lpParams->lpItemInstance) {
                    LogError("ProcessEvent", "CmdIdSetCulledItemOwner: ItemInstance is null");
                    break;
                }

                if (nullptr == lpParams->lpNewOwner) {
                    LogError("ProcessEvent", "CmdIdSetCulledItemOwner: NewOwner is null");
                    break;
                }

                CheatManager::Culling::SetCulledItemOwner(
                    lpParams->lpItemInstance,
                    lpParams->lpNewOwner
                );

                break;
            }

            case CommandId::CmdIdCheatManagerExecute: {
                LogMessage("ProcessEvent", "Command: Execute Cheat Manager Command", true);
                
                CheatManager::BufferParamsExecuteCheat *lpParams =
                    static_cast<CheatManager::BufferParamsExecuteCheat*>(localBuffer.Params);

                LogMessage(
                    "ProcessEvent",
                    "CheatManagerExecute - Function ID: " +
                    std::to_string(static_cast<uint16_t>(lpParams->FunctionId)),
                    true
                );

                CheatManager::CheatManagerExecute(
                    lpParams
                );

                break;
            }

            case CommandId::CmdIdEnableCheats: {
                LogMessage("ProcessEvent", "Command: Enable Cheats");
                if (nullptr == localBuffer.Params) {
                    LogError("ProcessEvent", "CmdIdEnableCheats: Params are null");
                    break;
                }

                CheatManager::CheatManagerEnableParams *lpParams =
                    static_cast<CheatManager::CheatManagerEnableParams*>(localBuffer.Params);

                LogMessage(
                    "ProcessEvent",
                    "CheatManagerEnable - Enabling cheats for player controller: " +
                    CoreUtils::HexConvert(reinterpret_cast<uintptr_t>(lpParams->lpLocalPlayerController)),
                    true
                );

                CheatManager::CheatManagerEnableCheats(
                    lpParams
                );

                break;
            }

            case CommandId::CmdIdUnlockAchievement: {
                LogMessage("ProcessEvent", "Command: Unlock Achievement");
                if (nullptr == localBuffer.Params) {
                    LogError("ProcessEvent", "CmdIdUnlockAchievement: Params are null");
                    break;
                }
                Params::UnlockAchievement* lpParams =
                    static_cast<Params::UnlockAchievement*>(localBuffer.Params);
                if (nullptr == lpParams->lpPlayerState) {
                    LogError("ProcessEvent", "CmdIdUnlockAchievement: PlayerState is null");
                    break;
                }
                LogMessage(
                    "ProcessEvent",
                    "UnlockAchievement - Player: " +
                    lpParams->lpPlayerState->GetPlayerName().ToString() +
                    ", Achievement: " + lpParams->AchievementName.ToString(),
                    true
                );
                lpParams->lpPlayerState->AwardAchievement(lpParams->AchievementName);
                break;
            }

            case CommandId::CmdIdSetCollision: {
                LogMessage("ProcessEvent", "Command: Set Collision");
                if (nullptr == localBuffer.Params) {
                    LogError("ProcessEvent", "CmdIdSetCollision: Params are null");
                    break;
                }
                Params::SetCollision* lpParams =
                    static_cast<Params::SetCollision*>(localBuffer.Params);
                if (nullptr == lpParams->lpPlayerState) {
                    LogError("ProcessEvent", "CmdIdSetCollision: PlayerState is null");
                    break;
                }
                LogMessage(
                    "ProcessEvent",
                    "SetCollision - Player: " +
                    lpParams->lpPlayerState->GetPlayerName().ToString() +
                    ", New State: " + std::string(lpParams->bNewCollisionState ? "Enabled" : "Disabled"),
                    true
                );
                lpParams->lpPlayerState->SetActorEnableCollision(lpParams->bNewCollisionState);
                break;
            }

            case CommandId::CmdIdSetGameMode: {
                LogMessage("ProcessEvent", "Command: Set Game Mode", true);

                if (nullptr == localBuffer.Params) {
                    LogError("ProcessEvent", "CmdIdSetGameMode: Params are null");
                    break;
                }

                Params::SetGameMode* lpParams =
                    static_cast<Params::SetGameMode*>(localBuffer.Params);

                if (nullptr == lpParams->lpSurvivalModeManager) {
                    LogError("ProcessEvent", "CmdIdSetGameMode: SurvivalModeManager is null");
                    break;
                }

                LogMessage(
                    "ProcessEvent",
                    "SetGameMode - New Game Mode: " +
                    std::to_string(static_cast<uint16_t>(lpParams->eNewGameMode)),
                    true
                );

                lpParams->lpSurvivalModeManager->SetGameMode(lpParams->eNewGameMode);
                break;
            }

            case CommandId::CmdIdDebugEvent: {
                // ApplyDamage
                break;
            }
            
            default: {
                LogError(
                    "ProcessEvent",
                    "Unknown Command ID: " + std::to_string(static_cast<uint16_t>(localBuffer.Id))
                );
                break;
            }
        }


        // Clean up memory
        if (nullptr != localBuffer.Deleter && nullptr != localBuffer.Params) {
            localBuffer.Deleter(localBuffer.Params);
        }

        CommandBufferCookedForExecution.store(false);

        CommandBufferCondition.notify_all();
    }
}