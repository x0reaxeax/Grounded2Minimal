// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#include "HookManager.hpp"
#include "UnrealUtils.hpp"
#include <Windows.h>

namespace HookManager {
    struct HookTable
    {
        // Find by unique integer ID
        template <class TMap>
        static typename TMap::mapped_type* GetHookById(
            TMap& s_Hooks,
            std::mutex& s_HookMutex,
            int32_t iHookId
        )
        {
            std::lock_guard<std::mutex> lockGuard(s_HookMutex);

            for (auto& kv : s_Hooks)
            {
                auto& hook = kv.second;
                if (hook.iUniqueId == iHookId)
                {
                    return &hook;
                }
            }

            return nullptr;
        }

        // Find by function pointer
        template <class TMap, class TPred>
        static typename TMap::mapped_type* FindIf(
            TMap& s_Hooks,
            std::mutex& s_HookMutex,
            TPred Pred
        )
        {
            std::lock_guard<std::mutex> lockGuard(s_HookMutex);

            for (auto& kv : s_Hooks)
            {
                auto& hook = kv.second;
                if (true == Pred(hook))
                {
                    return &hook;
                }
            }

            return nullptr;
        }

        // Set debug filter on all hooks
        template <class TMap>
        static bool SetHookDebugFilterAll(
            TMap& s_Hooks,
            std::mutex& s_HookMutex,
            const std::string& cszDebugFilter,
            const char* cszLogTag
        )
        {
            std::lock_guard<std::mutex> lockGuard(s_HookMutex);

            for (auto& kv : s_Hooks)
            {
                auto& hook = kv.second;
                hook.szDebugFilter = cszDebugFilter;
                hook.bDebugFilterEnabled.store(!cszDebugFilter.empty());
            }

            LogMessage(
                cszLogTag,
                std::string("Set debug filter on all hooks to '") + cszDebugFilter + "'"
            );

            return true;
        }

        // Set debug filter by ID
        template <class TMap>
        static bool SetHookDebugFilterById(
            TMap& s_Hooks,
            std::mutex& s_HookMutex,
            int32_t iHookId,
            const std::string& cszDebugFilter,
            const char* cszLogTag,
            int32_t iAllHooksSentinel
        )
        {
            if (iHookId == iAllHooksSentinel)
            {
                return SetHookDebugFilterAll(s_Hooks, s_HookMutex, cszDebugFilter, cszLogTag);
            }

            std::lock_guard<std::mutex> lockGuard(s_HookMutex);

            for (auto& kv : s_Hooks)
            {
                auto& hook = kv.second;
                if (hook.iUniqueId == iHookId)
                {
                    hook.szDebugFilter = cszDebugFilter;
                    hook.bDebugFilterEnabled.store(!cszDebugFilter.empty());

                    LogMessage(
                        cszLogTag,
                        "Set debug filter on hook ID " + std::to_string(iHookId) + " to '" + cszDebugFilter + "'"
                    );

                    return true;
                }
            }

            LogError(
                cszLogTag,
                "SetHookDebugFilterById: Hook ID not found: " + std::to_string(iHookId)
            );

            return false;
        }

        template <class TMap, class TPrinter>
        static void ListActiveHooks(
            const TMap& s_Hooks,
            std::mutex& s_HookMutex,
            const char* cszLogTag,
            const char* cszHookType,
            TPrinter PrintTarget
        ) {
            std::lock_guard<std::mutex> lockGuard(s_HookMutex);

            LogMessage(
                cszLogTag,
                std::string("Active ") + cszHookType + " Hooks: " + std::to_string(s_Hooks.size())
            );

            for (const auto& kv : s_Hooks)
            {
                const auto& hook = kv.second;

                std::string szFilterInfo;
                if (true == hook.bDebugFilterEnabled.load())
                {
                    szFilterInfo = "  * Active Debug Filter: '" + hook.szDebugFilter + "'";
                } else
                {
                    szFilterInfo = "  * No active debug filter";
                }

                LogMessage(
                    cszLogTag,
                    "Hook Info:\n"
                    "  * Target:    " + PrintTarget(hook) + "\n"
                    "  * Name:      " + hook.szHookName + "\n"
                    "  * Hook ID:   " + std::to_string(hook.iUniqueId) + "\n"
                    "  * Hook Type: " + cszHookType + "\n" +
                    szFilterInfo
                );
            }
        }
    };


    bool ProcessEventHooker::InstallHook(
        SDK::UObject* lpObject,
        HookedFn fnHook,
        const std::string& cszHookName
    ) {
        if (nullptr == lpObject || nullptr == fnHook) {
            return false;
        }

        std::lock_guard<std::mutex> lockGuard(s_HookMutex);

        void** lpVTable = *reinterpret_cast<void***>(lpObject);
        if (nullptr == lpVTable) {
            return false;
        }

        if (s_HookedVTables.contains(lpVTable)) {
            LogMessage(
                "HookManager",
                "VTable already hooked, skipping duplicate hook"
            );
            return false;
        }

        DWORD dwOldProtect;
        if (!VirtualProtect(
            &lpVTable[SDK::Offsets::ProcessEventIdx],
            sizeof(void*),
            PAGE_EXECUTE_READWRITE,
            &dwOldProtect
        )) {
            return false;
        }

        auto fnOriginal = reinterpret_cast<ProcessEvent_t>(lpVTable[SDK::Offsets::ProcessEventIdx]);
        lpVTable[SDK::Offsets::ProcessEventIdx] = reinterpret_cast<void*>(fnHook);

        VirtualProtect(
            &lpVTable[SDK::Offsets::ProcessEventIdx],
            sizeof(void*),
            dwOldProtect,
            &dwOldProtect
        );

        const std::string objFullName = lpObject->GetFullName();
        const std::string effectiveHookName = cszHookName.empty() ? lpObject->GetName() : cszHookName;

        // Atomic-friendly in-place construction (no copy/move)
        std::pair<std::unordered_map<SDK::UObject*, HookData>::iterator, bool> insRes =
            s_Hooks.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(lpObject),
                std::forward_as_tuple(
                    lpVTable,
                    fnOriginal,
                    fnHook,
                    objFullName,
                    effectiveHookName,
                    "" // no debug filter by default
                )
            );

        s_HookedVTables.insert(lpVTable);

        LogMessage(
            "HookManager",
            "Hook installed on: '" + lpObject->GetName() + "' [UID: " + std::to_string(insRes.first->second.iUniqueId) + "]"
        );
        return true;
    }

    void ProcessEventHooker::RestoreHooks(void) {
        std::lock_guard<std::mutex> lockGuard(s_HookMutex);

        for (const auto& [lpObject, hookInfo] : s_Hooks) {
            DWORD dwOldProtect;
            if (!VirtualProtect(
                &hookInfo.VTable[SDK::Offsets::ProcessEventIdx],
                sizeof(void*),
                PAGE_EXECUTE_READWRITE,
                &dwOldProtect
            )) {
                LogError(
                    "HookManager",
                    "Failed to restore hook on: '" + hookInfo.szObjName + "'" +
                    " (VirtualProtect - E" + std::to_string(GetLastError()) + ")"
                );
                continue;
            }

            hookInfo.VTable[SDK::Offsets::ProcessEventIdx] = reinterpret_cast<void*>(hookInfo.OriginalProcessEvent);
            VirtualProtect(
                &hookInfo.VTable[SDK::Offsets::ProcessEventIdx],
                sizeof(void*),
                dwOldProtect,
                &dwOldProtect
            );

            LogMessage("HookManager", "Restored hook on: '" + hookInfo.szObjName + "'");
        }

        s_Hooks.clear();
        s_HookedVTables.clear();
    }

    void ProcessEventHooker::ListActiveHooks(void) {
        HookTable::ListActiveHooks(
            s_Hooks,
            s_HookMutex,
            "HookManager",
            "ProcessEvent",
            [](const HookData& r) -> std::string {
                return r.szObjName;
            }
        );
    }

    ProcessEvent_t ProcessEventHooker::GetHookOriginalFunctionById(
        int32_t iHookId
    ) {
        std::lock_guard<std::mutex> lockGuard(s_HookMutex);
        for (const auto& [lpObj, hookInfo] : s_Hooks) {
            if (hookInfo.iUniqueId == iHookId) {
                return hookInfo.OriginalProcessEvent;
            }
        }
        return nullptr;
    }

    ProcessEventHooker::HookData* ProcessEventHooker::GetHookById(
        int32_t iHookId
    ) {
        return HookTable::GetHookById(s_Hooks, s_HookMutex, iHookId);
    }

    ProcessEventHooker::HookData* ProcessEventHooker::GetHookByObject(
        SDK::UObject* Object
    ) {
        if (nullptr == Object) {
            return nullptr;
        }
        std::lock_guard<std::mutex> lockGuard(s_HookMutex);
        auto it = s_Hooks.find(Object);
        return (it != s_Hooks.end()) ? &it->second : nullptr;
    }

    ProcessEventHooker::HookData* ProcessEventHooker::GetHookByHookedFunction(
        HookedFn fnHook
    ) {
        return HookTable::FindIf(
            s_Hooks,
            s_HookMutex,
            [fnHook](const HookData& r) -> bool
            {
                return r.HookFn == fnHook;
            }
        );
    }

    bool ProcessEventHooker::SetHookDebugFilterAll(
        const std::string& szDebugFilter
    ) {
        return HookTable::SetHookDebugFilterAll(
            s_Hooks,
            s_HookMutex,
            szDebugFilter,
            "HookManager"
        );
    }

    bool ProcessEventHooker::SetHookDebugFilterById(
        int32_t iHookId,
        const std::string& szDebugFilter
    ) {
        return HookTable::SetHookDebugFilterById(
            s_Hooks,
            s_HookMutex,
            iHookId,
            szDebugFilter,
            "HookManager",
            HookManager::UniqueHookIdSpecial::AllHooks
        );
    }

    bool NativeHooker::SwapExecFunctionPtr(
        SDK::UFunction* lpFunc,
        void* lpNewPtr,
        void** lppOutOld
    ) {
        void** lpSlot = reinterpret_cast<void**>(&(lpFunc->ExecFunction));
        DWORD dwOldProt = 0;
        if (!VirtualProtect(
            lpSlot,
            sizeof(void*),
            PAGE_EXECUTE_READWRITE,
            &dwOldProt
        )) {
            LogError(
                "NativeHooker",
                "VirtualProtect failed: " + std::to_string(GetLastError())
            );
            return false;
        }

        void* lpPrev = *lpSlot;
        *lpSlot = lpNewPtr;

        VirtualProtect(
            lpSlot,
            sizeof(void*),
            dwOldProt,
            &dwOldProt
        );

        if (nullptr != lppOutOld) {
            *lppOutOld = lpPrev;
        }
        return true;
    }

    NativeHooker::HookEntry* NativeHooker::HookNativeFunction(
        const std::string& cszFuncNameNeedle,
        NativeHooker::NativeFunc_t fnHook,
        const std::string& cszHookName
    ) {
        if (cszFuncNameNeedle.empty() || nullptr == fnHook) {
            LogError(
                "NativeHooker",
                "Invalid parameters"
            );
            return nullptr;
        }

        SDK::UFunction* lpTargetFunc = UnrealUtils::FindFunction(cszFuncNameNeedle);
        if (nullptr == lpTargetFunc) {
            LogError(
                "NativeHooker",
                "UFunction not found: '" + cszFuncNameNeedle + "'"
            );
            return nullptr;
        }
        if (!NativeHooker::IsNative(lpTargetFunc)) {
            LogError(
                "NativeHooker",
                "Target function is not native ('" + lpTargetFunc->GetFullName() + "')"
            );
            return nullptr;
        }

        if (nullptr == lpTargetFunc->ExecFunction) {
            LogError(
                "NativeHooker",
                "Target function has null ExecFunction pointer ('" + lpTargetFunc->GetFullName() + "')"
            );
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(nh_Mutex);

        auto it = nh_Hooks.find(lpTargetFunc);
        if (it != nh_Hooks.end()) {
            LogMessage(
                "NativeHooker",
                "Skipping already hooked function: '" + cszFuncNameNeedle + "'"
            );
            return &it->second;
        }

        void* lpOld = nullptr;
        if (!SwapExecFunctionPtr(
            lpTargetFunc,
            reinterpret_cast<void*>(fnHook),
            &lpOld
        )) {
            LogError(
                "NativeHooker",
                "Function pointer swap failed for: '" + lpTargetFunc->GetFullName() + "'"
            );
            return nullptr;
        }

        // Atomic in-place construction
        std::pair<std::unordered_map<SDK::UFunction*, HookEntry>::iterator, bool> insRes =
            nh_Hooks.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(lpTargetFunc),
                std::forward_as_tuple(
                    lpTargetFunc,
                    reinterpret_cast<NativeFunc_t>(lpOld),
                    fnHook,
                    lpTargetFunc->GetFullName(),
                    cszHookName,
                    "" // no debug filter by default
                )
            );

        LogMessage(
            "NativeHooker",
            "Successfully hooked native UFunction: '" + insRes.first->second.szHookName + "'"
        );
        return &insRes.first->second;
    }

    bool NativeHooker::Restore(SDK::UFunction* lpTargetFunc) {
        if (nullptr == lpTargetFunc) {
            LogError(
                "NativeHooker",
                "Restore failed: null function pointer"
            );
            return false;
        }
        std::lock_guard<std::mutex> lock(nh_Mutex);
        auto it = nh_Hooks.find(lpTargetFunc);
        if (it == nh_Hooks.end()) {
            LogMessage(
                "NativeHooker",
                "No hook found for the given function pointer"
            );
            return false;
        }

        HookEntry& hookEntry = it->second;
        if (!SwapExecFunctionPtr(
            hookEntry.FuncObj,
            reinterpret_cast<void*>(hookEntry.OriginalFn),
            nullptr
        )) {
            LogError(
                "NativeHooker",
                "Restore failed on function '" + hookEntry.szHookName + "'"
            );
            return false;
        }
        LogMessage(
            "NativeHooker",
            "Restored hook on function '" + hookEntry.szHookName + "'"
        );
        nh_Hooks.erase(it);
        return true;
    }

    void NativeHooker::RestoreAll(void) {
        std::lock_guard<std::mutex> lock(nh_Mutex);
        for (auto& kv : nh_Hooks) {
            auto* func = kv.first;
            HookEntry& hookEntry = kv.second;
            if (!SwapExecFunctionPtr(
                func,
                reinterpret_cast<void*>(hookEntry.OriginalFn),
                nullptr
            )) {
                LogError(
                    "NativeHooker",
                    "Failed to restore original hook on function '" + hookEntry.szHookName + "'"
                );
            } else {
                LogMessage(
                    "NativeHooker",
                    "Restored hook on function '" + hookEntry.szHookName + "'"
                );
            }
        }
        nh_Hooks.clear();
    }

    NativeHooker::NativeFunc_t NativeHooker::GetOriginal(SDK::UFunction* lpTargetFunc) {
        std::lock_guard<std::mutex> lock(nh_Mutex);
        auto it = nh_Hooks.find(lpTargetFunc);
        return (it == nh_Hooks.end()) ? nullptr : it->second.OriginalFn;
    }

    void NativeHooker::ListActiveHooks(void) {
        HookTable::ListActiveHooks(
            nh_Hooks,
            nh_Mutex,
            "NativeHooker",
            "Native",
            [](const HookEntry& r) -> std::string {
                return r.szFnName;
            }
        );
    }

    NativeHooker::HookEntry* NativeHooker::GetHookById(
        int32_t iHookId
    ) {
        return HookTable::GetHookById(nh_Hooks, nh_Mutex, iHookId);
    }

    NativeHooker::HookEntry *NativeHooker::GetHookByHookedFunction(
        NativeHooker::NativeFunc_t fnHook
    ) {
        return HookTable::FindIf(
            nh_Hooks,
            nh_Mutex,
            [fnHook](const HookEntry& r) -> bool {
                return r.HookFn == fnHook;
            }
        );
    }

    bool NativeHooker::SetHookDebugFilterAll(
        const std::string& szDebugFilter
    ) {
        return HookTable::SetHookDebugFilterAll(
            nh_Hooks,
            nh_Mutex,
            szDebugFilter,
            "NativeHooker"
        );
    }

    bool NativeHooker::SetHookDebugFilterById(
        int32_t iHookId,
        const std::string& szDebugFilter
    ) {
        return HookTable::SetHookDebugFilterById(
            nh_Hooks,
            nh_Mutex,
            iHookId,
            szDebugFilter,
            "NativeHooker",
            HookManager::UniqueHookIdSpecial::AllHooks
        );
    }
}