// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "UTFUtils.h"
#include "../Core/Prefix.h"
#include "../Core/Types.h"

namespace Utility
{
    bool XlGetCurrentDirectory(uint32 dim, char dst[]);
    bool XlGetCurrentDirectory(uint32 dim, ucs2 dst[]);
    uint64 XlGetCurrentFileTime();
    uint32 XlSignalAndWait(XlHandle hSig, XlHandle hWait, uint32 waitTime);

    void XlGetProcessPath(utf8 dst[], size_t bufferCount);
    void XlGetProcessPath(ucs2 dst[], size_t bufferCount);
    void XlChDir(const utf8 path[]);
    void XlChDir(const ucs2 path[]);
	void XlDeleteFile(const utf8 path[]);
	void XlDeleteFile(const ucs2 path[]);

    void XlOutputDebugString(const char* format);
    void XlMessageBox(const char* content, const char* title);

    const char* XlGetCommandLine();

    typedef size_t ModuleId;
    ModuleId GetCurrentModuleId();
}


