#include "Grounded2Minimal.hpp"
#include "CoreUtils.hpp"

#include <Windows.h>
#include <algorithm>

#pragma comment (lib, "Version.lib")

namespace CoreUtils {
    static std::string g_szHexConvertBuffer;

    std::string HexConvert(
        const uint64_t uValue
    ) {
        g_szHexConvertBuffer.clear();
        g_szHexConvertBuffer.reserve(24);
        for (int i = 15; i >= 0; --i) {
            uint8_t byte = (uValue >> (i * 4)) & 0xF;
            if (byte < 10) {
                g_szHexConvertBuffer.push_back('0' + byte);
            } else {
                g_szHexConvertBuffer.push_back('A' + (byte - 10));
            }
        }
        return g_szHexConvertBuffer;
    }

    bool StringContainsCaseInsensitive(
        const std::string& szHaystack,
        const std::string& szNeedle
    ) {
        std::string szHaystackLower = szHaystack;
        std::string szNeedleLower = szNeedle;

        std::transform(
            szHaystackLower.begin(),
            szHaystackLower.end(),
            szHaystackLower.begin(),
            [](unsigned char c) {
                return std::tolower(c);
            }
        );

        std::transform(
            szNeedleLower.begin(),
            szNeedleLower.end(),
            szNeedleLower.begin(),
            [](unsigned char c) {
                return std::tolower(c);
            }
        );

        return szHaystackLower.find(szNeedleLower) != std::string::npos;
    }

    bool WideStringToUtf8(
        const std::wstring& szWideString,
        std::string &szUtf8String
    ) {
        int iSizeNeeded = WideCharToMultiByte(
            CP_UTF8,
            0,
            szWideString.data(),
            (int) szWideString.size(),
            nullptr,
            0,
            nullptr,
            nullptr
        );
        if (iSizeNeeded <= 0) {
            return false;
        }

        std::string szBuffer(iSizeNeeded, 0);
        int iConverted = WideCharToMultiByte(
            CP_UTF8,
            0,
            szWideString.data(),
            (int) szWideString.size(),
            &szBuffer[0],
            iSizeNeeded,
            nullptr,
            nullptr
        );
        if (iConverted <= 0) {
            return false;
        }

        szUtf8String = std::move(szBuffer);
        return true;
    }

    bool Utf8ToWideString(
        const std::string& szUtf8String,
        std::wstring &szWideString
    ) {
        int iSizeNeeded = MultiByteToWideChar(
            CP_UTF8,
            0,
            szUtf8String.data(),
            (int) szUtf8String.size(),
            nullptr,
            0
        );
        if (iSizeNeeded <= 0) {
            return false; // Conversion failed ( - thanks for this inline suggestion copilot, very handy)
        }
        std::wstring szBuffer(iSizeNeeded, 0);
        int iConverted = MultiByteToWideChar(
            CP_UTF8,
            0,
            szUtf8String.data(),
            (int) szUtf8String.size(),
            &szBuffer[0],
            iSizeNeeded
        );
        if (iConverted <= 0) {
            return false;
        }
        szWideString = std::move(szBuffer);
        return true;
    }

    bool GetVersionFromResource(
        VersionInfo& versionInfo
    ) {
        HMODULE hModule = GetModuleHandleA("Grounded2Minimal.dll");

        HRSRC hResource = FindResource(
            hModule,
            MAKEINTRESOURCE(VS_VERSION_INFO),
            RT_VERSION
        );
        if (nullptr == hResource) {
            return false;
        }

        HGLOBAL hGlobal = LoadResource(hModule, hResource);
        if (nullptr == hGlobal) {
            return false;
        }

        LPVOID pVersionInfo = LockResource(hGlobal);
        if (!pVersionInfo) {
            return false;
        }

        VS_FIXEDFILEINFO* pFileInfo = nullptr;
        UINT uiLen;

        if (VerQueryValueW(
            pVersionInfo,
            L"\\",
            (LPVOID*) &pFileInfo,
            &uiLen
        )) {
            versionInfo.major = HIWORD(pFileInfo->dwFileVersionMS);
            versionInfo.minor = LOWORD(pFileInfo->dwFileVersionMS);
            versionInfo.patch = HIWORD(pFileInfo->dwFileVersionLS);
            versionInfo.build = LOWORD(pFileInfo->dwFileVersionLS);
            return true;
        }

        return false;
    }
}