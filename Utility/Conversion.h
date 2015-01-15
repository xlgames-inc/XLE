// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "StringUtils.h"
#include "../Core/Types.h"

namespace Conversion
{
    template<typename Output, typename Input>
        Output Convert(Input input);

    template<> inline std::basic_string<char> Convert(uint64 input)
    {
        char buffer[64];
        XlUI64toA(input, buffer, dimof(buffer), 10);
        return buffer;
    }

    template<> inline std::basic_string<wchar_t> Convert(uint64 input)
    {
        wchar_t buffer[64];
        _ui64tow_s(input, buffer, dimof(buffer), 10);
        return buffer;
    }

    template<> inline float Convert<float, const char*>(const char input[])    { return XlAtoF32(input); }
    template<> inline uint32 Convert<uint32, const char*>(const char input[])  { return XlAtoUI32(input); }
    template<> inline int32 Convert<int32, const char*>(const char input[])    { return XlAtoI32(input); }

    template<> inline float Convert<float, char*>(char input[])    { return XlAtoF32(input); }
    template<> inline uint32 Convert<uint32, char*>(char input[])  { return XlAtoUI32(input); }
    template<> inline int32 Convert<int32, char*>(char input[])    { return XlAtoI32(input); }
}
