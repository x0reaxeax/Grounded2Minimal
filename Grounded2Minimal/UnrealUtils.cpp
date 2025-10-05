#include "UnrealUtils.hpp"
#include "CoreUtils.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <thread>

namespace UnrealUtils {
    SDK::UWorld *GetWorld(bool bCached) {
        if (bCached && g_lpWorld != nullptr) {
            return g_lpWorld;
        }
        int32_t iRetryCount = 0;
        const int32_t iMaxRetries = 20;
        SDK::UWorld *lpWorld = nullptr;
        do {
            lpWorld = SDK::UWorld::GetWorld();
            if (nullptr == lpWorld) {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                if (++iRetryCount >= iMaxRetries) {
                    LogError(
                        "GetWorld", 
                        "Failed to get UWorld after " 
                        + std::to_string(iMaxRetries) + " retries, aborting.."
                    );
                    return nullptr;
                }
            }
        } while (nullptr == lpWorld);
        g_lpWorld = lpWorld;

        return lpWorld;
    }

    SDK::APawn* GetLocalPawn(void) {
        SDK::UWorld *lpWorld = GetWorld();
        if (CheckNullAndLog(lpWorld, "UWorld", "Misc")) {
            return nullptr;
        }

        SDK::ULocalPlayer *lpLocalPlayer = lpWorld->OwningGameInstance->LocalPlayers[0];
        if (CheckNullAndLog(lpLocalPlayer, "LocalPlayer", "Misc")) {
            return nullptr;
        }

        return lpLocalPlayer->PlayerController->Pawn;
    }

    SDK::ABP_SurvivalPlayerCharacter_C* GetLocalSurvivalPlayerCharacter(void) {
        SDK::APawn *lpPawn = GetLocalPawn();
        if (nullptr == lpPawn) {
            LogError("Misc", "Pawn is NULL");
            return nullptr;
        }

        return static_cast<SDK::ABP_SurvivalPlayerCharacter_C*>(lpPawn);
    }

    SDK::USurvivalGameplayStatics* GetSurvivalGameplayStatics(void) {
        SDK::USurvivalGameplayStatics* lpStatics = nullptr;
        
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
                if (!CoreUtils::StringContainsCaseInsensitive(
                    lpObj->GetFullName(),
                    szBlueprintClassPrefix
                )) {
                    continue; // skip non-blueprint classes
                }
            }

            if (!szTargetClassNameNeedle.empty()) {
                if (!CoreUtils::StringContainsCaseInsensitive(
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

            if (!CoreUtils::StringContainsCaseInsensitive(
                lpObj->GetName(),
                szTargetFunctionNameNeedle
            )) {
                continue;
            }

            SDK::UFunction *lpFunction = static_cast<SDK::UFunction*>(lpObj);
            LogMessage(
                "Dump", 
                "Found function: '" + lpFunction->GetFullName() + "' ['" + lpFunction->GetName() + "']"
            );
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

            if (!CoreUtils::StringContainsCaseInsensitive(
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

    // Don't cache this one, since we now have auto-retry on GetWorld()
    void DumpConnectedPlayers(
        std::vector<SDK::APlayerState*> *vlpPlayerStatesOut
    ) {
        SDK::UWorld *lpWorld = GetWorld();
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

    bool IsPlayerHostAuthority(
        int32_t iPlayerId
    ) {
        SDK::UWorld *lpWorld = GetWorld(true);
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
                if (!CoreUtils::StringContainsCaseInsensitive(
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
            if (CoreUtils::StringContainsCaseInsensitive(szFullObjectName, szTargetItemTypeName)) {
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
                if (CoreUtils::StringContainsCaseInsensitive(
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
                        if (CoreUtils::StringContainsCaseInsensitive(
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
                if (!CoreUtils::StringContainsCaseInsensitive(
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

    int32_t GetLocalPlayerId(bool bCached) {
        if (bCached && -1 != g_iLocalPlayerId) {
            return g_iLocalPlayerId;
        }
        SDK::UWorld *lpWorld = GetWorld();
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

        if (lpPlayerState->PlayerId < 200) {
            LogError(
                "Lookup",
                "PlayerId is less than 200, this is likely a bot or non-local player: "
                + std::to_string(lpPlayerState->PlayerId)
            );
            return -1;
        }

        // Refresh the cached player ID if requested
        g_iLocalPlayerId = lpPlayerState->PlayerId;

        return lpPlayerState->PlayerId;
    }

    SDK::APlayerState *GetLocalPlayerState(void) {
        SDK::UWorld *lpWorld = GetWorld(true);
        if (nullptr == lpWorld) {
            LogError("Lookup", "UWorld is NULL");
            return nullptr;
        }

        SDK::ULocalPlayer *lpLocalPlayer = lpWorld->OwningGameInstance->LocalPlayers[0];
        if (nullptr == lpLocalPlayer) {
            return nullptr;
        }

        SDK::APlayerState *lpPlayerState = lpLocalPlayer->PlayerController->PlayerState;
        if (nullptr == lpPlayerState) {
            LogError("Lookup", "PlayerState is NULL for local player");
            return nullptr;
        }

        return lpPlayerState;
    }

    int32_t GetPlayerIdByName(
        const std::string& szPlayerName
    ) {
        SDK::UWorld *lpWorld = GetWorld(true);
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
        SDK::UWorld *lpWorld = GetWorld(true);
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
        SDK::UWorld *lpWorld = GetWorld(true);
        if (nullptr == lpWorld) {
            LogError("PlayerController", "UWorld is NULL");
            return nullptr;
        }

        SDK::UGameInstance *lpOwningGameInstance = lpWorld->OwningGameInstance;
        if (nullptr == lpOwningGameInstance) {
            LogError("PlayerController", "OwningGameInstance is NULL");
            return nullptr;
        }

        int32_t iLocalPlayerId = GetLocalPlayerId();
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

    SDK::ASurvivalPlayerController *GetLocalSurvivalPlayerController(void) {
        SDK::UWorld *lpWorld = GetWorld();
        if (nullptr == lpWorld) {
            LogError("CheatManagerInit", "Failed to get UWorld instance");
            return nullptr;
        }
        
        int32_t iPlayerId = UnrealUtils::GetLocalPlayerId(true);
        for (int32_t i = 0; i < lpWorld->GObjects->Num(); i++) {
            SDK::UObject *lpObj = lpWorld->GObjects->GetByIndex(i);
            if (nullptr == lpObj) {
                continue;
            }
            if (!lpObj->IsA(SDK::ASurvivalPlayerController::StaticClass())) {
                continue;
            }

            SDK::ASurvivalPlayerController *lpSurvivalPlayerControllerCandidate =
                static_cast<SDK::ASurvivalPlayerController*>(lpObj);

            if (nullptr == lpSurvivalPlayerControllerCandidate) {
                LogError("CheatManagerInit", "Failed to cast to ASurvivalPlayerController");
                continue;
            }

            SDK::APlayerState *lpPlayerState = lpSurvivalPlayerControllerCandidate->PlayerState;
            if (nullptr == lpPlayerState) {
                LogError("CheatManagerInit", "PlayerState is NULL for ASurvivalPlayerController");
                continue;
            }

            if (iPlayerId != lpPlayerState->PlayerId) {
                continue; // Not the local player controller
            }

            LogMessage(
                "FindLocalSPC",
                "Found SurvivalPlayerController at index "
                + std::to_string(i) + " ["
                + std::to_string(i + 1) + "/"
                + std::to_string(lpWorld->GObjects->Num()) + "]",
                true
            );

            return lpSurvivalPlayerControllerCandidate;
        }
        return nullptr;
    }

    SDK::UPartyComponent *FindLocalPlayerParty(void) {
        SDK::UWorld *lpWorld = GetWorld(true);
        if (nullptr == lpWorld) {
            LogError("PartyParser", "Failed to get UWorld instance");
            return nullptr;
        }

        int32_t iLocalPlayerId = UnrealUtils::GetLocalPlayerId();

        for (int32_t i = 0; i < SDK::UPartyComponent::GObjects->Num(); i++) {
            SDK::UObject *lpObj = lpWorld->GObjects->GetByIndex(i);
            if (nullptr == lpObj) {
                continue;
            }
            if (!lpObj->IsA(SDK::UPartyComponent::StaticClass())) {
                continue;
            }

            LogMessage(
                "PartyParser",
                "Evaluating possible UPartyComponent.."
            );

            SDK::UPartyComponent *lpPartyComponent = static_cast<SDK::UPartyComponent*>(lpObj);
            if (nullptr == lpPartyComponent) {
                LogError("PartyParser", "Failed to cast to UPartyComponent");
                continue;
            }

            LogMessage(
                "PartyParser",
                "Evaluating party of " + std::to_string(lpPartyComponent->PartyMembers.Num())
                + " members"
            );
            LogMessage(
                "PartyParser",
                "Evaluating party of " + std::to_string(lpPartyComponent->PlayerIdentities.Num())
                + " identities"
            );

            for (int32_t j = 0; j < lpPartyComponent->PartyMembers.Num(); j++) {
                SDK::ASurvivalPlayerCharacter *lpPartyMember = lpPartyComponent->PartyMembers[j];
                if (nullptr == lpPartyMember) {
                    LogError("PartyParser", "Party member is NULL at index " + std::to_string(j));
                    continue;
                }

                SDK::APlayerState *lpPlayerState = lpPartyMember->PlayerState;
                if (nullptr == lpPlayerState) {
                    LogError(
                        "PartyParser",
                        "PlayerState is NULL for party member at index " + std::to_string(j)
                    );
                    continue;
                }
                if (lpPlayerState->PlayerId == iLocalPlayerId) {
                    return lpPartyComponent; // Found the local player in the party
                } else {
                    LogMessage(
                        "PartyParser",
                        "PlayerState ID " + std::to_string(lpPlayerState->PlayerId)
                        + " does not match local player ID " + std::to_string(iLocalPlayerId)
                    );
                }
            }
        }

        return nullptr;
    }

    SDK::ASurvivalPlayerState *GetLocalSurvivalPlayerState(void) {
        SDK::UWorld *lpWorld = GetWorld(true);
        if (nullptr == lpWorld) {
            LogError("PartyParser", "Failed to get UWorld instance");
            return nullptr;
        }

        int32_t iLocalPlayerId = UnrealUtils::GetLocalPlayerId();

        for (int32_t i = 0; i < lpWorld->GObjects->Num(); i++) {
            SDK::UObject *lpObj = lpWorld->GObjects->GetByIndex(i);
            if (nullptr == lpObj) {
                continue;
            }
            if (!lpObj->IsA(SDK::UPartyComponent::StaticClass())) {
                continue;
            }

            SDK::UPartyComponent *lpPartyComponent = static_cast<SDK::UPartyComponent*>(lpObj);
            if (nullptr == lpPartyComponent) {
                LogError("PartyParser", "Failed to cast to UPartyComponent");
                continue;
            }

            for (int32_t j = 0; j < lpPartyComponent->PartyMembers.Num(); j++) {
                SDK::ASurvivalPlayerCharacter *lpPartyMember = lpPartyComponent->PartyMembers[j];
                if (nullptr == lpPartyMember) {
                    LogError("PartyParser", "Party member is NULL at index " + std::to_string(j));
                    continue;
                }

                SDK::APlayerState *lpPlayerState = lpPartyMember->PlayerState;
                if (nullptr == lpPlayerState) {
                    LogError(
                        "PartyParser",
                        "PlayerState is NULL for party member at index " + std::to_string(j)
                    );
                    continue;
                }
                if (lpPlayerState->PlayerId == iLocalPlayerId) {
                    return lpPartyMember->GetAssociatedPlayerState();
                }
            }
        }
        LogError("PartyParser", "Local player state not found in any party");
        return nullptr; // Local player state not found in any party
    }

    void DumpSurvivalCheatManagerInstances(void) {
        SDK::UWorld *lpWorld = GetWorld(true);
        if (nullptr == lpWorld) {
            LogError("PartyParser", "Failed to get UWorld instance");
            return;
        }

        for (int32_t i = 0; i < lpWorld->GObjects->Num(); i++) {
            SDK::UObject *lpObj = lpWorld->GObjects->GetByIndex(i);
            if (nullptr == lpObj) {
                continue;
            }
            if (!lpObj->IsA(SDK::USurvivalCheatManager::StaticClass())) {
                continue;
            }

            SDK::USurvivalCheatManager *lpSurvivalCheatManager = static_cast<SDK::USurvivalCheatManager*>(lpObj);
            if (nullptr == lpSurvivalCheatManager) {
                LogError("PartyParser", "Failed to cast to USurvivalCheatManager");
                continue;
            }

            LogMessage(
                "Dump",
                "Found SurvivalCheatManager instance: " + lpSurvivalCheatManager->GetFullName()
                + " (index: " + std::to_string(lpSurvivalCheatManager->Index) + ")"
            );
        }
    }
}