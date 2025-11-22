#include "PlayerCache.hpp"
#include "UnrealUtils.hpp"

#include <thread>

namespace PlayerCache {
    struct CachedData g_CachedData{};

    CachedPlayer* EnsureCachedPlayer(
        SDK::APlayerState* lpPlayerState
    ) {
        if (nullptr == lpPlayerState) {
            return nullptr;
        }

        if (INVALID_PLAYER_ID == lpPlayerState->PlayerId) {
            return nullptr;
        }

        auto& map = g_CachedData.CachedPlayers;

        if (auto it = map.find(lpPlayerState); it != map.end()) {
            return &it->second;
        }

        auto [iter, inserted] = map.try_emplace(lpPlayerState);
        iter->second.PlayerState = lpPlayerState;
        return &iter->second;
    }

    CachedPlayer* AttachCachedPlayerData(
        SDK::APlayerState* lpPlayerState
    ) {
        if (nullptr == lpPlayerState) {
            return nullptr;
        }

        if (INVALID_PLAYER_ID == lpPlayerState->PlayerId) {
            return nullptr;
        }

        CachedPlayer* lpCachedPlayer = EnsureCachedPlayer(lpPlayerState);
        if (nullptr == lpCachedPlayer) {
            return nullptr;
        }
        
        int32_t iMaxRetries = 20;

        do {
            lpCachedPlayer->AssociatedPartyComponent = UnrealUtils::FindLocalPlayerParty(
                lpPlayerState->PlayerId
            );
            lpCachedPlayer->SurvivalPlayerCharacter = UnrealUtils::GetSurvivalPlayerCharacterById(
                lpPlayerState->PlayerId
            );
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        } while (
            (
                nullptr == lpCachedPlayer->AssociatedPartyComponent
                || 
                nullptr == lpCachedPlayer->SurvivalPlayerCharacter
            ) 
            && --iMaxRetries > 0
        );

        LogMessage(
            "PlayerCache",
            "Attached cached data for player ID "
            + std::to_string(lpPlayerState->PlayerId),
            true
        );

        return lpCachedPlayer;
    }

    CachedPlayer* GetCachedPlayer(
        SDK::APlayerState* lpPlayerState
    )
    {
        if (nullptr == lpPlayerState) {
            return nullptr;
        }

        auto& map = g_CachedData.CachedPlayers;
        auto it = map.find(lpPlayerState);
        if (it == map.end()) {
            return nullptr;
        }

        LogMessage(
            "PlayerCache",
            "Retrieving cached player state for player ID "
            + std::to_string(lpPlayerState->PlayerId),
            true
        );

        return &it->second;
    }

    CachedPlayer* GetCachedPlayerById(int32_t iPlayerId)
    {
        if (INVALID_PLAYER_ID == iPlayerId) {
            return nullptr;
        }

        for (auto* lpPlayerState : g_CachedData.Players) {
            if (!lpPlayerState) {
                continue;
            }

            if (lpPlayerState->PlayerId == iPlayerId) {
                LogMessage(
                    "PlayerCache",
                    "Retrieving cached player state for player ID "
                    + std::to_string(iPlayerId),
                    true
                );
                return GetCachedPlayer(lpPlayerState);
            }
        }

        return nullptr;
    }

    void BuildPlayerCache(void) {
        g_CachedData.CachedPlayers.clear();

        for (auto* lpPlayerState : g_CachedData.Players) {
            if (nullptr == lpPlayerState) {
                continue;
            }

            if (lpPlayerState->PlayerId == INVALID_PLAYER_ID) {
                continue;
            }

            AttachCachedPlayerData(lpPlayerState);
        }

        LogMessage(
            "CacheControl",
            "Built player cache for " + std::to_string(g_CachedData.Players.size()) + " players"
        );
    }

    void BuildPlayerCache(
        std::vector<SDK::APlayerState*> *vPlayerStates
    ) {
        if (nullptr == vPlayerStates) {
            return;
        }

        if (vPlayerStates->empty()) {
            return;
        }

        LogMessage(
            "CacheControl",
            "Building player cache from existing player states"
        );

        g_CachedData.Players = *vPlayerStates;
        BuildPlayerCache();
    }

    void ClearPlayerCache(void) {
        g_CachedData.CachedPlayers.clear();

        LogMessage(
            "CacheControl",
            "Cleared player cache",
            true
        );
    }

    void RemoveCachedPlayer(
        SDK::APlayerState* lpPlayerState
    ) {
        if (nullptr != lpPlayerState) {
            g_CachedData.CachedPlayers.erase(lpPlayerState);

            LogMessage(
                "CacheControl",
                "Removed cached data for player ID "
                + std::to_string(lpPlayerState->PlayerId),
                true
            );
        }
    }
} // namespace PlayerCache