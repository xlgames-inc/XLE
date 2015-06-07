// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "StreamDOM.h"
#include "StreamFormatter.h"
#include "XmlStreamFormatter.h"
#include "../StringFormat.h"

namespace Utility
{

    template <typename Formatter>
        unsigned Document<Formatter>::ParseElement(Formatter& formatter)
    {
        Formatter::InteriorSection section;
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
            if (next == Formatter::Blob::BeginElement) {
                auto newElement = ParseElement(formatter);
                if (lastChild == ~0u) {
                    _elements[e]._firstChild = newElement;
                } else {
                    _elements[lastChild]._nextSibling = newElement;
                }
                lastChild = newElement;
            } else if (next == Formatter::Blob::AttributeName) {
                Formatter::InteriorSection name;
                Formatter::InteriorSection value;
                if (!formatter.TryReadAttribute(name, value))
                    Throw(FormatException(
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
            } else if (next == Formatter::Blob::EndElement) {
                break;
            } else {
                Throw(FormatException(
                    StringMeld<128>() << "Got unexpected blob type (" << unsigned(next) << ") while parsing element in StreamDOM",
                    formatter.GetLocation()));
            }
        }

        if (!formatter.TryReadEndElement()) 
            Throw(FormatException(
                "Expected end element in StreamDOM", formatter.GetLocation()));

        return e;
    }

    template <typename Formatter>
        auto Document<Formatter>::Element(const value_type name[]) -> DocElementHelper<Formatter>
    {
            // look for top-level element with a name that matches the given name
            // exactly.
        if (!_elements.size()) return DocElementHelper<Formatter>();

        auto expectedNameLen = (ptrdiff_t)XlStringLen(name);
        for (unsigned e=0; e!=~0u;) {
            const auto& ele = _elements[e];
            auto nameLen = ele._name._end - ele._name._start;
            if (nameLen == expectedNameLen && !XlComparePrefix(ele._name._start, name, nameLen))
                return DocElementHelper<Formatter>(e, *this);

            e=ele._nextSibling;
        }

        return DocElementHelper<Formatter>();
    }

    template <typename Formatter>
        Document<Formatter>::Document(Formatter& formatter)
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
        
        if (formatter.PeekNext() != Formatter::Blob::None)
            Throw(FormatException(
                "Unexpected trailing characters in StreamDOM", formatter.GetLocation()));
    }

    template <typename Formatter>
        Document<Formatter>::~Document()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename Formatter>
        auto DocElementHelper<Formatter>::Element(const value_type name[]) -> DocElementHelper<Formatter>
    {
        if (_index == ~0u) return DocElementHelper<Formatter>();
        auto& ele = _doc->_elements[_index];

        auto expectedNameLen = (ptrdiff_t)XlStringLen(name);
        for (unsigned e=ele._firstChild; e!=~0u;) {
            const auto& ele = _doc->_elements[e];
            auto nameLen = ele._name._end - ele._name._start;
            if (nameLen == expectedNameLen && !XlComparePrefix(ele._name._start, name, nameLen))
                return DocElementHelper<Formatter>(e, *_doc);

            e=ele._nextSibling;
        }

        return DocElementHelper<Formatter>();
    }

    template<typename Formatter>
        auto DocElementHelper<Formatter>::FirstChild() -> DocElementHelper<Formatter>
    {
        if (_index == ~0u) return DocElementHelper<Formatter>();
        auto& ele = _doc->_elements[_index];
        return DocElementHelper<Formatter>(ele._firstChild, *_doc);
    }

    template<typename Formatter>
        auto DocElementHelper<Formatter>::NextSibling() -> DocElementHelper<Formatter>
    {
        if (_index == ~0u) return DocElementHelper<Formatter>();
        auto& ele = _doc->_elements[_index];
        return DocElementHelper<Formatter>(ele._nextSibling, *_doc);
    }

    template<typename Formatter>
        auto DocElementHelper<Formatter>::Name() const -> std::basic_string<value_type>
    {
        if (_index == ~0u) return std::basic_string<value_type>();
        auto& ele = _doc->_elements[_index];
        return std::basic_string<value_type>(ele._name._start, ele._name._end);
    }

    template<typename Formatter>
        auto DocElementHelper<Formatter>::AttributeOrEmpty(const value_type name[]) -> std::basic_string<value_type>
    {
        if (_index == ~0u) return std::basic_string<value_type>();
        auto a = FindAttribute(name, &name[XlStringLen(name)]);
        if (a == ~0u) return std::basic_string<value_type>();
        const auto& attrib = _doc->_attributes[a];
        return std::basic_string<value_type>(attrib._value._start, attrib._value._end);
    }

    template<typename Formatter>
        unsigned DocElementHelper<Formatter>::FindAttribute(const value_type* nameStart, const value_type* nameEnd)
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

    template<typename Formatter>
        DocElementHelper<Formatter>::DocElementHelper(unsigned elementIndex, Document<Formatter>& doc)
    {
        _doc = &doc;
        _index = elementIndex;
    }

    template<typename Formatter>
        DocElementHelper<Formatter>::DocElementHelper()
    {
        _index = ~0u;
        _doc = nullptr;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    template class Document<InputStreamFormatter<utf8>>;
    template class Document<InputStreamFormatter<ucs2>>;
    template class Document<InputStreamFormatter<ucs4>>;
    template class Document<XmlInputStreamFormatter<utf8>>;

    template class DocElementHelper<InputStreamFormatter<utf8>>;
    template class DocElementHelper<InputStreamFormatter<ucs2>>;
    template class DocElementHelper<InputStreamFormatter<ucs4>>;
    template class DocElementHelper<XmlInputStreamFormatter<utf8>>;

}
