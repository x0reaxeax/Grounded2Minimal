#include "Logging.hpp"

std::atomic<bool> GlobalOutputEnabled{ true };
std::atomic<bool> g_bDebug = { false };

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

bool CheckNullAndLog(
    const void* lpcPtr,
    const std::string& szPtrName,
    const std::string& szContext
) {
    if (nullptr == lpcPtr) {
        std::string szFullContext = szContext.empty() ? "General" : szContext;
        if (CheckGlobalOutputEnabled()) {
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
    if (!CheckGlobalOutputEnabled() && !IsDebugOutputEnabled()) {
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
    if (!CheckGlobalOutputEnabled() && !IsDebugOutputEnabled()) {
        return;
    }

    if (bOnlyDebug && !IsDebugOutputEnabled()) {
        return; // Skip logging if debug mode is off
    }
    std::cout << "[" << szPrefix << "] " << szMessage << std::endl;
}

void LogMessage(
    const std::wstring& wszPrefix,
    const std::wstring& wszMessage,
    bool bOnlyDebug
) {
    if (!CheckGlobalOutputEnabled() && !IsDebugOutputEnabled()) {
        return;
    }
    if (bOnlyDebug && !IsDebugOutputEnabled()) {
        return; // Skip logging if debug mode is off
    }

    std::wcout << L"[" << wszPrefix << L"] " << wszMessage << std::endl;
}

void LogError(
    const std::string& szPrefix, 
    const std::string& szMessage, 
    bool bOnlyDebug
) {
    if (!CheckGlobalOutputEnabled() && !IsDebugOutputEnabled()) {
        return;
    }

    if (bOnlyDebug && !IsDebugOutputEnabled()) {
        return; // Skip logging if debug mode is off
    }
    std::cout << "[" << szPrefix << "] ERROR: " << szMessage << std::endl;
}