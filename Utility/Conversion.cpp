// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Conversion.h"
#include "StringUtils.h"
#include "PtrUtils.h"
#include "../Core/SelectConfiguration.h"
#include <algorithm>

namespace Conversion
{

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<> std::basic_string<char> Convert(uint64 input)
    {
        char buffer[64];
        XlUI64toA(input, buffer, dimof(buffer), 10);
        return buffer;
    }

    template<> std::basic_string<wchar_t> Convert(uint64 input)
    {
        wchar_t buffer[64];
        _ui64tow_s(input, buffer, dimof(buffer), 10);
        return buffer;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<> float Convert(const char input[])    { return XlAtoF32(input); }
    template<> uint32 Convert(const char input[])   { return XlAtoUI32(input); }
    template<> int32 Convert(const char input[])    { return XlAtoI32(input); }
    template<> int64 Convert(const char input[])    { return XlAtoI64(input); }
    template<> uint64 Convert(const char input[])   { return XlAtoUI64(input); }

    template<> bool Convert(const char input[])
    {
        if (    !XlCompareStringI(input, "true")
            ||  !XlCompareStringI(input, "yes")
            ||  !XlCompareStringI(input, "t")
            ||  !XlCompareStringI(input, "y")) {
            return true;
        }
        int asInt = 0;
        if (XlSafeAtoi(input, &asInt)) {
            return !!asInt;
        }
        return false;
    }

    template<> float Convert(const std::basic_string<utf8>& input)      { return Convert<float>((const char*)input.c_str()); }
    template<> uint32 Convert(const std::basic_string<utf8>& input)     { return Convert<uint32>((const char*)input.c_str()); }
    template<> int32 Convert(const std::basic_string<utf8>& input)      { return Convert<int32>((const char*)input.c_str()); }
    template<> int64 Convert(const std::basic_string<utf8>& input)      { return Convert<int64>((const char*)input.c_str()); }
    template<> uint64 Convert(const std::basic_string<utf8>& input)     { return Convert<uint64>((const char*)input.c_str()); }
    template<> bool Convert(const std::basic_string<utf8>& input)       { return Convert<bool>((const char*)input.c_str()); }

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<> std::basic_string<ucs2> Convert(const std::basic_string<utf8>& input)
    {
        std::basic_string<ucs2> result;
        result.resize(input.size());
        utf8_2_ucs2(
            AsPointer(input.begin()), input.size(),
            AsPointer(result.begin()), result.size());
        return std::move(result);
    }

    template<> std::basic_string<ucs4> Convert(const std::basic_string<utf8>& input)
    {
        std::basic_string<ucs4> result;
        result.resize(input.size());
        utf8_2_ucs4(
            AsPointer(input.begin()), input.size(),
            AsPointer(result.begin()), result.size());
        return std::move(result);
    }

    template<> std::basic_string<utf8> Convert(const std::basic_string<ucs2>& input)
    {
        std::basic_string<utf8> result;
        result.resize(input.size());
        ucs2_2_utf8(
            AsPointer(input.begin()), input.size(),
            AsPointer(result.begin()), result.size());
        return std::move(result);
    }

    template<> std::basic_string<ucs4> Convert(const std::basic_string<ucs2>& input)
    {
        std::basic_string<ucs4> result;
        result.resize(input.size());
        ucs2_2_ucs4(
            AsPointer(input.begin()), input.size(),
            AsPointer(result.begin()), result.size());
        return std::move(result);
    }

    template<> std::basic_string<char> Convert(const std::basic_string<ucs2>& input)
    {
        std::basic_string<char> result;
        result.resize(input.size());
        ucs2_2_utf8(
            AsPointer(input.begin()), input.size(),
            (utf8*)AsPointer(result.begin()), result.size());
        return std::move(result);
    }

    template<> std::basic_string<utf8> Convert(const std::basic_string<ucs4>& input)
    {
        std::basic_string<utf8> result;
        result.resize(input.size());
        ucs4_2_utf8(
            AsPointer(input.begin()), input.size(),
            AsPointer(result.begin()), result.size());
        return std::move(result);
    }

    template<> std::basic_string<ucs2> Convert(const std::basic_string<ucs4>& input)
    {
        std::basic_string<ucs2> result;
        result.resize(input.size());
        ucs4_2_ucs2(
            AsPointer(input.begin()), input.size(),
            AsPointer(result.begin()), result.size());
        return std::move(result);
    }

    template<> std::basic_string<char> Convert(const std::basic_string<ucs4>& input)
    {
        std::basic_string<char> result;
        result.resize(input.size());
        ucs4_2_utf8(
            AsPointer(input.begin()), input.size(),
            (utf8*)AsPointer(result.begin()), result.size());
        return std::move(result);
    }

    template<> std::basic_string<wchar_t> Convert(const std::basic_string<ucs4>& input)
    {
        std::basic_string<wchar_t> result;
        result.resize(input.size());
        ucs4_2_ucs2(
            AsPointer(input.begin()), input.size(),
            (ucs2*)AsPointer(result.begin()), result.size());
        return std::move(result);
    }

    template<> std::basic_string<char> Convert(const std::basic_string<utf8>& input)
    {
        return reinterpret_cast<const std::basic_string<char>&>(input);
    }

    template<> std::basic_string<wchar_t> Convert(const std::basic_string<ucs2>& input)
    {
        return reinterpret_cast<const std::basic_string<wchar_t>&>(input);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	template<> std::basic_string<ucs2> Convert(StringSection<utf8> input)
	{
		std::basic_string<ucs2> result;
		result.resize(input.Length());
		utf8_2_ucs2(
			AsPointer(input.begin()), input.Length(),
			AsPointer(result.begin()), result.size());
		return std::move(result);
	}

	template<> std::basic_string<ucs4> Convert(StringSection<utf8> input)
	{
		std::basic_string<ucs4> result;
		result.resize(input.Length());
		utf8_2_ucs4(
			AsPointer(input.begin()), input.Length(),
			AsPointer(result.begin()), result.size());
		return std::move(result);
	}

	template<> std::basic_string<utf8> Convert(StringSection<ucs2> input)
	{
		std::basic_string<utf8> result;
		result.resize(input.Length());
		ucs2_2_utf8(
			AsPointer(input.begin()), input.Length(),
			AsPointer(result.begin()), result.size());
		return std::move(result);
	}

	template<> std::basic_string<ucs4> Convert(StringSection<ucs2> input)
	{
		std::basic_string<ucs4> result;
		result.resize(input.Length());
		ucs2_2_ucs4(
			AsPointer(input.begin()), input.Length(),
			AsPointer(result.begin()), result.size());
		return std::move(result);
	}

	template<> std::basic_string<char> Convert(StringSection<ucs2> input)
	{
		std::basic_string<char> result;
		result.resize(input.Length());
		ucs2_2_utf8(
			AsPointer(input.begin()), input.Length(),
			(utf8*)AsPointer(result.begin()), result.size());
		return std::move(result);
	}

	template<> std::basic_string<utf8> Convert(StringSection<ucs4> input)
	{
		std::basic_string<utf8> result;
		result.resize(input.Length());
		ucs4_2_utf8(
			AsPointer(input.begin()), input.Length(),
			AsPointer(result.begin()), result.size());
		return std::move(result);
	}

	template<> std::basic_string<ucs2> Convert(StringSection<ucs4> input)
	{
		std::basic_string<ucs2> result;
		result.resize(input.Length());
		ucs4_2_ucs2(
			AsPointer(input.begin()), input.Length(),
			AsPointer(result.begin()), result.size());
		return std::move(result);
	}

	template<> std::basic_string<char> Convert(StringSection<ucs4> input)
	{
		std::basic_string<char> result;
		result.resize(input.Length());
		ucs4_2_utf8(
			AsPointer(input.begin()), input.Length(),
			(utf8*)AsPointer(result.begin()), result.size());
		return std::move(result);
	}

	template<> std::basic_string<wchar_t> Convert(StringSection<ucs4> input)
	{
		std::basic_string<wchar_t> result;
		result.resize(input.Length());
		ucs4_2_ucs2(
			AsPointer(input.begin()), input.Length(),
			(ucs2*)AsPointer(result.begin()), result.size());
		return std::move(result);
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<> ptrdiff_t Convert(
        ucs2 output[], size_t outputDim,
        const utf8* begin, const utf8* end)
    {
        return utf8_2_ucs2(begin, end-begin, output, outputDim);
    }

    template<> ptrdiff_t Convert(
        ucs4 output[], size_t outputDim,
        const utf8* begin, const utf8* end)
    {
        return utf8_2_ucs4(begin, end-begin, output, outputDim);
    }

    template<> ptrdiff_t Convert(
        char output[], size_t outputDim,
        const utf8* begin, const utf8* end)
    {
        XlCopyNString(output, outputDim, (const char*)begin, end-begin);
        return true;
    }

    template<> ptrdiff_t Convert(
        wchar_t output[], size_t outputDim,
        const utf8* begin, const utf8* end)
    {
        return utf8_2_ucs2(begin, end-begin, (ucs2*)output, outputDim);
    }

        //////////      //////////      //////////

    template<> ptrdiff_t Convert(
        utf8 output[], size_t outputDim,
        const ucs2* begin, const ucs2* end)
    {
        return ucs2_2_utf8(begin, end-begin, output, outputDim);
    }

    template<> ptrdiff_t Convert(
        ucs4 output[], size_t outputDim,
        const ucs2* begin, const ucs2* end)
    {
        return ucs2_2_ucs4(begin, end-begin, output, outputDim);
    }

    template<> ptrdiff_t Convert(
        char output[], size_t outputDim,
        const ucs2* begin, const ucs2* end)
    {
        return ucs2_2_utf8(begin, end-begin, (utf8*)output, outputDim);
    }

    template<> ptrdiff_t Convert(
        wchar_t output[], size_t outputDim,
        const ucs2* begin, const ucs2* end)
    {
        XlCopyNString((ucs2*)output, outputDim, begin, end-begin);
        return true;
    }

        //////////      //////////      //////////

    template<> ptrdiff_t Convert(
        utf8 output[], size_t outputDim,
        const ucs4* begin, const ucs4* end)
    {
        return ucs4_2_utf8(begin, end-begin, output, outputDim);
    }

    template<> ptrdiff_t Convert(
        ucs2 output[], size_t outputDim,
        const ucs4* begin, const ucs4* end)
    {
        return ucs4_2_ucs2(begin, end-begin, output, outputDim);
    }

    template<> ptrdiff_t Convert(
        char output[], size_t outputDim,
        const ucs4* begin, const ucs4* end)
    {
        return ucs4_2_utf8(begin, end-begin, (utf8*)output, outputDim);
    }

    template<> ptrdiff_t Convert(
        wchar_t output[], size_t outputDim,
        const ucs4* begin, const ucs4* end)
    {
        return ucs4_2_ucs2(begin, end-begin, (ucs2*)output, outputDim);
    }

        //////////      //////////      //////////

    template<> ptrdiff_t Convert(
        utf8 output[], size_t outputDim,
        const char* begin, const char* end)
    {
        return Convert(output, outputDim, (const utf8*)begin, (const utf8*)end);
    }

    template<> ptrdiff_t Convert(
        ucs2 output[], size_t outputDim,
        const char* begin, const char* end)
    {
        return Convert(output, outputDim, (const utf8*)begin, (const utf8*)end);
    }

    template<> ptrdiff_t Convert(
        ucs4 output[], size_t outputDim,
        const char* begin, const char* end)
    {
        return Convert(output, outputDim, (const utf8*)begin, (const utf8*)end);
    }

    template<> ptrdiff_t Convert(
        wchar_t output[], size_t outputDim,
        const char* begin, const char* end)
    {
        return Convert((ucs2*)output, outputDim, (const utf8*)begin, (const utf8*)end);
    }

            //////////      //////////      //////////

    template<> ptrdiff_t Convert(
        utf8 output[], size_t outputDim,
        const wchar_t* begin, const wchar_t* end)
    {
        return Convert(output, outputDim, (const ucs2*)begin, (const ucs2*)end);
    }

    template<> ptrdiff_t Convert(
        ucs2 output[], size_t outputDim,
        const wchar_t* begin, const wchar_t* end)
    {
        return Convert(output, outputDim, (const ucs2*)begin, (const ucs2*)end);
    }

    template<> ptrdiff_t Convert(
        ucs4 output[], size_t outputDim,
        const wchar_t* begin, const wchar_t* end)
    {
        return Convert(output, outputDim, (const ucs2*)begin, (const ucs2*)end);
    }

    template<> ptrdiff_t Convert(
        char output[], size_t outputDim,
        const wchar_t* begin, const wchar_t* end)
    {
        return Convert((utf8*)output, outputDim, (const ucs2*)begin, (const ucs2*)end);
    }

}
