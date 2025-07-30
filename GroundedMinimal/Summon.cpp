// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#include "UnrealUtils.hpp"
#include "CoreUtils.hpp"
#include "Command.hpp"
#include "Summon.hpp"

namespace Summon {
    void SummonClass(
        const std::string& szClassName
    ) {
        if (szClassName.empty()) {
            LogError("Summon", "Class name is empty");
            return;
        }
        
        SDK::UWorld *lpWorld = SDK::UWorld::GetWorld();
        if (nullptr == lpWorld) {
            LogError("Summon", "Failed to get UWorld instance");
            return;
        }

        SDK::UGameInstance *lpOwningGameInstance = lpWorld->OwningGameInstance;
        if (nullptr == lpOwningGameInstance) {
            LogError("Summon", "Failed to get UGameInstance instance");
            return;
        }

        int32_t iLocalPlayerId = UnrealUtils::GetLocalPlayerId();
        for (int32_t i = 0; i < lpOwningGameInstance->LocalPlayers.Num(); i++) {
            SDK::ULocalPlayer *lpLocalPlayer = lpOwningGameInstance->LocalPlayers[i];
            if (nullptr == lpLocalPlayer) {
                continue;
            }

            SDK::APlayerController *lpLocalPlayerController = lpLocalPlayer->PlayerController;
            if (nullptr == lpLocalPlayerController) {
                continue;
            }

            SDK::APlayerState *lpPlayerState = lpLocalPlayerController->PlayerState;
            if (nullptr == lpPlayerState) {
                continue;
            }

            if (iLocalPlayerId != lpPlayerState->PlayerId) {
                continue;
            }

            if (!lpPlayerState->HasAuthority()) {
                LogError("Summon", "Local player does not have authority to summon classes");
                return;
            }

            std::wstring wszClassName;
            if (!CoreUtils::Utf8ToWideString(
                szClassName,
                wszClassName
            )) {
                LogError("Summon", "Failed to convert class name to wide string");
                return;
            }

            SDK::FString fszTargetClassName(wszClassName.c_str());

            auto* lpParams = new (std::nothrow) BufferParamsSummon{
                .iPlayerId = iLocalPlayerId,
                .fszClassName = fszTargetClassName,
                .lpLocalPlayerController = lpLocalPlayerController
            };

            if (nullptr == lpParams) {
                LogError("Summon", "Failed to allocate memory for BufferParamsSummon");
                return;
            }

            Command::SubmitTypedCommand(
                Command::CommandId::CmdIdSummon,
                lpParams
            );
        }
    }
}