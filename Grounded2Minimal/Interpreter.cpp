#include "CheatManager.hpp"
#include "Interpreter.hpp"
#include "UnrealUtils.hpp"
#include "ItemSpawner.hpp"
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

    static void HandleItemDump(void) {
        std::string szTableName;
        if (!ReadInterpreterInput(
            "[DataTableItemDump] Enter DataTable name to dump items: ",
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

    void HandleClassDump(void) {
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

    void HandleGetAuthority(void) {
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

    void HandleSpawnItem(void) {
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

    void HandleSummon(void) {
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

    ConsoleCommand g_Commands[] = {
        {"F_DataTableNeedle", "Search for DataTable", HandleDataTableSearch },
        {"F_ItemDump", "Dump DataTable items", HandleItemDump },
        {"F_FindItemTable", "Find DataTable for item", HandleFindItemTable },
        {"F_FunctionDump", "Dump functions", HandleFunctionDump },
        {"F_ClassDump", "Dump classes by name", HandleClassDump },
        {"H_GetAuthority", "Check host authority", HandleGetAuthority },
        {"I_SpawnItem", "Spawn item", HandleSpawnItem },
        {"I_ItemSpawn", "Spawn item", HandleSpawnItem },
        {"S_SummonClass", "Summon an internal class", HandleSummon },
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
        }
    };

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
                    LogError("PlayerResolver", "Unable to determine local player ID");
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

        try {
            return std::stoi(szInput);
        } catch (const std::exception& e) {
            LogError("Input", "Invalid number format", true);
            return iDefaultValue;
        }
    }
}