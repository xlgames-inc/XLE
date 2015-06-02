// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "StreamFormatter.h"
#include <vector>

namespace Utility
{
    template <typename CharType>
        class Document
    {
    public:
        using Section = typename InputStreamFormatter<CharType>::InteriorSection;

        class Attribute
        {
        public:
            Section _name, _value;
            unsigned _next;
        };

        class Element
        {
        public:
            Section _name;
            unsigned _firstAttribute;
            unsigned _firstChild;
            unsigned _nextSibling;
        };

        Document(InputStreamFormatter<CharType>& formatter);
        ~Document();
    protected:
        std::vector<Element>    _elements;
        std::vector<Attribute>  _attributes;

        unsigned ParseElement(InputStreamFormatter<CharType>& formatter);
    };
}

using namespace Utility;
