// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#ifndef _GROUNDED2_UNREAL_UTILS_HPP
#define _GROUNDED2_UNREAL_UTILS_HPP

#include "Grounded2Minimal.hpp"
#include <vector>

#include "SDK/BP_SurvivalPlayerCharacter_classes.hpp"
#include "SDK/BP_SurvivalPlayerCharacter_parameters.hpp"

namespace UnrealUtils {

    // Structured data for DataTable information
    struct DataTableInfo {
        std::string szTableName;
        std::vector<std::string> vszItemNames;

        DataTableInfo() = default;
        DataTableInfo(const std::string& szName) : szTableName(szName) {}

        // Helper methods
        bool IsEmpty() const {
            return vszItemNames.empty();
        }
        size_t GetItemCount() const {
            return vszItemNames.size();
        }
    };

    SDK::UWorld *GetWorld(bool bCached = false);

    SDK::UEngine *GetEngine(void);

    SDK::UUserInterfaceStatics *GetUserInterfaceStatics(void);

    SDK::AGameUI *GetGameUI(void);

    SDK::APawn *GetLocalPawn(void);

    SDK::ABP_SurvivalPlayerCharacter_C *GetLocalSurvivalPlayerCharacter_C(void);

    void DumpClasses(
        std::vector<std::string>* vszClassesOut = nullptr,
        const std::string& szTargetClassNameNeedle = "",
        bool bOnlyBlueprints = false
    );

    void DumpFunctions(
        const std::string& szTargetFunctionNameNeedle
    );

    SDK::UFunction *FindFunction(
        const std::string& szTargetFunctionNameNeedle
    );

    std::vector<SDK::UFunction*> FindFunctionByAddress(
        const DWORD64 qwTargetPage
    );

    void DumpConnectedPlayers(
        std::vector<SDK::APlayerState*> *vlpPlayerStatesOut = nullptr,
        bool bHideOutput = false
    );

    bool IsPlayerHostAuthority(
        int32_t iPlayerId = 0
    );

    void PrintFoundItemInfo(
        SDK::ASpawnedItem *lpSpawnedItem,
        int32_t iItemIndex,
        bool* lpbFoundAtLeastOne
    );

    void FindDataTableByName(
        const std::string& szTargetTableStringNeedle
    );

    void ParseRowMapManually(
        SDK::UDataTable *lpDataTable,
        std::vector<std::string> *vlpRowNamesOut = nullptr,
        const std::string& szFilterNeedle = ""
    );

    SDK::ASpawnedItem *GetSpawnedItemByIndex(
        int32_t iItemIndex,
        const std::string& szTargetItemTypeName = ""
    );

    void FindSpawnedItemByType(
        const std::string& szTargetItemTypeName
    );

    bool GetItemRowMap(
        SDK::UDataTable *lpDataTable,
        const std::string& szItemName,
        SDK::FDataTableRowHandle *lpRowHandleOut
    );

    SDK::UDataTable * GetDataTablePointer(
        const std::string& szTargetTableName
    );

    void FindMatchingDataTableForItemName(
        const std::string& szItemName
    );

    int32_t GetLocalPlayerId(bool bCached = false);

    SDK::ASurvivalPlayerState *GetSurvivalPlayerStateById(
        int32_t iTargetPlayerId
    );

    SDK::AGameModeBase *GetSurvivalGameModeBase(void);

    SDK::USurvivalGameplayStatics* GetSurvivalGameplayStatics(void);

    int32_t GetPlayerIdByName(
        const std::string& szPlayerName
    );

    void DumpAllDataTablesAndItems(
        std::vector<DataTableInfo> *vlpDataTableInfoOut = nullptr,
        const std::string& szDataTableFilterNeedle = ""
    );

    SDK::UGameInstance *GetOwningGameInstance(void);

    SDK::APlayerController *GetLocalPlayerController(void);

    SDK::APlayerController *GetPlayerControllerById(
        int32_t iPlayerId
    );

    SDK::ASurvivalPlayerController *GetLocalSurvivalPlayerControllerFast(void);
    SDK::ASurvivalPlayerController *GetLocalSurvivalPlayerController(void);

    SDK::UPartyComponent *FindLocalPlayerParty(
        int32_t iTargetPlayerId = INVALID_PLAYER_ID,
        bool bCached = false
    );

    SDK::ASurvivalPlayerCharacter *GetSurvivalPlayerCharacterById(
        int32_t iTargetPlayerId = INVALID_PLAYER_ID,
        bool bCached = false
    );

    SDK::ASurvivalPlayerState *GetLocalSurvivalPlayerState(void);

    void DumpSurvivalCheatManagerInstances(void);

    SDK::USurvivalModeManagerComponent *GetSurvivalModeManagerComponent(
        void
    );

    SDK::USurvivalGameModeSettings* GetSurvivalGameModeSettings(
        void
    );

    const std::string GameTypeToString(
        SDK::EGameType eGameType
    );

    namespace GameStatics {
        bool IsHandyGnatEnabled(void);
        bool IsBuildingIntegrityEnabled(void);
        bool IsAutoCompleteBuildingsEnabled(void);
        bool IsPetInvincibilityEnabled(void);
        bool IsFreeCraftingEnabled(void);
        float GetPlayerDamageMultiplier(void);
    }
    const int x = sizeof(SDK::FVector);
    namespace FrameWalker {
        // https://github.com/EpicGames/UnrealEngine/blob/5.6/Engine/Source/Runtime/CoreUObject/Public/UObject/Stack.h
        // https://github.com/EpicGames/UnrealEngine/blob/5.6/Engine/Source/Runtime/Core/Public/Misc/OutputDevice.h
        class FOutputDevice {
        public:
            FOutputDevice() = default;

            FOutputDevice(FOutputDevice&&) = default;
            FOutputDevice(const FOutputDevice&) = default;
            FOutputDevice& operator=(FOutputDevice&&) = default;
            FOutputDevice& operator=(const FOutputDevice&) = default;

            virtual ~FOutputDevice() = default;

            // FOutputDevice interface.
            virtual void Serialize(const TCHAR* V, int32_t Verbosity, const SDK::FName& Category) = 0;
            virtual void Serialize(const TCHAR* V, int32_t Verbosity, const SDK::FName& Category, const double Time)
            {
                Serialize(V, Verbosity, Category);
            }

            virtual void SerializeRecord(const void* Record);

            virtual void Flush()
            {
            }

            /**
             * Closes output device and cleans up. This can't happen in the destructor
             * as we might have to call "delete" which cannot be done for static/ global
             * objects.
             */
            virtual void TearDown()
            {
            }

            void SetSuppressEventTag(bool bInSuppressEventTag)
            {
                bSuppressEventTag = bInSuppressEventTag;
            }
            FORCEINLINE bool GetSuppressEventTag() const { return bSuppressEventTag; }
            void SetAutoEmitLineTerminator(bool bInAutoEmitLineTerminator)
            {
                bAutoEmitLineTerminator = bInAutoEmitLineTerminator;
            }
            FORCEINLINE bool GetAutoEmitLineTerminator() const { return bAutoEmitLineTerminator; }

            /**
             * Dumps the contents of this output device's buffer to an archive (supported by output device that have a memory buffer)
             * @param Ar Archive to dump the buffer to
             */
            virtual void Dump(class FArchive& Ar)
            {
            }

            /**
            * @return whether this output device is a memory-only device
            */
            virtual bool IsMemoryOnly() const
            {
                return false;
            }

            /**
             * @return whether this output device can be used on any thread.
             */
            virtual bool CanBeUsedOnAnyThread() const
            {
                return false;
            }

            /**
             * @return whether this output device can be used from multiple threads simultaneously without any locking
             */
            virtual bool CanBeUsedOnMultipleThreads() const
            {
                return false;
            }

            /**
             * @return whether this output device can be used after a panic (crash or fatal error) has been flagged.
             * @note The return value is cached by AddOutputDevice because calling this during a panic may fail.
             */
            virtual bool CanBeUsedOnPanicThread() const
            {
                return false;
            }

        protected:
            /** Whether to output the 'Log: ' type front... */
            bool bSuppressEventTag;
            /** Whether to output a line-terminator after each log call... */
            bool bAutoEmitLineTerminator;
        };

        template<typename T, int32_t N>
        struct TInlineAllocator {
            alignas(T) uint8_t InlineData[sizeof(T) * N];
        };

        template<typename T, typename Allocator = void>
        struct TArray {
            T* Data;
            int32_t Num;
            int32_t Max;

            Allocator Alloc;
        };

        typedef uint32_t CodeSkipSizeType;
        using FlowStackType = TArray<CodeSkipSizeType, TInlineAllocator<uint8_t, 8>>;

        struct FFrame : public FOutputDevice {
        public:
            // Variables.
            SDK::UFunction* Node;
            SDK::UObject* Object;
            uint8_t* Code;
            uint8_t* Locals;

            SDK::FProperty* MostRecentProperty;
            uint8_t* MostRecentPropertyAddress;
            uint8_t* MostRecentPropertyContainer;

            /** The execution flow stack for compiled Kismet code */
            FlowStackType FlowStack;

            /** Previous frame on the stack */
            FFrame* PreviousFrame;

            /** contains information on any out parameters */
            void* OutParms;

            /** If a class is compiled in then this is set to the property chain for compiled-in functions. In that case, we follow the links to setup the args instead of executing by code. */
            void* PropertyChainForCompiledIn;

            /** Currently executed native function */
            SDK::UFunction* CurrentNativeFunction;
        };
    }
}

#endif // _GROUNDED2_UNREAL_UTILS_HPP