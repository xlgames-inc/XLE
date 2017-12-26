// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Utility/IteratorUtils.h"

namespace ShaderSourceParser
{
    class Error
    {
    public:
        unsigned _lineStart, _charStart;
        unsigned _lineEnd, _charEnd;
        std::basic_string<char> _message;
    };

    namespace Exceptions
    {
        class ParsingFailure : public std::exception
        {
        public:
            IteratorRange<const Error*> GetErrors() const { return MakeIteratorRange(_errors); }
            const char* what() const noexcept;

            ParsingFailure(IteratorRange<Error*> errors) never_throws;
            ~ParsingFailure();
        private:
            std::vector<Error> _errors;
        };
    }
}
