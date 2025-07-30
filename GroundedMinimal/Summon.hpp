// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#ifndef _GROUNDED_MINIMAL_SUMMON_HPP
#define _GROUNDED_MINIMAL_SUMMON_HPP

#include "GroundedMinimal.hpp"

namespace Summon {

    struct BufferParamsSummon {
        int32_t iPlayerId;
        SDK::FString fszClassName;                          // Name of the class to summon
        SDK::APlayerController *lpLocalPlayerController;    // Pointer to the local player controller
    };

    void SummonClass(
        const std::string& szClassName
    );
} // namespace Summon

#endif // _GROUNDED_MINIMAL_SUMMON_HPP