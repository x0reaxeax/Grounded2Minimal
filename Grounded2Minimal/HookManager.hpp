// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#ifndef _GROUNDED2_HOOK_MANAGER_HPP
#define _GROUNDED2_HOOK_MANAGER_HPP

#include "Grounded2Minimal.hpp"

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <iostream>
#include <mutex>
#include <vector>
#include <algorithm>

typedef void (*ProcessEvent_t)(const SDK::UObject *, SDK::UFunction *, LPVOID);

class HookManager {
public:
    using HookedFn = void(__fastcall*)(SDK::UObject*, SDK::UFunction*, void*);

    static bool InstallHook(
        SDK::UObject* Object,
        HookedFn HookFn,
        ProcessEvent_t* OutOriginal = nullptr
    );
    static void RestoreHooks(void);
    static void ListActiveHooks(void);

private:
    struct HookData {
        void** VTable;
        ProcessEvent_t OriginalFn;
        HookedFn HookFn;
        std::string szObjName; // store name for safe logging
    };

    static std::unordered_map<SDK::UObject*, HookData> s_Hooks;
    static std::unordered_set<void**> s_HookedVTables;
    static std::mutex s_HookMutex;
};

namespace NativeHooker {

    using NativeFunc_t = void(*)(SDK::UObject* lpObject, void *lpFFrame, void* lpResult);
    
    struct HookEntry {
        SDK::UFunction*  FuncObj;
        NativeFunc_t     Original;
        NativeFunc_t     Hook;
        std::string      Name;
    };

    // Hook by short name (first match wins, flawless victory, fatality)
    HookEntry* HookNativeFunction(
        const std::string& cszNeedle,
        NativeFunc_t fnHook, 
        NativeFunc_t* pfnOutOriginal = nullptr
    );

    bool Restore(SDK::UFunction* lpTargetFunc);
    void RestoreAll(void);
    NativeFunc_t GetOriginal(SDK::UFunction* lpTargetFunc);
}


#endif // _GROUNDED_HOOK_MANAGER_HPP