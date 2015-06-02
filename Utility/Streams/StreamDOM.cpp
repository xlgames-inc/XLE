// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "StreamDOM.h"
#include "../StringFormat.h"

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
            ElementDesc newElement;
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
                    _elements[e]._firstChild = newElement;
                } else {
                    _elements[lastChild]._nextSibling = newElement;
                }
                lastChild = newElement;
            } else if (next == InputStreamFormatter<CharType>::Blob::AttributeName) {
                InputStreamFormatter<CharType>::InteriorSection name;
                InputStreamFormatter<CharType>::InteriorSection value;
                if (!formatter.TryReadAttribute(name, value))
                    ThrowException(FormatException(
                        "Error while reading attribute in StreamDOM", formatter.GetLocation()));

                AttributeDesc attrib;
                attrib._name = name;
                attrib._value = value;
                attrib._nextSibling = ~0u;
                _attributes.push_back(attrib);
                auto a = (unsigned)_attributes.size()-1;
                if (lastAttribute == ~0u) {
                    _elements[e]._firstAttribute = a;
                } else {
                    _attributes[lastAttribute]._nextSibling = a;
                }
                lastAttribute = a;
            } else if (next == InputStreamFormatter<CharType>::Blob::EndElement) {
                break;
            } else {
                ThrowException(FormatException(
                    StringMeld<128>() << "Got unexpected blob type (" << unsigned(next) << ") while parsing element in StreamDOM",
                    formatter.GetLocation()));
            }
        }

        if (!formatter.TryReadEndElement()) 
            ThrowException(FormatException(
                "Expected end element in StreamDOM", formatter.GetLocation()));

        return e;
    }

    template <typename CharType>
        auto Document<CharType>::Element(const CharType name[]) -> ElementHelper
    {
            // look for top-level element with a name that matches the given name
            // exactly.
        if (!_elements.size()) return ElementHelper();

        auto expectedNameLen = (ptrdiff_t)XlStringLen(name);
        for (unsigned e=0; e!=~0u;) {
            const auto& ele = _elements[e];
            auto nameLen = ele._name._end - ele._name._start;
            if (nameLen == expectedNameLen && !XlComparePrefix(ele._name._start, name, nameLen))
                return ElementHelper(e, *this);

            e=ele._nextSibling;
        }

        return ElementHelper();
    }

    template <typename CharType>
        Document<CharType>::Document(InputStreamFormatter<CharType>& formatter)
    {
        _elements.reserve(32);
        _attributes.reserve(64);

            // Parse the input formatter, building a tree
            // of elements and a list of attributes.
            // We can start with several top-level elements
        unsigned lastEle = ~0u;
        for (;;) {
            auto newEle = ParseElement(formatter);
            if (newEle == ~0u) break;
            if (lastEle != ~0u) _elements[lastEle]._nextSibling = newEle;
            lastEle = newEle;
        }
        
        if (formatter.PeekNext() != InputStreamFormatter<CharType>::Blob::None)
            ThrowException(FormatException(
                "Unexpected trailing characters in StreamDOM", formatter.GetLocation()));
    }

    template <typename CharType>
        Document<CharType>::~Document()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename CharType>
        auto Document<CharType>::ElementHelper::Element(const CharType name[]) -> ElementHelper
    {
        if (_index == ~0u) return ElementHelper();
        auto& ele = _doc->_elements[_index];

        auto expectedNameLen = (ptrdiff_t)XlStringLen(name);
        for (unsigned e=ele._firstChild; e!=~0u;) {
            const auto& ele = _doc->_elements[e];
            auto nameLen = ele._name._end - ele._name._start;
            if (nameLen == expectedNameLen && !XlComparePrefix(ele._name._start, name, nameLen))
                return ElementHelper(e, *_doc);

            e=ele._nextSibling;
        }

        return ElementHelper();
    }

    template<typename CharType>
        auto Document<CharType>::ElementHelper::FirstChild() -> ElementHelper
    {
        if (_index == ~0u) return ElementHelper();
        auto& ele = _doc->_elements[_index];
        return ElementHelper(ele._firstChild, *_doc);
    }

    template<typename CharType>
        auto Document<CharType>::ElementHelper::NextSibling() -> ElementHelper
    {
        if (_index == ~0u) return ElementHelper();
        auto& ele = _doc->_elements[_index];
        return ElementHelper(ele._nextSibling, *_doc);
    }

    template<typename CharType>
        std::basic_string<CharType> Document<CharType>::ElementHelper::Name() const
    {
        if (_index == ~0u) return std::basic_string<CharType>();
        auto& ele = _doc->_elements[_index];
        return std::basic_string<CharType>(ele._name._start, ele._name._end);
    }

    template<typename CharType>
        std::basic_string<CharType> Document<CharType>::ElementHelper::AttributeOrEmpty(const CharType name[])
    {
        if (_index == ~0u) return std::basic_string<CharType>();
        auto a = FindAttribute(name, &name[XlStringLen(name)]);
        if (a == ~0u) return std::basic_string<CharType>();
        const auto& attrib = _doc->_attributes[a];
        return std::basic_string<CharType>(attrib._value._start, attrib._value._end);
    }

    template<typename CharType>
        unsigned Document<CharType>::ElementHelper::FindAttribute(const CharType* nameStart, const CharType* nameEnd)
    {
        assert(_index != ~0u);
        auto& ele = _doc->_elements[_index];
        auto expectedNameLen = nameEnd - nameStart;

        for (unsigned a=ele._firstAttribute; a!=~0u;) {
            const auto& attrib = _doc->_attributes[a];
            auto nameLen = attrib._name._end - attrib._name._start;
            if (nameLen == expectedNameLen && !XlComparePrefix(attrib._name._start, nameStart, nameLen))
                return a;

            a=attrib._nextSibling;
        }

        return ~0u;
    }

    template<typename CharType>
        Document<CharType>::ElementHelper::ElementHelper(unsigned elementIndex, Document<CharType>& doc)
    {
        _doc = &doc;
        _index = elementIndex;
    }

    template<typename CharType>
        Document<CharType>::ElementHelper::ElementHelper()
    {
        _index = ~0u;
        _doc = nullptr;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    template class Document<utf8>;
    template class Document<ucs2>;
    template class Document<ucs4>;
}
