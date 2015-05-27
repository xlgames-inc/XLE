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
        virtual int64 Tell() = 0;
        virtual int64 Write(const void* p, int len) = 0;
        virtual void WriteChar(utf8 ch) = 0;
        virtual void WriteChar(ucs2 ch) = 0;
        virtual void WriteChar(ucs4 ch) = 0;

        virtual void WriteString(const utf8* start, const utf8* end) = 0;
        virtual void WriteString(const ucs2* start, const ucs2* end) = 0;
        virtual void WriteString(const ucs4* start, const ucs4* end) = 0;

        inline void WriteNullTerm(const utf8* nullTerm) { assert(nullTerm); WriteString(nullTerm, &nullTerm[XlStringLen(nullTerm)]); }
        inline void WriteNullTerm(const ucs2* nullTerm) { assert(nullTerm); WriteString(nullTerm, &nullTerm[XlStringLen(nullTerm)]); }
        inline void WriteNullTerm(const ucs4* nullTerm) { assert(nullTerm); WriteString(nullTerm, &nullTerm[XlStringLen(nullTerm)]); }

        virtual void Flush() = 0;

        virtual ~OutputStream() {}
    };

    std::unique_ptr<OutputStream>   OpenFileOutput(const char* path, const char* mode);

}

using namespace Utility;

