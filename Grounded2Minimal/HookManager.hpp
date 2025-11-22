// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#ifndef _GROUNDED2_HOOK_MANAGER_HPP
#define _GROUNDED2_HOOK_MANAGER_HPP

#include "Grounded2Minimal.hpp"
#include "CoreUtils.hpp"

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <iostream>
#include <mutex>
#include <vector>
#include <algorithm>
#include <atomic>


#define NATIVEHOOK          void
#define PROCESSEVENTHOOK    void

namespace HookManager {
    enum UniqueHookIdSpecial : int32_t {
        Invalid = -1,
        AllHooks = 0
    };
    
    enum class HookType : uint8_t {
        Invalid = 0,
        ProcessEvent = 1,
        NativeFunction = 2
    };

    class ProcessEventHooker {
    public:
        using HookedFn = void(__fastcall*)(SDK::UObject*, SDK::UFunction*, void*);

        struct HookData {
            friend class ProcessEventHooker;

        public:
            ProcessEvent_t      OriginalFn;                                     // Original ProcessEvent function pointer                   [safe to read]
            HookedFn            HookFn;                                         // Pointer to the hook function                             [safe to read]
            std::string         szHookName;                                     // Friendly hook name                                       [safe to read/write]
            std::string         szDebugFilter;                                  // Debug filter needle (case-insensitive, substring match)  [safe to read/write]
            std::atomic<bool>   bDebugFilterEnabled{ false };                   // Debug filter enabled flag                                [safe to read/write]
            int32_t             iUniqueId = CoreUtils::GenerateUniqueId();      // Unique hook ID                                           [safe to read]

            HookData(
                void** _VTable,
                ProcessEvent_t _OriginalFn,
                HookedFn _HookFn,
                std::string _szObjName,
                std::string _szHookName,
                std::string _szDebugFilter
            )
                : VTable(_VTable)
                , OriginalFn(_OriginalFn)
                , HookFn(_HookFn)
                , szObjName(std::move(_szObjName))
                , szHookName(std::move(_szHookName))
                , szDebugFilter(std::move(_szDebugFilter)) {
            }

        private:
            void**              VTable;                                         // Pointer to the object's vtable
            //HookedFn            HookFn;                                         // Pointer to the hook function
            std::string         szObjName;                                      // Internal FullName of the hooked object
        };

        static bool InstallHook(
            SDK::UObject* Object,
            HookedFn HookFn,
            const std::string &cszHookName
        );
        static void RestoreHooks(void);
        static void ListActiveHooks(void);
        static ProcessEvent_t GetHookOriginalFunctionById(int32_t iHookId);
        static HookData* GetHookById(int32_t iHookId);
        static HookData* GetHookByObject(SDK::UObject* Object);
        static HookData* GetHookByHookedFunction(HookedFn fnHook);

        static bool SetHookDebugFilterAll(
            const std::string& szDebugFilter
        );

        static bool SetHookDebugFilterById(
            int32_t iHookId,
            const std::string& szDebugFilter
        );

    private:
        inline static std::unordered_map<SDK::UObject*, HookData> s_Hooks;
        inline static std::unordered_set<void**> s_HookedVTables;
        inline static std::mutex s_HookMutex;
    };

    class NativeHooker {
    public:
        using NativeFunc_t = void(*)(SDK::UObject* lpObject, void *lpFFrame, void* lpResult);

        struct HookEntry {
            friend class NativeHooker;
            NativeFunc_t       OriginalFn;                                     // Original function pointer [safe to read]
            NativeFunc_t       HookFn;                                         // Pointer to hook function  [safe to read]
            std::string        szHookName;                                     // Friendly hook name        [safe to read/write]
            std::string        szDebugFilter;                                  // Debug filter needle       [safe to read/write]
            std::atomic<bool>  bDebugFilterEnabled{ false };                   // Debug filter enabled flag [safe to read/write]
            int32_t            iUniqueId = CoreUtils::GenerateUniqueId();      // Unique hook ID            [safe to read]

            // Constructor is public so we can in-place construct inside unordered_map
            HookEntry(
                SDK::UFunction* _FuncObj,
                NativeFunc_t _OriginalFn,
                NativeFunc_t _HookFn,
                std::string _szFnName,
                std::string _szHookName,
                std::string _szDebugFilter
            )
                : FuncObj(_FuncObj)
                , OriginalFn(_OriginalFn)
                , HookFn(_HookFn)
                , szFnName(std::move(_szFnName))
                , szHookName(_szHookName.empty() ? szFnName : std::move(_szHookName))
                , szDebugFilter(std::move(_szDebugFilter)) {
            }

        private:
            SDK::UFunction*    FuncObj;                                        // Target native function object
            //NativeFunc_t       HookFn;                                         // Pointer to hook function
            std::string        szFnName;                                       // Internal FullName of target hooked function
        };

        // Hook by short name (first match wins)
        static HookEntry* HookNativeFunction(
            const std::string& cszFuncNameNeedle,
            NativeFunc_t fnHook,
            const std::string &cszHookName
        );

        static bool Restore(SDK::UFunction* lpTargetFunc);
        static void RestoreAll(void);
        static NativeFunc_t GetOriginal(SDK::UFunction* lpTargetFunc);
        static void ListActiveHooks(void);
        static HookEntry* GetHookById(int32_t iHookId);
        static HookEntry* GetHookByHookedFunction(NativeFunc_t fnHook);

        static bool SetHookDebugFilterAll(
            const std::string& szDebugFilter
        );

        static bool SetHookDebugFilterById(
            int32_t iHookId,
            const std::string& szDebugFilter
        );

    private:
        static inline std::unordered_map<SDK::UFunction*, HookEntry> nh_Hooks;
        static inline std::mutex nh_Mutex;

        static inline bool IsNative(const SDK::UFunction* lpcTargetFunc) {
            constexpr uint32_t FUNC_Native = 0x00000400;
            return lpcTargetFunc && (lpcTargetFunc->FunctionFlags & FUNC_Native);
        };

        static bool SwapExecFunctionPtr(
            SDK::UFunction* lpFunc,
            void* lpNewPtr,
            void** lppOutOld
        );
    };

    static HookType GetHookTypeById(
        int32_t iHookId
    ) {
        if (nullptr != ProcessEventHooker::GetHookById(iHookId)) {
            return HookType::ProcessEvent;
        }
        if (nullptr != NativeHooker::GetHookById(iHookId)) {
            return HookType::NativeFunction;
        }

        return HookType::Invalid;
    }
}

#endif // _GROUNDED2_HOOK_MANAGER_HPP