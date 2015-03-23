// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/BlockSerializer.h"
#include "../Core/Types.h"
#include <string>
#include <vector>

namespace Serialization { class NascentBlockSerializer; }

namespace Utility
{
    namespace ImpliedTyping
    {
        enum class TypeCat : uint8 { Void, Bool, Int, UInt, Float };
        enum class TypeHint : uint8 { None, Vector, Color };
        class TypeDesc
        {
        public:
            TypeCat     _type;
            TypeHint    _typeHint;
            uint16      _arrayCount;

            void    Serialize(Serialization::NascentBlockSerializer& serializer) const;
            TypeDesc(TypeCat cat = TypeCat::UInt, uint16 arrayCount = 1, TypeHint hint = TypeHint::None);
            uint32 GetSize() const;
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
                const void* dest, size_t destSize);

        template <typename Type>
            Type Parse(const char expression[]);

        std::string AsString(const void* data, size_t dataSize, const TypeDesc&);
    }

        //////////////////////////////////////////////////////////////////
            //      P A R A M E T E R   B O X                       //
        //////////////////////////////////////////////////////////////////

            //      a handy abstraction to represent a number of 
            //      parameters held together. We must be able to
            //      quickly merge and filter values in this table.

    #pragma pack(push)
    #pragma pack(1)

    class ParameterBox
    {
    public:
        typedef uint32 ParameterNameHash;

        using TypeDesc = ImpliedTyping::TypeDesc;

        void SetParameter(const char name[], const void* data, const ImpliedTyping::TypeDesc& type);
        void SetParameter(const char name[], const char data[]);
        template<typename Type> void SetParameter(const char name[], Type value);
        template<typename Type> std::pair<bool, Type> GetParameter(const char name[]) const;
        template<typename Type> std::pair<bool, Type> GetParameter(ParameterNameHash name) const;

        uint64  GetHash() const;
        uint64  GetParameterNamesHash() const;
        uint64  TranslateHash(const ParameterBox& source) const;

        void    BuildStringTable(std::vector<std::pair<const char*, std::string>>& defines) const;
        void    OverrideStringTable(std::vector<std::pair<const char*, std::string>>& defines) const;

        void    MergeIn(const ParameterBox& source);

        static ParameterNameHash    MakeParameterNameHash(const std::string& name);
        static ParameterNameHash    MakeParameterNameHash(const char name[]);

        bool    ParameterNamesAreEqual(const ParameterBox& other) const;

        void    Serialize(Serialization::NascentBlockSerializer& serializer) const;

        ParameterBox();
        ParameterBox(std::initializer_list<std::pair<const char*, const char*>>);
        ParameterBox(ParameterBox&& moveFrom);
        ParameterBox& operator=(ParameterBox&& moveFrom);
        ~ParameterBox();
    private:
        mutable uint64      _cachedHash;
        mutable uint64      _cachedParameterNameHash;

        Serialization::Vector<ParameterNameHash>            _parameterHashValues;
        Serialization::Vector<std::pair<uint32, uint32>>    _offsets;
        Serialization::Vector<char>     _names;
        Serialization::Vector<uint8>    _values;
        Serialization::Vector<TypeDesc> _types;

        const void* GetValue(size_t index) const;
        uint64      CalculateHash() const;
        uint64      CalculateParameterNamesHash() const;
    };

    #pragma pack(pop)

}

using namespace Utility;
