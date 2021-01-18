// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Core/Types.h"
#include "../StringUtils.h"
#include "../UTFUtils.h"
#include "../StringUtils.h"
#include <memory>
#include <assert.h>

namespace Utility
{
    class XL_UTILITY_API OutputStream 
    {
    public:
        using size_type = size_t;
        virtual size_type   Tell() = 0;
        virtual void        Write(const void*, size_type) = 0;
        virtual void        Write(StringSection<utf8>) = 0;
        virtual void        WriteChar(char ch) = 0;             // note -- this is always a single byte character (ie, use the string version if you want to write multibyte characters)
        virtual void        Flush() = 0;

        virtual ~OutputStream() = default;
    };

}

using namespace Utility;

