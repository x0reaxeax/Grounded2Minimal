// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#include "Grounded2Minimal.hpp"
#include "ItemSpawner.hpp"
#include "UnrealUtils.hpp"
#include "Command.hpp"

#include <sstream>

#include "SDK/BP_SurvivalGameMode_classes.hpp"

namespace ItemSpawner {
    // Allows everyone (not just host) to utilize the host to spawn them items.
    bool GlobalCheatMode = false;

    bool GiveItemToPlayer(
        int32_t iPlayerId,
        const std::string& szItemName,
        const std::string& szDataTableName,
        int32_t iItemCount // No default argument here
    ) {
        SDK::UWorld *lpWorld = UnrealUtils::GetWorld(true);
        if (nullptr == lpWorld) {
            LogError("ItemSpawner", "UWorld is NULL");
            return false;
        }

        if (nullptr == lpWorld->AuthorityGameMode) {
            LogError("ItemSpawner", "Client has no host authority");
            return false;
        }

        SDK::ABP_SurvivalGameMode_C* lpSurvivalGameMode = static_cast<SDK::ABP_SurvivalGameMode_C*>(lpWorld->AuthorityGameMode);
        if (nullptr == lpSurvivalGameMode) {
            LogError("ItemSpawner", "AuthorityGameMode is not of type ABP_SurvivalGameMode_C");
            return false;
        }

        SDK::AGameStateBase *lpGameStateBase = lpWorld->GameState;
        if (nullptr == lpGameStateBase) {
            LogError("PlayerInfo", "Unable to get GameStateBase");
            return false;
        }

        int32_t iTotalPlayers = g_vPlayers.size();
        LogMessage(
            "ItemSpawner",
            "Total players in game: " + std::to_string(iTotalPlayers)
        );

        for (int32_t i = 0; i < iTotalPlayers; i++) {
            SDK::APlayerState *lpPlayerState = g_vPlayers.at(i);
            if (nullptr == lpPlayerState) {
                continue;
            }

            if (iPlayerId != lpPlayerState->PlayerId) {
                continue; // didnt wanna give it to this guy anyway
            }

            std::string szPlayerName = lpPlayerState->GetPlayerName().ToString();
            LogMessage(
                "ItemSpawner",
                "Found player: " + szPlayerName + " (ID: " + std::to_string(iPlayerId) + ")"
            );

            SDK::APawn *lpPawn = lpPlayerState->PawnPrivate;
            if (nullptr == lpPawn) {
                LogError("ItemSpawner", "No Pawn found for player: " + szPlayerName);
                continue;
            }

            SDK::ABP_SurvivalPlayerCharacter_C *lpPlayerCharacter = static_cast<SDK::ABP_SurvivalPlayerCharacter_C*>(lpPawn);
            if (nullptr == lpPlayerCharacter) {
                LogError("ItemSpawner", "No player character found for player: " + szPlayerName);
                continue;
            }

            SDK::UDataTable* lpDataTable = UnrealUtils::GetDataTablePointer(szDataTableName);
            if (nullptr == lpDataTable) {
                LogError("ItemSpawner", "DataTable not found: " + szDataTableName);
                return false; // what the fok are you spawning bro
            }

            SDK::FDataTableRowHandle ItemRowHandle;
            ZeroMemory(&ItemRowHandle, sizeof(ItemRowHandle));

            if (!UnrealUtils::GetItemRowMap(lpDataTable, szItemName, &ItemRowHandle)) {
                LogError("ItemSpawner", "Item not found in DataTable: " + szItemName);
                return false;
            }

            lpSurvivalGameMode->GrantItemsToPlayer(
                lpPlayerCharacter,
                ItemRowHandle,
                iItemCount
            );
            return true;
        }

        LogError("ItemSpawner", "Player ID " + std::to_string(iPlayerId) + " not found");
        return false;
    }

    void EvaluateChatSpawnRequestSafe(
        SafeChatMessageData* lpMessageData
    ) {
        DisableGlobalOutput();
        if (nullptr == lpMessageData) {
            LogError("ItemSpawner", "Null message data received", true);
            EnableGlobalOutput();   // could just fking goto to the end like in a normal language,
            // but the compiler complains about some shit, tupy kokot
            return;
        }

        // I forgot whether this is checked inside chat hook, so let's waste some time here
        if (!GlobalCheatMode) {
            LogMessage("ItemSpawner", "Global cheat mode is disabled", true);
            EnableGlobalOutput();
            return;
        }

        int32_t iHostPlayerId = UnrealUtils::GetLocalPlayerId(true);
        if (iHostPlayerId <= 0) {
            LogError("ItemSpawner", "Invalid host player ID", true);
            EnableGlobalOutput();
            return;
        }

        if (lpMessageData->szMessage.empty()) {
            LogError("ItemSpawner", "Chat message is empty", true);
            EnableGlobalOutput();
            return;
        }

        // Validate sender ID
        if (lpMessageData->iSenderId <= 0) {
            LogError(
                "ItemSpawner",
                "Invalid sender player ID: " + std::to_string(lpMessageData->iSenderId),
                true
            );
            EnableGlobalOutput();
            return;
        }

        const std::string szCommandPrefix = "/spawnitem";
        if (!lpMessageData->szMessage.contains(szCommandPrefix)) {
            // Not a spawn command - this is normal, not an error
            EnableGlobalOutput();
            return;
        }

        // Parse command
        size_t nPos = lpMessageData->szMessage.find(szCommandPrefix);
        if (nPos == std::string::npos) {
            LogError(
                "ItemSpawner",
                "Command prefix not found in chat message",
                true
            );
            EnableGlobalOutput();
            return;
        }

        std::string szCommandPart = lpMessageData->szMessage.substr(
            nPos + szCommandPrefix.length()
        );

        // Trim leading spaces
        szCommandPart.erase(0, szCommandPart.find_first_not_of(" \t"));

        // Validate command has content after prefix
        if (szCommandPart.empty()) {
            LogError(
                "ItemSpawner",
                "No parameters provided for spawn command",
                true
            );
            EnableGlobalOutput();
            return;
        }

        // Parse parameters
        std::istringstream issCommandStream(szCommandPart);
        std::string szTargetItemName;
        std::string szItemCountStr;

        if (!(issCommandStream >> szTargetItemName)) {
            LogError(
                "ItemSpawner",
                "Missing target item name in chat message",
                true
            );
            EnableGlobalOutput();
            return;
        }

        // Validate item name
        if (szTargetItemName.empty() || szTargetItemName.length() > 100) { // Reasonable limit
            LogError(
                "ItemSpawner",
                "Invalid item name: " + szTargetItemName,
                true
            );
            EnableGlobalOutput();
            return;
        }

        // Parse item count with validation
        int32_t iItemCount = 1; // Default
        if (issCommandStream >> szItemCountStr) {
            try {
                iItemCount = std::stoi(szItemCountStr);
            } catch (const std::invalid_argument& e) {
                LogError(
                    "ItemSpawner",
                    "Invalid item count format: " + szItemCountStr,
                    true
                );
                EnableGlobalOutput();
                return;
            } catch (const std::out_of_range& e) {
                LogError(
                    "ItemSpawner",
                    "Item count out of range: " + szItemCountStr,
                    true
                );
                EnableGlobalOutput();
                return;
            }
        }

        // Validate item count range
        if (iItemCount <= 0 || iItemCount > 999) { // enough is enough
            LogError(
                "ItemSpawner",
                "Item count out of valid range (1-999): " + std::to_string(iItemCount),
                true
            );
            EnableGlobalOutput();
            return;
        }

        const std::string szDataTableName = "Table_AllItems";
        SDK::UDataTable *lpAllItemsTable = UnrealUtils::GetDataTablePointer(szDataTableName);

        if (nullptr == lpAllItemsTable) {
            LogError(
                "ItemSpawner",
                "DataTable not found for item: " + szTargetItemName,
                true
            );
            EnableGlobalOutput();
            return; // DataTable not found for the item
        }

        if (!UnrealUtils::GetItemRowMap(
            lpAllItemsTable,
            szTargetItemName,
            nullptr
        )) {
            LogError(
                "ItemSpawner",
                "Item '" + szTargetItemName + "' not found in DataTable: " + szDataTableName,
                true
            );
            EnableGlobalOutput();
            return; // Item not found in the DataTable
        }

        LogMessage(
            "ItemSpawner", "Spawning item: " + szTargetItemName +
            " (Count: " + std::to_string(iItemCount) +
            ", Table: " + szDataTableName +
            ", Player: " + std::to_string(lpMessageData->iSenderId) +
            " - " + lpMessageData->szSenderName + ")",
            true
        );

        // Create command parameters
        auto* lpParams = new (std::nothrow) BufferParamsSpawnItem{
            .iPlayerId = lpMessageData->iSenderId,
            .szItemName = szTargetItemName,
            .szDataTableName = szDataTableName,
            .iCount = iItemCount
        };

        if (nullptr == lpParams) {
            LogError(
                "ItemSpawner",
                "Failed to allocate memory for spawn command parameters",
                true
            );
            EnableGlobalOutput();
            return;
        }

        // Submit command with error handling
        try {
            Command::SubmitTypedCommand(
                Command::CommandId::CmdIdSpawnItem,
                lpParams
            );
            LogMessage(
                "ItemSpawner",
                "Spawn command submitted successfully",
                true
            );
        } catch (const std::exception& e) {
            LogError(
                "ItemSpawner",
                "Exception submitting spawn command: " + std::string(e.what()),
                true
            );
            delete lpParams;
        } catch (...) {
            LogError(
                "ItemSpawner",
                "Unknown exception submitting spawn command",
                true
            );
            delete lpParams; // and fuk ur skins too
        }

_RYUJI:
        // Wait for command processing to complete
        /*while (Command::CommandBufferCookedForExecution) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }*/
        Command::WaitForCommandBufferReady();

        EnableGlobalOutput();
    }
}