// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

/// @file   C2Cycle.hpp
/// @brief  C2Cycle w/ dumb O(n^2) cleaning logic.
///         Hope you like wasting CPU cycles, because I sure do.
/// @author x0reaxeax
///
/// GroundedMinimal - https://github.com/x0reaxeax/GroundedMinimal

#ifndef _GROUNDED_COLLECT_CULL_CYCLE_HPP
#define _GROUNDED_COLLECT_CULL_CYCLE_HPP

#include "GroundedMinimal.hpp"

#include <cstdint>
#include <mutex>
#include <vector>

#define CLUSTER_MAX_ITEMS       4       // how many items is that?
#define CLUSTER_DEFAULT_RADIUS  3600.f

namespace C2Cycle {
    // Determines if any player can execute C2Cycle via a chat command, great idea, right? xd
    extern bool GlobalC2Authority; 

    extern std::atomic<bool> C2ThreadRunning;

    struct BufferParamsCullItem {
        SDK::ASpawnedItem* lpItemInstance;
    };

    static inline std::vector<SDK::ASpawnedItem*> CollectedItems;
    static inline std::mutex ListMutex;

    // Hotspots of clumped ant hoarded piles of junk, these are to be C2Cycled(TM pending)
    static inline const SDK::FVector HotSpotCoordinates[] = {
        SDK::FVector(-79856.8750f, -50729.2148f, 1383.6895f),       // Garbage Bags
        SDK::FVector(-76211.1719f, -60515.4375f, -586.0028f),       // Black Ant Hill
        SDK::FVector(-76162.9688f, -52019.0547f, -3374.2712f),      // Black Ant Hill (Depths)
        SDK::FVector(-9618.6758f, -12242.2109f, 18.5499f),          // Red Ant Hill
        SDK::FVector(69901.6953f, 43789.5703f, 4818.2900f),         // Fire Ant Hill (Lawnmower)
        SDK::FVector(52790.2266f, 40769.7930f, 2314.1882f),         // Fire Ant Hill (Jungle Anthill)
        SDK::FVector(53103.6133f, 40397.4922f, 2320.2317f)          // Fire Ant Hill (Depths)
    };

    // Names of clumped ant hoarded food items to be C2Cycled(TM pending)
    static inline const std::string TargetFoodItemNames[] = {
        "BP_World_Food_Apple_C",
        "BP_World_Food_Hot_Dog_C",
        "BP_World_Food_Cookie_Sandwich_C",
        "BP_World_Food_Rotten_C",
        "BP_World_HoneyDew_C",
        "BP_World_Food_Donut_C"
    };

    static inline float DistSquared(
        const SDK::FVector& A,
        const SDK::FVector& B
    ) {
        const float DX = A.X - B.X;
        const float DY = A.Y - B.Y;
        const float DZ = A.Z - B.Z;
        return (DX * DX) + (DY * DY) + (DZ * DZ);
    }

    void CullItemByItemIndex(
        int32_t ItemIndex
    );

    void CullAllItemInstances(
        std::string TargetItemTypeName
    );

    void CreateC2CycleThread(void);
    void StopC2CycleThread(void);

    void GameThread CullItemInstance(
        SDK::ASpawnedItem *lpItemToCull
    );

    void GameThread C2Cycle(void);

} // namespace C2Cycle

#endif // ! _GROUNDED_COLLECT_CULL_CYCLE_HPP