#ifndef _GROUNDED2_MINIMAL_CHEAT_MANAGER_HPP
#define _GROUNDED2_MINIMAL_CHEAT_MANAGER_HPP

#include "Grounded2Minimal.hpp"

namespace CheatManager {
    namespace StaticCheats {
        bool SetMaxActiveMutations(
            uint32_t uMaxMutations
        );
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

    //bool Initialize(void);
    bool ManualInitialize(void);
}

#endif // _GROUNDED2_MINIMAL_CHEAT_MANAGER_HPP