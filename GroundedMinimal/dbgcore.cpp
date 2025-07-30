// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 x0reaxeax

#include "GroundedMinimal.hpp"

//////////////////////////////////////////////////////////////////////////////
/// DBGCORE HOOKS (omg this smells like The C Programming Language - The Best Programming Language, yum)

MiniDumpReadDumpStream_t lpMiniDumpReadDumpStream = NULL;
MiniDumpWriteDump_t lpMiniDumpWriteDump = NULL;

HMODULE g_hOriginalDll = NULL;

EXTERN_C DLLEXPORT BOOL WINAPI MiniDumpReadDumpStream(
    PVOID BaseOfDump,
    ULONG StreamNumber,
    LPVOID Dir,
    PVOID Buffer,
    ULONG BufferSize
) {
    if (NULL == lpMiniDumpReadDumpStream) {
        return FALSE;
    }
    return lpMiniDumpReadDumpStream(
        BaseOfDump,
        StreamNumber,
        Dir,
        Buffer,
        BufferSize
    );
}

EXTERN_C DLLEXPORT BOOL WINAPI MiniDumpWriteDump(
    HANDLE hProcess,
    DWORD ProcessId,
    HANDLE hFile,
    UINT32 DumpType,
    LPVOID ExceptionParam,
    LPVOID UserStreamParam,
    LPVOID CallbackParam
) {
    if (NULL == lpMiniDumpWriteDump) {
        return FALSE;
    }
    return lpMiniDumpWriteDump(
        hProcess,
        ProcessId,
        hFile,
        DumpType,
        ExceptionParam,
        UserStreamParam,
        CallbackParam
    );
}

bool WINAPI SideLoadInit(
    void
) {
    g_hOriginalDll = LoadLibraryA(
        "C:\\Windows\\System32\\DBGCORE.DLL"
    );

    if (NULL == g_hOriginalDll) {
        return FALSE;
    }

    lpMiniDumpReadDumpStream = (MiniDumpReadDumpStream_t) GetProcAddress(
        g_hOriginalDll,
        "MiniDumpReadDumpStream"
    );

    if (NULL == lpMiniDumpReadDumpStream) {
        return FALSE;
    }

    lpMiniDumpWriteDump = (MiniDumpWriteDump_t) GetProcAddress(
        g_hOriginalDll,
        "MiniDumpWriteDump"
    );

    if (NULL == lpMiniDumpWriteDump) {
        return FALSE;
    }

    return TRUE;
}