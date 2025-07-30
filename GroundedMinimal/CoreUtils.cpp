#include "GroundedMinimal.hpp"

namespace CoreUtils {
    std::string g_szHexConvertBuffer;
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
}