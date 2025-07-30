// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#ifndef _GROUNDED_UNREAL_UTILS_HPP
#define _GROUNDED_UNREAL_UTILS_HPP

#include "GroundedMinimal.hpp"
#include <vector>

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

    SDK::APawn *GetLocalPawn(
        void
    );

    SDK::ABP_SurvivalPlayerCharacter_C *GetSurvivalPlayerCharacter(
        VOID
    );

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
        std::vector<SDK::APlayerState*> *vlpPlayerStatesOut = nullptr
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

    int32_t GetLocalPlayerId(
        void
    );

    int32_t GetPlayerIdByName(
        const std::string& szPlayerName
    );

    void DumpAllDataTablesAndItems(
        std::vector<DataTableInfo> *vlpDataTableInfoOut = nullptr,
        const std::string& szDataTableFilterNeedle = ""
    );

    SDK::UGameInstance *GetOwningGameInstance(void);

    SDK::APlayerController *GetLocalPlayerController(void);
};

#endif // _GROUNDED_UNREAL_UTILS_HPP