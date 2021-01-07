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
        unsigned Document<Formatter>::ParseElement(Formatter& formatter, bool rootElement)
    {
        typename Formatter::InteriorSection section;
        if (!rootElement && !formatter.TryBeginElement(section))
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
                auto newElement = ParseElement(formatter, false);
                if (lastChild == ~0u) {
                    _elements[e]._firstChild = newElement;
                } else {
                    _elements[lastChild]._nextSibling = newElement;
                }
                lastChild = newElement;
            } else if (next == Formatter::Blob::AttributeName) {
                typename Formatter::InteriorSection name;
                typename Formatter::InteriorSection value;
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
                if (rootElement)
                    Throw(FormatException(
                        "Unexpected end element in document root element", formatter.GetLocation()));
                break;
            } else if (next == Formatter::Blob::None && rootElement) {
                // For the root element, we'll get "None" when we hit the end of the file,
                // at which point we should exit
                return e;
			} else if (next == Formatter::Blob::CharacterData) {
				typename Formatter::InteriorSection dummy;
                if (!formatter.TryCharacterData(dummy))
                    Throw(FormatException(
                        "Error while reading character data in StreamDOM", formatter.GetLocation()));
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
        Internal::DocElementMarker<Formatter> Document<Formatter>::RootElement() const
    {
        if (_elements.empty())
            return {};
        return Internal::DocElementMarker<Formatter>{0, *this};
    }

    template <typename Formatter>
        Document<Formatter>::Document(Formatter& formatter)
    {
        _elements.reserve(32);
        _attributes.reserve(64);

            // Parse the input formatter, building a tree
            // of elements and a list of attributes.
            // We can start with several top-level elements
        ParseElement(formatter, true);
    }

    template <typename Formatter>
        Document<Formatter>::Document() 
    {
    }

    template <typename Formatter>
        Document<Formatter>::~Document()
    {
        assert(_activeMarkerCount==0);
    }

    template <typename Formatter>
        Document<Formatter>::Document(Document&& moveFrom) never_throws
    {
        assert(!moveFrom._activeMarkerCount);
        _elements = std::move(moveFrom._elements);
        _attributes = std::move(moveFrom._attributes);
    }

    template <typename Formatter>
        auto Document<Formatter>::operator=(Document&& moveFrom) never_throws -> Document&
    {
        assert(!_activeMarkerCount && !moveFrom._activeMarkerCount);
        _elements = std::move(moveFrom._elements);
        _attributes = std::move(moveFrom._attributes);
        return *this;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    namespace Internal
    {
        template<typename Formatter>
            auto DocElementMarker<Formatter>::Element(const char_type name[]) const -> DocElementMarker<Formatter>
        {
            if (_index == ~0u) return {};
            auto& ele2 = _doc->_elements[_index];

            auto expectedNameLen = (ptrdiff_t)XlStringLen(name);
            for (unsigned e=ele2._firstChild; e!=~0u;) {
                const auto& ele = _doc->_elements[e];
                auto nameLen = ele._name._end - ele._name._start;
                if (nameLen == expectedNameLen && !XlComparePrefix(ele._name._start, name, nameLen))
                    return DocElementMarker<Formatter>{e, *_doc};

                e=ele._nextSibling;
            }

            return {};
        }

        template<typename Formatter>
            DocAttributeMarker<Formatter> DocElementMarker<Formatter>::Attribute(const char_type name[]) const
        {
            if (_index == ~0u) return {};
            return DocAttributeMarker<Formatter>{FindAttribute(name), *_doc};
        }

        template<typename Formatter>
            auto DocElementMarker<Formatter>::begin_children() const -> DocElementIterator<Formatter>
        {
            if (_index == ~0u) return {};
            auto& ele = _doc->_elements[_index];
            return DocElementIterator<Formatter>(
                DocElementMarker<Formatter>{ele._firstChild, *_doc});
        }

        template<typename Formatter>
            auto DocElementMarker<Formatter>::end_children() const -> DocElementIterator<Formatter>
        {
            return {};
        }

        template<typename Formatter>
            auto DocElementMarker<Formatter>::Name() const -> typename Formatter::InteriorSection
        {
            if (_index == ~0u) return {};
            auto& ele = _doc->_elements[_index];
            return ele._name;
        }

        template<typename Formatter>
            unsigned DocElementMarker<Formatter>::FindAttribute(StringSection<char_type> name) const
        {
            if (_index == ~0u) return ~0u;
            
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
            DocAttributeIterator<Formatter> DocElementMarker<Formatter>::begin_attributes() const
        {
            if (_index == ~0u) return {};
            const auto& e = _doc->_elements[_index];
            return DocAttributeIterator<Formatter>(
                DocAttributeMarker<Formatter>{e._firstAttribute, *_doc});
        }

        template<typename Formatter>
            DocAttributeIterator<Formatter> DocElementMarker<Formatter>::end_attributes() const
        {
            return {};
        }

        template<typename Formatter>
            DocElementMarker<Formatter>::DocElementMarker(unsigned elementIndex, const Document<Formatter>& doc)
        {
            _doc = &doc;
            _index = elementIndex;
            DEBUG_ONLY( ++_doc->_activeMarkerCount; )
        }

        template<typename Formatter>
            DocElementMarker<Formatter>::DocElementMarker()
        {
            _index = ~0u;
            _doc = nullptr;
        }

        template<typename Formatter>
            DocElementMarker<Formatter>::~DocElementMarker()
        {
            #if defined(_DEBUG)
                if (_doc) {
                    assert(_doc->_activeMarkerCount > 0);
                    --_doc->_activeMarkerCount;
                }
            #endif
        }

        template<typename Formatter>
            auto DocElementMarker<Formatter>::operator=(const DocElementMarker& cloneFrom) -> DocElementMarker&
        {
            #if defined(_DEBUG)
                if (_doc) {
                    assert(_doc->_activeMarkerCount > 0);
                    --_doc->_activeMarkerCount;
                }
            #endif
            _doc = cloneFrom._doc;
            _index = cloneFrom._index;
            #if defined(_DEBUG)
                if (_doc)
                    ++_doc->_activeMarkerCount;
            #endif
            return *this;
        }

        template<typename Formatter>
            DocElementMarker<Formatter>::DocElementMarker(const DocElementMarker& cloneFrom)
        {
            _doc = cloneFrom._doc;
            _index = cloneFrom._index;
            #if defined(_DEBUG)
                if (_doc)
                    ++_doc->_activeMarkerCount;
            #endif
        }

        template<typename Formatter>
            auto DocElementMarker<Formatter>::operator=(DocElementMarker&& moveFrom) never_throws -> DocElementMarker&
        {
            #if defined(_DEBUG)
                if (_doc) {
                    assert(_doc->_activeMarkerCount > 0);
                    --_doc->_activeMarkerCount;
                }
            #endif
            _doc = moveFrom._doc;
            _index = moveFrom._index;
            moveFrom._doc = nullptr;
            moveFrom._index = ~0u;
            return *this;
        }

        template<typename Formatter>
            DocElementMarker<Formatter>::DocElementMarker(DocElementMarker&& moveFrom) never_throws
        {
            _doc = moveFrom._doc;
            _index = moveFrom._index;
            moveFrom._doc = nullptr;
            moveFrom._index = ~0u;
        }

        template<typename Formatter>
            auto DocElementIterator<Formatter>::operator++() -> DocElementIterator<Formatter>&
        {
            if (_value._index != ~0u) {
                auto& ele = _value._doc->_elements[_value._index];
                _value._index = ele._nextSibling;
            }
            return *this;
        }

        template<typename Formatter>
            auto DocElementIterator<Formatter>::operator++(int) -> DocElementIterator<Formatter>&
        {
            if (_value._index != ~0u) {
                auto& ele = _value._doc->_elements[_value._index];
                _value._index = ele._nextSibling;
            }
            return *this;
        }

///////////////////////////////////////////////////////////////////////////////////////////////////

        template<typename Formatter>
            typename Formatter::InteriorSection DocAttributeMarker<Formatter>::Name() const
        {
            if (_index == ~unsigned(0)) return {};
            return _doc->_attributes[_index]._name;
        }

        template<typename Formatter>
            typename Formatter::InteriorSection DocAttributeMarker<Formatter>::Value() const
        {
            if (_index == ~unsigned(0)) return {};
            return _doc->_attributes[_index]._value;
        }

        template<typename Formatter>
            DocAttributeMarker<Formatter>::DocAttributeMarker(
                unsigned attributeIndex, 
                const Document<Formatter>& doc)
        : _doc(&doc), _index(attributeIndex)
        {
            DEBUG_ONLY( ++_doc->_activeMarkerCount; )
        }

        template<typename Formatter>
            DocAttributeMarker<Formatter>::DocAttributeMarker() 
        : _doc(nullptr), _index(~0u) {}

        template<typename Formatter>
            DocAttributeMarker<Formatter>::~DocAttributeMarker()
        {
            #if defined(_DEBUG)
                if (_doc) {
                    assert(_doc->_activeMarkerCount > 0);
                    --_doc->_activeMarkerCount;
                }
            #endif
        }

        template<typename Formatter>
            auto DocAttributeMarker<Formatter>::operator=(const DocAttributeMarker& cloneFrom) -> DocAttributeMarker&
        {
            #if defined(_DEBUG)
                if (_doc) {
                    assert(_doc->_activeMarkerCount > 0);
                    --_doc->_activeMarkerCount;
                }
            #endif
            _doc = cloneFrom._doc;
            _index = cloneFrom._index;
            #if defined(_DEBUG)
                if (_doc)
                    ++_doc->_activeMarkerCount;
            #endif
            return *this;
        }

        template<typename Formatter>
            DocAttributeMarker<Formatter>::DocAttributeMarker(const DocAttributeMarker& cloneFrom)
        {
            _doc = cloneFrom._doc;
            _index = cloneFrom._index;
            #if defined(_DEBUG)
                if (_doc)
                    ++_doc->_activeMarkerCount;
            #endif
        }

        template<typename Formatter>
            auto DocAttributeMarker<Formatter>::operator=(DocAttributeMarker&& moveFrom) never_throws -> DocAttributeMarker&
        {
            #if defined(_DEBUG)
                if (_doc) {
                    assert(_doc->_activeMarkerCount > 0);
                    --_doc->_activeMarkerCount;
                }
            #endif
            _doc = moveFrom._doc;
            _index = moveFrom._index;
            moveFrom._doc = nullptr;
            moveFrom._index = ~0u;
            return *this;
        }

        template<typename Formatter>
            DocAttributeMarker<Formatter>::DocAttributeMarker(DocAttributeMarker&& moveFrom) never_throws
        {
            _doc = moveFrom._doc;
            _index = moveFrom._index;
            moveFrom._doc = nullptr;
            moveFrom._index = ~0u;
        }

        template<typename Formatter>
            auto DocAttributeIterator<Formatter>::operator++() -> DocAttributeIterator<Formatter>&
        {
            if (_value._index != ~0u) {
                const auto& a = _value._doc->_attributes[_value._index];
                _value._index = a._nextSibling;
            }
            return *this;
        }

        template<typename Formatter>
            auto DocAttributeIterator<Formatter>::operator++(int) -> DocAttributeIterator<Formatter>&
        {
            if (_value._index != ~0u) {
                const auto& a = _value._doc->_attributes[_value._index];
                _value._index = a._nextSibling;
            }
            return *this;
        }

    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    template class Document<InputStreamFormatter<utf8>>;
    template class Document<InputStreamFormatter<ucs2>>;
    template class Document<InputStreamFormatter<ucs4>>;
    template class Document<InputStreamFormatter<char>>;
    template class Document<XmlInputStreamFormatter<utf8>>;

    template class Internal::DocElementMarker<InputStreamFormatter<utf8>>;
    template class Internal::DocElementMarker<InputStreamFormatter<ucs2>>;
    template class Internal::DocElementMarker<InputStreamFormatter<ucs4>>;
    template class Internal::DocElementMarker<InputStreamFormatter<char>>;
    template class Internal::DocElementMarker<XmlInputStreamFormatter<utf8>>;

    template class Internal::DocAttributeMarker<InputStreamFormatter<utf8>>;
    template class Internal::DocAttributeMarker<InputStreamFormatter<ucs2>>;
    template class Internal::DocAttributeMarker<InputStreamFormatter<ucs4>>;
    template class Internal::DocAttributeMarker<InputStreamFormatter<char>>;
    template class Internal::DocAttributeMarker<XmlInputStreamFormatter<utf8>>;
    
    template class Internal::DocElementIterator<InputStreamFormatter<utf8>>;
    template class Internal::DocElementIterator<InputStreamFormatter<ucs2>>;
    template class Internal::DocElementIterator<InputStreamFormatter<ucs4>>;
    template class Internal::DocElementIterator<InputStreamFormatter<char>>;
    template class Internal::DocElementIterator<XmlInputStreamFormatter<utf8>>;

    template class Internal::DocAttributeIterator<InputStreamFormatter<utf8>>;
    template class Internal::DocAttributeIterator<InputStreamFormatter<ucs2>>;
    template class Internal::DocAttributeIterator<InputStreamFormatter<ucs4>>;
    template class Internal::DocAttributeIterator<InputStreamFormatter<char>>;
    template class Internal::DocAttributeIterator<XmlInputStreamFormatter<utf8>>;

}
