#ifndef _GROUNDED2_MINIMAL_PLAYER_CACHE_HPP
#define _GROUNDED2_MINIMAL_PLAYER_CACHE_HPP

#include "Grounded2Minimal.hpp"

namespace PlayerCache {
    struct CachedPlayer {
        SDK::APlayerState* PlayerState = nullptr;
        SDK::ASurvivalPlayerCharacter *SurvivalPlayerCharacter = nullptr;
        SDK::UPartyComponent *AssociatedPartyComponent = nullptr;
    };

    struct CachedData {
        // Cached player list
        std::vector<SDK::APlayerState*> Players;
        // Cached player extra info
        std::unordered_map<SDK::APlayerState*, CachedPlayer> CachedPlayers;
        // Cached local player ID
        int32_t LocalPlayerId = INVALID_PLAYER_ID;
        // Cached world instance
        SDK::UWorld* WorldInstance = nullptr;
    };

    // Unreal cached data
    extern struct CachedData g_CachedData;

    CachedPlayer* AttachCachedPlayerData(
        SDK::APlayerState* lpPlayerState
    );

    CachedPlayer* GetCachedPlayer(
        SDK::APlayerState* lpPlayerState
    );

    CachedPlayer* GetCachedPlayerById(
        int32_t iPlayerId
    );

    void BuildPlayerCache(void);

    void BuildPlayerCache(
        std::vector<SDK::APlayerState*> *vPlayerStates
    );
    void ClearPlayerCache(void);

    void RemoveCachedPlayer(
        SDK::APlayerState* lpPlayerState
    );

    void InvalidateCache(void);
}

#endif // _GROUNDED2_MINIMAL_PLAYER_CACHE_HPP