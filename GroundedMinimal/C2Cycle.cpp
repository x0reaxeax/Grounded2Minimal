// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

/// @file   C2Cycle.cpp
/// @brief  C2Cycle w/ dumb O(n^2) cleaning logic.
///         Hope you like wasting CPU cycles, because I sure do.
/// @author x0reaxeax
///
/// Grounded2Minimal - https://github.com/x0reaxeax/Grounded2Minimal

#include "GroundedMinimal.hpp"
#include "UnrealUtils.hpp"
#include "C2Cycle.hpp"
#include "Command.hpp"

#include <mutex>
#include <iomanip>

namespace C2Cycle {
    bool GlobalC2Authority = false;
    std::atomic<bool> C2ThreadRunning{ false };
    
    static uint64_t CycleCounter = 1;

    static __forceinline void PrintItemCullInfo(
        SDK::ASpawnedItem *lpItemA,
        SDK::ASpawnedItem *lpItemB,
        float fDistance2
    ) {
        LogMessage(
            "Cull", 
            "  * " + lpItemA->GetName() 
            + " <-> " + lpItemB->GetName() 
            + " (d2 = " + std::to_string(fDistance2) + ")"
        );
    }

    void CreateC2CycleThread(
        void
    ) {
        if (C2ThreadRunning.load()) {
            LogError("C2Cycle", "C2Cycle thread is already running");
            return;
        }
        C2ThreadRunning.store(true);

        LogMessage("C2Cycle", "Starting C2Cycle thread...", true);
        std::thread([]() {
            while (C2ThreadRunning.load()) {
                std::this_thread::sleep_for(std::chrono::seconds(10));
                DisableGlobalOutput();
                Command::SubmitTypedCommand<void*>(
                    Command::CommandId::CmdIdC2Cycle,
                    nullptr
                );

                Command::WaitForCommandBufferReady();
                EnableGlobalOutput();
            }
        }).detach();
    }

    void StopC2CycleThread(void) {
        if (!C2ThreadRunning.load()) {
            LogMessage("C2Cycle", "C2Cycle thread is not running", true);
            return;
        }

        LogMessage("C2Cycle", "Stopping C2Cycle thread...", true);
        C2ThreadRunning.store(false);
        LogMessage("C2Cycle", "C2Cycle thread stopped", true);
    }

    void GameThread CullItemInstance(
        SDK::ASpawnedItem *lpItemToCull
    ) {
        if (nullptr == lpItemToCull) {
            return;
        }

        if (lpItemToCull->IsActorBeingDestroyed() || lpItemToCull->bActorIsBeingDestroyed) {
            return;
        }

        SDK::FVector vItemLocation = lpItemToCull->K2_GetActorLocation();
        LogMessage("Cull", "Culling item: " + lpItemToCull->GetName() + 
                  " at location: (" + std::to_string(vItemLocation.X) + 
                  ", " + std::to_string(vItemLocation.Y) + 
                  ", " + std::to_string(vItemLocation.Z) + ")");

        lpItemToCull->SetLifeSpan(0.01f);
    }

    void CullItemByItemIndex(
        int32_t iItemIndex
    ) {
        if (!UnrealUtils::IsPlayerHostAuthority()) {
            LogError("Cull", "Culling can only be executed by the host");
            return;
        }
        
        SDK::ASpawnedItem *lpItemToCull = UnrealUtils::GetSpawnedItemByIndex(iItemIndex);
        if (nullptr == lpItemToCull) {
            LogError("Cull", "Invalid item index: " + std::to_string(iItemIndex));
            return;
        }

        if (lpItemToCull->bActorIsBeingDestroyed) {
            LogError("Cull", "Item is already being destroyed: " + lpItemToCull->GetName());
            return; // Item is already being destroyed
        }

        BufferParamsCullItem *lpParams = new BufferParamsCullItem{
            .lpItemInstance = lpItemToCull
        };

        Command::SubmitTypedCommand(
            Command::CommandId::CmdIdCullItemInstance,
            lpParams
        );
    }

    void CullAllItemInstances(
        std::string szTargetItemTypeName
    ) {
        if (szTargetItemTypeName.empty()) {
            LogError("Cull", "Target object name is empty");
            return;
        }

        for (int32_t i = 0; i < SDK::ASpawnedItem::GObjects->Num(); i++) {
            SDK::UObject *lpObj = SDK::ASpawnedItem::GObjects->GetByIndex(i);
            if (nullptr == lpObj) {
                continue;
            }

            if (!lpObj->IsA(SDK::ASpawnedItem::StaticClass())) {
                continue;
            }

            SDK::ASpawnedItem *lpSpawnedItem = static_cast<SDK::ASpawnedItem*>(lpObj);

            std::string szFullObjectName = lpSpawnedItem->GetFullName();
            if (!StringContainsCaseInsensitive(
                szFullObjectName, 
                szTargetItemTypeName
            )) {
                continue;
            }

            // Don't cull if item FVector coordinates are all 0
            SDK::FVector vItemLocation = lpSpawnedItem->K2_GetActorLocation();
            if (vItemLocation.X == 0.0f && vItemLocation.Y == 0.0f && vItemLocation.Z == 0.0f) {
                continue;
            }

            //CullItemInstance(static_cast<SDK::ASpawnedItem*>(lpObj));
            BufferParamsCullItem *lpParams = new BufferParamsCullItem{
                .lpItemInstance = lpSpawnedItem
            };

            Command::SubmitTypedCommand(
                Command::CommandId::CmdIdCullItemInstance, 
                lpParams
            );
        }
    }

    void CollectTargetFoodItemCandidates(
        void
    ) {
        std::lock_guard<std::mutex> lockGuard(ListMutex);
        CollectedItems.clear();

        for (int32_t i = 0; i < SDK::ASpawnedItem::GObjects->Num(); i++) {
            SDK::UObject *lpObj = SDK::ASpawnedItem::GObjects->GetByIndex(i);
            if (nullptr == lpObj) {
                continue;
            } 
            
            if (!lpObj->IsA(SDK::ASpawnedItem::StaticClass())) {
                continue;
            }

            // check if the full name is within the target names
            bool bFoundMatch = false;
            //int32_t iMatchIndex = -1;
            for (const auto& szTargetObjectName : TargetFoodItemNames) {
                if (lpObj->GetFullName().contains(szTargetObjectName)) {
                    bFoundMatch = true;
                    break; // match
                }
            }

            if (!bFoundMatch) {
                continue; // Skip this object if it doesn't match any target name
            }

            // Cast the object to ASpawnedItem
            SDK::ASpawnedItem *lpSpawnedItem = static_cast<SDK::ASpawnedItem*>(lpObj);
            if (lpSpawnedItem->bActorIsBeingDestroyed) {
                continue;
            }

            // Check if the item is within CLUSTER_DEFAULT_RADIUS of the hotspot coordinates
            for (const auto& vHotSpot : C2Cycle::HotSpotCoordinates) {
                const float fDistance2 = DistSquared(lpSpawnedItem->K2_GetActorLocation(), vHotSpot);

                if (fDistance2 > CLUSTER_DEFAULT_RADIUS * CLUSTER_DEFAULT_RADIUS) {
                    continue; // Skip items that are too far away
                }

                LogMessage("Collect", "  * Found item: " + lpSpawnedItem->GetName());
                CollectedItems.emplace_back(lpSpawnedItem);
                break;
            }
        }
        
        LogMessage(
            "Collect", 
            "Total collected: " + std::to_string(CollectedItems.size()) + " items."
        );
        LogMessage("Collect", "=========================================================");
    }

    static void GameThread CullClumpedItems(
        float fClusterRadius = CLUSTER_DEFAULT_RADIUS
    ) {
        std::lock_guard<std::mutex> lockGuard(ListMutex);
        if (CollectedItems.empty()) {
            LogMessage("Cull", "No items to cull");
            return;
        }

        uint64_t qwTotalDestroyed = 0;
        const float cfRadiusSquared = fClusterRadius * fClusterRadius;
        
        for (size_t i = 0; i < CollectedItems.size(); ++i) {
            SDK::ASpawnedItem *lpItemA = CollectedItems[i];
            if (nullptr == lpItemA || lpItemA->bActorIsBeingDestroyed) {
                continue;
            }

            int32_t iNearbyItemCount = 0;
            std::vector<SDK::ASpawnedItem *> vNearbyItemCluster;

            const SDK::FVector vLocA = lpItemA->K2_GetActorLocation();
            for (size_t j = i + 1; j < CollectedItems.size(); ++j) {
                SDK::ASpawnedItem *lpItemB = CollectedItems[j];
                if (nullptr == lpItemB || lpItemB->bActorIsBeingDestroyed) {
                    continue;
                }

                const float cfDistance2 = DistSquared(vLocA, lpItemB->K2_GetActorLocation());
                if (cfDistance2 <= cfRadiusSquared) {
                    PrintItemCullInfo(lpItemA, lpItemB, cfDistance2);
                    vNearbyItemCluster.push_back(lpItemB);
                    ++iNearbyItemCount;
                }
            }

            if (iNearbyItemCount >= CLUSTER_MAX_ITEMS) {
                // Keep only lpItemA and cull all nearby items
                for (size_t k = 0; k < vNearbyItemCluster.size(); ++k) {
                    SDK::ASpawnedItem *lpItemToCull = vNearbyItemCluster[k];
                    if (nullptr != lpItemToCull && !lpItemToCull->bActorIsBeingDestroyed) {
                        CullItemInstance(lpItemToCull);
                        qwTotalDestroyed++;
                        vNearbyItemCluster[k] = nullptr; // Mark as culled
                    }
                }
            }
        }
        
        LogMessage("Cull", "Total items destroyed: " + std::to_string(qwTotalDestroyed));
        LogMessage("Cull", "=========================================================");
        CollectedItems.clear();
    }

    void GameThread C2Cycle(void) {
        if (!UnrealUtils::IsPlayerHostAuthority()) {
            LogError("C2", "C2Cycle can only be executed by the host");
            return;
        }

        LogMessage("C2", "Cleanup cycle " + std::to_string(CycleCounter) + " started");

        // If this output is enabled and there's a f ton of items, the game will straight up freeze for a few seconds.
        // Leaving it disabled by default.
        bool bGlobalOutputEnabled = GlobalOutputEnabled;
        DisableGlobalOutput();
        LogMessage("Collect", "Collecting items...");
        CollectTargetFoodItemCandidates();
        CullClumpedItems();
        if (bGlobalOutputEnabled) {
            EnableGlobalOutput();
        }
        LogMessage("C2", "Cleanup cycle " + std::to_string(CycleCounter++) + " completed");

        if (bGlobalOutputEnabled) {
            // Redraw interpreter input symbol, the only direct std::cout allowed overall
            std::cout << "$: " << std::flush;
        }
    }
}