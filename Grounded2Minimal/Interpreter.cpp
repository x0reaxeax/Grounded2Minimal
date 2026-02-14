#include "CheatManager.hpp"
#include "Interpreter.hpp"
#include "UnrealUtils.hpp"
#include "ItemSpawner.hpp"
#include "HookManager.hpp"
#include "Command.hpp"

#include <functional>
#include <vector>
#include <cctype>
#include <algorithm>

namespace Interpreter {
    namespace KeyBinds {

        static struct _KeyBindThread {
            HANDLE KeyBindThreadHandle = nullptr;
            DWORD ThreadId = 0;
            std::atomic<uint8_t> bRunning = false;
        } KeyBindThread{};

        using HandlerFn = void(*)();

        enum class Trigger : uint8_t {
            OnKeyDown,
            OnKeyUp,
            WhileDown
        };

        enum ModMask : uint8_t {
            ModNone = 0,
            ModCtrl = 1 << 0,
            ModAlt = 1 << 1,
            ModShift = 1 << 2
        };

        inline uint8_t operator|(ModMask a, ModMask b) {
            return (uint8_t) a | (uint8_t) b;
        }

        namespace Functions {
            static void HandleBuildAll(void) {
                CheatManager::BufferParamsExecuteCheat* lpParams = new CheatManager::BufferParamsExecuteCheat{
                    .FunctionId = CheatManager::CheatManagerFunctionId::BuildAllBuildings
                };

                Command::SubmitTypedCommand(
                    Command::CommandId::CmdIdCheatManagerExecute,
                    lpParams
                );

                if (IsDebugOutputEnabled()) {
                    LogMessage("KeyBind", "Build All Structures command executed", true);
                }
            }

            static void HandleBuildAnywhere(void) {
                g_GameOptions.BuildAnywhere.fetch_xor(1, std::memory_order_acq_rel);

                const bool bNow = g_GameOptions.BuildAnywhere.load(std::memory_order_acquire);
                LPCWSTR cwszMessage = bNow ? L"BuildAnywhere enabled" : L"BuildAnywhere disabled";

                SDK::FString fszMessage = SDK::FString(cwszMessage);
                UnrealUtils::GetGameUI()->PostGenericMessage(fszMessage, nullptr);

                if (IsDebugOutputEnabled()) {
                    LogMessage(
                        L"KeyBind",
                        cwszMessage,
                        true
                    );
                }
            }
        }

        struct KeyBind {
            DWORD keyCode = 0;
            uint8_t mod = ModNone;
            Trigger trigger = Trigger::OnKeyDown;
            HandlerFn handler = nullptr;

            // runtime state
            bool wasDown = false;
        };

        LPCSTR KeyBindConfigFile = "G2MinimalConfig.ini";
        static std::vector<KeyBind> g_vKeyBinds;

        // Name -> handler table
        static constexpr struct {
            const char* Name;
            HandlerFn   Handler;
            uint8_t     DefaultMods;
            Trigger     DefaultTrigger;
        } kBindings[] = {
            {"BuildAll",      Functions::HandleBuildAll,      ModNone, Trigger::OnKeyDown},
            {"BuildAnywhere", Functions::HandleBuildAnywhere, ModNone, Trigger::OnKeyDown},
        };

        static HandlerFn HandlerFromName(
            const std::string& szName, 
            uint8_t* pOutMods = nullptr, 
            Trigger* pOutTrig = nullptr
        ) {
            for (const auto& e : kBindings) {
                if (szName == e.Name) {
                    if (nullptr != pOutMods) {
                        *pOutMods = e.DefaultMods;
                    }
                    if (nullptr != pOutTrig) {
                        *pOutTrig = e.DefaultTrigger;
                    }
                    return e.Handler;
                }
            }
            return nullptr;
        }

        bool IsKeyDown(DWORD dwKeyCode) {
            if (g_hConsole == GetForegroundWindow()) {
                return false;
            }
            return (GetAsyncKeyState((int) dwKeyCode) & 0x8000) != 0;
        }

        static uint8_t ReadModsOnce(void) {
            uint8_t m = ModNone;
            if (IsKeyDown(VK_CONTROL)) { m |= ModCtrl; }
            if (IsKeyDown(VK_MENU)) { m |= ModAlt; }
            if (IsKeyDown(VK_SHIFT)) { m |= ModShift; }
            return m;
        }

        static bool ModsMatch(uint8_t ucRequired, uint8_t ucCurrent) {
            return (ucCurrent & ucRequired) == ucRequired;
        }

        static bool AddBind(
            DWORD dwKeyCode, 
            uint8_t ucMods, 
            Trigger trigger, 
            HandlerFn fnHandler
        ) {
            if (!fnHandler || dwKeyCode == 0) {
                LogError("AddBind", "Invalid request");
                return false;
            }

            KeyBind bind;
            bind.keyCode = dwKeyCode;
            bind.mod = ucMods;
            bind.trigger = trigger;
            bind.handler = fnHandler;
            bind.wasDown = false;

            g_vKeyBinds.emplace_back(bind);
            return true;
        }

        void Tick(void) {
            if (g_G2MOptions.bRunning.load() == false) {
                return;
            }

            const uint8_t modsNow = ReadModsOnce();

            for (auto& bind : g_vKeyBinds) {
                if (!bind.handler || bind.keyCode == 0) {
                    continue;
                }

                if (!ModsMatch(bind.mod, modsNow)) {
                    bind.wasDown = false;
                    continue;
                }

                const bool bIsDown = IsKeyDown(bind.keyCode);

                switch (bind.trigger) {
                    case Trigger::OnKeyDown:
                        if (bIsDown && !bind.wasDown) bind.handler();
                        break;
                    case Trigger::OnKeyUp:
                        if (!bIsDown && bind.wasDown) bind.handler();
                        break;
                    case Trigger::WhileDown:
                        if (bIsDown) bind.handler();
                        break;
                }

                bind.wasDown = bIsDown;
            }
        }

        DWORD WINAPI KeyBindThreadProc(LPVOID lpParam) {
            UNREFERENCED_PARAMETER(lpParam);
            while (KeyBindThread.bRunning.load(std::memory_order_relaxed)) {
                Tick();
                Sleep(25);
            }
            return EXIT_SUCCESS;
        }

        bool CreateDefaultConfig(void) {
            // one bind per line: Name=0xVK
            LPCSTR cszDefaultConfig =
                "BuildAll=0x70\n"
                "BuildAnywhere=0x72\n";

            FILE* fpConfig = nullptr;
            if (EXIT_SUCCESS != fopen_s(&fpConfig, KeyBindConfigFile, "w")) {
                LogError(
                    "KeyBinds", 
                    "Failed to open key bind config file for writing: " + std::string(KeyBindConfigFile)
                );
                return false;
            }

            if (fprintf(fpConfig, "%s", cszDefaultConfig) < 0) {
                LogError(
                    "KeyBinds", 
                    "Failed to write default config to file: " + std::string(KeyBindConfigFile)
                );
                fclose(fpConfig);
                return false;
            }

            fclose(fpConfig);
            return true;
        }

        static bool ParseLineKeyValue(
            const std::string& szLine, 
            std::string& pszOutKey, 
            std::string& pszoutVal
        ) {
            const size_t eq = szLine.find('=');
            if (eq == std::string::npos || eq == 0 || eq + 1 >= szLine.size()) {
                return false;
            }
            pszOutKey = szLine.substr(0, eq);
            pszoutVal = szLine.substr(eq + 1);
            return true;
        }

        bool ReloadKeyBinds(void) {
            g_vKeyBinds.clear();

            FILE* fpConfig = nullptr;
            if (EXIT_SUCCESS != fopen_s(&fpConfig, KeyBindConfigFile, "r")) {
                LogError(
                    "KeyBinds", 
                    "Failed to open key bind config file: " + std::string(KeyBindConfigFile)
                );
                return false;
            }

            char szLineBuffer[256];
            while (fgets(szLineBuffer, sizeof(szLineBuffer), fpConfig)) {
                std::string line(szLineBuffer);

                // strip whitespace
                line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
                if (line.empty()) {
                    continue;
                }

                // allow comments
                if (line[0] == '#' || line.starts_with("//")) {
                    continue;
                }

                std::string key, val;
                if (!ParseLineKeyValue(line, key, val)) {
                    continue;
                }

                uint8_t defaultMods = ModNone;
                Trigger defaultTrig = Trigger::OnKeyDown;

                HandlerFn handler = HandlerFromName(key, &defaultMods, &defaultTrig);
                if (!handler) {
                    if (IsDebugOutputEnabled()) {
                        LogMessage("KeyBinds", "Unknown key bind name in config: " + key, true);
                    }
                    continue;
                }

                DWORD dwVkCode = 0;
                try {
                    // accept both "0x70" and "70"
                    dwVkCode = std::stoul(val, nullptr, 16);
                } catch (const std::exception& e) {
                    LogError("KeyBinds", "Error parsing key code for " + key + ": " + e.what());
                    continue;
                }

                if (AddBind(dwVkCode, defaultMods, defaultTrig, handler)) {
                    CHAR szBuf[64] = { 0 };
                    snprintf(szBuf, sizeof(szBuf), "0x%02lX", (DWORD) dwVkCode);

                    LogMessage(
                        "KeyBinds",
                        "Loaded key bind: " + key + " = " + std::string(szBuf),
                        true
                    );
                }
            }

            fclose(fpConfig);
            return true;
        }

        bool Initialize(void) {
            if (!CoreUtils::FileExists(KeyBindConfigFile)) {
                LogMessage(
                    "KeyBinds", 
                    "Key bind config file not found, creating default config"
                );
                if (!CreateDefaultConfig()) {
                    LogError("KeyBinds", "Failed to create default key bind config file");
                    return false;
                }
            }

            LogMessage("KeyBinds", "Loading key bind config from file");
            if (!ReloadKeyBinds()) {
                LogError("KeyBinds", "Failed to load key bind config");
                return false;
            }

            KeyBindThread.bRunning.store(true, std::memory_order_relaxed);
            KeyBindThread.KeyBindThreadHandle = CreateThread(
                nullptr,
                0,
                KeyBindThreadProc,
                nullptr,
                0,
                &KeyBindThread.ThreadId
            );

            if (nullptr == KeyBindThread.KeyBindThreadHandle) {
                KeyBindThread.bRunning.store(false, std::memory_order_relaxed);
                LogError("KeyBinds", "Failed to create key bind thread");
                return false;
            }

            return true;
        }

        void Shutdown(void) {
            KeyBindThread.bRunning.store(false, std::memory_order_relaxed);
            if (nullptr != KeyBindThread.KeyBindThreadHandle) {
                constexpr DWORD dwWaitMs = 5000;
                const DWORD dwWaitRes = WaitForSingleObject(KeyBindThread.KeyBindThreadHandle, dwWaitMs);
                if (WAIT_OBJECT_0 != dwWaitRes) {
                    LogError("KeyBinds", "Key bind thread did not exit within timeout");
                    // intentionally continue to avoid deadlocking tool unload
                }

                CloseHandle(KeyBindThread.KeyBindThreadHandle);
                KeyBindThread.KeyBindThreadHandle = nullptr;
            }
        }
    } // namespace KeyBinds


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
                "Invalid hook ID, must be 0 (all) or a valid hook ID"
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

        if (HookManager::UniqueHookIdSpecial::AllHooks == iHookId) {
            HookManager::ProcessEventHooker::SetHookDebugFilterAll(szInputDebugFilter);
            HookManager::NativeHooker::SetHookDebugFilterAll(szInputDebugFilter);
            if (szInputDebugFilter.empty()) {
                LogMessage("DebugFilter", "Cleared global debug filter");
            } else {
                LogMessage(
                    "DebugFilter",
                    "Global debug filter needle set to '" + szInputDebugFilter + "'"
                );
            }
            return;
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

    static void HandleGameModeSwitch(void) {
        /*
         * Normal                                   = 1,
	     * Relaxed                                  = 2,
	     * Hard                                     = 3,
	     * Creative                                 = 4,
	     * CreativeCreatures                        = 5,
	     * Custom                                   = 6,
	     * Inventor                                 = 7,
        */
        const std::string cszGameModes = "Available game modes:\n"
            " 1 - Normal\n"
            " 2 - Relaxed\n"
            " 3 - Hard\n"
            " 4 - Creative\n"
            " 5 - CreativeCreatures\n"
            " 6 - Custom\n"
            " 7 - Inventor\n";

        std::cout << cszGameModes;

        int32_t iGameMode = ReadIntegerInput(
            "[GameModeSwitch] Enter game mode ID to switch to: ",
            -1
        );

        if (iGameMode < 1 || iGameMode > 7) {
            LogError(
                "GameModeSwitch",
                "Invalid game mode ID, must be between 1 and 7"
            );
            return;
        }

        CheatManager::InvokedCheats::SetGameMode(
            static_cast<SDK::EGameMode>(iGameMode)
        );

        LogMessage(
            "GameModeSwitch",
            "Switched game mode to ID " + std::to_string(iGameMode)
        );
    }

    static void HandleGameTypeSwitch(void) {
        /*
        	Story                                    = 0,
	        Playground                               = 1,
        */

        const std::string cszGameTypes = "Available game types:\n"
            " 0 - Story\n"
            " 1 - Playground\n";

        std::cout << cszGameTypes;

        int32_t iGameType = ReadIntegerInput(
            "[GameTypeSwitch] Enter game type ID to switch to: ",
            -1
        );

        if (iGameType < 0 || iGameType > 1) {
            LogError(
                "GameTypeSwitch",
                "Invalid game type ID, must be 0 or 1"
            );
            return;
        }

        CheatManager::StaticCheats::SetGameType(
            static_cast<SDK::EGameType>(iGameType)
        );

        LogMessage(
            "GameTypeSwitch",
            "Switched game type to ID " + std::to_string(iGameType)
        );
    }

    static void HandleSetPlayerDamageMultiplier(void) {
        float fDamageMultiplier = ReadFloatInput(
            "[SetPlayerDamageMultiplier] Enter new player damage multiplier: ",
            -1.0f
        );
        if (fDamageMultiplier < 0.0f) {
            LogError(
                "SetPlayerDamageMultiplier",
                "Invalid damage multiplier, must be non-negative"
            );
            return;
        }

        CheatManager::StaticCheats::SetPlayerDamageMultiplier(fDamageMultiplier);
        g_GameOptions.GameStatics.PlayerDamageMultiplier.store(fDamageMultiplier, std::memory_order_relaxed);

        LogMessage(
            "SetPlayerDamageMultiplier",
            "Player damage multiplier set to " + std::to_string(fDamageMultiplier)
        );
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
        { "H_SetGameMode", "Switch game mode", HandleGameModeSwitch },
        { "H_SetGameType", "Switch game type", HandleGameTypeSwitch },
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
        { "X_ReloadBinds", "Reload key binds from config file", []() {
            if (KeyBinds::ReloadKeyBinds()) {
                LogMessage("KeyBinds", "Key binds reloaded successfully");
            } else {
                LogError("KeyBinds", "Failed to reload key binds");
            }
        }},
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
            "X_ShowGameOptions", "Show current game options", []() {
            // TODO: streamline this shi
                LogMessage(
                    "GameOptions",
                    "Current Game Options:\n" +
                    std::string(" - Build Anywhere          - ") + (g_GameOptions.BuildAnywhere.load() ? "enabled" : "disabled") + "\n" +
                    std::string(" - Handy GnatForce Enable  - ") + (g_GameOptions.GameStatics.HandyGnatForceEnable.load() ? "enabled" : "disabled") + "\n" +
                    std::string(" - Auto Complete Buildings - ") + (g_GameOptions.GameStatics.AutoCompleteBuildings.load() ? "enabled" : "disabled") + "\n" +
                    std::string(" - Building Integrity      - ") + (g_GameOptions.GameStatics.BuildingIntegrity.load() ? "enabled" : "disabled") + "\n" +
                    std::string(" - God Mode                - ") + (g_GameOptions.GodMode.load() ? "enabled" : "disabled") + "\n" +
                    std::string(" - Infinite Stamina        - ") + (g_GameOptions.InfiniteStamina.load() ? "enabled" : "disabled") + "\n" + 
                    std::string(" - Pet Invincibility       - ") + (g_GameOptions.GameStatics.InvinciblePets.load() ? "enabled" : "disabled") + "\n" +
                    std::string(" - Free Crafting           - ") + (g_GameOptions.GameStatics.FreeCrafting.load() ? "enabled" : "disabled") + "\n" +
                    std::string(" - Damage Multiplier       - ") + std::to_string(g_GameOptions.GameStatics.PlayerDamageMultiplier.load()) + "\n"
                );
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
        }},
        { "OPT_ToggleHandyGnat", "Force enable/disable Handy Gnat availability", []() {
            CheatManager::StaticCheats::ToggleHandyGnat(
                g_GameOptions.GameStatics.HandyGnatForceEnable.fetch_xor(1, std::memory_order_acq_rel)
            );
            LogMessage(
                "HandyGnat",
                "Handy Gnat force enable is now " + std::string(
                    g_GameOptions.GameStatics.HandyGnatForceEnable.load() ? "enabled" : "disabled"
                )
            );
        }},
        { "OPT_ToggleAutoCompleteBuildings", "Toggle auto-completion of building placements", []() {
            CheatManager::StaticCheats::ToggleAutoCompleteBuildings(
                g_GameOptions.GameStatics.AutoCompleteBuildings.fetch_xor(1, std::memory_order_acq_rel)
            );
            LogMessage(
                "AutoCompleteBuildings",
                "Auto-completion of building placements is now " + std::string(
                    g_GameOptions.GameStatics.AutoCompleteBuildings.load() ? "enabled" : "disabled"
                )
            );
        }},
        { "OPT_ToggleBuildingIntegrity", "Toggle building integrity checks", []() {
            CheatManager::StaticCheats::ToggleBuildingIntegrity(
                g_GameOptions.GameStatics.BuildingIntegrity.fetch_xor(1, std::memory_order_acq_rel)
            );
            LogMessage(
                "BuildingIntegrity",
                "Building integrity checks are now " + std::string(
                    g_GameOptions.GameStatics.BuildingIntegrity.load() ? "enabled" : "disabled"
                )
            );

        }},
        { "OPT_ToggleFreeCrafting", "Toggle free crafting", []() {
            CheatManager::StaticCheats::ToggleFreeCrafting(
                g_GameOptions.GameStatics.FreeCrafting.fetch_xor(1, std::memory_order_acq_rel)
            );
            LogMessage(
                "FreeCrafting",
                "Free crafting is now " + std::string(
                    g_GameOptions.GameStatics.FreeCrafting.load() ? "enabled" : "disabled"
                )
            );
        }},
        { "OPT_TogglePetInvincibility", "Toggle pet invincibility", []() {
            CheatManager::StaticCheats::ToggleInvinciblePets(
                g_GameOptions.GameStatics.InvinciblePets.fetch_xor(1, std::memory_order_acq_rel)
            );
            LogMessage(
                "InvinciblePets",
                "Pet invincibility is now " + std::string(
                    g_GameOptions.GameStatics.InvinciblePets.load() ? "enabled" : "disabled"
                )
            );
        }}, 
        { "OPT_SetPlayerDamageMultiplier", "Set player damage multiplier", HandleSetPlayerDamageMultiplier }
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