#ifndef _GROUNDED_MINIMAL_CORE_UTILS_HPP
#define _GROUNDED_MINIMAL_CORE_UTILS_HPP

#include "GroundedMinimal.hpp"

namespace CoreUtils {
    std::string HexConvert(
        const uint64_t uValue
    );

    bool WideStringToUtf8(
        const std::wstring& szWideString,
        std::string &szUtf8String
    );

    bool Utf8ToWideString(
        const std::string& szUtf8String,
        std::wstring &szWideString
    );
} // namespace CoreUtils

#endif // _GROUNDED_MINIMAL_CORE_UTILS_HPP