#include "Logging.hpp"

std::atomic<bool> GlobalOutputEnabled{ true };
std::atomic<bool> g_bDebug = { false };

const char *G2M_LOG_PATH = "Grounded2MinimalLog.txt";

GLOBALHANDLE g_hLogFile = INVALID_HANDLE_VALUE;

///////////////////////////////////////////////////////
/// Logging n stuff

bool IsGlobalOutputEnabled(void) {
    return GlobalOutputEnabled.load();
}

void EnableGlobalOutput(void) {
    GlobalOutputEnabled.store(true);
}

void DisableGlobalOutput(void) {
    GlobalOutputEnabled.store(false);
}

bool IsDebugOutputEnabled(void) {
    return g_bDebug.load();
}

void EnableDebugOutput(void) {
    g_bDebug.store(true);
}

void DisableDebugOutput(void) {
    g_bDebug.store(false);
}

static bool IsHandleValid(
    const HANDLE hHandle
) {
    return (INVALID_HANDLE_VALUE != hHandle) && (nullptr != hHandle);
}

bool CheckNullAndLog(
    const void* lpcPtr,
    const std::string& szPtrName,
    const std::string& szContext
) {
    if (nullptr == lpcPtr) {
        std::string szFullContext = szContext.empty() ? "General" : szContext;
        if (IsGlobalOutputEnabled()) {
            std::cout << "[" << szFullContext << "] " << szPtrName << " is NULL." << std::endl;
        }
        return true;
    }
    return false;
}

void LogChar(
    const char cChar,
    bool bOnlyDebug
) {
    if (IsHandleValid(g_hLogFile)) {
        DWORD dwBytesWritten = 0;
        WriteFile(
            g_hLogFile,
            &cChar,
            sizeof(cChar),
            &dwBytesWritten,
            nullptr
        );
        // flush the file buffers to ensure immediate write
        FlushFileBuffers(g_hLogFile);
    }

    if (!IsGlobalOutputEnabled() && !IsDebugOutputEnabled()) {
        return;
    }

    if (bOnlyDebug && !IsDebugOutputEnabled()) {
        return; // Skip logging if debug mode is off
    }

    std::cout << cChar << std::endl;
}

void LogMessage(
    const std::string& szPrefix,
    const std::string& szMessage,
    bool bOnlyDebug
) {
    std::string szLogMessage = "[" + szPrefix + "] " + szMessage + "\r\n";
    if (IsHandleValid(g_hLogFile)) {
        DWORD dwBytesWritten = 0;
        WriteFile(
            g_hLogFile,
            szLogMessage.c_str(),
            static_cast<DWORD>(szLogMessage.length()),
            &dwBytesWritten,
            nullptr
        );
        // flush the file buffers to ensure immediate write
        FlushFileBuffers(g_hLogFile);
    }

    if (!IsGlobalOutputEnabled() && !IsDebugOutputEnabled()) {
        return;
    }

    if (bOnlyDebug && !IsDebugOutputEnabled()) {
        return; // Skip logging if debug mode is off
    }
    //std::cout << "[" << szPrefix << "] " << szMessage << std::endl;
    std::cout << szLogMessage;
}

void LogMessage(
    const std::wstring& wszPrefix,
    const std::wstring& wszMessage,
    bool bOnlyDebug
) {
    std::wstring wszLogMessage = L"[" + wszPrefix + L"] " + wszMessage + L"\r\n";
    if (IsHandleValid(g_hLogFile)) {
        DWORD dwBytesWritten = 0;
        WriteFile(
            g_hLogFile,
            wszLogMessage.c_str(),
            static_cast<DWORD>(wszLogMessage.length() * sizeof(wchar_t)),
            &dwBytesWritten,
            nullptr
        );
        // flush the file buffers to ensure immediate write
        FlushFileBuffers(g_hLogFile);
    }

    if (!IsGlobalOutputEnabled() && !IsDebugOutputEnabled()) {
        return;
    }
    if (bOnlyDebug && !IsDebugOutputEnabled()) {
        return; // Skip logging if debug mode is off
    }

    //std::wcout << L"[" << wszPrefix << L"] " << wszMessage << std::endl;
    std::wcout << wszLogMessage;
}

void LogError(
    const std::string& szPrefix, 
    const std::string& szMessage, 
    bool bOnlyDebug
) {
    std::string szErrorMessage = "[" + szPrefix + "] ERROR: " + szMessage + "\r\n";
    if (IsHandleValid(g_hLogFile)) {
        DWORD dwBytesWritten = 0;
        WriteFile(
            g_hLogFile,
            szErrorMessage.c_str(),
            static_cast<DWORD>(szErrorMessage.length()),
            &dwBytesWritten,
            nullptr
        );

        // flush the file buffers to ensure immediate write
        FlushFileBuffers(g_hLogFile);
    }

    if (!IsGlobalOutputEnabled() && !IsDebugOutputEnabled()) {
        return;
    }

    if (bOnlyDebug && !IsDebugOutputEnabled()) {
        return; // Skip logging if debug mode is off
    }
    //std::cout << "[" << szPrefix << "] ERROR: " << szMessage << std::endl;
    std::cout << szErrorMessage;
}

HANDLE *InitalizeLogFile(void) {
    g_hLogFile = CreateFileA(
        G2M_LOG_PATH,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (INVALID_HANDLE_VALUE == g_hLogFile) {
        LogError(
            "Logging",
            "Failed to create log file: " + std::string(G2M_LOG_PATH)
        );
        return nullptr;
    }

    return &g_hLogFile;
}