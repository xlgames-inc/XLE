// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "UTFUtils.h"
#include "Streams/Serialization.h"
#include "../Core/Types.h"
#include <string>
#include <vector>

namespace Utility
{
    namespace ImpliedTyping
    {
        enum class TypeCat : uint8 { Void, Bool, Int8, UInt8, Int16, UInt16, Int32, UInt32, Float };
        enum class TypeHint : uint8 { None, Vector, Matrix, Color, String };
        class TypeDesc
        {
        public:
            TypeCat     _type;
            TypeHint    _typeHint;
            uint16      _arrayCount;

            TypeDesc(TypeCat cat = TypeCat::UInt32, uint16 arrayCount = 1, TypeHint hint = TypeHint::None);
            uint32 GetSize() const;

            template<typename Stream> void Serialize(Stream& serializer) const;
            friend bool operator==(const TypeDesc& lhs, const TypeDesc& rhs);
        };

        /// Calculate type of an object given in string form.
        /// Object should be formatted in one of the following C++ like patterns:
        /// 
        ///  "1u" (or "1ui" or "1ul", etc)
        ///  "1b" (or "true" or "false")
        ///  ".3" (or "0.3f", etc)
        ///  "{1u, 2u, 3u}" (or "[1u, 2u, 3u]")
        ///  "{1u, 2u, 3u}c" or "{1u, 2u, 3u}v"
        ///
        /// This is intended for storing common basic types in text files, and 
        /// for use while entering data in tools. We want the type of the data to
        /// be implied by the string representing the data (without needing an
        /// extra field to describe the type).
        ///
        /// This kind of thing is useful when interfacing with scripting languages
        /// like HLSL and Lua. There are only a few basic types that we need
        /// to support.
        ///
        /// But sometimes we also want to had hints for out to interpret the data.
        /// For example, 3 floats could be a vector or a colour. We will use C++
        /// like postfix characters for this (eg, "{1,1,1}c" is a color)
        TypeDesc TypeOf(const char expression[]);
        template<typename Type> TypeDesc TypeOf();

        template<typename CharType>
            TypeDesc Parse(
                const CharType expressionBegin[], 
                const CharType expressionEnd[],
                void* dest, size_t destSize);

        template <typename Type>
            std::pair<bool, Type> Parse(const char expression[]);

        template <typename Type>
            std::pair<bool, Type> Parse(const char* expressionBegin, const char* expressionEnd);

        template <typename Type>
            std::pair<bool, Type> Parse(const utf8* expressionBegin, const utf8* expressionEnd);

        bool Cast(
            void* dest, size_t destSize, TypeDesc destType,
            const void* src, TypeDesc srcType);

        std::string AsString(const void* data, size_t dataSize, const TypeDesc&, bool strongTyping = false);

        template<typename Type>
            inline std::string AsString(const Type& type, bool strongTyping = false)
            {
                return AsString(&type, sizeof(Type), TypeOf<Type>(), strongTyping);
            }
    }

    class OutputStreamFormatter;
    template<typename CharType> class InputStreamFormatter;

///////////////////////////////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////
            //      P A R A M E T E R   B O X                       //
        //////////////////////////////////////////////////////////////////

            //      a handy abstraction to represent a number of 
            //      parameters held together. We must be able to
            //      quickly merge and filter values in this table.

    #pragma pack(push)
    #pragma pack(1)

    #define T1(Type) template<typename Type>

    class ParameterBox
    {
    public:
        typedef uint32 ParameterNameHash;

        class ParameterName
        {
        public:
            ParameterNameHash _hash;

            ParameterName(const std::basic_string<utf8>& name);
            ParameterName(const utf8 name[]);
            ParameterName(const char name[]);
            ParameterName(ParameterNameHash hash);
        };

        static ParameterNameHash    MakeParameterNameHash(const std::basic_string<utf8>& name);
        static ParameterNameHash    MakeParameterNameHash(const utf8 name[]);
        static ParameterNameHash    MakeParameterNameHash(const char name[]);

        using TypeDesc = ImpliedTyping::TypeDesc;

        ////////////////////////////////////////////////////////////////////////////////////////
            //      S E T                                                   //
        ////////////////////////////////////////////////////////////////////////////////////////

        void            SetParameter(const utf8 name[], const void* data, const TypeDesc& type);
        void            SetParameter(const utf8 name[], const char* stringDataBegin, const char* stringDataEnd);
        void            SetParameter(const utf8 name[], const char* stringDataBegin);
        void            SetParameter(const utf8 name[], const std::string& stringData);
        T1(Type) void   SetParameter(const utf8 name[], Type value);
        
        ////////////////////////////////////////////////////////////////////////////////////////
            //      G E T                                                   //
        ////////////////////////////////////////////////////////////////////////////////////////

        T1(Type) std::pair<bool, Type>  GetParameter(ParameterName name) const;
        T1(Type) Type   GetParameter(ParameterName name, const Type& def) const;
        bool            GetParameter(ParameterName name, void* dest, const TypeDesc& destType) const;
        bool            HasParameter(ParameterName name) const;
        TypeDesc        GetParameterType(ParameterName name) const;

        T1(CharType) std::basic_string<CharType> GetString(ParameterName name) const;
        T1(CharType) bool   GetString(ParameterName name, CharType dest[], size_t destCount) const;

        ////////////////////////////////////////////////////////////////////////////////////////
            //      H A S H   V A L U E S                                   //
        ////////////////////////////////////////////////////////////////////////////////////////

        uint64  GetHash() const;
        uint64  GetParameterNamesHash() const;
        uint64  CalculateFilteredHashValue(const ParameterBox& source) const;
        bool    AreParameterNamesEqual(const ParameterBox& other) const;

        ////////////////////////////////////////////////////////////////////////////////////////
            //      M E R G I N G   &   I T E R A T O R                     //
        ////////////////////////////////////////////////////////////////////////////////////////

        void    MergeIn(const ParameterBox& source);

        class Iterator
        {
        public:
            bool                IsEnd() const;

            const utf8*         Name() const;
            const void*         RawValue() const;
            const TypeDesc&     Type() const;
            ParameterNameHash   HashName() const;

            const void*         ValueTableEnd() const;

            void operator++();

        private:
            size_t                  _index;
            const ParameterBox*     _box;

            Iterator(const ParameterBox& box, size_t index);
            Iterator();
            friend class ParameterBox;
        };

        Iterator    Begin() const;
        Iterator    At(size_t index) const;
        size_t      GetCount() const;

        ////////////////////////////////////////////////////////////////////////////////////////
            //      S E R I A L I S A T I O N                               //
        ////////////////////////////////////////////////////////////////////////////////////////

        template<typename CharType>
            void    Serialize(OutputStreamFormatter& stream) const;

        template<typename Stream> void Serialize(Stream& serializer) const;

        ParameterBox();
        ParameterBox(std::initializer_list<std::pair<const utf8*, const char*>>);
        template<typename CharType>
            ParameterBox(InputStreamFormatter<CharType>& stream, 
                const void* defaultValue = nullptr, 
                const ImpliedTyping::TypeDesc& defaultValueType = ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::Void, 0u));
        ParameterBox(ParameterBox&& moveFrom);
        ParameterBox& operator=(ParameterBox&& moveFrom);
        ~ParameterBox();
    private:
        mutable uint64      _cachedHash;
        mutable uint64      _cachedParameterNameHash;

        SerializableVector<ParameterNameHash>            _hashNames;
        SerializableVector<std::pair<uint32, uint32>>    _offsets;
        SerializableVector<utf8>         _names;
        SerializableVector<uint8>        _values;
        SerializableVector<TypeDesc>     _types;

        const void*         GetValue(size_t index) const;
        uint64              CalculateHash() const;
        uint64              CalculateParameterNamesHash() const;
    };

    #pragma pack(pop)

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename Type> 
        Type ParameterBox::GetParameter(ParameterName name, const Type& def) const
    {
        auto q = GetParameter<Type>(name);
        if (q.first) return q.second;
        return def;
    }

    namespace ImpliedTyping
    {
        template<typename Stream>
            void TypeDesc::Serialize(Stream& serializer) const
        {
            ::Serialize(serializer, *(uint32*)this);
        }
    }

    inline ParameterBox::ParameterName::ParameterName(const std::basic_string<utf8>& name)
    {
        _hash = ParameterBox::MakeParameterNameHash(name);
    }

    inline ParameterBox::ParameterName::ParameterName(const utf8 name[])
    {
        _hash = ParameterBox::MakeParameterNameHash(name);
    }

    inline ParameterBox::ParameterName::ParameterName(const char name[])
    {
        _hash = ParameterBox::MakeParameterNameHash(name);
    }

    inline ParameterBox::ParameterName::ParameterName(ParameterNameHash hash)
    {
        _hash = hash;
    }

    template<typename Stream>
        void ParameterBox::Serialize(Stream& serializer) const
    {
        ::Serialize(serializer, _cachedHash);
        ::Serialize(serializer, _cachedParameterNameHash);
        ::Serialize(serializer, _hashNames);
        ::Serialize(serializer, _offsets);
        ::Serialize(serializer, _names);
        ::Serialize(serializer, _values);
        ::Serialize(serializer, _types);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    inline auto ParameterBox::Begin() const -> Iterator
    {
        return Iterator(*this, 0);
    }

    inline auto ParameterBox::At(size_t index) const -> Iterator
    {
        if (index >= _offsets.size()) return Iterator();
        return Iterator(*this, index);
    }

    inline bool        ParameterBox::Iterator::IsEnd() const
    {
        return _index >= _box->_offsets.size();
    }

    inline const utf8* ParameterBox::Iterator::Name() const
    {
        return &_box->_names[_box->_offsets[_index].first];
    }

    inline auto        ParameterBox::Iterator::Type() const -> const TypeDesc&
    {
        return _box->_types[_index];
    }

    inline auto   ParameterBox::Iterator::HashName() const -> ParameterNameHash
    {
        return _box->_hashNames[_index];
    }

    inline void ParameterBox::Iterator::operator++()
    {
        ++_index;
    }

    inline ParameterBox::Iterator::Iterator(const ParameterBox& box, size_t index)
    : _box(&box), _index(index)
    {}

    inline ParameterBox::Iterator::Iterator() : _box(nullptr), _index(0) {}

    using StringTable = std::vector<std::pair<const utf8*, std::string>>;
    void    BuildStringTable(StringTable& defines, const ParameterBox& box);
    void    OverrideStringTable(StringTable& defines, const ParameterBox& box);
}

using namespace Utility;
