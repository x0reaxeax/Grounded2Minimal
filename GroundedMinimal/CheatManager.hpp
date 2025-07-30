#ifndef _GROUNDED_MINIMAL_CHEAT_MANAGER_HPP
#define _GROUNDED_MINIMAL_CHEAT_MANAGER_HPP

#include "GroundedMinimal.hpp"

#define CMGR_PARAM_1 0
#define CMGR_PARAM_2 1
#define CMGR_PARAM_3 2
#define CMGR_PARAM_4 3

namespace CheatManager {
    enum class CheatManagerFunctionId : uint32_t {
        None = 0,
        AddWhiteMolars,
        AddGoldMolars,
        AddRawScience,
        CompleteActiveDefensePoint,
        ToggleHud,
        UnlockAllRecipes,
        UnlockAllLandmarks,
        UnlockMonthlyTechTrees,
        UnlockMutations,
        UnlockScabs,
        Max
    };

    extern bool CheatManagerEnabled;
    extern SDK::USurvivalCheatManager* SurvivalCheatManager;
    
    struct BufferParamsExecuteCheat {
        CheatManagerFunctionId FunctionId;
        //uint64_t FunctionParams[4];
        union {
            uint64_t FunctionParams[4]; // 4 parameters, can be used for different functions
            struct {
                uint64_t Param1;
                uint64_t Param2;
                uint64_t Param3;
                uint64_t Param4;
            };
        };
    };

    typedef BufferParamsExecuteCheat CheatManagerParams;

    void GameThread CheatManagerExecute(
        const CheatManagerParams* lpParams
    );

    void UnlockMultiplayerCheatManager(void);

    bool Initialize(void);
}

#endif // _GROUNDED_MINIMAL_CHEAT_MANAGER_HPP