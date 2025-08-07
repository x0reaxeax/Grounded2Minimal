// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#include "GroundedMinimal.hpp"
#include "UnrealUtils.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>

namespace UnrealUtils {
    SDK::APawn* GetLocalPawn(void) {
        SDK::UWorld *lpWorld = SDK::UWorld::GetWorld();
        if (CheckNullAndLog(lpWorld, "UWorld", "Misc")) {
            return nullptr;
        }

        SDK::ULocalPlayer *lpLocalPlayer = lpWorld->OwningGameInstance->LocalPlayers[0];
        if (CheckNullAndLog(lpLocalPlayer, "LocalPlayer", "Misc")) {
            return nullptr;
        }

        return lpLocalPlayer->PlayerController->Pawn;
    }

    SDK::ABP_SurvivalPlayerCharacter_C* GetSurvivalPlayerCharacter(void) {
        SDK::APawn *lpPawn = GetLocalPawn();
        if (nullptr == lpPawn) {
            LogError("Misc", "Pawn is NULL");
            return nullptr;
        }

        return static_cast<SDK::ABP_SurvivalPlayerCharacter_C*>(lpPawn);
    }

    void DumpClasses(
        std::vector<std::string>* vszClassesOut,
        const std::string& szTargetClassNameNeedle,
        bool bOnlyBlueprints
    ) {
        std::string szBlueprintClassPrefix = "BP_";
        for (int32_t i = 0; i < SDK::UClass::GObjects->Num(); i++) {
            SDK::UObject* lpObj = SDK::UClass::GObjects->GetByIndex(i);
            if (nullptr == lpObj) {
                continue;
            }
            if (!lpObj->IsA(SDK::UClass::StaticClass())) {
                continue;
            }

            if (bOnlyBlueprints) {
                if (!StringContainsCaseInsensitive(
                    lpObj->GetFullName(),
                    szBlueprintClassPrefix
                )) {
                    continue; // skip non-blueprint classes
                }
            }

            if (!szTargetClassNameNeedle.empty()) {
                if (!StringContainsCaseInsensitive(
                    lpObj->GetFullName(),
                    szTargetClassNameNeedle
                )) {
                    continue; // skip classes that don't match the needle
                }
            }

            SDK::UClass *lpClass = static_cast<SDK::UClass*>(lpObj);
            LogMessage("Dump", "Found class: '" + lpClass->GetName() + "'");
            if (nullptr != vszClassesOut) {
                std::string szClassShortName = lpClass->GetName();
                vszClassesOut->push_back(szClassShortName);
            }
        }
    }

    void DumpFunctions(
        const std::string& szTargetFunctionNameNeedle
    ) {
        for (int32_t i = 0; i < SDK::UFunction::GObjects->Num(); ++i) {
            SDK::UObject* lpObj = SDK::UFunction::GObjects->GetByIndex(i);
            if (nullptr == lpObj) {
                continue;
            }

            if (!lpObj->IsA(SDK::UFunction::StaticClass())) {
                continue;
            }

            if (!StringContainsCaseInsensitive(
                lpObj->GetName(), 
                szTargetFunctionNameNeedle
            )) {
                continue;
            }

            SDK::UFunction *lpFunction = static_cast<SDK::UFunction*>(lpObj);
            LogMessage("Dump", "Found function: " + lpFunction->GetFullName());
        }
    }

    SDK::UFunction* FindFunction(
        const std::string& szPartialName
    ) {
        for (INT32 i = 0; i < SDK::UFunction::GObjects->Num(); ++i) {
            SDK::UObject* lpObj = SDK::UFunction::GObjects->GetByIndex(i);
            if (nullptr == lpObj) {
                continue;
            }

            if (!lpObj->IsA(SDK::UFunction::StaticClass())) {
                continue;
            }

            if (!StringContainsCaseInsensitive(
                lpObj->GetName(), 
                szPartialName
            )) {
                continue;
            }

            SDK::UFunction *lpFunction = static_cast<SDK::UFunction*>(lpObj);
            return lpFunction;
        }
        return nullptr; // function not found
    }

    void DumpConnectedPlayers(
        std::vector<SDK::APlayerState*> *vlpPlayerStatesOut
    ) {
        SDK::UWorld *lpWorld = SDK::UWorld::GetWorld();
        if (nullptr == lpWorld) {
            return;
        }

        SDK::AGameStateBase *lpGameStateBase = lpWorld->GameState;
        if (nullptr == lpGameStateBase) {
            LogError("PlayerInfo", "Unable to get GameStateBase");
            return;
        }

        for (int32_t i = 0; i < lpGameStateBase->PlayerArray.Num(); i++) {
            SDK::APlayerState *lpPlayerState = lpGameStateBase->PlayerArray[i];
            if (nullptr == lpPlayerState) {
                continue;
            }

            // Add to output vector if provided
            if (nullptr != vlpPlayerStatesOut) {
                vlpPlayerStatesOut->push_back(lpPlayerState);
            }

            SDK::FString fszPlayerName = lpPlayerState->GetPlayerName();
            bool bHasAuthority = lpPlayerState->HasAuthority();

            LogMessage(
                L"PlayerInfo",
                L"Player: '" + fszPlayerName.ToWString() + L"'\n" +
                L"  * Authority:  " + std::wstring(bHasAuthority ? L"Yes" : L"No") + L"\n" +
                L"  * ID:         " + std::to_wstring(lpPlayerState->PlayerId) + L"\n" +
                L"  * Index:      " + std::to_wstring(lpPlayerState->Index) + L"\n" +
                L"  * ArrayIndex: " + std::to_wstring(i)
            );
        }
    }

    void DumpConnectedPlayers3(
        std::vector<SDK::APlayerState*> *vlpPlayerStatesOut
    ) {
        SDK::UWorld *lpWorld = SDK::UWorld::GetWorld();
        if (nullptr == lpWorld) {
            return;
        }
        SDK::TArray<SDK::AActor*> alpFoundActors;
        SDK::UGameplayStatics::GetAllActorsOfClass(
            lpWorld,
            SDK::APlayerState::StaticClass(),
            &alpFoundActors
        );

        for (SDK::AActor* lpActor : alpFoundActors) {
            SDK::APlayerState *lpPlayerState = static_cast<SDK::APlayerState*>(lpActor);
            if (nullptr == lpPlayerState) {
                continue;
            }

            if (lpPlayerState->PlayerId < 240) {
                // Skip player states with PlayerId < 240, these are likely not valid players
                continue;
            }

            // Add to output vector if provided
            if (nullptr != vlpPlayerStatesOut) {
                vlpPlayerStatesOut->push_back(lpPlayerState);
            }

            SDK::FString fszPlayerName = lpPlayerState->GetPlayerName();
            bool bHasAuthority = lpPlayerState->HasAuthority();

            LogMessage(
                L"PlayerInfo",
                L"Player: '" + fszPlayerName.ToWString() + L"'\n" +
                L"  * Authority:  " + std::wstring(bHasAuthority ? L"Yes" : L"No") + L"\n" +
                L"  * ID:         " + std::to_wstring(lpPlayerState->PlayerId) + L"\n" +
                L"  * Index:      " + std::to_wstring(lpPlayerState->Index) + L"\n"
            );
        }
    }

    bool IsPlayerHostAuthority(
        int32_t iPlayerId
    ) {
        SDK::UWorld *lpWorld = SDK::UWorld::GetWorld();
        if (nullptr == lpWorld) {
            return false;
        }

        if (nullptr == lpWorld->AuthorityGameMode) {
            LogError("HostCheck", "AuthorityGameMode is NULL, cannot determine host");
            return false;
        }

        if (0 == iPlayerId) {
            // Local player check
            SDK::ULocalPlayer *lpLocalPlayer = lpWorld->OwningGameInstance->LocalPlayers[0];
            if (nullptr == lpLocalPlayer) {
                LogError("HostCheck", "LocalPlayer is NULL");
                return false;
            }

            SDK::APlayerController *lpPlayerController = lpLocalPlayer->PlayerController;
            if (nullptr == lpPlayerController) {
                LogError("HostCheck", "PlayerController is NULL");
                return false;
            }

            SDK::APlayerState *lpPlayerState = lpPlayerController->PlayerState;
            if (nullptr == lpPlayerState) {
                LogError("HostCheck", "PlayerState is NULL");
                return false;
            }

            SDK::FString fszPlayerName = lpPlayerState->GetPlayerName();
            bool bHasAuthority = lpPlayerState->HasAuthority();

            LogMessage(
                "HostCheck", 
                "Local player: " + fszPlayerName.ToString() 
                + " (Authority: " + std::string(bHasAuthority ? "Yes" : "No") + ")", 
                true
            );

            return bHasAuthority;
        }

        for (int32_t i = 0; i < lpWorld->GameState->PlayerArray.Num(); i++) {
            SDK::APlayerState *lpPlayerState = lpWorld->GameState->PlayerArray[i];
            if (nullptr == lpPlayerState) {
                continue;
            }

            if (iPlayerId != lpPlayerState->PlayerId) {
                continue; // Skip to the next player if the ID does not match
            }

            SDK::FString fszPlayerName = lpPlayerState->GetPlayerName();
            bool bHasAuthority = lpPlayerState->HasAuthority();

            LogMessage(
                "HostCheck", 
                "Player: " + fszPlayerName.ToString() 
                + " (Authority: " + std::string(bHasAuthority ? "Yes" : "No") + ")"
            );

            return bHasAuthority;
        }

        LogError("HostCheck", "No player found in the PlayerArray");
        return false;
    }

    void PrintFoundItemInfo(
        SDK::ASpawnedItem *lpSpawnedItem,
        int32_t iItemIndex,
        bool* lpbFoundAtLeastOne
    ) {
        if (nullptr == lpSpawnedItem || nullptr == lpbFoundAtLeastOne) {
            return;
        }

        if (!(*lpbFoundAtLeastOne)) {
            LogMessage("", "====================================================");
            *lpbFoundAtLeastOne = true;
        }

        std::ostringstream ossFind;
        ossFind << "[Find " << std::setfill('0') << std::setw(4) << iItemIndex
                << "] Found '" << lpSpawnedItem->Class->GetFullName()
                << "': " << lpSpawnedItem->GetFullName();
        LogMessage("", ossFind.str());

        std::ostringstream ossIndex;
        ossIndex << "  * Item Index: " << lpSpawnedItem->Index;
        LogMessage("", ossIndex.str());

        SDK::FVector vItemLocation = lpSpawnedItem->K2_GetActorLocation();
        std::ostringstream ossLocation;
        ossLocation << "  * Location: ("
            << std::fixed << std::setprecision(4)
            << vItemLocation.X << ", "
            << vItemLocation.Y << ", "
            << vItemLocation.Z << ")";
        LogMessage("", ossLocation.str());        
    }

    SDK::ASpawnedItem *GetSpawnedItemByIndex(
        int32_t iItemIndex,
        const std::string& szTargetItemTypeName
    ) {
        for (int32_t i = 0; i < SDK::ASpawnedItem::GObjects->Num(); i++) {
            SDK::UObject *lpObj = SDK::ASpawnedItem::GObjects->GetByIndex(i);
            if (nullptr == lpObj) {
                continue;
            }

            if (!lpObj->IsA(SDK::ASpawnedItem::StaticClass())) {
                continue;
            }

            SDK::ASpawnedItem *lpSpawnedItem = static_cast<SDK::ASpawnedItem*>(lpObj);
            if (lpSpawnedItem->Index != iItemIndex) {
                continue; // Skip if the index does not match
            }

            // Name verification
            if (!szTargetItemTypeName.empty()) {
                if (!StringContainsCaseInsensitive(
                    lpSpawnedItem->Class->GetFullName(), 
                    szTargetItemTypeName
                )) {
                    continue;
                }
            }

            return lpSpawnedItem;
        }
        return nullptr;
    }

    void FindSpawnedItemByType(
        const std::string& szTargetItemTypeName
    ) {
        bool bFoundAtLeastOne = false;
        for (int32_t i = 0; i < SDK::ASpawnedItem::GObjects->Num(); i++) {
            SDK::UObject *lpObj = SDK::ASpawnedItem::GObjects->GetByIndex(i);
            if (nullptr == lpObj) {
                continue;
            }

            if (!lpObj->IsA(SDK::ASpawnedItem::StaticClass())) {
                continue;
            }

            std::string szFullObjectName = lpObj->GetFullName();
            if (StringContainsCaseInsensitive(szFullObjectName, szTargetItemTypeName)) {
                SDK::ASpawnedItem *lpSpawnedItem = static_cast<SDK::ASpawnedItem*>(lpObj);
                PrintFoundItemInfo(lpSpawnedItem, i, &bFoundAtLeastOne);
            }
        }

        if (bFoundAtLeastOne) {
            LogMessage("", "====================================================");
        }
    }

    void FindDataTableByName(
        const std::string& szTargetTableStringNeedle
    ) {
        for (int32_t i = 0; i < SDK::UObject::GObjects->Num(); ++i) {
            SDK::UObject* lpObj = SDK::UObject::GObjects->GetByIndex(i);
            if (nullptr == lpObj) {
                continue;
            }

            if (lpObj->IsA(SDK::UDataTable::StaticClass())) {
                if (StringContainsCaseInsensitive(
                    lpObj->GetName(), 
                    szTargetTableStringNeedle
                )) {
                    // yea first we do a negative check and now we flip it, 
                    // slip some `continue` in, great programming skills, yap yap
                    LogMessage("DataTable", "Found DataTable: " + lpObj->GetName());
                }
            }
        }
    }

    void ParseRowMapManually(
        SDK::UDataTable *lpDataTable,
        std::vector<std::string> *vlpRowNamesOut,
        const std::string& szFilterNeedle
    ) {
        if (nullptr == lpDataTable) {
            return;
        }

        SDK::TMap<class SDK::FName, SDK::uint8*> &rowMap = lpDataTable->RowMap;

        LogMessage(
            "DataTable", 
            "Dumping RowMap entries for DataTable: " + lpDataTable->GetName()
            +
            " (filter: '" + szFilterNeedle + "')"
        );
        
        int32_t iValidEntryCount = 0;
        for (const auto& entry : rowMap) {
            const SDK::FName& rowName = entry.Key();
            uint8_t* lpRowData = entry.Value();

            if (nullptr != lpRowData) {
                LogMessage("RowMap", "Row Name: " + rowName.ToString());
                iValidEntryCount++;

                if (nullptr != vlpRowNamesOut) {
                    // check if filter is applied, and if yes, only add contains(filter)
                    if (!szFilterNeedle.empty()) {
                        if (StringContainsCaseInsensitive(
                            rowName.ToString(), 
                            szFilterNeedle
                        )) {
                            vlpRowNamesOut->push_back(rowName.ToString());
                        }
                    } else {
                        // No filter, add all valid row names
                        vlpRowNamesOut->push_back(rowName.ToString());
                    }
                }
            }
        }
        
        LogMessage(
            "DataTable", 
            "Finished dumping RowMap entries. Found " 
            + std::to_string(iValidEntryCount) + " valid entries"
        );
    }

    bool GetItemRowMap(
        SDK::UDataTable *lpDataTable,
        const std::string& szItemName,
        SDK::FDataTableRowHandle *lpRowHandleOut
    ) {
        if (nullptr == lpDataTable) {
            return false;
        }

        SDK::TMap<class SDK::FName, SDK::uint8*> &rowMap = lpDataTable->RowMap;

        for (const auto& entry : rowMap) {
            const SDK::FName& rowName = entry.Key();
            uint8_t* lpRowData = entry.Value();

            if (nullptr != lpRowData && rowName.ToString() == szItemName) {
                LogMessage(
                    "DataTable", 
                    "Found item '" + szItemName + "' in DataTable: " + lpDataTable->GetName()
                );
                if (nullptr != lpRowHandleOut) {
                    *lpRowHandleOut = SDK::FDataTableRowHandle(lpDataTable, rowName);
                }
                return true;
            }
        }

        return false;
    }

    void DumpAllDataTablesAndItems(
        std::vector<DataTableInfo> *vlpDataTableInfoOut,
        const std::string& szDataTableFilterNeedle
    ) {
        for (int32_t i = 0; i < SDK::UObject::GObjects->Num(); ++i) {
            SDK::UObject* lpObj = SDK::UObject::GObjects->GetByIndex(i);
            if (nullptr == lpObj) {
                continue;
            }

            if (!lpObj->IsA(SDK::UDataTable::StaticClass())) {
                continue;
            }

            SDK::UDataTable *lpDataTable = static_cast<SDK::UDataTable*>(lpObj);

            if (!szDataTableFilterNeedle.empty()) {
                if (!StringContainsCaseInsensitive(
                    lpDataTable->GetName(),
                    szDataTableFilterNeedle
                )) {
                    // filter not matched, skip this bih
                    continue;
                }
            }

            if (nullptr != vlpDataTableInfoOut) {
                // Create a DataTableInfo entry
                DataTableInfo tableInfo(lpDataTable->GetName());
                
                // Parse items for this table - this will populate tableInfo.vszItemNames
                ParseRowMapManually(lpDataTable, &tableInfo.vszItemNames);
                
                // Store the table name before moving
                std::string szTableName = tableInfo.szTableName;
                
                // Add the complete info to the output vector
                vlpDataTableInfoOut->push_back(std::move(tableInfo));
                
                LogMessage(
                    "DataTable", 
                    "Added DataTable '" + szTableName 
                    + "' with " + std::to_string(vlpDataTableInfoOut->back().GetItemCount()) + " items"
                );
            } else {
                // If no output vector, just log the table information
                ParseRowMapManually(lpDataTable, nullptr);
            }
        }
        
        if (nullptr != vlpDataTableInfoOut) {
            LogMessage(
                "DataTable", 
                "Total DataTables processed: " + std::to_string(vlpDataTableInfoOut->size())
            );
        }
    }

    SDK::UDataTable* GetDataTablePointer(
        const std::string& szTargetTable
    ) {
        for (INT32 i = 0; i < SDK::UObject::GObjects->Num(); ++i) {
            SDK::UObject* lpObj = SDK::UObject::GObjects->GetByIndex(i);
            if (nullptr == lpObj) {
                continue;
            }

            if (lpObj->IsA(SDK::UDataTable::StaticClass())) {
                if (lpObj->GetName() == szTargetTable) {
                    LogMessage("DataTable", "Found DataTable: " + lpObj->GetName());
                    return static_cast<SDK::UDataTable*>(lpObj);
                }
            }
        }
        return nullptr;
    }

    void FindMatchingDataTableForItemName(
        const std::string& szItemName
    ) {
        for (int32_t i = 0; i < SDK::UObject::GObjects->Num(); ++i) {
            SDK::UObject* lpObj = SDK::UObject::GObjects->GetByIndex(i);
            if (nullptr == lpObj) {
                continue;
            }

            if (!lpObj->IsA(SDK::UDataTable::StaticClass())) {
                continue;
            }

            SDK::UDataTable *lpDataTable = static_cast<SDK::UDataTable*>(lpObj);

            if (GetItemRowMap(lpDataTable, szItemName, nullptr)) {
                LogMessage(
                    "DataTable",
                    "Found item '" + szItemName + "' in DataTable '" + lpDataTable->GetName() + "'"
                );
            }
        }
    }

    int32_t GetLocalPlayerId(void) {
        SDK::UWorld *lpWorld = SDK::UWorld::GetWorld();
        if (nullptr == lpWorld) {
            LogError("Lookup", "UWorld is NULL");
            return -1;
        }

        SDK::ULocalPlayer *lpLocalPlayer = lpWorld->OwningGameInstance->LocalPlayers[0];
        if (nullptr == lpLocalPlayer) {
            return -1;
        }

        SDK::APlayerState *lpPlayerState = lpLocalPlayer->PlayerController->PlayerState;
        if (nullptr == lpPlayerState) {
            LogError("Lookup", "PlayerState is NULL for local player");
            return -1;
        }

        return lpPlayerState->PlayerId;
    }

    int32_t GetPlayerIdByName(
        const std::string& szPlayerName
    ) {
        SDK::UWorld *lpWorld = SDK::UWorld::GetWorld();
        if (nullptr == lpWorld) {
            return -1;
        }

        SDK::AGameStateBase *lpGameStateBase = lpWorld->GameState;
        if (nullptr == lpGameStateBase) {
            LogError("PlayerInfo", "Unable to get GameStateBase");
            return -1;
        }

        for (int32_t i = 0; i < lpGameStateBase->PlayerArray.Num(); i++) {
            SDK::APlayerState *lpPlayerState = lpGameStateBase->PlayerArray[i];
            if (nullptr == lpPlayerState) {
                continue;
            }

            SDK::FString fszPlayerName = lpPlayerState->GetPlayerName();
            if (fszPlayerName.ToString() != szPlayerName) {
                continue;
            }

            return lpPlayerState->PlayerId;
        }
        return -1;
    }

    SDK::UGameInstance *GetOwningGameInstance(void) {
        SDK::UWorld *lpWorld = SDK::UWorld::GetWorld();
        if (nullptr == lpWorld) {
            LogError("GameInstance", "UWorld is NULL");
            return nullptr;
        }
        SDK::UGameInstance *lpOwningGameInstance = lpWorld->OwningGameInstance;
        if (nullptr == lpOwningGameInstance) {
            LogError("GameInstance", "OwningGameInstance is NULL");
            return nullptr;
        }
        return lpOwningGameInstance;
    }

    SDK::APlayerController *GetLocalPlayerController(void) {
        SDK::UWorld *lpWorld = SDK::UWorld::GetWorld();
        if (nullptr == lpWorld) {
            LogError("PlayerController", "UWorld is NULL");
            return nullptr;
        }

        SDK::UGameInstance *lpOwningGameInstance = lpWorld->OwningGameInstance;
        if (nullptr == lpOwningGameInstance) {
            LogError("PlayerController", "OwningGameInstance is NULL");
            return nullptr;
        }

        int32_t iLocalPlayerId = UnrealUtils::GetLocalPlayerId();
        for (int32_t i = 0; i < lpWorld->OwningGameInstance->LocalPlayers.Num(); i++) {
            SDK::ULocalPlayer *lpLocalPlayer = lpOwningGameInstance->LocalPlayers[i];
            if (nullptr == lpLocalPlayer) {
                continue;
            }

            SDK::APlayerController *lpPlayerController = lpLocalPlayer->PlayerController;
            if (nullptr == lpPlayerController) {
                continue;
            }

            SDK::APlayerState *lpPlayerState = lpPlayerController->PlayerState;
            if (nullptr == lpPlayerState) {
                continue;
            }

            if (iLocalPlayerId == lpPlayerState->PlayerId) {
                return lpPlayerController; // Found the local player controller
            }
        }

        return nullptr;
    }
}