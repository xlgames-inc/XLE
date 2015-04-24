// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Core/Types.h"
#include "../Detail/API.h"
#include "../UTFUtils.h"
#include <memory>

namespace Utility
{

    enum StreamSeekType {
        SST_CUR
    };

    class XL_UTILITY_API InputStream 
    {
    public:
        virtual int Read(void* p, int len) = 0;
        virtual bool Seek(StreamSeekType type, int64 offset) = 0;
        virtual ~InputStream() {}
    };


    class XL_UTILITY_API OutputStream 
    {
    public:
        virtual int64 Tell() = 0;
        virtual int64 Write(const void* p, int len) = 0;
        virtual void WriteChar(utf8 ch) = 0;
        virtual void WriteChar(ucs2 ch) = 0;
        virtual void WriteChar(ucs4 ch) = 0;

        virtual void WriteString(const utf8* s) = 0;
        virtual void WriteString(const ucs2* s) = 0;
        virtual void WriteString(const ucs4* s) = 0;

        virtual void Flush() = 0;

        virtual ~OutputStream() {}
    };

    std::unique_ptr<InputStream>    OpenFileInput(const char* path, const char* mode);
    std::unique_ptr<OutputStream>   OpenFileOutput(const char* path, const char* mode);

    std::unique_ptr<InputStream>    OpenMemoryInput(const void* s, int len);
    std::unique_ptr<OutputStream>   OpenFixedMemoryOutput(void* s, int len);
    std::unique_ptr<OutputStream>   OpenMemoryOutput();

}

using namespace Utility;

