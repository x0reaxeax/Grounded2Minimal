// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#ifndef _GROUNDED2_UNREAL_UTILS_HPP
#define _GROUNDED2_UNREAL_UTILS_HPP

#include "Grounded2Minimal.hpp"
#include <vector>

#include "SDK/BP_SurvivalPlayerCharacter_classes.hpp"
#include "SDK/BP_SurvivalPlayerCharacter_parameters.hpp"

namespace UnrealUtils {

    // Structured data for DataTable information
    struct DataTableInfo {
        std::string szTableName;
        std::vector<std::string> vszItemNames;

        DataTableInfo() = default;
        DataTableInfo(const std::string& szName) : szTableName(szName) {}

        // Helper methods
        bool IsEmpty() const {
            return vszItemNames.empty();
        }
        size_t GetItemCount() const {
            return vszItemNames.size();
        }
    };

    SDK::UWorld *GetWorld(bool bCached = false);

    SDK::APawn *GetLocalPawn(void);

    SDK::ABP_SurvivalPlayerCharacter_C *GetLocalSurvivalPlayerCharacter(void);

    void DumpClasses(
        std::vector<std::string>* vszClassesOut = nullptr,
        const std::string& szTargetClassNameNeedle = "",
        bool bOnlyBlueprints = false
    );

    void DumpFunctions(
        const std::string& szTargetFunctionNameNeedle
    );

    SDK::UFunction *FindFunction(
        const std::string& szTargetFunctionNameNeedle
    );

    void DumpConnectedPlayers(
        std::vector<SDK::APlayerState*> *vlpPlayerStatesOut = nullptr,
        bool bHideOutput = false
    );

    bool IsPlayerHostAuthority(
        int32_t iPlayerId = 0
    );

    void PrintFoundItemInfo(
        SDK::ASpawnedItem *lpSpawnedItem,
        int32_t iItemIndex,
        bool* lpbFoundAtLeastOne
    );

    void FindDataTableByName(
        const std::string& szTargetTableStringNeedle
    );

    void ParseRowMapManually(
        SDK::UDataTable *lpDataTable,
        std::vector<std::string> *vlpRowNamesOut = nullptr,
        const std::string& szFilterNeedle = ""
    );

    SDK::ASpawnedItem *GetSpawnedItemByIndex(
        int32_t iItemIndex,
        const std::string& szTargetItemTypeName = ""
    );

    void FindSpawnedItemByType(
        const std::string& szTargetItemTypeName
    );

    bool GetItemRowMap(
        SDK::UDataTable *lpDataTable,
        const std::string& szItemName,
        SDK::FDataTableRowHandle *lpRowHandleOut
    );

    SDK::UDataTable * GetDataTablePointer(
        const std::string& szTargetTableName
    );

    void FindMatchingDataTableForItemName(
        const std::string& szItemName
    );

    int32_t GetLocalPlayerId(bool bCached = false);

    SDK::ASurvivalPlayerState *GetSurvivalPlayerStateById(
        int32_t iTargetPlayerId
    );

    SDK::AGameModeBase *GetSurvivalGameModeBase(void);

    int32_t GetPlayerIdByName(
        const std::string& szPlayerName
    );

    void DumpAllDataTablesAndItems(
        std::vector<DataTableInfo> *vlpDataTableInfoOut = nullptr,
        const std::string& szDataTableFilterNeedle = ""
    );

    SDK::UGameInstance *GetOwningGameInstance(void);

    SDK::APlayerController *GetLocalPlayerController(void);

    SDK::APlayerController *GetPlayerControllerById(
        int32_t iPlayerId
    );

    SDK::ASurvivalPlayerController *GetLocalSurvivalPlayerController(void);

    SDK::UPartyComponent *FindLocalPlayerParty(
        int32_t iTargetPlayerId = INVALID_PLAYER_ID,
        bool bCached = false
    );

    SDK::ASurvivalPlayerCharacter *GetSurvivalPlayerCharacterById(
        int32_t iTargetPlayerId,
        bool bCached = false
    );

    SDK::ASurvivalPlayerState *GetLocalSurvivalPlayerState(void);

    void DumpSurvivalCheatManagerInstances(void);
}

#endif // _GROUNDED2_UNREAL_UTILS_HPP