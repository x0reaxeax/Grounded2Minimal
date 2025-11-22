// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#include "CheatManager.hpp"
#include "UnrealUtils.hpp"
#include "ItemSpawner.hpp"
#include "CoreUtils.hpp"
#include "Command.hpp"
#include <iostream>

namespace Command {
    std::mutex CommandBufferMutex = {};
    std::condition_variable CommandBufferCondition = {};
    std::atomic<bool> CommandBufferCookedForExecution{ false }; // Atomic variable

    CommandBuffer GameCommandBuffer = {
        .Id = CommandId::CmdIdNone,
        .Params = nullptr
    };

    void WaitForCommandBufferReady(void) {
        std::unique_lock<std::mutex> lock(CommandBufferMutex);
        CommandBufferCondition.wait(lock, []() {
            return !CommandBufferCookedForExecution.load();
        });
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

                LogMessage(
                    "ProcessEvent",
                    "ItemSpawn - " + std::string(bRet ? "Success" : "Failure")
                );
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
                
                if (nullptr == lpParams->lpLocalPlayerController) {
                    LogError("ProcessEvent", "CmdIdSummon: LocalPlayerController is null");
                    break;
                }

                lpParams->lpLocalPlayerController->EnableCheats();

                SDK::UCheatManager *lpCheatManager = lpParams->lpLocalPlayerController->CheatManager;
                if (nullptr == lpCheatManager) {
                    LogError(
                        "ProcessEvent",
                        "CmdIdSummon: CheatManager is not initialized, aborting.."
                    );
                    break;
                }
                
                LogMessage(
                    "ProcessEvent",
                    "Summon - Player ID: " + std::to_string(lpParams->iPlayerId) +
                    ", Class: " + lpParams->fszClassName.ToString()
                );

                lpCheatManager->Summon(
                    lpParams->fszClassName
                );

                break;
            }

            case CommandId::CmdIdCullItemInstance: {
                // no output for this one to cut spam and save performance
                //LogMessage("ProcessEvent", "Command: Cull Item");
                
                CheatManager::Culling::BufferParamsCullItemInstance *lpParams =
                    static_cast<CheatManager::Culling::BufferParamsCullItemInstance*>(localBuffer.Params);

                if (nullptr == lpParams->lpItemInstance) {
                    //LogError("ProcessEvent", "CmdIdCullItemInstance: ItemInstance is null");
                    break;
                }

                CheatManager::Culling::CullItemInstance(
                    lpParams->lpItemInstance
                );

                break;

            }

            case CommandId::CmdIdCheatManagerExecute: {
                LogMessage("ProcessEvent", "Command: Execute Cheat Manager Command");
                
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

        lockUnique.unlock();
        CommandBufferCondition.notify_all();
    }
}