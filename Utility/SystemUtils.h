// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "UTFUtils.h"
#include "../Core/Prefix.h"
#include "../Core/Types.h"
#include <string>

namespace Utility
{
    bool XlGetCurrentDirectory(uint32 dim, char dst[]);
    uint64 XlGetCurrentFileTime();
    uint32 XlSignalAndWait(XlHandle hSig, XlHandle hWait, uint32 waitTime);

    void XlGetProcessPath(utf8 dst[], size_t bufferCount);
    void XlChDir(const utf8 path[]);
	void XlDeleteFile(const utf8 path[]);
    void XlMoveFile(const utf8 destination[], const utf8 source[]);

    void XlOutputDebugString(const char* format);
    void XlMessageBox(const char* content, const char* title);

    const char* XlGetCommandLine();

    typedef size_t ModuleId;
    ModuleId GetCurrentModuleId();

    std::string SystemErrorCodeAsString(int errorCode);
}


