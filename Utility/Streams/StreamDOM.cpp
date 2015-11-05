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
        if (!formatter.TryBeginElement(section))
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
                if (!formatter.TryAttribute(name, value))
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

        if (!formatter.TryEndElement()) 
            Throw(FormatException(
                "Expected end element in StreamDOM", formatter.GetLocation()));

        return e;
    }

    template <typename Formatter>
        auto Document<Formatter>::Element(const value_type name[]) const -> DocElementHelper<Formatter>
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

    template<typename Formatter>
        DocElementHelper<Formatter> Document<Formatter>::FirstChild() const
    {
        if (!_elements.size()) return DocElementHelper<Formatter>();
        return DocElementHelper<Formatter>(0, *this);
    }

    template<typename Formatter>
        DocAttributeHelper<Formatter> Document<Formatter>::FirstAttribute() const
    {
        if (_firstRootAttribute==~0u) return DocAttributeHelper<Formatter>();
        return DocAttributeHelper<Formatter>(_firstRootAttribute, *this);
    }

    template<typename Formatter>
        unsigned Document<Formatter>::FindAttribute(StringSection<value_type> name) const
    {
        if (_attributes.empty()) return ~0u;

        for (auto a=_firstRootAttribute; a!=~0u;) {
            const auto& attrib = _attributes[a];
            if (XlEqString(attrib._name, name))
                return a;

            a=attrib._nextSibling;
        }

        return ~0u;
    }

    template<typename Formatter>
        DocAttributeHelper<Formatter> Document<Formatter>::Attribute(const value_type name[]) const
    {
        return DocAttributeHelper<Formatter>(FindAttribute(name), *this);
    }

    template <typename Formatter>
        Document<Formatter>::Document(Formatter& formatter)
    {
        _elements.reserve(32);
        _attributes.reserve(64);
        _firstRootAttribute = ~0u;

            // Parse the input formatter, building a tree
            // of elements and a list of attributes.
            // We can start with several top-level elements
        unsigned lastEle = ~0u;
        unsigned lastAttrib = ~0u;
        for (;;) {
            switch (formatter.PeekNext()) {
            case Formatter::Blob::AttributeName:
                {
                    Formatter::InteriorSection name, value;
                    if (formatter.TryAttribute(name, value)) {
                        _attributes.push_back(AttributeDesc{name, value, ~0u});
                        if (lastAttrib != ~0u) {
                            _attributes[lastAttrib]._nextSibling = unsigned(_attributes.size()-1);
                        } else {
                            _firstRootAttribute = unsigned(_attributes.size()-1);
                        }
                        lastAttrib = unsigned(_attributes.size()-1);
                    }
                }
                continue;

            case Formatter::Blob::BeginElement:
                {
                    auto newEle = ParseElement(formatter);
                    if (newEle == ~0u) break;
                    if (lastEle != ~0u) _elements[lastEle]._nextSibling = newEle;
                    lastEle = newEle;
                }
                continue;

            default:
                break;
            }

            break;
        }
        
        //      Sometimes we serialize in some elements as a "Document"
        //      In these cases, there may be more data in the file...
        // if (formatter.PeekNext() != Formatter::Blob::None)
        //     Throw(FormatException(
        //         "Unexpected trailing characters in StreamDOM", formatter.GetLocation()));
    }

    template <typename Formatter>
        Document<Formatter>::Document() 
    {
        _firstRootAttribute = ~0u;
    }

    template <typename Formatter>
        Document<Formatter>::~Document()
    {}

    template <typename Formatter>
        Document<Formatter>::Document(Document&& moveFrom)
    {
        _elements = std::move(moveFrom._elements);
        _attributes = std::move(moveFrom._attributes);
        _firstRootAttribute = moveFrom._firstRootAttribute; moveFrom._firstRootAttribute = ~0u;
    }

    template <typename Formatter>
        auto Document<Formatter>::operator=(Document&& moveFrom) -> Document&
    {
        _elements = std::move(moveFrom._elements);
        _attributes = std::move(moveFrom._attributes);
        _firstRootAttribute = moveFrom._firstRootAttribute; moveFrom._firstRootAttribute = ~0u;
        return *this;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename Formatter>
        auto DocElementHelper<Formatter>::Element(const value_type name[]) const -> DocElementHelper<Formatter>
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
        DocAttributeHelper<Formatter> DocElementHelper<Formatter>::Attribute(const value_type name[]) const
    {
        if (_index == ~0u) DocAttributeHelper<Formatter>();
        return DocAttributeHelper<Formatter>(FindAttribute(name), *_doc);
    }

    template<typename Formatter>
        auto DocElementHelper<Formatter>::FirstChild() const -> DocElementHelper<Formatter>
    {
        if (_index == ~0u) return DocElementHelper<Formatter>();
        auto& ele = _doc->_elements[_index];
        return DocElementHelper<Formatter>(ele._firstChild, *_doc);
    }

    template<typename Formatter>
        auto DocElementHelper<Formatter>::NextSibling() const -> DocElementHelper<Formatter>
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
        auto DocElementHelper<Formatter>::RawName() const -> typename Formatter::InteriorSection
    {
        if (_index == ~0u) return typename Formatter::InteriorSection();
        auto& ele = _doc->_elements[_index];
        return ele._name;
    }

    template<typename Formatter>
        unsigned DocElementHelper<Formatter>::FindAttribute(StringSection<value_type> name) const
    {
        assert(_index != ~0u);
        auto& ele = _doc->_elements[_index];
        for (unsigned a=ele._firstAttribute; a!=~0u;) {
            const auto& attrib = _doc->_attributes[a];
            if (XlEqString(attrib._name, name))
                return a;

            a=attrib._nextSibling;
        }

        return ~0u;
    }

    template<typename Formatter>
        DocAttributeHelper<Formatter> DocElementHelper<Formatter>::FirstAttribute() const
    {
        if (_index == ~0u) return DocAttributeHelper<Formatter>();
        const auto& e = _doc->_elements[_index];
        return DocAttributeHelper<Formatter>(e._firstAttribute, *_doc);
    }

    template<typename Formatter>
        DocElementHelper<Formatter>::DocElementHelper(unsigned elementIndex, const Document<Formatter>& doc)
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

    template<typename Formatter>
        auto DocAttributeHelper<Formatter>::Name() const -> std::basic_string<value_type>
    {
        if (_index == ~unsigned(0)) return std::basic_string<value_type>();
        return std::basic_string<value_type>(
            _doc->_attributes[_index]._name._start,
            _doc->_attributes[_index]._name._end);
    }

    template<typename Formatter>
        auto DocAttributeHelper<Formatter>::Value() const -> std::basic_string<value_type>
    {
        if (_index == ~unsigned(0)) return std::basic_string<value_type>();
        return std::basic_string<value_type>(
            _doc->_attributes[_index]._value._start,
            _doc->_attributes[_index]._value._end);
    }

    template<typename Formatter>
        typename Formatter::InteriorSection DocAttributeHelper<Formatter>::RawName() const
    {
        if (_index == ~unsigned(0)) return typename Formatter::InteriorSection();
        return _doc->_attributes[_index]._name;
    }

    template<typename Formatter>
        typename Formatter::InteriorSection DocAttributeHelper<Formatter>::RawValue() const
    {
        if (_index == ~unsigned(0)) return typename Formatter::InteriorSection();
        return _doc->_attributes[_index]._value;
    }

    template<typename Formatter>
        DocAttributeHelper<Formatter> DocAttributeHelper<Formatter>::Next() const
    {
        if (_index == ~0u) return DocAttributeHelper<Formatter>();
        const auto& a = _doc->_attributes[_index];
        return DocAttributeHelper<Formatter>(a._nextSibling, *_doc);
    }

    template<typename Formatter>
        DocAttributeHelper<Formatter>::DocAttributeHelper(
            unsigned attributeIndex, 
            const Document<Formatter>& doc)
    : _doc(&doc), _index(attributeIndex)
    {}

    template<typename Formatter>
        DocAttributeHelper<Formatter>::DocAttributeHelper() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    template class Document<InputStreamFormatter<utf8>>;
    template class Document<InputStreamFormatter<ucs2>>;
    template class Document<InputStreamFormatter<ucs4>>;
    template class Document<XmlInputStreamFormatter<utf8>>;

    template class DocElementHelper<InputStreamFormatter<utf8>>;
    template class DocElementHelper<InputStreamFormatter<ucs2>>;
    template class DocElementHelper<InputStreamFormatter<ucs4>>;
    template class DocElementHelper<XmlInputStreamFormatter<utf8>>;

    template class DocAttributeHelper<InputStreamFormatter<utf8>>;
    template class DocAttributeHelper<InputStreamFormatter<ucs2>>;
    template class DocAttributeHelper<InputStreamFormatter<ucs4>>;
    template class DocAttributeHelper<XmlInputStreamFormatter<utf8>>;

}
