#include "PlayerCache.hpp"
#include "UnrealUtils.hpp"

#include <algorithm>

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
        
        bool bUpdated = false;
        if (!UnrealUtils::IsValidUObject(lpCachedPlayer->AssociatedPartyComponent)) {
            lpCachedPlayer->AssociatedPartyComponent = nullptr;
            lpCachedPlayer->AssociatedPartyComponent = UnrealUtils::FindLocalPlayerParty(
                lpPlayerState->PlayerId
            );
            bUpdated = true;
        }

        if (
            UnrealUtils::IsValidUObject(lpCachedPlayer->AssociatedPartyComponent)
            && !UnrealUtils::IsValidUObject(lpCachedPlayer->SurvivalPlayerCharacter)
        ) {
            lpCachedPlayer->SurvivalPlayerCharacter = nullptr;
            lpCachedPlayer->SurvivalPlayerCharacter = UnrealUtils::GetSurvivalPlayerCharacterById(
                lpPlayerState->PlayerId
            );
            bUpdated = true;
        }

        if (bUpdated) {
            LogMessage(
                "PlayerCache",
                "Refreshed cached data for player ID "
                + std::to_string(lpPlayerState->PlayerId),
                true
            );
        }

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
            int32_t iLocalPlayerId = UnrealUtils::GetLocalPlayerId(true);
            return (INVALID_PLAYER_ID != iLocalPlayerId)
                ? GetCachedPlayerById(iLocalPlayerId)
                : nullptr;
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
        auto& map = g_CachedData.CachedPlayers;
        for (auto it = map.begin(); it != map.end();) {
            if (
                !UnrealUtils::IsValidUObject(it->first)
                || std::find(
                    g_CachedData.Players.begin(),
                    g_CachedData.Players.end(),
                    it->first
                ) == g_CachedData.Players.end()
            ) {
                it = map.erase(it);
            } else {
                ++it;
            }
        }

        for (auto* lpPlayerState : g_CachedData.Players) {
            if (!UnrealUtils::IsValidUObject(lpPlayerState)) {
                continue;
            }

            if (lpPlayerState->PlayerId == INVALID_PLAYER_ID) {
                continue;
            }

            AttachCachedPlayerData(lpPlayerState);
        }

        LogMessage(
            "CacheControl",
            "Refreshed player cache for " + std::to_string(g_CachedData.Players.size()) + " players",
            true
        );
    }

    void BuildPlayerCache(
        std::vector<SDK::APlayerState*> *vPlayerStates
    ) {
        if (nullptr == vPlayerStates) {
            return;
        }

        if (vPlayerStates->empty()) {
            g_CachedData.Players.clear();
            ClearPlayerCache();
            return;
        }

        LogMessage(
            "CacheControl",
            "Refreshing player cache from existing player states",
            true
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

    void InvalidateCache(void) {
        ClearPlayerCache();
        g_CachedData.Players.clear();
        g_CachedData.LocalPlayerId = INVALID_PLAYER_ID;
        g_CachedData.WorldInstance = nullptr;
        
        LogMessage(
            "CacheControl",
            "Invalidated cached data",
            true
        );
    }
} // namespace PlayerCache