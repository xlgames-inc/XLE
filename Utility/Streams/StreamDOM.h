// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <vector>
#include <string>

namespace Utility
{
    class OutputStreamFormatter;

    template<typename Formatter>
        class DocElementHelper;

    template <typename Formatter>
        class Document
    {
    public:
        using value_type = typename Formatter::value_type;
        using Section = typename Formatter::InteriorSection;

        DocElementHelper<Formatter> Element(const value_type name[]);

        Document(Formatter& formatter);
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

        unsigned ParseElement(Formatter& formatter);

        friend class DocElementHelper<Formatter>;
    };

    template<typename Formatter>
        class DocElementHelper
    {
    public:
        using value_type = typename Formatter::value_type;

        template<typename Type>
            std::pair<bool, Type> Attribute(const value_type name[]);

        template<typename Type>
            Type Attribute(const value_type name[], const Type& def);

        DocElementHelper Element(const value_type name[]);
        DocElementHelper FirstChild();
        DocElementHelper NextSibling();

        std::basic_string<value_type> Name() const;
        std::basic_string<value_type> AttributeOrEmpty(const value_type name[]);

        template<typename Type>
            Type operator()(const value_type name[], const Type& def) { return Attribute(name, def); }

        template<typename Type>
            std::basic_string<value_type> operator[](const value_type name[]) { return Attribute(name); }

        operator bool() const { return _index != ~0u; }
        bool operator!() const { return _index == ~0u; }
    protected:
        Document<Formatter>* _doc;
        unsigned _index;

        DocElementHelper(unsigned elementIndex, Document<Formatter>& doc);
        DocElementHelper();

        unsigned FindAttribute(const value_type* nameStart, const value_type* nameEnd);

        friend class Document<Formatter>;
    };

    template<typename Type> Type Default() { return Type(); }
    template<typename Type, typename std::enable_if<std::is_pointer<Type>::value>::type* = nullptr>
        Type* Default() { return nullptr; }

    template<typename Formatter>
        template<typename Type>
            std::pair<bool, Type> DocElementHelper<Formatter>::Attribute(const value_type name[])
    {
        if (_index == ~0u) std::make_pair(false, Default<Type>());
        auto a = FindAttribute(name, &name[XlStringLen(name)]);
        if (a == ~0u) return std::make_pair(false, Default<Type>());
        const auto& attrib = _doc->_attributes[a];
        return ImpliedTyping::Parse<Type>(attrib._value._start, attrib._value._end);
    }

    template<typename Formatter>
        template<typename Type>
            Type DocElementHelper<Formatter>::Attribute(const value_type name[], const Type& def)
    {
        auto temp = Attribute<Type>(name);
        if (temp.first) return std::move(temp.second);
        return def;
    }

    template<typename Type, typename CharType>
        inline void Serialize(OutputStreamFormatter& formatter, const CharType name[], const Type& obj)
    {
        formatter.WriteAttribute(
            name, 
            Conversion::Convert<std::basic_string<CharType>>(ImpliedTyping::AsString(obj, true)));
    }

    template<typename CharType>
        inline void Serialize(OutputStreamFormatter& formatter, const CharType name[], const CharType str[])
    {
        formatter.WriteAttribute(name, str);
    }

    template<typename FirstType, typename SecondType, typename CharType>
        inline void Serialize(OutputStreamFormatter& formatter, const CharType name[], const std::pair<FirstType, SecondType>& obj)
    {
        auto ele = formatter.BeginElement(name);
        Serialize(formatter, u("First"), obj.first);
        Serialize(formatter, u("Second"), obj.second);
        formatter.EndElement(ele);
    }

    template<typename Type, typename Formatter>
        inline Type Deserialize(DocElementHelper<Formatter>& ele, const typename Formatter::value_type name[], const Type& obj)
    {
        return ele(name, obj);
    }

    template<typename FirstType, typename SecondType, typename Formatter>
        inline std::pair<FirstType, SecondType> Deserialize(DocElementHelper<Formatter>& ele, const typename Formatter::value_type name[], const std::pair<FirstType, SecondType>& def)
    {
        auto subEle = ele.Element(name);
        if (!subEle) return def;
        return std::make_pair(
            Deserialize(subEle, u("First"), def.first),
            Deserialize(subEle, u("Second"), def.second));
    }
}

using namespace Utility;
