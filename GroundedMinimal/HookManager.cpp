// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#include "HookManager.hpp"
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

    s_Hooks[lpObject] = { lpVTable, fnOriginal, fnHook };
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
        if (!VirtualProtect(&hookInfo.VTable[SDK::Offsets::ProcessEventIdx], sizeof(void*), PAGE_EXECUTE_READWRITE, &dwOldProtect)) {
            continue;
        }

        hookInfo.VTable[SDK::Offsets::ProcessEventIdx] = reinterpret_cast<void*>(hookInfo.OriginalFn);
        VirtualProtect(&hookInfo.VTable[SDK::Offsets::ProcessEventIdx], sizeof(void*), dwOldProtect, &dwOldProtect);

        LogMessage("HookManager", "Restored hook on: " + lpObject->GetFullName());
    }

    s_Hooks.clear();
    s_HookedVTables.clear();
}

void HookManager::ListActiveHooks(void) {
    std::lock_guard<std::mutex> lockGuard(s_HookMutex);
    LogMessage("HookManager", "Active Hooks: " + std::to_string(s_Hooks.size()));
    for (const auto& [lpObj, _] : s_Hooks) {
        LogMessage("HookManager", "  * " + lpObj->GetFullName());
    }
}