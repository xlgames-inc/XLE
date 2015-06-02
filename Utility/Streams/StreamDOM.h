// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "StreamFormatter.h"
#include <vector>
#include <string>

namespace Utility
{
    template <typename CharType>
        class Document
    {
    public:
        using Section = typename InputStreamFormatter<CharType>::InteriorSection;

        class ElementHelper;
        ElementHelper Element(const CharType name[]);

        Document(InputStreamFormatter<CharType>& formatter);
        ~Document();
    protected:
        class AttributeDesc
        {
        public:
            Section _name, _value;
            unsigned _nextSibling;
        };

        class ElementDesc
        {
        public:
            Section _name;
            unsigned _firstAttribute;
            unsigned _firstChild;
            unsigned _nextSibling;
        };
        
        std::vector<ElementDesc>    _elements;
        std::vector<AttributeDesc>  _attributes;

        unsigned ParseElement(InputStreamFormatter<CharType>& formatter);
    };

    template<typename CharType>
        class Document<CharType>::ElementHelper
    {
    public:
        template<typename Type>
            std::pair<bool, Type> Attribute(const CharType name[]);

        template<typename Type>
            Type Attribute(const CharType name[], const Type& def);

        ElementHelper Element(const CharType name[]);
        ElementHelper FirstChild();
        ElementHelper NextSibling();

        std::basic_string<CharType> Name() const;
        std::basic_string<CharType> AttributeOrEmpty(const CharType name[]);

        template<typename Type>
            Type operator()(const CharType name[], const Type& def) { return Attribute(name, def); }

        template<typename Type>
            std::basic_string<CharType> operator[](const CharType name[]) { return Attribute(name); }

        operator bool() const { return _index != ~0u; }
    protected:
        Document<CharType>* _doc;
        unsigned _index;

        ElementHelper(unsigned elementIndex, Document<CharType>& doc);
        ElementHelper();

        unsigned FindAttribute(const CharType* nameStart, const CharType* nameEnd);

        friend class Document<CharType>;
    };

    template<typename Type> Type Default() { return Type(); }
    template<typename Type, typename std::enable_if<std::is_pointer<Type>::value>::type* = nullptr>
        Type* Default() { return nullptr; }

    template<typename CharType>
        template<typename Type>
            std::pair<bool, Type> Document<CharType>::ElementHelper::Attribute(const CharType name[])
    {
        if (_index == ~0u) std::make_pair(false, Default<Type>());
        auto a = FindAttribute(name, &name[XlStringLen(name)]);
        if (a == ~0u) return std::make_pair(false, Default<Type>());
        const auto& attrib = _doc->_attributes[a];
        return ImpliedTyping::Parse<Type>(attrib._value._start, attrib._value._end);
    }

    template<typename CharType>
        template<typename Type>
            Type Document<CharType>::ElementHelper::Attribute(const CharType name[], const Type& def)
    {
        auto temp = Attribute<Type>(name);
        if (temp.first) return std::move(temp.second);
        return def;
    }
}

using namespace Utility;
