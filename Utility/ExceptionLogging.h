// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Exceptions.h"
#include <ostream>

// Use namespace Exceptions and std instead of Utility here,
// because we should use the namespace which the rhs of the stream operator is in.
namespace Exceptions
{
    template<typename CharType, typename CharTraits>
    std::basic_ostream<CharType, CharTraits> &operator<<(
            std::basic_ostream<CharType, CharTraits> &stream,
            const ::Exceptions::BasicLabel &exception)
    {
        stream << exception.what();
        return stream;
    }
}

namespace std
{
    template<typename CharType, typename CharTraits>
    std::basic_ostream<CharType, CharTraits> &operator<<(
            std::basic_ostream<CharType, CharTraits> &stream,
            const std::exception &exception)
    {
        stream << exception.what();
        return stream;
    }

    template<typename CharType, typename CharTraits>
    std::basic_ostream<CharType, CharTraits> &operator<<(
            std::basic_ostream<CharType, CharTraits> &stream,
            const std::exception_ptr &exception_ptr)
    {
        TRY {
            std::rethrow_exception(exception_ptr);
        } CATCH (const std::exception &e) {
            stream << e.what();
        } CATCH (const std::string &e) {
            stream << e;
        } CATCH (const char *e) {
            stream << e;
        } CATCH (...) {
            stream << "Unknown Exception";
        } CATCH_END
        return stream;
    }
}