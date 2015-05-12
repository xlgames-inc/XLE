// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../UTFUtils.h"
#include "../StringUtils.h"
#include "../MemoryUtils.h"
#include "../Detail/API.h"
#include <assert.h>
#include <algorithm>

namespace Utility
{

    // path operations
    XL_UTILITY_API void XlNormalizePath(char* dst, int count, const char* path);
    XL_UTILITY_API void XlNormalizePath(ucs2* dst, int count, const ucs2* path);

    // remove relative path in the middle
    XL_UTILITY_API void XlSimplifyPath(char* dst, int count, const char* path, const char* sep);
    XL_UTILITY_API void XlSimplifyPath(ucs2* dst, int count, const ucs2* path, const ucs2* sep);

    XL_UTILITY_API void XlToUnixPath(char* dst, int count, const char* path);
    XL_UTILITY_API void XlToUnixPath(ucs2* dst, int count, const ucs2* path);
    XL_UTILITY_API void XlToDosPath(char* dst, int count, const char* path);
    XL_UTILITY_API void XlToDosPath(ucs2* dst, int count, const ucs2* path);

    XL_UTILITY_API void XlConcatPath(char* dst, int count, const char* a, const char* b, const char* bEnd);
    XL_UTILITY_API void XlConcatPath(ucs2* dst, int count, const ucs2* a, const ucs2* b, const ucs2* bEnd);

    XL_UTILITY_API void XlMakeRelPath(char* dst, int count, const char* root, const char* path);
    XL_UTILITY_API void XlResolveRelPath(char* dst, int count, const char* base, const char* rel);

    XL_UTILITY_API template<typename CharType> const CharType* XlExtension(const CharType* path);
    XL_UTILITY_API void XlChopExtension(char* path);
    XL_UTILITY_API void XlDirname(char* dst, int count, const char* path);
    XL_UTILITY_API void XlDirname(ucs2* dst, int count, const ucs2* path);
    XL_UTILITY_API void XlBasename(char* dst, int count, const char* path);
    XL_UTILITY_API const char* XlBasename(const char* path);

}

