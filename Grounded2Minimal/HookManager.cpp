// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#include "HookManager.hpp"
#include "UnrealUtils.hpp"
#include <Windows.h>

std::unordered_map<SDK::UObject*, HookManager::HookData> HookManager::s_Hooks;
std::unordered_set<void**> HookManager::s_HookedVTables;
std::mutex HookManager::s_HookMutex;

bool HookManager::InstallHook(
    SDK::UObject* lpObject,
    HookedFn fnHook,
    ProcessEvent_t* lpOutOriginal
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

    // Store object name for safe logging later
    HookData hookData{ lpVTable, fnOriginal, fnHook, lpObject->GetFullName() };
    s_Hooks[lpObject] = std::move(hookData);
    s_HookedVTables.insert(lpVTable);

    if (nullptr != lpOutOriginal) {
        *lpOutOriginal = fnOriginal;
    }

    LogMessage(
        "HookManager",
        "Hook installed on: " + lpObject->GetName()
    );
    return true;
}

void HookManager::RestoreHooks(void) {
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

        hookInfo.VTable[SDK::Offsets::ProcessEventIdx] = reinterpret_cast<void*>(hookInfo.OriginalFn);
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

void HookManager::ListActiveHooks(void) {
    std::lock_guard<std::mutex> lockGuard(s_HookMutex);
    LogMessage("HookManager", "Active Hooks: " + std::to_string(s_Hooks.size()));
    for (const auto& [lpObj, hookInfo] : s_Hooks) {
        LogMessage("HookManager", "  * " + hookInfo.szObjName);
    }
}

namespace NativeHooker {

    static std::unordered_map<SDK::UFunction*, HookEntry> g_Hooks;
    static std::mutex g_Mutex;

    static bool IsNative(const SDK::UFunction* lpcTargetFunc) {
        constexpr uint32_t FUNC_Native = 0x00000400;
        return lpcTargetFunc && (lpcTargetFunc->FunctionFlags & FUNC_Native);
    }

    static bool SwapExecFunctionPtr(
        SDK::UFunction* lpFunc, 
        void* lpNewPtr, 
        void** lppOutOld
    ) {
        // Treat the member as a raw pointer slot
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

    HookEntry* HookNativeFunction(
        const std::string& cszNeedle, 
        NativeFunc_t fnHook, 
        NativeFunc_t* pfnOutOriginal
    ) {
        if (cszNeedle.empty() || nullptr == fnHook) {
            LogError(
                "NativeHooker", 
                "Invalid parameters"
            );
            return nullptr;
        }

        SDK::UFunction* lpTargetFunc = UnrealUtils::FindFunction(cszNeedle);
        if (nullptr == lpTargetFunc) {
            LogError(
                "NativeHooker", 
                "UFunction not found: '" + cszNeedle + "'"
            );
            return nullptr;
        }
        if (!IsNative(lpTargetFunc)) {
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

        std::lock_guard<std::mutex> lock(g_Mutex);

        auto it = g_Hooks.find(lpTargetFunc);
        if (it != g_Hooks.end()) {
            LogMessage(
                "NativeHooker", 
                "Skipping already hooked function: '" + it->second.Name + "'"
            );
            if (nullptr != pfnOutOriginal) {
                *pfnOutOriginal = it->second.Original;
            }
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

        HookEntry hookEntry{
            .FuncObj = lpTargetFunc,
            .Original = reinterpret_cast<NativeFunc_t>(lpOld),
            .Hook = fnHook,
            .Name = lpTargetFunc->GetFullName()
        };
        auto [insIt, _] = g_Hooks.emplace(lpTargetFunc, hookEntry);

        if (nullptr != pfnOutOriginal) {
            *pfnOutOriginal = hookEntry.Original;
        }
        LogMessage(
            "NativeHooker", 
            "Successfully hooked native UFunction: '" + hookEntry.Name + "'"
        );
        return &insIt->second;
    }

    bool Restore(SDK::UFunction* lpTargetFunc) {
        if (nullptr == lpTargetFunc) {
            LogError(
                "NativeHooker", 
                "Restore failed: null function pointer"
            );
            return false;
        }
        std::lock_guard<std::mutex> lock(g_Mutex);
        auto it = g_Hooks.find(lpTargetFunc);
        if (it == g_Hooks.end()) {
            LogMessage(
                "NativeHooker", 
                "No hook found for the given function pointer"
            );
            return false;
        }

        HookEntry& hookEntry = it->second;
        if (!SwapExecFunctionPtr(
            hookEntry.FuncObj, 
            reinterpret_cast<void*>(hookEntry.Original), 
            nullptr
        )) {
            LogError(
                "NativeHooker", 
                "Restore failed on function '" + hookEntry.Name + "'"
            );
            return false;
        }
        LogMessage(
            "NativeHooker", 
            "Restored hook on function '" + hookEntry.Name + "'"
        );
        g_Hooks.erase(it);
        return true;
    }

    void RestoreAll(void) {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (auto& [func, hookEntry] : g_Hooks) {
            if (!SwapExecFunctionPtr(
                func, 
                reinterpret_cast<void*>(hookEntry.Original), 
                nullptr
            )) {
                LogError(
                    "NativeHooker", 
                    "Failed to restore original hook on function '" + hookEntry.Name + "'"
                );
            } else {
                LogMessage(
                    "NativeHooker", 
                    "Restored hook on function '" + hookEntry.Name + "'"
                );
            }
        }
        g_Hooks.clear();
    }

    NativeFunc_t GetOriginal(SDK::UFunction* lpTargetFunc) {
        std::lock_guard<std::mutex> lock(g_Mutex);
        auto it = g_Hooks.find(lpTargetFunc);
        return (it == g_Hooks.end()) ? nullptr : it->second.Original;
    }
}