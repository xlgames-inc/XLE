// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)


#pragma once

#include "Prefix.h"
#include <exception>
#include <string.h>
#if !FEATURE_EXCEPTIONS
    #include <stdlib.h>     // (for exit)
#endif
#include <stdarg.h>
#include <stdio.h>      // note -- could be avoided if we moved the constructor for BasicLabel into .cpp file

#if FEATURE_EXCEPTIONS

    #define TRY { try 
    #define CATCH(x) catch(x) 
    #define RETHROW throw 
    #define CATCH_END } 

#else 

    #define TRY { if (true) 
    #define CATCH(x) else if (false) 
    #define RETHROW 
    #define CATCH_END } 

    #pragma warning(disable:4127)       // conditional expression is constant (must be disabled when using these macros)

#endif 

namespace Exceptions
{
    class BasicLabel : public std::exception
    {
    public:
        BasicLabel(const char format[], ...) never_throws;
        BasicLabel(const BasicLabel& copyFrom) never_throws;
        BasicLabel& operator=(const BasicLabel& copyFrom) never_throws;

        virtual const char* what() const never_throws /*override*/ { return _buffer; }
    protected:
        BasicLabel() never_throws;
        char _buffer[512];
    };

    inline BasicLabel::BasicLabel() never_throws { _buffer[0] = '\0'; }

    inline BasicLabel::BasicLabel(const char format[], ...) never_throws
    {
        va_list args;
        va_start(args, format);
        _vsnprintf_s(_buffer, _TRUNCATE, format, args);
        va_end(args);
    }

    inline BasicLabel::BasicLabel(const BasicLabel& copyFrom) never_throws
    {
        memcpy(_buffer, copyFrom._buffer, sizeof(_buffer));
    }

    inline BasicLabel& BasicLabel::operator=(const BasicLabel& copyFrom) never_throws
    {
        memcpy(_buffer, copyFrom._buffer, sizeof(_buffer));
        return *this;
    }
}

namespace Utility
{
    #if FEATURE_EXCEPTIONS
        template <class E> inline void ThrowException(const E& e)      { throw e; }
    #else
        template <class E> inline void ThrowException(const E& e)
        {
            // OutputDebugString("Suppressed thrown exception");
            exit(-1);
        }
    #endif
}


using namespace Utility;
