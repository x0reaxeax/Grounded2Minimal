#ifndef _GROUNDED2_MINIMAL_CHEAT_MANAGER_HPP
#define _GROUNDED2_MINIMAL_CHEAT_MANAGER_HPP

#include "Grounded2Minimal.hpp"
#include "UnrealUtils.hpp"

namespace CheatManager {
    namespace StaticCheats {
        // Get/Set enum
        typedef enum EStaticCheatOp : uint8_t {
            Get = 0,
            Set = 1,
            _Debug = 0xFF
        } EStaticCheatOp;

        template<
            typename TObject,
            typename TValue,
            typename Resolver,
            typename Getter,
            typename Setter
        >
        static TValue HandleStaticCheatImpl(
            EStaticCheatOp eOperation,
            int32_t iTargetPlayerId,
            const std::string &szCheatName,
            Resolver&& fnResolveObject,
            Getter&& fnGetValue,
            Setter&& fnSetValue,
            TValue NewValue,
            TValue ErrorValue
        ) {
            TObject* lpObject = fnResolveObject(iTargetPlayerId);
            if (!lpObject) {
                LogError(
                    "CheatManager",
                    "Failed to resolve target object for '" + szCheatName + "'"
                );
                return ErrorValue;
            }

            switch (eOperation) {
                case EStaticCheatOp::Set: {
                    fnSetValue(lpObject, NewValue);
                    TValue Current = fnGetValue(lpObject);
                    LogMessage(
                        "CheatManager",
                        "Set '" + szCheatName + "' to '" + std::to_string(Current) + "'",
                        true
                    );
                    return Current;
                }
                case EStaticCheatOp::Get: {
                    TValue Current = fnGetValue(lpObject);
                    LogMessage(
                        "CheatManager",
                        "Current '" + szCheatName + "' value: '" + std::to_string(Current) + "'",
                        true
                    );
                    return Current;
                }
                default:
                    return ErrorValue;
            }
        }

        int32_t MaxActiveMutations(EStaticCheatOp eOperation, int32_t iTargetPlayerId, uint32_t uNewSetValue);
        int32_t MaxCozinessLevelAchieved(EStaticCheatOp eOperation, int32_t iTargetPlayerId, uint32_t uNewSetValue);
        float NearbyStorageRadius(EStaticCheatOp eOperation, int32_t iTargetPlayerId, float fNewSetValue);
        float ChillRateMultiplier(EStaticCheatOp eOperation, int32_t iTargetPlayerId, float fNewSetValue);
        float SizzleRateMultiplier(EStaticCheatOp eOperation, int32_t iTargetPlayerId, float fNewSetValue);
        float PerfectBlockWindow(EStaticCheatOp eOperation, int32_t iTargetPlayerId, float fNewSetValue);
        float DodgeDistance(EStaticCheatOp eOperation, int32_t iTargetPlayerId, float fNewSetValue);
        float CurrentFoodLevel(EStaticCheatOp eOperation, int32_t iTargetPlayerId, float fNewSetValue);
        float CurrentWaterLevel(EStaticCheatOp eOperation, int32_t iTargetPlayerId, float fNewSetValue);
    }
    
    namespace InvokedCheats {
        void SetPlayerCollision(
            bool bNewCollisionState
        );
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