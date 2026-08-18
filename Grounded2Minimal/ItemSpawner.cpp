// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#include "Grounded2Minimal.hpp"
#include "ItemSpawner.hpp"
#include "UnrealUtils.hpp"
#include "Command.hpp"

#include <sstream>

#include "SDK/BP_SurvivalGameMode_classes.hpp"
#include "SDK/BP_SurvivalGameMode_parameters.hpp"


namespace ItemSpawner {
    // Allows everyone (not just host) to utilize the host to spawn them items.
    std::atomic<bool> GlobalCheatMode{ false };

    SDK::ABP_SurvivalPlayerCharacter_C* ResolveCurrentPlayerCharacter(
        SDK::APlayerState* lpPlayerState
    ) {
        SDK::AController* lpController = nullptr;
        SDK::APawn* lpPawn = nullptr;
        bool bLocalPlayer = false;

        SDK::ASurvivalPlayerController* lpLocalController =
            UnrealUtils::GetLocalSurvivalPlayerControllerFast();

        if (
            UnrealUtils::IsValidUObject(lpLocalController)
            && UnrealUtils::IsValidUObject(lpLocalController->PlayerState)
            && lpLocalController->PlayerState->PlayerId == lpPlayerState->PlayerId
        ) {
            bLocalPlayer = true;
            lpController = lpLocalController;
            lpPawn = lpLocalController->Pawn;
        } else {
            lpPawn = lpPlayerState->PawnPrivate;
            if (UnrealUtils::IsValidUObject(lpPawn)) {
                lpController = lpPawn->Controller;
            }
        }

        if (
            !UnrealUtils::IsValidUObject(lpController)
            || !UnrealUtils::IsValidUObject(lpPawn)
            || lpController->Pawn != lpPawn
            || (
                bLocalPlayer
                ? (
                    !UnrealUtils::IsValidUObject(lpController->PlayerState)
                    || 
                    lpController->PlayerState->PlayerId != lpPlayerState->PlayerId
                )
                : lpController->PlayerState != lpPlayerState
            )
        ) {
            return nullptr;
        }

        if (!lpPawn->IsA(SDK::ABP_SurvivalPlayerCharacter_C::StaticClass())) {
            return nullptr;
        }

        return static_cast<SDK::ABP_SurvivalPlayerCharacter_C*>(lpPawn);
    }

    bool GrantItemsToCurrentPlayer(
        SDK::ABP_SurvivalGameMode_C* lpSurvivalGameMode,
        SDK::ABP_SurvivalPlayerCharacter_C* lpPlayerCharacter,
        const SDK::FDataTableRowHandle& ItemRowHandle,
        int32_t iItemCount
    ) {
        if (iItemCount <= 0) {
            // no takze vy nic nechcete, idem dopice odtialto
            return false;
        }

        if (
            !UnrealUtils::IsValidUObject(lpSurvivalGameMode)
            || !UnrealUtils::IsValidUObject(lpSurvivalGameMode->Class)
            || !UnrealUtils::IsValidUObject(lpPlayerCharacter)
        ) {
            return false;
        }

        SDK::UFunction* lpGrantItemsFunction = lpSurvivalGameMode->Class->GetFunction(
            "BP_SurvivalGameMode_C",
            "GrantItemsToPlayer"
        );
        if (!UnrealUtils::IsValidUObject(lpGrantItemsFunction)) {
            LogError("ItemSpawner", "Unable to resolve the current GrantItemsToPlayer function");
            return false;
        }

        SDK::Params::BP_SurvivalGameMode_C_GrantItemsToPlayer Params{};
        Params.SurvivalPlayer = lpPlayerCharacter;
        Params.ItemData = ItemRowHandle;
        Params.Count = iItemCount;

        lpSurvivalGameMode->ProcessEvent(lpGrantItemsFunction, &Params);
        return true;
    }

    bool __gamethread GiveItemToPlayer(
        int32_t iPlayerId,
        const std::string& szItemName,
        const std::string& szDataTableName,
        int32_t iItemCount // No default argument here
    ) {
        if (iItemCount <= 0) {
            LogError("ItemSpawner", "Item count must be greater than zero");
            return false;
        }

        if (szItemName.empty()) {
            LogError("ItemSpawner", "Item name cannot be empty");
            return false;
        }

        if (IsReloadInProgress()) {
            LogError(
                "ItemSpawner", 
                "Cannot spawn items while runtime reload is in progress"
            );
            return false;
        }

        SDK::UWorld *lpWorld = SDK::UWorld::GetWorld();
        if (!UnrealUtils::IsValidUObject(lpWorld)) {
            LogError("ItemSpawner", "Current UWorld is unavailable");
            return false;
        }

        SDK::AGameModeBase* lpAuthorityGameMode = lpWorld->AuthorityGameMode;
        if (!UnrealUtils::IsValidUObject(lpAuthorityGameMode)) {
            LogError("ItemSpawner", "Client has no host authority");
            return false;
        }

        if (!lpAuthorityGameMode->IsA(SDK::ABP_SurvivalGameMode_C::StaticClass())) {
            LogError("ItemSpawner", "AuthorityGameMode is not of type ABP_SurvivalGameMode_C");
            return false;
        }

        SDK::ABP_SurvivalGameMode_C* lpSurvivalGameMode =
            static_cast<SDK::ABP_SurvivalGameMode_C*>(lpAuthorityGameMode);

        SDK::AGameStateBase *lpGameStateBase = lpWorld->GameState;
        if (!UnrealUtils::IsValidUObject(lpGameStateBase)) {
            LogError("PlayerInfo", "Unable to get GameStateBase");
            return false;
        }

        if (lpSurvivalGameMode->GameState != lpGameStateBase) {
            LogError("ItemSpawner", "GameMode does not belong to the current GameState");
            return false;
        }

        auto& players = lpGameStateBase->PlayerArray;
        const int32_t iTotalPlayers = players.Num();
        if (
            iTotalPlayers < 0
            || iTotalPlayers > players.Max()
            || iTotalPlayers > 256
            || (
                iTotalPlayers > 0
                && !UnrealUtils::IsReadableMemory(
                    players.GetDataPtr(),
                    sizeof(SDK::APlayerState*) * static_cast<size_t>(iTotalPlayers)
                )
            )
        ) {
            LogError("ItemSpawner", "PlayerArray is unavailable");
            return false;
        }

        LogMessage("ItemSpawner", "Total players in game: " + std::to_string(iTotalPlayers));

        for (int32_t i = 0; i < iTotalPlayers; ++i) {
            SDK::APlayerState *lpPlayerState = players[i];
            if (!UnrealUtils::IsValidUObject(lpPlayerState)) {
                continue;
            }

            if (iPlayerId != lpPlayerState->PlayerId) {
                continue; // didn't wanna give it to this guy anyway
            }

            std::string szPlayerName = lpPlayerState->GetPlayerName().ToString();
            LogMessage(
                "ItemSpawner",
                "Found player: " + szPlayerName + " (ID: " + std::to_string(iPlayerId) + ")"
            );

            SDK::ABP_SurvivalPlayerCharacter_C *lpPlayerCharacter =
                ResolveCurrentPlayerCharacter(lpPlayerState);

            if (nullptr == lpPlayerCharacter) {
                LogError(
                    "ItemSpawner",
                    "Player controller, state, and possessed pawn are not synchronized for: "
                    + szPlayerName
                );
                continue;
            }

            SDK::UDataTable* lpDataTable = UnrealUtils::GetDataTablePointer(
                (szDataTableName.empty()) ? "Table_AllItems" : szDataTableName
            );

            if (!UnrealUtils::IsValidUObject(lpDataTable)) {
                LogError("ItemSpawner", "DataTable not found: " + szDataTableName);
                return false; // what the fok are you spawning bro
            }

            SDK::FDataTableRowHandle ItemRowHandle;
            ZeroMemory(&ItemRowHandle, sizeof(ItemRowHandle));

            if (!UnrealUtils::GetItemRowMap(lpDataTable, szItemName, &ItemRowHandle)) {
                LogError("ItemSpawner", "Item not found in DataTable: " + szItemName);
                return false;
            }

            if (
                SDK::UWorld::GetWorld() != lpWorld
                || !UnrealUtils::IsValidUObject(lpSurvivalGameMode)
                || !UnrealUtils::IsValidUObject(lpPlayerCharacter)
                || lpWorld->AuthorityGameMode != lpSurvivalGameMode
                || lpWorld->GameState != lpGameStateBase
                || lpSurvivalGameMode->GameState != lpGameStateBase
            ) {
                LogError("ItemSpawner", "World changed while preparing the spawn command");
                return false;
            }

            return GrantItemsToCurrentPlayer(
                lpSurvivalGameMode,
                lpPlayerCharacter,
                ItemRowHandle,
                iItemCount
            );
        }

        LogError(
            "ItemSpawner", 
            "Player ID " + std::to_string(iPlayerId) + " not found"
        );
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
        if (!GlobalCheatMode.load()) {
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
                    "Invalid item count format: '" + szItemCountStr + "' (" + std::string(e.what()) + ")",
                    true
                );
                EnableGlobalOutput();
                return;
            } catch (const std::out_of_range& e) {
                LogError(
                    "ItemSpawner",
                    "Item count out of range: '" + szItemCountStr + "' (" + std::string(e.what()) + ")",
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
#pragma warning(suppress : 4102)
_RYUJI:
        // Wait for command processing to complete
        /*while (Command::CommandBufferCookedForExecution) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }*/
        Command::WaitForCommandBufferReady();

        EnableGlobalOutput();
    }
}