#ifndef _GROUNDED2_MINIMAL_INTERPRETER_HPP
#define _GROUNDED2_MINIMAL_INTERPRETER_HPP


#include "Grounded2Minimal.hpp"

namespace Interpreter {
    namespace KeyBinds {
        bool Initialize(void);
        void Shutdown(void);
        void Tick(void);
    }

    struct ConsoleCommand {
        std::string szCommand;
        std::string szDescription;
        void (*fnHandler)();
    };

    bool IsCommandAvailable(
        const std::string& szInput
    );

    int32_t ResolvePlayerId(
        const std::string& szPrompt
    );

    bool ReadInterpreterInput(
        const std::string& szPrompt,
        std::string& szOutInput
    );

    int32_t ReadIntegerInput(
        const std::string& szPrompt,
        int32_t iDefaultValue = -1
    );

    float ReadFloatInput(
        const std::string& szPrompt,
        float fDefaultValue = -1.0f
    );
}


#endif // _GROUNDED2_MINIMAL_INTERPRETER_HPP