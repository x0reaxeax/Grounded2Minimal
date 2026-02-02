#ifndef _GROUNDED2_MINIMAL_CHEAT_MANAGER_HPP
#define _GROUNDED2_MINIMAL_CHEAT_MANAGER_HPP

#include "Grounded2Minimal.hpp"
#include "UnrealUtils.hpp"

namespace CheatManager {
    namespace StaticCheats {
        // Get/Set enum
        typedef enum EStaticCheatOp : uint8_t {
            Invalid = 0,
            Get = 1,
            Set = 2,
            _Debug = 0xFF
        } EStaticCheatOp;

        int32_t MaxActiveMutations(EStaticCheatOp eOperation, int32_t iTargetPlayerId, int32_t iNewSetValue);
        int32_t MaxCozinessLevelAchieved(EStaticCheatOp eOperation, int32_t iTargetPlayerId, int32_t iNewSetValue);
        float NearbyStorageRadius(EStaticCheatOp eOperation, int32_t iTargetPlayerId, float fNewSetValue);
        float ChillRateMultiplier(EStaticCheatOp eOperation, int32_t iTargetPlayerId, float fNewSetValue);
        float SizzleRateMultiplier(EStaticCheatOp eOperation, int32_t iTargetPlayerId, float fNewSetValue);
        float PerfectBlockWindow(EStaticCheatOp eOperation, int32_t iTargetPlayerId, float fNewSetValue);
        float DodgeDistance(EStaticCheatOp eOperation, int32_t iTargetPlayerId, float fNewSetValue);
        float CurrentFoodLevel(EStaticCheatOp eOperation, int32_t iTargetPlayerId, float fNewSetValue);
        float CurrentWaterLevel(EStaticCheatOp eOperation, int32_t iTargetPlayerId, float fNewSetValue);
        float StaminaRegenRate(EStaticCheatOp eOperation, int32_t iTargetPlayerId, float fNewRegenRate);
        float StaminaRegenDelay(EStaticCheatOp eOperation, int32_t iTargetPlayerId, float fNewRegenDelay);
    }
    
    namespace InvokedCheats {
        void SetPlayerCollision(
            bool bNewCollisionState
        );

        void ServerBuildAllStructures(void);
    }

    namespace Summon {
        struct BufferParamsSummon {
            int32_t iPlayerId;
            SDK::FString fszClassName;                          // Name of the class to summon
            SDK::APlayerController *lpLocalPlayerController;    // Pointer to the local player controller
        };

        void SummonClass(
            const std::string& szClassName
        );
    }

    namespace Culling {
        struct BufferParamsCullItemInstance {
            SDK::ASpawnedItem *lpItemInstance;
        };

        void __gamethread CullItemInstance(
            SDK::ASpawnedItem *lpItemToCull
        );

        void CullItemByItemIndex(
            int32_t iItemIndex
        );

        void CullAllItemInstances(
            std::string &szTargetItemTypeName,
            bool bSilent = false
        );
    }

    enum class CheatManagerFunctionId : uint32_t {
        None = 0,
        AddWhiteMolars,
        AddGoldMolars,
        AddRawScience,
        ToggleHud,
        UnlockAllRecipes,
        UnlockAllLandmarks,
        UnlockMutations,
        UnlockScabs,
        ToggleAnalyzer,
        ToggleGod,
        ToggleStamina,
        UnlockOmniTool,
        BuildAllBuildings,
        Max,
        _Debug = 0xFFFFFFFF
    };

    // TODO: Get rid of this
    extern bool CheatManagerEnabled;
    extern SDK::USurvivalCheatManager* SurvivalCheatManager;

    struct BufferParamsExecuteCheat {
        CheatManagerFunctionId FunctionId;
        union {
            uint64_t FunctionParams[4];
            struct {
                uint64_t Param1;
                uint64_t Param2;
                uint64_t Param3;
                uint64_t Param4;
            };
        };
        int32_t TargetPlayerId = UnrealUtils::GetLocalPlayerId(true);
    };


    struct BufferParamsEnableCheats {
        SDK::APlayerController *lpLocalPlayerController;    // Pointer to the local player controller
    };

    typedef BufferParamsExecuteCheat CheatManagerParams;
    typedef BufferParamsEnableCheats CheatManagerEnableParams;

    bool IsSurvivalCheatManagerInitialized(void);

    void __gamethread CheatManagerEnableCheats(
        const CheatManagerEnableParams *lpParams
    );

    void __gamethread CheatManagerExecute(
        const CheatManagerParams* lpParams
    );

    SDK::UCheatManager *GetPlayersCheatManager(
        int32_t iPlayerId
    );

    SDK::USurvivalCheatManager *GetPlayersSurvivalCheatManager(
        int32_t iPlayerId
    );

    //bool Initialize(void);
    bool ManualInitialize(void);

    // Cleans up the artificially crafted cheat manager instance
    void Destroy(void);
}

#endif // _GROUNDED2_MINIMAL_CHEAT_MANAGER_HPP