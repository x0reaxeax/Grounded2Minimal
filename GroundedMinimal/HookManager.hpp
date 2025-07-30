// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#ifndef _GROUNDED_HOOK_MANAGER_HPP
#define _GROUNDED_HOOK_MANAGER_HPP

#include "GroundedMinimal.hpp"

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <iostream>
#include <mutex>
#include <vector>
#include <algorithm>

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
    };

    static std::unordered_map<SDK::UObject*, HookData> s_Hooks;
    static std::unordered_set<void**> s_HookedVTables;
    static std::mutex s_HookMutex;
};


#endif // _GROUNDED_HOOK_MANAGER_HPP