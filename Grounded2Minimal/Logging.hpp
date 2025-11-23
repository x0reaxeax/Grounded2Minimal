#ifndef _GROUNDED2_MINIMAL_LOGGING_HPP
#define _GROUNDED2_MINIMAL_LOGGING_HPP

#include <Windows.h>
#include <iostream>

extern std::atomic<bool> GlobalOutputEnabled;

#define CheckGlobalOutputEnabled() (GlobalOutputEnabled.load())

bool IsGlobalOutputEnabled(void);
void EnableGlobalOutput(void);
void DisableGlobalOutput(void);

bool IsFileLoggingEnabled(void);
void EnableFileLogging(void);
void DisableFileLogging(void);

bool IsDebugOutputEnabled(void);
void EnableDebugOutput(void);
void DisableDebugOutput(void);

bool CheckNullAndLog(
    const void* lpcPtr,
    const std::string& szPtrName,
    const std::string& szContext
);

void LogChar(
    const char cChar,
    bool bOnlyDebug = false
);

void LogMessage(
    const std::string& szPrefix,
    const std::string& szMessage,
    bool bOnlyDebug = false
);

void LogMessage(
    const std::wstring& wszPrefix,
    const std::wstring& wszMessage,
    bool bOnlyDebug = false
);

void LogError(
    const std::string& szPrefix,
    const std::string& szMessage,
    bool bOnlyDebug = false
);

HANDLE *InitalizeLogFile(void);

#endif // _GROUNDED2_MINIMAL_LOGGING_HPP