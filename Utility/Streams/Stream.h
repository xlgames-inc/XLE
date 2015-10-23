// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Core/Types.h"
#include "../StringUtils.h"
#include "../Detail/API.h"
#include "../UTFUtils.h"
#include "../StringUtils.h"
#include <memory>
#include <assert.h>

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
        using size_type = size_t;
        virtual size_type   Tell() = 0;
        virtual void        Write(const void*, size_type) = 0;
        virtual void        Write(StringSection<utf8>) = 0;
        virtual void        Write(StringSection<ucs2>) = 0;
        virtual void        Write(StringSection<ucs4>) = 0;
        virtual void        WriteChar(utf8) = 0;
        virtual void        WriteChar(ucs2) = 0;
        virtual void        WriteChar(ucs4) = 0;
        virtual void        Flush() = 0;

        virtual ~OutputStream() {}
    };

    std::unique_ptr<OutputStream>   OpenFileOutput(const char* path, const char* mode);

}

using namespace Utility;

