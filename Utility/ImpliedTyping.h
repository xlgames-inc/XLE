// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Optional.h"
#include "IteratorUtils.h"
#include "StringUtils.h"            // for StringSection

namespace Utility
{
    namespace ImpliedTyping
    {
        enum class TypeCat : uint8_t { Void, Bool, Int8, UInt8, Int16, UInt16, Int32, UInt32, Int64, UInt64, Float, Double };
        enum class TypeHint : uint8_t { None, Vector, Matrix, Color, String };
        enum class CastType : uint8_t { Narrowing, Equal, Widening};
        class TypeDesc
        {
        public:
            TypeCat     _type;
            TypeHint    _typeHint;
            uint16_t    _arrayCount;

            TypeDesc(TypeCat cat = TypeCat::UInt32, uint16_t arrayCount = 1, TypeHint hint = TypeHint::None);
            uint32_t GetSize() const;

            template<typename Stream> void SerializeMethod(Stream& serializer) const;
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
                StringSection<CharType> expression,
                void* dest, size_t destSize);

        template <typename Type>
            std::optional<Type> Parse(StringSection<char> expression);

        template <typename Type>
            std::optional<Type> Parse(StringSection<utf8> expression);

        bool Cast(
            IteratorRange<void*> dest, TypeDesc destType,
            IteratorRange<const void*> src, TypeDesc srcType);
        
        CastType CalculateCastType(TypeCat testType, TypeCat againstType);

        std::string AsString(const void* data, size_t dataSize, const TypeDesc&, bool strongTyping = false);
        std::string AsString(IteratorRange<const void*> data, const TypeDesc&, bool strongTyping = false);

        template<typename Type>
            inline std::string AsString(const Type& type, bool strongTyping = false)
            {
                return AsString(&type, sizeof(Type), TypeOf<Type>(), strongTyping);
            }
        
        void Cleanup();
    }
}

