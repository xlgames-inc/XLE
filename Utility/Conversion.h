// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Types.h"
#include "UTFUtils.h"
#include "StringUtils.h"
#include <string>

namespace Conversion
{
    template<typename Output> Output Convert(uint64);
    template<typename Output> Output Convert(const char[]);
    template<typename Type> const Type& Convert(const Type& input) { return input; }

    template<typename Output> Output Convert(const std::basic_string<utf8>& input);
    template<typename Output> Output Convert(const std::basic_string<ucs2>& input);
    template<typename Output> Output Convert(const std::basic_string<ucs4>& input);

    template<typename OutputElement, typename InputElement>
        bool Convert(
            OutputElement output[], size_t outputDim,
            const InputElement* begin, const InputElement* end);

    template<typename OutputElement, typename InputElement>
        bool Convert(
            OutputElement output[], size_t outputDim,
            const std::basic_string<InputElement>& str);

    template<typename OutputElement, typename InputElement>
        bool ConvertNullTerminated(
            OutputElement output[], size_t outputDim,
            const InputElement* begin);

    template<typename OutputElement, typename InputElement>
        bool ConvertNullTerminated(
            OutputElement output[], size_t outputDim,
            const InputElement* begin)
        {
            if (outputDim <= 1) return false;
            return Convert(output, outputDim-1, begin, begin+inputLen);
        }

///////////////////////////////////////////////////////////////////////////////////////////////////

        // We want "char" and "wchar_t" to behave as "utf8" and "uc2", respectively

    template<typename Output> Output Convert(const std::basic_string<char>& input)
    {
        return Convert<Output>(reinterpret_cast<const std::basic_string<utf8>&>(input));
    }

    template<typename Output> Output Convert(const std::basic_string<wchar_t>& input)
    {
        return Convert<Output>(reinterpret_cast<const std::basic_string<ucs2>&>(input));
    }

    template<> inline std::basic_string<char> Convert(const std::basic_string<char>& input) { return input; }
    template<> inline std::basic_string<wchar_t> Convert(const std::basic_string<wchar_t>& input) { return input; }
    template<> inline std::basic_string<utf8> Convert(const std::basic_string<utf8>& input) { return input; }
    template<> inline std::basic_string<ucs2> Convert(const std::basic_string<ucs2>& input) { return input; }
    template<> inline std::basic_string<ucs4> Convert(const std::basic_string<ucs4>& input) { return input; }

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename OutputElement, typename InputElement>
        bool Convert(
            OutputElement output[], size_t outputDim,
            const std::basic_string<InputElement>& str)
    {
        return Convert(output, outputDim, AsPointer(str.cbegin()), AsPointer(str.cend()));
    }

    template<typename Element>
        bool Convert(
            Element output[], size_t outputDim,
            const Element* begin, const Element* end)
    {
        XlCopyNString(output, outputDim, begin, end-begin);
        return true;
    }
}
