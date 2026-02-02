#include "CheatManager.hpp"
#include "Interpreter.hpp"
#include "UnrealUtils.hpp"
#include "ItemSpawner.hpp"
#include "HookManager.hpp"
#include "Command.hpp"

namespace Interpreter {

    static void HandleDataTableSearch(void) {
        std::string szTableName;
        if (!ReadInterpreterInput(
            "[DataTableSearch] Enter DataTable name to search: ",
            szTableName
        )) {
            LogError(
                "DataTableDump", "Invalid input, please provide a table name"
            );
            return;
        }
        UnrealUtils::FindDataTableByName(szTableName);
    }

    // It's called ItemDump cuz legacy reasons, but it actually works for any DataTable entries
    static void HandleItemDump(void) {
        std::string szTableName;
        if (!ReadInterpreterInput(
            "[DataTableItemDump] Enter DataTable name to dump entries: ",
            szTableName
        )) {
            LogError(
                "DataTableItemDump", 
                "Invalid input, please provide a table name"
            );
            return;
        }

        SDK::UDataTable* lpDataTable = UnrealUtils::GetDataTablePointer(szTableName);
        if (nullptr == lpDataTable) {
            LogError(
                "DataTableItemDump", 
                "DataTable not found: " + szTableName
            );
        } else {
            UnrealUtils::ParseRowMapManually(lpDataTable);
        }
    }

    static void HandleFindItemTable(void) {
        std::string szItemName;
        if (!ReadInterpreterInput(
            "[DataTableItemSearch] Enter item name to find DataTable for: ",
            szItemName
        )) {
            LogError("DataTableItemSearch", "Invalid input, please provide an item name");
            return;
        }
        UnrealUtils::FindMatchingDataTableForItemName(szItemName);
    }

    static void HandleFunctionDump(void) {
        std::string szFunctionName;
        if (!ReadInterpreterInput(
            "[FunctionDump] Enter function name to search: ",
            szFunctionName
        )) {
            LogError("FunctionDump", "Invalid input, please provide a function name");
            return;
        }
        UnrealUtils::DumpFunctions(szFunctionName);
    }

    static void HandleClassDump(void) {
        std::string szClassName;
        if (!ReadInterpreterInput(
            "[ClassDump] Enter class name needle to search: ",
            szClassName
        )) {
            LogError(
                "ClassDump", 
                "Invalid input, please provide a class name"
            );
            return;
        }

        UnrealUtils::DumpClasses(nullptr, szClassName);
    }

    static void HandleGetAuthority(void) {
        if (UnrealUtils::IsPlayerHostAuthority()) {
            LogMessage(
                "Authority", 
                "You have authority as the host"
            );
        } else {
            LogError(
                "Authority", 
                "You do not have authority as the host"
            );
        }
    }

    static void HandleSpawnItem(void) {
        if (!g_G2MOptions.bIsClientHost) {
            LogError(
                "ItemSpawner",
                "Cannot spawn items without host authority"
            );
            return;
        }

        std::string szItemName, szDataTableName;

        if (
            !ReadInterpreterInput("[ItemSpawner] Enter item to spawn: ", szItemName)
            ||
            !ReadInterpreterInput("[ItemSpawner] Enter DataTable name: ", szDataTableName)
        ) {
            LogError("ItemSpawner", "Invalid input, please provide all fields");
            return;
        }

        int32_t iPlayerId = ReadIntegerInput(
            "[ItemSpawner] Enter player ID (empty for local): ",
            UnrealUtils::GetLocalPlayerId()
        );
        int32_t iItemCount = ReadIntegerInput(
            "[ItemSpawner] Enter item count (default 1): ",
            1
        );

        auto* lpParams = new ItemSpawner::BufferParamsSpawnItem{
            .iPlayerId = iPlayerId,
            .szItemName = szItemName,
            .szDataTableName = szDataTableName,
            .iCount = iItemCount
        };

        Command::SubmitTypedCommand(Command::CommandId::CmdIdSpawnItem, lpParams);
    }

    static void HandleSummon(void) {
        if (!g_G2MOptions.bIsClientHost) {
            LogError(
                "Summon",
                "Cannot summon classes without host authority"
            );
            return;
        }

        std::string szClassName;
        if (!ReadInterpreterInput(
            "[Summon] Enter class name to summon: ",
            szClassName
        )) {
            LogError("Summon", "Invalid input, please provide a class name");
            return;
        }

        if (szClassName.empty()) {
            LogError("Summon", "Class name cannot be empty");
            return;
        }

        CheatManager::Summon::SummonClass(
            szClassName
        );
    }

    static void HandleBuildAnywhere(void) {
        g_GameOptions.BuildAnywhere.fetch_xor(1, std::memory_order_acq_rel);
        LogMessage(
            "GameOptions",
            "PlaceAnywhere is now " + std::string(
                g_GameOptions.BuildAnywhere.load() ? "enabled" : "disabled"
            ),
            false
        );
    }

    static void HandleSetCollision(void) {
        bool bNewCollision = (bool) ReadIntegerInput(
            "[SetCollision] Collision [0/1]: ",
            1
        );

        CheatManager::InvokedCheats::SetPlayerCollision(bNewCollision);
    }

#pragma warning (push)
#pragma warning (disable: 26813) // bitwise op on enum
    static void HandleMaxActiveMutations(
        CheatManager::StaticCheats::EStaticCheatOp eOperation
    ) {
        using StaticOp = CheatManager::StaticCheats::EStaticCheatOp;

        const bool bIsSet = (eOperation == StaticOp::Set);
        std::string szOperationStr = bIsSet ? "set" : "get";

        int32_t iMaxMutations = 0;

        if (bIsSet) {
            iMaxMutations = ReadIntegerInput(
                "[MaxActiveMutations] Enter max active mutations: ",
                -1
            );

            if (iMaxMutations < 0) {
                LogError(
                    "MaxActiveMutations",
                    "Invalid max active mutations, must be non-negative"
                );
                return;
            }

            LogMessage(
                "MaxActiveMutations",
                "Setting max active mutations to " + std::to_string(iMaxMutations)
            );
        }

        int32_t iTargetPlayerId = ReadIntegerInput(
            "[MaxActiveMutations] Enter target player ID (empty for local): ",
            UnrealUtils::GetLocalPlayerId(true)
        );

        int32_t iResult = CheatManager::StaticCheats::MaxActiveMutations(
            eOperation,
            iTargetPlayerId,
            iMaxMutations   // ignored for Get
        );

        if (iResult < 0) {
            LogError(
                "MaxActiveMutations",
                "Failed to " + szOperationStr + " max active mutations"
            );
            return;
        }

        if (bIsSet) {
            LogMessage(
                "MaxActiveMutations",
                "Max active mutations successfully set to " + std::to_string(iResult) +
                " for player ID " + std::to_string(iTargetPlayerId)
            );
        } else {
            LogMessage(
                "MaxActiveMutations",
                "Current max active mutations for player ID " +
                std::to_string(iTargetPlayerId) + ": " +
                std::to_string(iResult)
            );
        }
    }

    static void HandleMaxCozinessLevelAchieved(
        CheatManager::StaticCheats::EStaticCheatOp eOperation
    ) {
        using StaticOp = CheatManager::StaticCheats::EStaticCheatOp;

        const bool bIsSet = (eOperation == StaticOp::Set);
        std::string szOperationStr = bIsSet ? "set" : "get";

        int32_t iCozinessLevel = 0;

        if (bIsSet) {
            iCozinessLevel = ReadIntegerInput(
                "[MaxCozinessLevelAchieved] Enter max coziness level: ",
                -1
            );
            if (iCozinessLevel < 0) {
                LogError(
                    "MaxCozinessLevelAchieved",
                    "Invalid max coziness level, must be non-negative"
                );
                return;
            }
            LogMessage(
                "MaxCozinessLevelAchieved",
                "Setting max coziness level to " + std::to_string(iCozinessLevel)
            );
        }

        int32_t iTargetPlayerId = ReadIntegerInput(
            "[MaxCozinessLevelAchieved] Enter target player ID (empty for local): ",
            UnrealUtils::GetLocalPlayerId(true)
        );

        int32_t iResult = CheatManager::StaticCheats::MaxCozinessLevelAchieved(
            eOperation,
            iTargetPlayerId,
            iCozinessLevel   // ignored for Get
        );

        if (iResult < 0) {
            LogError(
                "MaxCozinessLevelAchieved",
                "Failed to " + szOperationStr + " max coziness level"
            );
            return;
        }

        if (bIsSet) {
            LogMessage(
                "MaxCozinessLevelAchieved",
                "Max coziness level successfully set to " + std::to_string(iResult) +
                " for player ID " + std::to_string(iTargetPlayerId)
            );
        } else {
            LogMessage(
                "MaxCozinessLevelAchieved",
                "Current max coziness level for player ID " +
                std::to_string(iTargetPlayerId) + ": " +
                std::to_string(iResult)
            );
        }
    }

    static void HandleStaminaRegenRate(
        CheatManager::StaticCheats::EStaticCheatOp eOperation
    ) {
        using StaticOp = CheatManager::StaticCheats::EStaticCheatOp;

        const bool bIsSet = (eOperation == StaticOp::Set);
        std::string szOperationStr = bIsSet ? "set" : "get";

        float fRegenRate = 0.0f;
        if (bIsSet) {
            fRegenRate = ReadFloatInput(
                "[StaminaRegenRate] Enter new stamina regen rate: ",
                -1.0f
            );
            if (fRegenRate < 0.0f) {
                LogError(
                    "StaminaRegenRate",
                    "Invalid stamina regen rate, must be non-negative"
                );
                return;
            }
            LogMessage(
                "StaminaRegenRate",
                "Setting stamina regen rate to " + std::to_string(fRegenRate)
            );
        }
        int32_t iTargetPlayerId = ReadIntegerInput(
            "[StaminaRegenRate] Enter target player ID (empty for local): ",
            UnrealUtils::GetLocalPlayerId(true)
        );
        float fResult = CheatManager::StaticCheats::StaminaRegenRate(
            eOperation,
            iTargetPlayerId,
            fRegenRate   // ignored for Get
        );
        if (fResult < 0.0f) {
            LogError(
                "StaminaRegenRate",
                "Failed to " + szOperationStr + " stamina regen rate"
            );
            return;
        }
        if (bIsSet) {
            LogMessage(
                "StaminaRegenRate",
                "Stamina regen rate successfully set to " + std::to_string(fResult) +
                " for player ID " + std::to_string(iTargetPlayerId)
            );
        } else {
            LogMessage(
                "StaminaRegenRate",
                "Current stamina regen rate for player ID " +
                std::to_string(iTargetPlayerId) + ": " +
                std::to_string(fResult)
            );
        }
    }

    static void HandleStaminaRegenDelay(
        CheatManager::StaticCheats::EStaticCheatOp eOperation
    ) {
        using StaticOp = CheatManager::StaticCheats::EStaticCheatOp;
        const bool bIsSet = (eOperation == StaticOp::Set);
        std::string szOperationStr = bIsSet ? "set" : "get";
        float fRegenDelay = 0.0f;
        if (bIsSet) {
            fRegenDelay = ReadFloatInput(
                "[StaminaRegenDelay] Enter new stamina regen delay: ",
                -1.0f
            );
            if (fRegenDelay < 0.0f) {
                LogError(
                    "StaminaRegenDelay",
                    "Invalid stamina regen delay, must be non-negative"
                );
                return;
            }
            LogMessage(
                "StaminaRegenDelay",
                "Setting stamina regen delay to " + std::to_string(fRegenDelay)
            );
        }
        int32_t iTargetPlayerId = ReadIntegerInput(
            "[StaminaRegenDelay] Enter target player ID (empty for local): ",
            UnrealUtils::GetLocalPlayerId(true)
        );
        float fResult = CheatManager::StaticCheats::StaminaRegenDelay(
            eOperation,
            iTargetPlayerId,
            fRegenDelay   // ignored for Get
        );
        if (fResult < 0.0f) {
            LogError(
                "StaminaRegenDelay",
                "Failed to " + szOperationStr + " stamina regen delay"
            );
            return;
        }
        if (bIsSet) {
            LogMessage(
                "StaminaRegenDelay",
                "Stamina regen delay successfully set to " + std::to_string(fResult) +
                " for player ID " + std::to_string(iTargetPlayerId)
            );
        } else {
            LogMessage(
                "StaminaRegenDelay",
                "Current stamina regen delay for player ID " +
                std::to_string(iTargetPlayerId) + ": " +
                std::to_string(fResult)
            );
        }
    }

#pragma warning (pop) // C26813

    static void HandleCullItemByIndex(void) {
        int32_t iItemIndex = ReadIntegerInput(
            "[Culling] Enter item index to cull: ",
            -1
        );
        if (iItemIndex < 0) {
            LogError(
                "Culling", 
                "Invalid item index, must be non-negative"
            );
            return;
        }

        LogMessage(
            "Culling", 
            "Culling item ID " + std::to_string(iItemIndex) + "..."
        );

        CheatManager::Culling::CullItemByItemIndex(iItemIndex);
    }

    static void HandleCullItemType(void) {
        std::string szItemTypeName;
        if (!ReadInterpreterInput(
            "[Culling] Enter item type name to cull all: ",
            szItemTypeName
        )) {
            LogError(
                "Culling", 
                "Invalid input, please provide an item type name"
            );
            return;
        }
        LogMessage(
            "Culling", 
            "Culling all items of type '" + szItemTypeName + "'..."
        );
        CheatManager::Culling::CullAllItemInstances(
            szItemTypeName,
            false
        );
    }

    static void HandleBuildAllStructures(void) {
        if (!g_G2MOptions.bIsClientHost) {
            LogError(
                "BuildAllStructures",
                "Cannot build structures without host authority"
            );
            return;
        }

        CheatManager::BufferParamsExecuteCheat *lpParams = new CheatManager::BufferParamsExecuteCheat{
            .FunctionId = CheatManager::CheatManagerFunctionId::BuildAllBuildings
        };

        Command::SubmitTypedCommand(
            Command::CommandId::CmdIdCheatManagerExecute,
            lpParams
        );
    }

    static void HandleSetDebugFilter(void) {
        int32_t iHookId = ReadIntegerInput(
            "[DebugFilter] Enter hook ID to apply filter to (empty for all): ",
            HookManager::UniqueHookIdSpecial::AllHooks
        );

        if (iHookId < 0) {
            LogError(
                "DebugFilter", 
                "Invalid hook ID, must be -1 (all) or a valid hook ID"
            );
            return;
        }

        std::string szInputDebugFilter;

        if (!ReadInterpreterInput(
            "[DebugFilter] Enter debug function substring filter: ",
            szInputDebugFilter
        )) {
            LogError(
                "DebugFilter",
                "Invalid input, please provide a valid debug filter"
            );
            return;
        }

        if (
            szInputDebugFilter == ""
            ||
            szInputDebugFilter == " "
            ||
            szInputDebugFilter == "\n"
        ) {
            szInputDebugFilter.clear();
        }

        HookManager::HookType eHookType = HookManager::GetHookTypeById(iHookId);
        switch (eHookType) {
            case HookManager::HookType::ProcessEvent:
                HookManager::ProcessEventHooker::SetHookDebugFilterById(
                    iHookId,
                    szInputDebugFilter
                );
                break;
            case HookManager::HookType::NativeFunction:
                HookManager::NativeHooker::SetHookDebugFilterById(
                    iHookId,
                    szInputDebugFilter
                );
                break;
            case HookManager::HookType::Invalid:
            default:
                LogError(
                    "DebugFilter",
                    "Invalid hook ID, no such hook found: " + std::to_string(iHookId)
                );
                return;
        }

        if (szInputDebugFilter.empty()) {
            LogMessage("DebugFilter", "Cleared debug filter");
        } else {
            LogMessage(
                "DebugFilter",
                "DebugFilter needle set to '" + szInputDebugFilter + "'"
            );
        }
    }

    void PrintAvailableCommands(void);

    ConsoleCommand g_Commands[] = {
        { "Help", "Show available commands", PrintAvailableCommands },
        { "help", "Show available commands", PrintAvailableCommands },
        { "C_CullItemInstance", "Cull item by instance ID", HandleCullItemByIndex},
        { "C_CullItemType", "Cullall items of set type", HandleCullItemType },
        { "F_DataTableNeedle", "Search for DataTable", HandleDataTableSearch },
        { "F_EntryDump", "Dump DataTable entries", HandleItemDump },
        { "F_FindItemTable", "Find DataTable for item", HandleFindItemTable },
        { "F_FunctionDump", "Dump functions", HandleFunctionDump },
        { "F_ClassDump", "Dump classes by name", HandleClassDump },
        { "H_GetAuthority", "Check host authority", HandleGetAuthority },
        { "I_SpawnItem", "Spawn item", HandleSpawnItem },
        { "I_ItemSpawn", "Spawn item", HandleSpawnItem },
        { "S_SummonClass", "Summon an internal class", HandleSummon },
        { "S_BuildAll", "Build all structures for player", HandleBuildAllStructures },
        {
            "P_ShowPlayers", "Show connected players", []() { 
                UnrealUtils::DumpConnectedPlayers(); 
            }
        },
        {
            "X_DebugToggle", "Toggle debug mode", []() {
                (IsDebugOutputEnabled()) ? DisableDebugOutput() : EnableDebugOutput();

                LogMessage(
                    "Debug",
                    "Debug mode " + std::string(IsDebugOutputEnabled() ? "enabled" : "disabled")
                );
            }
        },
        { "X_DebugFilter", "Set debug function filter", HandleSetDebugFilter },
        {
            "X_HookInfo", "Show installed hooks", []() {
                LogMessage("HookInfo", "Listing ProcessEvent hooks..");
                HookManager::ProcessEventHooker::ListActiveHooks();
                LogMessage("HookInfo", "Listing Native function hooks..");
                HookManager::NativeHooker::ListActiveHooks();
            }
        },
        {
            "X_ToggleDebugPlayerReport", "Toggle automatic timed player report when debug output is enabled", []() {
                g_G2MOptions.bHideAutoPlayerDbgInfo.fetch_xor(1, std::memory_order_acq_rel);
                LogMessage(
                    "Debug",
                    "Automatic player report " + std::string(
                        g_G2MOptions.bHideAutoPlayerDbgInfo.load() ? "disabled" : "enabled"
                    )
                );
            }
        },
        {
            "X_InitCheatManager", "Initialize cheat manager", []() {
                if (!CheatManager::IsSurvivalCheatManagerInitialized()) {
                    LogMessage(
                        "CheatManager", 
                        "Cheat manager already initialized, skipping"
                    );
                    return;
                }
            
                LogMessage(
                    "CheatManager", 
                    "Initializing cheat manager.."
                );
                if (!CheatManager::ManualInitialize()) {
                    LogError(
                        "CheatManager", 
                        "Failed to initialize cheat manager"
                    );
                } else {
                    LogMessage(
                        "CheatManager",
                        "Cheat manager initialized successfully"
                    );
                }
            }
        },
        { 
            "OPT_BuildAnywhere", "Toggle BuildAnywhere option", []() {
                // todo: make a macro for global output enable for a function call
                bool bGlobalOutput = IsGlobalOutputEnabled();
                EnableGlobalOutput();
                HandleBuildAnywhere();
                if (!bGlobalOutput) {
                    DisableGlobalOutput();
                }
            }
        }, 
        //{ "OPT_SetCollision", "Toggle SetCollision option", HandleSetCollision }, // removed cuz dont know what the fuck it does xd
        { "OPT_GetMaxActiveMutations", "Get max active mutations", []() {
            HandleMaxActiveMutations(
                CheatManager::StaticCheats::EStaticCheatOp::Get
            );
        }},
        { "OPT_SetMaxActiveMutations", "Set max active mutations", []() {
            HandleMaxActiveMutations(
                CheatManager::StaticCheats::EStaticCheatOp::Set
            );
        }},
        { "OPT_GetMaxCozinessLevelAchieved", "Get max coziness level achieved", []() {
            HandleMaxCozinessLevelAchieved(
                CheatManager::StaticCheats::EStaticCheatOp::Get
            );
        }},
        { "OPT_SetMaxCozinessLevelAchieved", "Set max coziness level achieved", []() {
            HandleMaxCozinessLevelAchieved(
                CheatManager::StaticCheats::EStaticCheatOp::Set
            );
        }}
    };

    void PrintAvailableCommands(
        void
    ) {
        const int32_t iCommandCount = _ARRAYSIZE(g_Commands);
        LogMessage("Help", "Available commands:");
        for (int32_t i = 0; i < iCommandCount; i++) {
            LogMessage(
                "Help", 
                " - " + g_Commands[i].szCommand + ": " + g_Commands[i].szDescription
            );
        }
    }

    bool IsCommandAvailable(
        const std::string& szInput
    ) {
        // Check predefined commands
        const int32_t iCommandCount = _ARRAYSIZE(g_Commands);

        for (int32_t i = 0; i < iCommandCount; i++) {
            if (szInput == g_Commands[i].szCommand) {
                g_Commands[i].fnHandler();
                return true;
            }
        }

        return false;
    }

    int32_t ResolvePlayerId(
        const std::string& szPrompt
    ) {
        char szInput[512] = { 0 };
        
        std::cout << szPrompt << std::flush;

        if (nullptr == fgets(
            szInput,
            sizeof(szInput) - 1,
            stdin
        )) {
            LogError("Input", "Failed to read input.");
            return false;
        }

        // strip newline bih
        szInput[strcspn(szInput, "\n")] = '\0';

        std::string s(szInput);

        int32_t iPlayerId = -1;
        int32_t iLocalPlayerId = UnrealUtils::GetLocalPlayerId();

        if (!s.empty()) {
            try {
                iPlayerId = std::stoi(szInput);
            } catch (const std::exception& e) {
                // Fall back to local player ID if available
                if (iLocalPlayerId == -1) {
                    LogError(
                        "PlayerResolver", 
                        "Unable to determine local player ID (" + std::string(e.what()) + ")"
                    );
                    return -1;
                }
                LogMessage(
                    "PlayerResolver", 
                    "Using local player ID (" + std::to_string(iLocalPlayerId) + ")"
                );
                return iLocalPlayerId;
            }
        } else {
            if (iLocalPlayerId == -1) {
                LogError("PlayerResolver", "Unable to determine player ID");
                return -1;
            }
            LogMessage(
                "PlayerResolver", 
                "Using local player ID (" + std::to_string(iLocalPlayerId) + ")"
            );
            return iLocalPlayerId; // use local player ID if input is empty
        }

        return iPlayerId;
    }

    bool ReadInterpreterInput(
        const std::string& szPrompt,
        std::string& szOutInput
    ) {
        char szInput[512] = { 0 };

        std::cout << szPrompt << std::flush;

        if (nullptr == fgets(
            szInput,
            sizeof(szInput) - 1,
            stdin
        )) {
            LogError("Input", "Failed to read input.");
            return false;
        }

        // strip newline bih
        szInput[strcspn(szInput, "\n")] = '\0';

        std::string s(szInput);

        // trim sucka front
        size_t start = s.find_first_not_of(" \t\r\n");
        // and trim sucka end
        size_t end = s.find_last_not_of(" \t\r\n");

        if (start == std::string::npos || end == std::string::npos) {
            szOutInput = "";
        } else {
            szOutInput = s.substr(start, end - start + 1);
        }

        return true;
    }

    int32_t ReadIntegerInput(
        const std::string& szPrompt,
        int32_t iDefaultValue
    ) {
        std::string szInput;
        if (!ReadInterpreterInput(szPrompt, szInput)) {
            LogError("Input", "No input provided", true);
            return iDefaultValue;
        }

        if (szInput.empty()) {
            return iDefaultValue;
        }

        try {
            return std::stoi(szInput);
        } catch (const std::exception& e) {
            LogError(
                "Input", 
                "Invalid number format (" + std::string(e.what()) + ")",
                true
            );
            return iDefaultValue;
        }
    }

    float ReadFloatInput(
        const std::string& szPrompt,
        float fDefaultValue
    ) {
        std::string szInput;
        if (!ReadInterpreterInput(szPrompt, szInput)) {
            LogError("Input", "No input provided", true);
            return fDefaultValue;
        }

        if (szInput.empty()) {
            return fDefaultValue;
        }

        try {
            return std::stof(szInput);
        } catch (const std::exception& e) {
            LogError(
                "Input", 
                "Invalid float format (" + std::string(e.what()) + ")",
                true
            );
            return fDefaultValue;
        }
    }
}