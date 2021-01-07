// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "StreamFormatter.h"
#include "../StringUtils.h"     // (for StringSection)
#include "../Optional.h"
#include <vector>
#include <string>

#include "../Conversion.h"
#include "../ImpliedTyping.h"

namespace Utility
{
    class OutputStreamFormatter;

    namespace Internal
    {
        template<typename Formatter> class DocElementMarker;
        template<typename Formatter> class DocAttributeMarker;
        template<typename Formatter> class DocElementIterator;
        template<typename Formatter> class DocAttributeIterator;
    }

    template <typename Formatter>
        class Document
    {
    public:
        Internal::DocElementMarker<Formatter> RootElement() const;

        Document();
        Document(Formatter& formatter);
        ~Document();

        Document(Document&& moveFrom) never_throws;
        Document& operator=(Document&& moveFrom) never_throws;
    protected:
        using Section = typename Formatter::InteriorSection;
        struct AttributeDesc
        {
            Section _name, _value;
            unsigned _nextSibling;
        };

        struct ElementDesc
        {
            Section _name;
            unsigned _firstAttribute;
            unsigned _firstChild;
            unsigned _nextSibling;
        };
        
        std::vector<ElementDesc>    _elements;
        std::vector<AttributeDesc>  _attributes;

        #if defined(_DEBUG)
            mutable unsigned _activeMarkerCount = 0;
        #endif

        unsigned ParseElement(Formatter& formatter, bool rootElement);

        friend class Internal::DocElementMarker<Formatter>;
        friend class Internal::DocAttributeMarker<Formatter>;
        friend class Internal::DocElementIterator<Formatter>;
        friend class Internal::DocAttributeIterator<Formatter>;
    };

    namespace Internal
    {
        template<typename Formatter> class DocElementIterator;
        template<typename Formatter> class DocAttributeIterator;

        template<typename Formatter>
            struct DocElementMarker
        {
            using char_type = typename Formatter::value_type;
            using Section = typename Formatter::InteriorSection;
            
            Section Name() const;

            // Find children elements
            DocElementMarker Element(const char_type name[]) const;

            // Find & query attributes
            DocAttributeMarker<Formatter> Attribute(const char_type name[]) const;
            template<typename Type>
                Type Attribute(const char_type name[], const Type& def) const;
                
            template<typename Type>
                Type operator()(const char_type name[], const Type& def) const { return Attribute(name, def); }

            // Iterate over children elements
            DocElementIterator<Formatter> begin_children() const;
            DocElementIterator<Formatter> end_children() const;

            IteratorRange<DocElementIterator<Formatter>>
                children() const { return { begin_children(), end_children() }; }

            // Iterate over attributes
            DocAttributeIterator<Formatter> begin_attributes() const;
            DocAttributeIterator<Formatter> end_attributes() const;

            IteratorRange<DocAttributeIterator<Formatter>>
                attributes() const { return { begin_attributes(), end_attributes() }; }

            operator bool() const { return _index != ~0u; }
            bool operator!() const { return _index == ~0u; }

            friend bool operator==(const DocElementMarker& lhs, const DocElementMarker& rhs)
            {
                if (lhs._index != rhs._index)
                    return false;
                if (lhs._index == ~0u)
                    return true;
                return lhs._doc == rhs._doc;
            }

            friend bool operator!=(const DocElementMarker& lhs, const DocElementMarker& rhs)
            {
                if (lhs._index != rhs._index)
                    return true;
                if (lhs._index == ~0u)
                    return false;
                return lhs._doc != rhs._doc;
            }

            DocElementMarker& operator=(const DocElementMarker&);
            DocElementMarker(const DocElementMarker&);
            DocElementMarker& operator=(DocElementMarker&&) never_throws;
            DocElementMarker(DocElementMarker&&) never_throws;
            DocElementMarker();
            ~DocElementMarker();

        protected:
            DocElementMarker(unsigned elementIndex, const Document<Formatter>& doc);

            unsigned FindAttribute(StringSection<char_type> name) const;

            const Document<Formatter>* _doc;
            friend class DocElementIterator<Formatter>;
            friend class Document<Formatter>;
            unsigned _index;
        };
        
        template<typename Formatter>
            class DocElementIterator
        {
        public:
            using Value = DocElementMarker<Formatter>;

            Value& operator*()                  { return _value; }
            const Value& operator*() const      { return _value; }
            Value* operator->()                 { return &_value; }
            const Value* operator->() const     { return &_value; }

            DocElementIterator& operator++();
            DocElementIterator& operator++(int);

            friend bool operator==(
                const DocElementIterator<Formatter>& lhs,
                const DocElementIterator<Formatter>& rhs) { return lhs._value == rhs._value; }
            friend bool operator!=(
                const DocElementIterator<Formatter>& lhs,
                const DocElementIterator<Formatter>& rhs) { return lhs._value != rhs._value; }

            DocElementIterator() {}
        private:
            DocElementIterator(const Value& v) : _value(v) {}
            Value _value;
            friend class DocElementMarker<Formatter>;
        };

        template<typename Formatter>
            class DocAttributeMarker
        {
        public:
            using char_type = typename Formatter::value_type;

            template<typename Type>
                std::optional<Type> As() const;

            typename Formatter::InteriorSection Name() const;
            typename Formatter::InteriorSection Value() const;

            operator bool() const { return _index != ~0u; }
            bool operator!() const { return _index == ~0u; }

            friend bool operator==(const DocAttributeMarker& lhs, const DocAttributeMarker& rhs)
            {
                if (lhs._index != rhs._index)
                    return false;
                if (lhs._index == ~0u)
                    return true;
                return lhs._doc == rhs._doc;
            }

            friend bool operator!=(const DocAttributeMarker& lhs, const DocAttributeMarker& rhs)
            {
                if (lhs._index != rhs._index)
                    return true;
                if (lhs._index == ~0u)
                    return false;
                return lhs._doc != rhs._doc;
            }

            DocAttributeMarker& operator=(const DocAttributeMarker&);
            DocAttributeMarker(const DocAttributeMarker&);
            DocAttributeMarker& operator=(DocAttributeMarker&&) never_throws;
            DocAttributeMarker(DocAttributeMarker&&) never_throws;
            DocAttributeMarker();
            ~DocAttributeMarker();

        protected:
            const Document<Formatter>* _doc;
            unsigned _index;

            DocAttributeMarker(unsigned attributeIndex, const Document<Formatter>& doc);

            friend class DocElementMarker<Formatter>;
            friend class DocAttributeIterator<Formatter>;
        };

        template<typename Formatter>
            class DocAttributeIterator
        {
        public:
            using Value = DocAttributeMarker<Formatter>;

            Value& operator*()                  { return _value; }
            const Value& operator*() const      { return _value; }
            Value* operator->()                 { return &_value; }
            const Value* operator->() const     { return &_value; }

            DocAttributeIterator& operator++();
            DocAttributeIterator& operator++(int);

            friend bool operator==(
                const DocAttributeIterator<Formatter>& lhs,
                const DocAttributeIterator<Formatter>& rhs) { return lhs._value == rhs._value; }
            friend bool operator!=(
                const DocAttributeIterator<Formatter>& lhs,
                const DocAttributeIterator<Formatter>& rhs) { return lhs._value != rhs._value; }

            DocAttributeIterator() {}
        private:
            DocAttributeIterator(const Value& v) : _value(v) {}
            Value _value;
            friend class DocElementMarker<Formatter>;
        };
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    namespace ImpliedTyping
    {
        template <typename Type>
            std::optional<Type> Parse(const char* expressionBegin, const char* expressionEnd);
    }
    
    namespace Internal
    {
        template<typename Formatter>
            template<typename Type>
                Type DocElementMarker<Formatter>::Attribute(const char_type name[], const Type& def) const
        {
            auto temp = Attribute(name).template As<Type>();
            if (temp.has_value()) return temp.value();
            return def;
        }

        template<typename Formatter>
            template<typename Type>
                std::optional<Type> DocAttributeMarker<Formatter>::As() const
        {
            if (_index == ~unsigned(0)) return {};
            const auto& attrib = _doc->_attributes[_index];
            return ImpliedTyping::Parse<Type>(attrib._value);
        }
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

#if 0
    template<typename Type, typename Formatter>
        inline Type Deserialize(const typename Internal::DocElementIterator<Formatter>::Value& ele, const typename Formatter::char_type name[], const Type& obj)
    {
        return ele(name, obj);
    }

    template<typename FirstType, typename SecondType, typename Formatter>
        inline std::pair<FirstType, SecondType> Deserialize(const typename Internal::DocElementIterator<Formatter>::Value& ele, const typename Formatter::char_type name[], const std::pair<FirstType, SecondType>& def)
    {
        auto subEle = ele.Element(name);
        if (!subEle) return def;
        return std::make_pair(
            Deserialize(subEle, u("First"), def.first),
            Deserialize(subEle, u("Second"), def.second));
    }
#endif

}

using namespace Utility;
