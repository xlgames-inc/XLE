// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../StringUtils.h"
#include "../../Core/WinAPI/IncludeWindows.h"

namespace Utility
{

size_t XlUtf8To16(ucs2* utf16, size_t count, const utf8* utf8)
{
    return MultiByteToWideChar(CP_UTF8, 0, (LPCCH)utf8, -1, (wchar_t*)utf16, int(count));
}

size_t XlUtf16To8(utf8* utf8, size_t count, const ucs2* utf16)
{
    return WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)utf16, -1, (LPSTR)utf8, int(count), 0, 0);
}

size_t XlMultiToCp(unsigned int codePage, char* dst, size_t count, const char* src)
{
    static const int MAX_MULTI_TO_ACP_BUF_LEN = 512*7; // same as MAX_WARNING_LENGTH
    wchar_t tmp[MAX_MULTI_TO_ACP_BUF_LEN];
    MultiByteToWideChar(CP_UTF8, 0, src, -1, tmp, MAX_MULTI_TO_ACP_BUF_LEN);
    return WideCharToMultiByte(codePage, 0, tmp, -1, dst, int(count), 0, 0);
}

size_t XlCpToMulti(unsigned int codePage, char* dst, size_t count, const char* src)
{
    wchar_t tmp[4096];
    if (count >= dimof(tmp)) {
        return 0;
    }
    MultiByteToWideChar(codePage, 0, src, -1, tmp, int(count));
    return WideCharToMultiByte(CP_UTF8, 0, tmp, -1, dst, int(count), 0, 0);
}

size_t XlMultiToAcp(char* dst, size_t count, const char* src)
{
    static const int MAX_MULTI_TO_ACP_BUF_LEN = 512*7; // same as MAX_WARNING_LENGTH
    wchar_t tmp[MAX_MULTI_TO_ACP_BUF_LEN];
    MultiByteToWideChar(CP_UTF8, 0, src, -1, tmp, MAX_MULTI_TO_ACP_BUF_LEN);
    return WideCharToMultiByte(CP_ACP, 0, tmp, -1, dst, int(count), 0, 0);
}

size_t XlAcpToMulti(char* dst, size_t count, const char* src)
{
    wchar_t tmp[4096];
    if (count >= dimof(tmp)) {
        return 0;
    }
    MultiByteToWideChar(CP_ACP, 0, src, -1, tmp, int(count));
    return WideCharToMultiByte(CP_UTF8, 0, tmp, -1, dst, int(count), 0, 0);
}

size_t XlMultiToWide2(wchar_t* dst, size_t count, const char* src)
{
    return MultiByteToWideChar(CP_ACP, 0, src, -1, dst, int(count));
}

size_t XlWideToAcp(char* dst, size_t count, const unsigned* src)
{
    // if we have ucs4 -> utf16 function, can skip one step
    utf8 tmp[4096];
    if (count >= dimof(tmp)) {
        return 0;
    }
    XlWideToMulti(tmp, int(count), src);
    return XlMultiToAcp(dst, int(count), (char*)tmp);
}

size_t XlWide2ToAcp(char* dst, size_t count, const wchar_t* src)
{
    return WideCharToMultiByte(CP_ACP, 0, src, -1, dst, int(count), NULL, NULL);
}

size_t XlAcpToWide2(wchar_t* dst, size_t count, const char* src)
{
    return MultiByteToWideChar(CP_ACP, 0, src, -1, dst, int(count));
}

}

