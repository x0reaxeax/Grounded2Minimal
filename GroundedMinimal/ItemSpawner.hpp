// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#ifndef _GROUNDED_ITEM_SPAWNER_HPP
#define _GROUNDED_ITEM_SPAWNER_HPP

#include "GroundedMinimal.hpp"

#include <string>

namespace ItemSpawner {
    extern bool GlobalCheatMode;

    struct BufferParamsSpawnItem {
        int32_t iPlayerId;          // ID of the player to spawn items for, check GUI when in doubt
        std::string szItemName;     // Name of the item to spawn, check GUI when in doubt
        std::string szDataTableName;// Name of the data table to use for spawning items.... guess what..
        int32_t iCount;             // Number of items to spawn
    };

    struct SafeChatMessageData {
        int32_t iSenderId;
        std::string szMessage;
        std::string szSenderName;
        SDK::FColor Color;
        SDK::EChatBoxMessageType Type;
    };

    bool GiveItemToPlayer(
        int32_t iPlayerId,
        const std::string& szItemName,
        const std::string& szDataTableName,
        int32_t iItemCount = 1
    );

    // "hopefully thread-safe"(TM)
    void EvaluateChatSpawnRequestSafe(
        SafeChatMessageData* lpMessageData
    );

} // namespace ItemSpawner

#endif // _GROUNDED_ITEM_SPAWNER_HPP