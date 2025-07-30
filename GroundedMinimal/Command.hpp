// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#ifndef _GROUNDED_MINIMAL_COMMAND_HPP
#define _GROUNDED_MINIMAL_COMMAND_HPP

#include <cstdint>
#include <string>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

namespace Command {
    enum class Status : uint32_t { // woah, you can do this in cpp?? jk, i hate this fucking language
        CmdStatusIdle,
        CmdStatusReady,
        CmdStatusInvalid = 0xFF
    };

    enum class CommandId : uint16_t {
        CmdIdNone = 0,
        CmdIdSpawnItem,
        CmdIdEnumPlayers,
        CmdIdCullItemInstance,
        CmdIdC2Cycle,
        CmdIdSummon,
        CmdIdCheatManagerExecute,
        CmdIdMax
    };

    struct CommandBuffer {
        CommandId Id;
        void* Params;
        std::function<void(void*)> Deleter = nullptr;
    };

    extern std::mutex CommandBufferMutex;
    extern std::condition_variable CommandBufferCondition;

    extern CommandBuffer GameCommandBuffer;
    extern std::atomic<bool> CommandBufferCookedForExecution; // Made atomic

    template <typename T>
    inline void SubmitTypedCommand(
        CommandId Id,
        T* Params
    ) {
        std::unique_lock<std::mutex> lock(CommandBufferMutex);
        
        // Wait until buffer is free while holding the lock
        CommandBufferCondition.wait(lock, []() {
            return !CommandBufferCookedForExecution.load();
        });

        GameCommandBuffer.Id = Id;
        GameCommandBuffer.Params = static_cast<void*>(Params);
        if (nullptr != Params) {
            GameCommandBuffer.Deleter = [](void* ptr) {
                delete static_cast<T*>(ptr);
            };
        }
        CommandBufferCookedForExecution.store(true);
        
        lock.unlock();
        CommandBufferCondition.notify_one();
    }

    void WaitForCommandBufferReady(void);
    void ProcessCommands(void);
}

#endif // _GROUNDED_MINIMAL_COMMAND_HPP