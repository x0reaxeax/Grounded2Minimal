// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#include "CheatManager.hpp"
#include "ItemSpawner.hpp"
#include "UnrealUtils.hpp"
#include "CoreUtils.hpp"
#include "C2Cycle.hpp"
#include "Command.hpp"
#include "Summon.hpp"
#include <iostream>

namespace Command {
    std::mutex CommandBufferMutex = {};
    std::condition_variable CommandBufferCondition = {};
    std::atomic<bool> CommandBufferCookedForExecution{false}; // Atomic variable

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

    void ProcessCommands(void) {
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

            case CommandId::CmdIdC2Cycle: {
                // Insert a newline to separate output from interpreter read symbol
                // this is normally not needed, because stuff is either entered from the interpreter directly,
                // or the output is usually disabled completely, but it's beneficial to log C2Cycle execution.
                LogChar(' ');
                LogMessage("ProcessEvent", "Command: C2 Cycle");

                C2Cycle::C2Cycle();
                break;
            }

            case CommandId::CmdIdSummon: {
                LogMessage("ProcessEvent", "Command: Summon Item");

                if (nullptr == localBuffer.Params) {
                    LogError("ProcessEvent", "CmdIdSummon: Params are null");
                    break;
                }

                Summon::BufferParamsSummon* lpParams = static_cast<Summon::BufferParamsSummon*>(localBuffer.Params);
                if (nullptr == lpParams->lpLocalPlayerController) {
                    LogError("ProcessEvent", "CmdIdSummon: LocalPlayerController is null");
                    break;
                }

                lpParams->lpLocalPlayerController->EnableCheats();

                SDK::UCheatManager *lpCheatManager = lpParams->lpLocalPlayerController->CheatManager;
                if (nullptr == lpCheatManager) {
                    LogError(
                        "ProcessEvent", 
                        "CmdIdSummon: CheatManager is null, attempting to force-unlock.."
                    );

                    CheatManager::UnlockMultiplayerCheatManager();

                    // retry enabling cheats
                    lpParams->lpLocalPlayerController->EnableCheats();

                    lpCheatManager = lpParams->lpLocalPlayerController->CheatManager;
                    break;
                }

                // check again if CheatManager is still null after unlock attempt
                if (nullptr == lpCheatManager) {
                    LogError(
                        "ProcessEvent",
                        "CmdIdSummon: CheatManager is still null after unlock attempt, aborting.."
                    );
                    break;
                }

                lpCheatManager->Summon(
                    lpParams->fszClassName
                );

                LogMessage(
                    "ProcessEvent", 
                    "Summon - Player ID: " + std::to_string(lpParams->iPlayerId) + 
                    ", Class: " + lpParams->fszClassName.ToString()
                );

                break;
            }

            case CommandId::CmdIdCullItemInstance: {
                LogMessage("ProcessEvent", "Command: Cull Item");
                if (nullptr == localBuffer.Params) {
                    LogError("ProcessEvent", "CmdIdCullItem: Params are null");
                    break;
                }
                C2Cycle::BufferParamsCullItem* lpParams = static_cast<C2Cycle::BufferParamsCullItem*>(localBuffer.Params);
                if (nullptr == lpParams->lpItemInstance) {
                    LogError("ProcessEvent", "CmdIdCullItem: Item instance is null");
                    break;
                }

                C2Cycle::CullItemInstance(lpParams->lpItemInstance);
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