// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../PtrUtils.h"    // (for Default)
#include "../StringUtils.h" // (for StringSection)
#include <vector>
#include <string>

namespace Utility
{
    class OutputStreamFormatter;

    template<typename Formatter>
        class DocElementHelper;

    template<typename Formatter>
        class DocAttributeHelper;

    template <typename Formatter>
        class Document
    {
    public:
        using value_type = typename Formatter::value_type;
        using Section = typename Formatter::InteriorSection;

        DocAttributeHelper<Formatter> Attribute(const value_type name[]) const;
        DocElementHelper<Formatter> Element(const value_type name[]) const;

        DocElementHelper<Formatter> FirstChild() const;
        DocAttributeHelper<Formatter> FirstAttribute() const;

        template<typename Type>
            Type Attribute(const value_type name[], const Type& def) const;

        template<typename Type>
            Type operator()(const value_type name[], const Type& def) const { return Attribute(name, def); }

        Document();
        Document(Formatter& formatter);
        ~Document();

        Document(Document&& moveFrom) never_throws;
        Document& operator=(Document&& moveFrom) never_throws;
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
        unsigned                    _firstRootAttribute;

        unsigned ParseElement(Formatter& formatter);

        unsigned FindAttribute(StringSection<value_type> name) const;

        friend class DocElementHelper<Formatter>;
        friend class DocAttributeHelper<Formatter>;
    };

    template<typename Formatter>
        class DocElementHelper
    {
    public:
        using value_type = typename Formatter::value_type;

        DocElementHelper Element(const value_type name[]) const;
        DocElementHelper FirstChild() const;
        DocElementHelper NextSibling() const;

        DocAttributeHelper<Formatter> Attribute(const value_type name[]) const;
        DocAttributeHelper<Formatter> FirstAttribute() const;

        typename Formatter::InteriorSection Name() const;

        template<typename Type>
            Type Attribute(const value_type name[], const Type& def) const;
            
        template<typename Type>
            Type operator()(const value_type name[], const Type& def) const { return Attribute(name, def); }

        operator bool() const { return _index != ~0u; }
        bool operator!() const { return _index == ~0u; }
    protected:
        const Document<Formatter>* _doc;
        unsigned _index;

        DocElementHelper(unsigned elementIndex, const Document<Formatter>& doc);
        DocElementHelper();

        unsigned FindAttribute(StringSection<value_type> name) const;

        friend class Document<Formatter>;
    };

    template<typename Formatter>
        class DocAttributeHelper
    {
    public:
        using value_type = typename Formatter::value_type;

        template<typename Type>
            std::pair<bool, Type> As() const;

        typename Formatter::InteriorSection Name() const;
        typename Formatter::InteriorSection Value() const;

        DocAttributeHelper<Formatter> Next() const;

        operator bool() const { return _index != ~0u; }
        bool operator!() const { return _index == ~0u; }
    protected:
        const Document<Formatter>* _doc;
        unsigned _index;

        DocAttributeHelper(unsigned attributeIndex, const Document<Formatter>& doc);
        DocAttributeHelper();

        friend class DocElementHelper<Formatter>;
        friend class Document<Formatter>;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename Formatter>
        template<typename Type>
            Type Document<Formatter>::Attribute(const value_type name[], const Type& def) const
    {
        auto temp = Attribute(name).As<Type>();
        if (temp.first) return temp.second;
        return def;
    }

    template<typename Formatter>
        template<typename Type>
            Type DocElementHelper<Formatter>::Attribute(const value_type name[], const Type& def) const
    {
        auto temp = Attribute(name).As<Type>();
        if (temp.first) return temp.second;
        return def;
    }

    template<typename Formatter>
        template<typename Type>
            std::pair<bool, Type> DocAttributeHelper<Formatter>::As() const
    {
        if (_index == ~unsigned(0)) return std::make_pair(false, Default<Type>());
        const auto& attrib = _doc->_attributes[_index];
        return ImpliedTyping::Parse<Type>(attrib._value._start, attrib._value._end);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

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

    template<typename CharType>
        inline void Serialize(OutputStreamFormatter& formatter, const CharType name[], const std::basic_string<CharType>& str)
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
