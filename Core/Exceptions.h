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
#include <cstdio>      // note -- could be avoided if we moved the constructor for BasicLabel into .cpp file

#if FEATURE_EXCEPTIONS
    #include <type_traits>  // for std::is_base_of, used by exceptions
#endif

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
        virtual const char* what() const never_throws /*override*/ { return _buffer; }
        virtual bool CustomReport() const { return false; }
        
        BasicLabel(const char format[], ...) never_throws;
        BasicLabel(const char format[], va_list args) never_throws;
        BasicLabel(const BasicLabel& copyFrom) never_throws;
        BasicLabel& operator=(const BasicLabel& copyFrom) never_throws;
        virtual ~BasicLabel();
    protected:
        BasicLabel() never_throws;
        char _buffer[512];
    };

    inline BasicLabel::BasicLabel() never_throws { _buffer[0] = '\0'; }

#pragma warning(push)
#pragma warning(disable:4793)       // 'Exceptions::BasicLabel::BasicLabel' : function compiled as native :
    inline BasicLabel::BasicLabel(const char format[], ...) never_throws
    {
        va_list args;
        va_start(args, format);
        std::vsnprintf(_buffer, dimof(_buffer), format, args);
        va_end(args);
    }

    inline BasicLabel::BasicLabel(const char format[], va_list args) never_throws
    {
        std::vsnprintf(_buffer, dimof(_buffer), format, args);
    }
#pragma warning(pop)

    inline BasicLabel::BasicLabel(const BasicLabel& copyFrom) never_throws
    {
        memcpy(_buffer, copyFrom._buffer, sizeof(_buffer));
    }

    inline BasicLabel& BasicLabel::operator=(const BasicLabel& copyFrom) never_throws
    {
        memcpy(_buffer, copyFrom._buffer, sizeof(_buffer));
        return *this;
    }

    inline BasicLabel::~BasicLabel() {}
}

#if COMPILER_ACTIVE == COMPILER_TYPE_MSVC
    #define NO_RETURN __declspec(noreturn)
#else
    #define NO_RETURN
#endif

namespace Utility
{
    #if defined(__INTELLISENSE__)
        inline void Throw(...) {}
    #else
        #if FEATURE_EXCEPTIONS
            typedef void (*OnThrowCallback)(const ::Exceptions::BasicLabel&);
            inline OnThrowCallback& GlobalOnThrowCallback()
            {
                static OnThrowCallback s_result = nullptr;
                return s_result;
            }

            template <class E, typename std::enable_if<std::is_base_of<::Exceptions::BasicLabel, E>::value>::type* = nullptr>
                inline NO_RETURN void Throw(const E& e)
            {
                auto* callback = GlobalOnThrowCallback();
                if (callback) (*callback)(e);
                throw e;
            }

            template <class E, typename std::enable_if<!std::is_base_of<::Exceptions::BasicLabel, E>::value>::type* = nullptr>
                inline NO_RETURN void Throw(const E& e)
            {
                throw e;
            }
        #else
            template <class E> inline void Throw(const E& e)
            {
                // OutputDebugString("Suppressed thrown exception");
                exit(-1);
            }
        #endif
    #endif
}


using namespace Utility;
