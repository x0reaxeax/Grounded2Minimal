#ifndef _GROUNDED2_MINIMAL_CORE_UTILS_HPP
#define _GROUNDED2_MINIMAL_CORE_UTILS_HPP

#include <string>

namespace CoreUtils {
    std::string HexConvert(
        const uint64_t uValue
    );

    bool IsStringWildcard(
        const std::string& cszString
    );

    bool StringContainsCaseInsensitive(
        const std::string& cszHaystack,
        const std::string& cszNeedle
    );

    bool WideStringToUtf8(
        const std::wstring& szWideString,
        std::string &szUtf8String
    );

    bool Utf8ToWideString(
        const std::string& szUtf8String,
        std::wstring &szWideString
    );

    LPCWSTR InlineMultiByteToWideChar(
        LPCSTR szMultiByteString
    );

    LPCWSTR InlineStringToWideChar(
        std::string szString
    );

    bool GetVersionFromResource(
        VersionInfo& versionInfo
    );

    bool GetCurrentWorkingDirectory(
        std::string& szOutDirectory
    );

    bool FileExists(
        LPCSTR cszFilePath
    );

    int32_t GenerateUniqueId(void);
} // namespace CoreUtils

#endif // _GROUNDED_MINIMAL_CORE_UTILS_HPP