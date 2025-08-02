#include <Windows.h>
#include <TlHelp32.h>

#include <stdio.h>


int main(void) {

    CHAR cConsume = 0;
    INT iRet = EXIT_FAILURE;

    HANDLE hProcess = NULL;
    HANDLE hThread = NULL;
    
    CHAR szCurrentDir[MAX_PATH] = { 0 };
    CHAR szTargetDllPath[MAX_PATH * 2] = { 0 };
    LPCSTR cszTargetProcessName = "Grounded2-WinGRTS-Shipping.exe";
    
    // Get current directory
    if (0 == GetCurrentDirectoryA(
        sizeof(szCurrentDir),
        szCurrentDir
    )) {
        fprintf(
            stderr,
            "[-] GetCurrentDirectoryA() - E%lu\n", 
            GetLastError()
        );
        goto _FINAL;
    }

    snprintf(
        szTargetDllPath,
        sizeof(szTargetDllPath),
        "%s\\Grounded2Minimal.dll",
        szCurrentDir
    );


    HANDLE hFile = CreateFileA(
        szTargetDllPath,
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (INVALID_HANDLE_VALUE == hFile) {
        fprintf(
            stderr,
            "[-] DLL not found: '%s'\n", 
            szTargetDllPath
        );
        goto _FINAL;
    }
    CloseHandle(hFile);

    HANDLE hSnapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPPROCESS,
        0
    );

    if (INVALID_HANDLE_VALUE == hSnapshot) {
        fprintf(
            stderr,
            "[-] CreateToolhelp32Snapshot() - E%lu\n",
            GetLastError()
        );
        goto _FINAL;
    }

    PROCESSENTRY32 procEntry32 = {
        .dwSize = sizeof(PROCESSENTRY32)
    };

    if (!Process32First(hSnapshot, &procEntry32)) {
        fprintf(
            stderr,
            "[-] Process32First() - E%lu\n",
            GetLastError()
        );
        CloseHandle(hSnapshot);
        goto _FINAL;
    }

    DWORD dwTargetProcessId = 0;
    do {
        if (EXIT_SUCCESS == strncmp(
            procEntry32.szExeFile,
            cszTargetProcessName,
            MAX_PATH
        )) {
            dwTargetProcessId = procEntry32.th32ProcessID;
            break;
        }
    } while (Process32Next(hSnapshot, &procEntry32));
    CloseHandle(hSnapshot);

    if (0 == dwTargetProcessId) {
        fprintf(
            stderr,
            "[-] Grounded2 process not found.\n"
        );
        goto _FINAL;
    }

    printf(
        "[+] Found Grounded2 process: '%s' (PID: %lu)\n",
        cszTargetProcessName,
        dwTargetProcessId
    );

    hProcess = OpenProcess(
        PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD,
        FALSE,
        dwTargetProcessId
    );

    if (NULL == hProcess) {
        fprintf(
            stderr,
            "[-] OpenProcess() - E%lu\n",
            GetLastError()
        );
        goto _FINAL;
    }

    printf(
        "[+] Opened handle: %p\n",
        hProcess
    );

    LPVOID lpDllNameMemory = VirtualAllocEx(
        hProcess,
        NULL,
        strlen(szTargetDllPath) + 1,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );

    if (NULL == lpDllNameMemory) {
        fprintf(
            stderr,
            "[-] VirtualAllocEx() - E%lu\n",
            GetLastError()
        );
        goto _FINAL;
    }

    printf(
        "[+] Allocated memory in target process: %p\n",
        lpDllNameMemory
    );

    SIZE_T cbBytesWritten = 0;
    if (!WriteProcessMemory(
        hProcess,
        lpDllNameMemory,
        szTargetDllPath,
        strlen(szTargetDllPath) + 1,
        &cbBytesWritten
    )) {
        fprintf(
            stderr,
            "[-] WriteProcessMemory() - E%lu\n",
            GetLastError()
        );
        goto _FINAL;
    }

    printf(
        "[+] Wrote %zu bytes to target process memory.\n",
        cbBytesWritten
    );

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (NULL == hKernel32) {
        fprintf(
            stderr,
            "[-] GetModuleHandleA() - E%lu\n",
            GetLastError()
        );
        goto _FINAL;
    }

    FARPROC lpLoadLibraryA = GetProcAddress(
        hKernel32,
        "LoadLibraryA"
    );

    if (NULL == lpLoadLibraryA) {
        fprintf(
            stderr,
            "[-] GetProcAddress() - E%lu\n",
            GetLastError()
        );
        goto _FINAL;
    }

    printf(
        "[+] LoadLibraryA address: %p\n",
        lpLoadLibraryA
    );

    hThread = CreateRemoteThread(
        hProcess,
        NULL,
        0,
        (LPTHREAD_START_ROUTINE) lpLoadLibraryA,
        lpDllNameMemory,
        0,
        NULL
    );

    if (NULL == hThread) {
        fprintf(
            stderr,
            "[-] CreateRemoteThread() - E%lu\n",
            GetLastError()
        );
        goto _FINAL;
    }

    printf(
        "[+] Created remote thread in target process: %p\n",
        hThread
    );

    // Wait for the thread to finish
    WaitForSingleObject(hThread, INFINITE);
    printf("[+] Success\n");

    iRet = EXIT_SUCCESS;
    goto _SKIP;
_FINAL:
    cConsume = getchar();
    
_SKIP:
    if (NULL != hThread) {
        CloseHandle(hThread);
    }
    if (NULL != hProcess) {
        CloseHandle(hProcess);
    }

    return iRet;
}