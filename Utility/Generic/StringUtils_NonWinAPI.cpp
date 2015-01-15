// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../StringUtils.h"

int XlUtf8To16(wchar_t* utf16, int count, const char* utf8)
{
    return XlMultiToWide((unichar*)utf16, count, utf8);
}

int XlUtf16To8(char* utf8, int count, const wchar_t* utf16)
{
    return XlWideToMulti(utf8, count, (const unichar*)utf16);
}

int XlCpToMulti(unsigned int codePage, char* dst, int count, const char* src)
{
    // In OSX, ACP == UTF8 :)
    XlCopyString(dst, count, src);
    return XlStringSize(src);
}

int XlMultiToAcp(char* dst, int count, const char* src)
{
    // In OSX, ACP == UTF8 :)
    XlCopyString(dst, count, src);
    return XlStringSize(src);
}

int XlAcpToMulti(char* dst, int count, const char* src)
{
    // In OSX, ACP == UTF8 :)
    XlCopyString(dst, count, src);
    return XlStringSize(src);
}

int XlMultiToWide2(wchar_t* dst, int count, const char* src)
{
    // TODO: correct?
    return mbstowcs(dst, src, count);
}

int XlWideToAcp(char* dst, int count, const unichar* src)
{
    return XlWideToMulti(dst, count, src);
}

int XlWide2ToAcp(char* dst, int count, const wchar_t* src)
{
    return wcstombs(dst, src, count);
}

int XlAcpToWide2(wchar_t* dst, int count, const char* src)
{
    return mbstowcs(dst, src, count);
}

