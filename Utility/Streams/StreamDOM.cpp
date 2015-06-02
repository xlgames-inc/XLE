// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "StreamDOM.h"

namespace Utility
{

    template <typename CharType>
        unsigned Document<CharType>::ParseElement(InputStreamFormatter<CharType>& formatter)
    {
        InputStreamFormatter<CharType>::InteriorSection section;
        if (!formatter.TryReadBeginElement(section))
            return ~0u;

        unsigned e = ~0u;
        {
            Element newElement;
            newElement._name = section;
            newElement._firstAttribute = ~0u;
            newElement._firstChild = ~0u;
            newElement._nextSibling = ~0u;
            _elements.push_back(newElement);
            e = (unsigned)_elements.size()-1;
        }

        auto lastChild = ~0u;
        auto lastAttribute = ~0u;
        for (;;) {
            auto next = formatter.PeekNext();
            if (next == InputStreamFormatter<CharType>::Blob::BeginElement) {
                auto newElement = ParseElement(formatter);
                if (lastChild == ~0u) {
                    _elements[e]._firstChild = lastChild = newElement;
                } else {
                    _elements[lastChild]._nextSibling = lastChild = newElement;
                }
            } else if (next == InputStreamFormatter<CharType>::Blob::AttributeName) {
                InputStreamFormatter<CharType>::InteriorSection name;
                InputStreamFormatter<CharType>::InteriorSection value;
                if (!formatter.TryReadAttribute(name, value))
                    ThrowException(FormatException(
                        "Error while reading attribute in StreamDOM", formatter.GetLocation()));

                Attribute attrib;
                attrib._name = name;
                attrib._value = value;
                attrib._next = ~0u;
                _attributes.push_back(attrib);
                auto a = (unsigned)_attributes.size()-1;
                if (lastAttribute == ~0u) {
                    _elements[e]._firstAttribute = lastAttribute = a;
                } else {
                    _attributes[lastAttribute]._next = lastAttribute = a;
                }
            } else if (next == InputStreamFormatter<CharType>::Blob::EndElement) {
                break;
            } else {
                ThrowException(FormatException(
                    "Expected blob type while parsing element in StreamDOM", formatter.GetLocation()));
            }
        }

        if (!formatter.TryReadEndElement()) 
            ThrowException(FormatException(
                "Expected end element in StreamDOM", formatter.GetLocation()));

        return e;
    }

    template <typename CharType>
        Document<CharType>::Document(InputStreamFormatter<CharType>& formatter)
    {
        _elements.reserve(32);
        _attributes.reserve(64);

            // Parse the input formatter, building a tree
            // of elements and a list of attributes.
            // We can start with several top-level elements
        while (ParseElement(formatter) != ~0u) {};
        
        if (formatter.PeekNext() != InputStreamFormatter<CharType>::Blob::None)
            ThrowException(FormatException(
                "Unexpected trailing characters in StreamDOM", formatter.GetLocation()));
    }

    template <typename CharType>
        Document<CharType>::~Document()
    {}

    template class Document<utf8>;
    template class Document<ucs2>;
    template class Document<ucs4>;
}
