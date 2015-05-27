// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ParameterBox.h"
#include "MemoryUtils.h"
#include "PtrUtils.h"
#include "StringUtils.h"
#include "IteratorUtils.h"
#include "StringFormat.h"
#include "Conversion.h"
#include "Streams/StreamFormatter.h"
#include "../ConsoleRig/Log.h"
#include <algorithm>
#include <utility>
#include <regex>

// namespace std
// {
//     template<> class regex_traits<utf8> : public regex_traits<char> {};
//     // template<> class regex_traits<ucs2> : public regex_traits<char16_t> {};
//     // template<> class regex_traits<ucs4> : public regex_traits<char32_t> {};
// }

namespace Utility
{
    // XL_UTILITY_API bool     XlAtoBool(const utf8* str, const utf8** end_ptr = 0);
    // XL_UTILITY_API int32    XlAtoI32 (const utf8* str, const utf8** end_ptr = 0, int radix = 10);
    // XL_UTILITY_API int64    XlAtoI64 (const utf8* str, const utf8** end_ptr = 0, int radix = 10);
    // XL_UTILITY_API uint32   XlAtoUI32(const utf8* str, const utf8** end_ptr = 0, int radix = 10);
    // XL_UTILITY_API uint64   XlAtoUI64(const utf8* str, const utf8** end_ptr = 0, int radix = 10);
    // XL_UTILITY_API f32      XlAtoF32 (const utf8* str, const utf8** end_ptr = 0);
    // XL_UTILITY_API f64      XlAtoF64 (const utf8* str, const utf8** end_ptr = 0);
    // 
    // XL_UTILITY_API bool     XlAtoBool(const ucs2* str, const ucs2** end_ptr = 0);
    // XL_UTILITY_API int32    XlAtoI32 (const ucs2* str, const ucs2** end_ptr = 0, int radix = 10);
    // XL_UTILITY_API int64    XlAtoI64 (const ucs2* str, const ucs2** end_ptr = 0, int radix = 10);
    // XL_UTILITY_API uint32   XlAtoUI32(const ucs2* str, const ucs2** end_ptr = 0, int radix = 10);
    // XL_UTILITY_API uint64   XlAtoUI64(const ucs2* str, const ucs2** end_ptr = 0, int radix = 10);
    // XL_UTILITY_API f32      XlAtoF32 (const ucs2* str, const ucs2** end_ptr = 0);
    // XL_UTILITY_API f64      XlAtoF64 (const ucs2* str, const ucs2** end_ptr = 0);
    // 
    // XL_UTILITY_API bool     XlAtoBool(const ucs4* str, const ucs4** end_ptr = 0);
    // XL_UTILITY_API int32    XlAtoI32 (const ucs4* str, const ucs4** end_ptr = 0, int radix = 10);
    // XL_UTILITY_API int64    XlAtoI64 (const ucs4* str, const ucs4** end_ptr = 0, int radix = 10);
    // XL_UTILITY_API uint32   XlAtoUI32(const ucs4* str, const ucs4** end_ptr = 0, int radix = 10);
    // XL_UTILITY_API uint64   XlAtoUI64(const ucs4* str, const ucs4** end_ptr = 0, int radix = 10);
    // XL_UTILITY_API f32      XlAtoF32 (const ucs4* str, const ucs4** end_ptr = 0);
    // XL_UTILITY_API f64      XlAtoF64 (const ucs4* str, const ucs4** end_ptr = 0);

    static const unsigned NativeRepMaxSize = MaxPath * 4;

    namespace ImpliedTyping
    {
        uint32 TypeDesc::GetSize() const
        {
            switch (_type) {
            case TypeCat::Bool: return sizeof(bool)*std::max(1u,unsigned(_arrayCount));

            case TypeCat::Int8:
            case TypeCat::UInt8: return sizeof(uint8)*std::max(1u,unsigned(_arrayCount));

            case TypeCat::Int16:
            case TypeCat::UInt16: return sizeof(uint16)*std::max(1u,unsigned(_arrayCount));

            case TypeCat::Int32:
            case TypeCat::UInt32:
            case TypeCat::Float: return sizeof(unsigned)*std::max(1u,unsigned(_arrayCount));
            case TypeCat::Void:
            default: return 0;
            }
        }

        void TypeDesc::Serialize(Serialization::NascentBlockSerializer& serializer) const
        {
            Serialization::Serialize(serializer, *(uint32*)this);
        }

        bool operator==(const TypeDesc& lhs, const TypeDesc& rhs)
        {
                // (note -- ignoring type hint for this comparison (because the hint isn't actually related to the structure of the data)
            return lhs._type == rhs._type
                && lhs._arrayCount == rhs._arrayCount;
        }


        TypeDesc::TypeDesc(TypeCat cat, uint16 arrayCount, TypeHint hint)
        : _type(cat)
        , _typeHint(hint)
        , _arrayCount(arrayCount)
        {}

        template<> TypeDesc TypeOf<uint32>()        { return TypeDesc(TypeCat::UInt32); }
        template<> TypeDesc TypeOf<int32>()         { return TypeDesc(TypeCat::Int32); }
        template<> TypeDesc TypeOf<uint16>()        { return TypeDesc(TypeCat::UInt16); }
        template<> TypeDesc TypeOf<int16>()         { return TypeDesc(TypeCat::Int16); }
        template<> TypeDesc TypeOf<uint8>()         { return TypeDesc(TypeCat::UInt8); }
        template<> TypeDesc TypeOf<int8>()          { return TypeDesc(TypeCat::Int8); }
        template<> TypeDesc TypeOf<bool>()          { return TypeDesc(TypeCat::Bool); }
        template<> TypeDesc TypeOf<float>()         { return TypeDesc(TypeCat::Float); }
        template<> TypeDesc TypeOf<void>()          { return TypeDesc(TypeCat::Void); }
        template<> TypeDesc TypeOf<Float2>()        { return TypeDesc(TypeCat::Float, 2, TypeHint::Vector); }
        template<> TypeDesc TypeOf<Float3>()        { return TypeDesc(TypeCat::Float, 3, TypeHint::Vector); }
        template<> TypeDesc TypeOf<Float4>()        { return TypeDesc(TypeCat::Float, 4, TypeHint::Vector); }
        template<> TypeDesc TypeOf<Float3x3>()      { return TypeDesc(TypeCat::Float, 9, TypeHint::Matrix); }
        template<> TypeDesc TypeOf<Float3x4>()      { return TypeDesc(TypeCat::Float, 12, TypeHint::Matrix); }
        template<> TypeDesc TypeOf<Float4x4>()      { return TypeDesc(TypeCat::Float, 16, TypeHint::Matrix); }
        template<> TypeDesc TypeOf<UInt2>()         { return TypeDesc(TypeCat::UInt32, 2, TypeHint::Vector); }
        template<> TypeDesc TypeOf<UInt3>()         { return TypeDesc(TypeCat::UInt32, 3, TypeHint::Vector); }
        template<> TypeDesc TypeOf<UInt4>()         { return TypeDesc(TypeCat::UInt32, 4, TypeHint::Vector); }
        template<> TypeDesc TypeOf<const char*>()   { return TypeDesc(TypeCat::UInt8, (uint16)~uint16(0), TypeHint::String); }

        TypeDesc TypeOf(const char expression[]) 
        {
                // not implemented
            assert(0);
            return TypeDesc();
        }

        bool Cast(
            void* dest, size_t destSize, TypeDesc destType,
            const void* src, TypeDesc srcType)
        {
            if (destType._arrayCount <= 1) {
                    // casting single element. Will we read the first element
                    // of the 
                switch (destType._type) {
                case TypeCat::Bool:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(bool*)dest = *(bool*)src; return true;
                        case TypeCat::Int8: *(bool*)dest = !!*(int8*)src; return true;
                        case TypeCat::UInt8: *(bool*)dest = !!*(uint8*)src; return true;
                        case TypeCat::Int16: *(bool*)dest = !!*(int16*)src; return true;
                        case TypeCat::UInt16: *(bool*)dest = !!*(uint16*)src; return true;
                        case TypeCat::Int32: *(bool*)dest = !!*(int32*)src; return true;
                        case TypeCat::UInt32: *(bool*)dest = !!*(uint32*)src; return true;
                        case TypeCat::Float: *(bool*)dest = !!*(float*)src; return true;
                        }
                    }
                    break;

                case TypeCat::Int8:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(int8*)dest = int8(*(bool*)src); return true;
                        case TypeCat::Int8: *(int8*)dest = int8(*(int8*)src); return true;
                        case TypeCat::UInt8: *(int8*)dest = int8(*(uint8*)src); return true;
                        case TypeCat::Int16: *(int8*)dest = int8(*(int16*)src); return true;
                        case TypeCat::UInt16: *(int8*)dest = int8(*(uint16*)src); return true;
                        case TypeCat::Int32: *(int8*)dest = int8(*(int32*)src); return true;
                        case TypeCat::UInt32: *(int8*)dest = int8(*(uint32*)src); return true;
                        case TypeCat::Float: *(int8*)dest = int8(*(float*)src); return true;
                        }
                    }
                    break;

                case TypeCat::UInt8:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(uint8*)dest = uint8(*(bool*)src); return true;
                        case TypeCat::Int8: *(uint8*)dest = uint8(*(int8*)src); return true;
                        case TypeCat::UInt8: *(uint8*)dest = uint8(*(uint8*)src); return true;
                        case TypeCat::Int16: *(uint8*)dest = uint8(*(int16*)src); return true;
                        case TypeCat::UInt16: *(uint8*)dest = uint8(*(uint16*)src); return true;
                        case TypeCat::Int32: *(uint8*)dest = uint8(*(int32*)src); return true;
                        case TypeCat::UInt32: *(uint8*)dest = uint8(*(uint32*)src); return true;
                        case TypeCat::Float: *(uint8*)dest = uint8(*(float*)src); return true;
                        }
                    }
                    break;
                
                case TypeCat::Int16:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(int16*)dest = int16(*(bool*)src); return true;
                        case TypeCat::Int8: *(int16*)dest = int16(*(int8*)src); return true;
                        case TypeCat::UInt8: *(int16*)dest = int16(*(uint8*)src); return true;
                        case TypeCat::Int16: *(int16*)dest = int16(*(int16*)src); return true;
                        case TypeCat::UInt16: *(int16*)dest = int16(*(uint16*)src); return true;
                        case TypeCat::Int32: *(int16*)dest = int16(*(int32*)src); return true;
                        case TypeCat::UInt32: *(int16*)dest = int16(*(uint32*)src); return true;
                        case TypeCat::Float: *(int16*)dest = int16(*(float*)src); return true;
                        }
                    }
                    break;

                case TypeCat::UInt16:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(uint16*)dest = uint16(*(bool*)src); return true;
                        case TypeCat::Int8: *(uint16*)dest = uint16(*(int8*)src); return true;
                        case TypeCat::UInt8: *(uint16*)dest = uint16(*(uint8*)src); return true;
                        case TypeCat::Int16: *(uint16*)dest = uint16(*(int16*)src); return true;
                        case TypeCat::UInt16: *(uint16*)dest = uint16(*(uint16*)src); return true;
                        case TypeCat::Int32: *(uint16*)dest = uint16(*(int32*)src); return true;
                        case TypeCat::UInt32: *(uint16*)dest = uint16(*(uint32*)src); return true;
                        case TypeCat::Float: *(uint16*)dest = uint16(*(float*)src); return true;
                        }
                    }
                    break;
                
                case TypeCat::Int32:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(int32*)dest = int32(*(bool*)src); return true;
                        case TypeCat::Int8: *(int32*)dest = int32(*(int8*)src); return true;
                        case TypeCat::UInt8: *(int32*)dest = int32(*(uint8*)src); return true;
                        case TypeCat::Int16: *(int32*)dest = int32(*(int16*)src); return true;
                        case TypeCat::UInt16: *(int32*)dest = int32(*(uint16*)src); return true;
                        case TypeCat::Int32: *(int32*)dest = *(int32*)src; return true;
                        case TypeCat::UInt32: *(int32*)dest = int32(*(uint32*)src); return true;
                        case TypeCat::Float: *(int32*)dest = int32(*(float*)src); return true;
                        }
                    }
                    break;

                case TypeCat::UInt32:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(uint32*)dest = uint32(*(bool*)src); return true;
                        case TypeCat::Int8: *(uint32*)dest = uint32(*(int8*)src); return true;
                        case TypeCat::UInt8: *(uint32*)dest = uint32(*(uint8*)src); return true;
                        case TypeCat::Int16: *(uint32*)dest = uint32(*(int16*)src); return true;
                        case TypeCat::UInt16: *(uint32*)dest = uint32(*(uint16*)src); return true;
                        case TypeCat::Int32: *(uint32*)dest = uint32(*(int32*)src); return true;
                        case TypeCat::UInt32: *(uint32*)dest = *(uint32*)src; return true;
                        case TypeCat::Float: *(uint32*)dest = uint32(*(float*)src); return true;
                        }
                    }
                    break;

                case TypeCat::Float:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(float*)dest = float(*(bool*)src); return true;
                        case TypeCat::Int8: *(float*)dest = float(*(int8*)src); return true;
                        case TypeCat::UInt8: *(float*)dest = float(*(uint8*)src); return true;
                        case TypeCat::Int16: *(float*)dest = float(*(int16*)src); return true;
                        case TypeCat::UInt16: *(float*)dest = float(*(uint16*)src); return true;
                        case TypeCat::Int32: *(float*)dest = float(*(int32*)src); return true;
                        case TypeCat::UInt32: *(float*)dest = float(*(uint32*)src); return true;
                        case TypeCat::Float: *(float*)dest = *(float*)src; return true;
                        }
                    }
                    break;
                }
            } else {
                    // multiple array elements. We might need to remap elements
                void* destIterator = dest;
                const void* srcIterator = src;
                auto sizeRemaining = destSize;
                for (unsigned c=0; c<destType._arrayCount; ++c) {
                    if (sizeRemaining < TypeDesc(destType._type).GetSize()) {
                        return false;
                    }
                    if (c < srcType._arrayCount) {
                        if (!Cast(destIterator, sizeRemaining, TypeDesc(destType._type),
                            srcIterator, TypeDesc(srcType._type))) {
                            return false;
                        }

                        destIterator = PtrAdd(destIterator, TypeDesc(destType._type).GetSize());
                        srcIterator = PtrAdd(srcIterator, TypeDesc(srcType._type).GetSize());
                        sizeRemaining -= TypeDesc(destType._type).GetSize();
                    } else {
                            // using HLSL rules for filling in blanks:
                            //  element 3 is 1, but others are 0
                        unsigned value = (c==3)?1:0;
                        if (!Cast(destIterator, sizeRemaining, TypeDesc(destType._type),
                            &value, TypeDesc(TypeCat::UInt32))) {
                            return false;
                        }
                        destIterator = PtrAdd(destIterator, TypeDesc(destType._type).GetSize());
                        sizeRemaining -= TypeDesc(destType._type).GetSize();
                    }
                }
                return true;
            }

            return false;
        }


        template<typename CharType>
            TypeDesc Parse(
                const CharType expressionBegin[], 
                const CharType expressionEnd[], 
                void* dest, size_t destSize)
        {
                // parse string expression into native types.
                // We'll write the native object into the buffer and return a type desc
            static std::basic_regex<CharType> booleanTrue((const CharType*)R"(^(true)|(True)|(y)|(Y)|(yes)|(Yes)|(TRUE)|(Y)|(YES)$)");
            static std::basic_regex<CharType> booleanFalse((const CharType*)R"(^(false)|(False)|(n)|(N)|(no)|(No)|(FALSE)|(N)|(NO)$)");

            if (std::regex_match(expressionBegin, expressionEnd, booleanTrue)) {
                assert(destSize >= sizeof(bool));
                *(bool*)dest = true;
                return TypeDesc(TypeCat::Bool);
            } else if (std::regex_match(expressionBegin, expressionEnd, booleanFalse)) {
                assert(destSize >= sizeof(bool));
                *(bool*)dest = false;
                return TypeDesc(TypeCat::Bool);
            }

            static std::basic_regex<CharType> unsignedPattern(
                (const CharType*)R"(^\+?(([\d]+)|(0x[\da-fA-F]+))((u)|(ui)|(ul)|(ull)|(U)|(UI)|(UL)|(ULL))?$)");
            if (std::regex_match(expressionBegin, expressionEnd, unsignedPattern)) {
                assert(destSize >= sizeof(unsigned));
                auto len = expressionEnd - expressionBegin;
                if (len > 2 && (expressionBegin[0] == '0' && expressionBegin[1] == 'x')) {
                        // hex form
                    *(uint32*)dest = XlAtoUI32(&expressionBegin[2], nullptr, 16);
                } else {
                    *(uint32*)dest = XlAtoUI32(expressionBegin);
                }
                return TypeDesc(TypeCat::UInt32);
            }

            static std::basic_regex<CharType> signedPattern((const CharType*)R"(^[-\+]?(([\d]+)|(0x[\da-fA-F]+))((i)|(l)|(ll)|(I)|(L)|(LL))?$)");
            if (std::regex_match(expressionBegin, expressionEnd, signedPattern)) {
                assert(destSize >= sizeof(unsigned));
                auto len = expressionEnd - expressionBegin;
                if (len > 2 && (expressionBegin[0] == '0' && expressionBegin[1] == 'x')) {
                        // hex form
                    *(int32*)dest = XlAtoI32(&expressionBegin[2], nullptr, 16);
                } else {
                    *(int32*)dest = XlAtoI32(expressionBegin);
                }
                return TypeDesc(TypeCat::Int32);
            }

            static std::basic_regex<CharType> floatPattern((const CharType*)R"(^[-\+]?(([\d]*\.?[\d]+)|([\d]+\.))([eE][-\+]?[\d]+)?[fF]?$)");
            if (std::regex_match(expressionBegin, expressionEnd, floatPattern)) {
                assert(destSize >= sizeof(float));
                *(float*)dest = XlAtoF32(expressionBegin);
                return TypeDesc(TypeCat::Float);
            }

            {
                    // match for float array:
                // R"(^\{\s*[-\+]?(([\d]*\.?[\d]+)|([\d]+\.))([eE][-\+]?[\d]+)?[fF]\s*(\s*,\s*([-\+]?(([\d]*\.?[\d]+)|([\d]+\.))([eE][-\+]?[\d]+)?[fF]))*\s*\}[vc]?$)"

                static std::basic_regex<CharType> arrayPattern((const CharType*)R"(\{\s*([^,\s]+(?:\s*,\s*[^,\s]+)*)\s*\}([vcVC]?))");
                // std::match_results<typename std::basic_string<CharType>::const_iterator> cm; 
                std::match_results<const CharType*> cm; 
                if (std::regex_match(expressionBegin, expressionEnd, cm, arrayPattern)) {
                    static std::basic_regex<CharType> arrayElementPattern((const CharType*)R"(\s*([^,\s]+)\s*(?:,|$))");

                    const auto& subMatch = cm[1];
                    std::regex_iterator<const CharType*> rit(
                        subMatch.first, subMatch.second,
                        arrayElementPattern);
                    std::regex_iterator<const CharType*> rend;

                    auto dstIterator = dest;
                    auto dstIteratorSize = ptrdiff_t(destSize);

                    TypeCat cat = TypeCat::Void;
                    unsigned count = 0;
                    if (rit != rend) {
                        assert(rit->size() >= 1);
                        const auto& eleMatch = (*rit)[1];

                        auto subType = Parse(
                            eleMatch.first, eleMatch.second,
                            dstIterator, dstIteratorSize);

                        assert(subType._arrayCount <= 1);
                        assert(subType._type != TypeCat::Void);
                        cat = subType._type;

                        auto size = subType.GetSize();
                        dstIterator = PtrAdd(dstIterator, size);
                        dstIteratorSize -= size;
                        ++rit;
                        ++count;
                    }

                    assert(cat != TypeCat::Void);
                    for (;rit != rend; ++rit) {
                        assert(rit->size() >= 1);
                        const auto& eleMatch = (*rit)[1];

                        auto subType = Parse(
                            eleMatch.first, eleMatch.second,
                            dstIterator, dstIteratorSize);

                        if (subType._type != cat) {
                            bool castSuccess = Cast(   
                                dstIterator, size_t(dstIteratorSize), TypeDesc(cat),
                                dstIterator, subType);
                            if (castSuccess) { LogWarning << "Mixed types in while parsing array in ImpliedTypes::Parse (cast success)"; }
                            if (castSuccess) { LogWarning << "Mixed types in while parsing array in ImpliedTypes::Parse (cast failed)"; }
                            subType._type = cat;
                        }

                        assert(subType._arrayCount <= 1);
                        dstIterator = PtrAdd(dstIterator, subType.GetSize());
                        dstIteratorSize -= subType.GetSize();
                        ++count;
                    }

                    TypeHint hint = TypeHint::None;
                    if (cm.size() >= 2 && cm[2].length() >= 1) {
                        if (tolower(cm[2].str()[0]) == 'v') hint = TypeHint::Vector;
                        if (tolower(cm[2].str()[0]) == 'c') hint = TypeHint::Color;
                    }

                    return TypeDesc(cat, uint16(count), hint);
                }
            }

            return TypeDesc(TypeCat::Void);
        }

        template <typename Type> std::pair<bool, Type> Parse(const char expression[]) 
        {
            char buffer[NativeRepMaxSize];
            auto parseType = Parse(expression, &expression[XlStringLen(expression)], buffer, sizeof(buffer));
            if (parseType == TypeOf<Type>()) {
                return std::make_pair(true, *(Type*)buffer);
            } else {
                Type casted;
                if (Cast(&casted, sizeof(casted), TypeOf<Type>(),
                    buffer, parseType)) {
                    return std::make_pair(true, casted);
                }
            }
            return std::make_pair(false, Type());
        }

        std::string AsString(const void* data, size_t dataSize, const TypeDesc& desc)
        {
            if (desc._typeHint == TypeHint::String) {
                if (desc._type == TypeCat::UInt8 || desc._type == TypeCat::Int8) {
                    return std::string((const char*)data, (const char*)PtrAdd(data, desc._arrayCount * sizeof(char)));
                }
                if (desc._type == TypeCat::UInt16 || desc._type == TypeCat::Int16) {
                    return Conversion::Convert<std::string>(std::basic_string<wchar_t>((const wchar_t*)data, (const wchar_t*)PtrAdd(data, desc._arrayCount * sizeof(wchar_t))));
                }
            }

            std::stringstream result;
            assert(dataSize >= desc.GetSize());
            auto arrayCount = std::max(unsigned(desc._arrayCount), 1u);
            if (arrayCount > 1) result << "{";

            for (auto i=0u; i<arrayCount; ++i) {
                if (i!=0) result << ", ";
                switch (desc._type) {
                // case TypeCat::Bool:     if (!*(bool*)data) { result << "true"; } else { result << "true"; }; break;
                case TypeCat::Bool:     result << *(bool*)data; break;
                case TypeCat::Int8:     result << (int32)*(int8*)data; break;
                case TypeCat::UInt8:    result << (uint32)*(uint8*)data; break;
                case TypeCat::Int16:    result << *(int16*)data; break;
                case TypeCat::UInt16:   result << *(uint16*)data; break;
                case TypeCat::Int32:    result << *(int32*)data; break;
                case TypeCat::UInt32:   result << *(uint32*)data; break;
                case TypeCat::Float:    result << *(float*)data; break;
                case TypeCat::Void:     result << "<<void>>"; break;
                default:                result << "<<error>>"; break;
                }

                    // skip forward one element
                data = PtrAdd(data, TypeDesc(desc._type).GetSize());
            }

            if (arrayCount > 1) {
                result << "}";
                switch (desc._typeHint) {
                case TypeHint::Color: result << "c"; break;
                case TypeHint::Vector: result << "v"; break;
                }
            }

            return result.str();
        }

        template std::pair<bool, bool> Parse(const char expression[]);
        template std::pair<bool, unsigned> Parse(const char expression[]);
        template std::pair<bool, signed> Parse(const char expression[]);
        template std::pair<bool, float> Parse(const char expression[]);
        template std::pair<bool, Float2> Parse(const char expression[]);
        template std::pair<bool, Float3> Parse(const char expression[]);
        template std::pair<bool, Float4> Parse(const char expression[]);
        template std::pair<bool, Float3x3> Parse(const char expression[]);
        template std::pair<bool, Float3x4> Parse(const char expression[]);
        template std::pair<bool, Float4x4> Parse(const char expression[]);
        template std::pair<bool, UInt2> Parse(const char expression[]);
        template std::pair<bool, UInt3> Parse(const char expression[]);
        template std::pair<bool, UInt4> Parse(const char expression[]);

    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    ParameterBox::ParameterNameHash ParameterBox::MakeParameterNameHash(const std::basic_string<utf8>& name)
    {
        return Hash32(AsPointer(name.cbegin()), AsPointer(name.cend()));
    }

    ParameterBox::ParameterNameHash    ParameterBox::MakeParameterNameHash(const utf8 name[])
    {
        return Hash32(name, &name[XlStringLen(name)]);
    }

    ParameterBox::ParameterNameHash    ParameterBox::MakeParameterNameHash(const char name[])
    {
        return MakeParameterNameHash((const utf8*)name);
    }

    void ParameterBox::SetParameter(const utf8 name[], const char data[])
    {
        uint8 buffer[NativeRepMaxSize];
        auto len = XlStringLen(data);
        auto typeDesc = ImpliedTyping::Parse(data, &data[len], buffer, sizeof(buffer));
        if (typeDesc._type != ImpliedTyping::TypeCat::Void) {
            SetParameter(name, buffer, typeDesc);
        } else {
            // no conversion... just store a string
            using namespace ImpliedTyping;
            SetParameter(
                name, data,
                TypeDesc(TypeCat::UInt8, (uint16)len, TypeHint::String));
        }
    }

    void ParameterBox::SetParameter(const utf8 name[], const std::string& data)
    {
        uint8 buffer[NativeRepMaxSize];
        auto typeDesc = ImpliedTyping::Parse(AsPointer(data.cbegin()), AsPointer(data.cend()), buffer, sizeof(buffer));
        if (typeDesc._type != ImpliedTyping::TypeCat::Void) {
            SetParameter(name, buffer, typeDesc);
        } else {
            using namespace ImpliedTyping;
            SetParameter(
                name, data.c_str(),
                TypeDesc(TypeCat::UInt8, (uint16)data.size(), TypeHint::String));
        }
    }

    template<typename Type>
        void ParameterBox::SetParameter(const utf8 name[], Type value)
    {
        const auto insertType = ImpliedTyping::TypeOf<Type>();
        auto size = insertType.GetSize();
        assert(size == sizeof(Type)); (void)size;
        SetParameter(name, &value, insertType);
    }

    void ParameterBox::SetParameter(
        const utf8 name[], const void* value, 
        const ImpliedTyping::TypeDesc& insertType)
    {
        auto hash = MakeParameterNameHash(name);
        const auto valueSize = insertType.GetSize();
        auto i = std::lower_bound(_parameterHashValues.cbegin(), _parameterHashValues.cend(), hash);
        if (i==_parameterHashValues.cend()) {
                // push new value onto the end (including name & type info)
            _parameterHashValues.push_back(hash);

            auto valueOffset = _values.size();
            auto nameOffset = _names.size();
            
            _values.insert(_values.end(), (const uint8*)value, (const uint8*)PtrAdd(value, valueSize));

            auto nameLength = XlStringLen(name)+1;
            _names.insert(_names.end(), name, &name[nameLength]);

            _offsets.push_back(std::make_pair(unsigned(nameOffset), unsigned(valueOffset)));
            _types.push_back(insertType);

            _cachedHash = 0;
            _cachedParameterNameHash = 0;
            return;
        }

        size_t index = std::distance(_parameterHashValues.cbegin(), i);
        if (*i!=hash) {
                // insert new value in the middle somewhere
            _parameterHashValues.insert(i, hash);

            const auto nameLength = XlStringLen(name)+1;
            auto dstOffsets = _offsets[index];

            _offsets.insert(_offsets.begin()+index, dstOffsets);
            for (auto i2=_offsets.begin()+index+1; i2<_offsets.end(); ++i2) {
                i2->first += unsigned(nameLength);
                i2->second += valueSize;
            }

            _values.insert(
                _values.cbegin()+dstOffsets.second, 
                (uint8*)value, (uint8*)PtrAdd(value, valueSize));
            _names.insert(
                _names.cbegin()+dstOffsets.first, 
                name, &name[nameLength]);
            _types.insert(_types.begin() + index, insertType);

            _cachedHash = 0;
            _cachedParameterNameHash = 0;
            return;
        }

            // just update the value
        const auto offset = _offsets[index];
        const auto& existingType = _types[index];

        assert(!XlCompareString(&_names[offset.first], name));

        if (existingType.GetSize() == valueSize) {

                // same type, or type with the same size...
            XlCopyMemory(&_values[offset.second], (uint8*)value, valueSize);
            _types[index] = insertType;

        } else {

                // if the size of the type changes, we need to adjust the values table a bit
                // hopefully this should be an uncommon case
            auto dstOffsets = _offsets[index];
            signed sizeChange = signed(valueSize) - signed(existingType.GetSize());

            for (auto i2=_offsets.begin()+index+1; i2<_offsets.end(); ++i2) {
                i2->second += sizeChange;
            }

            _values.erase(
                _values.cbegin()+dstOffsets.second,
                _values.cbegin()+dstOffsets.second+existingType.GetSize());
            _values.insert(
                _values.cbegin()+dstOffsets.second, 
                (uint8*)value, (uint8*)PtrAdd(value, valueSize));
            _types[index] = insertType;

        }

        _cachedHash = 0;
    }

    template<typename Type>
        std::pair<bool, Type> ParameterBox::GetParameter(ParameterNameHash name) const
    {
        auto i = std::lower_bound(_parameterHashValues.cbegin(), _parameterHashValues.cend(), name);
        if (i!=_parameterHashValues.cend() && *i == name) {
            size_t index = std::distance(_parameterHashValues.cbegin(), i);
            auto offset = _offsets[index];

            if (_types[index] == ImpliedTyping::TypeOf<Type>()) {
                return std::make_pair(true, *(Type*)&_values[offset.second]);
            } else {
                Type result;
                if (ImpliedTyping::Cast(
                    &result, sizeof(result), ImpliedTyping::TypeOf<Type>(),
                    &_values[offset.second], _types[index])) {
                    return std::make_pair(true, result);
                }
            }
        }
        return std::make_pair(false, Type());
    }
    
    template<typename Type>
        std::pair<bool, Type> ParameterBox::GetParameter(const utf8 name[]) const
    {
        return GetParameter<Type>(MakeParameterNameHash(name));
    }

    bool ParameterBox::GetParameter(ParameterNameHash name, void* dest, const ImpliedTyping::TypeDesc& destType) const
    {
        auto i = std::lower_bound(_parameterHashValues.cbegin(), _parameterHashValues.cend(), name);
        if (i!=_parameterHashValues.cend() && *i == name) {
            size_t index = std::distance(_parameterHashValues.cbegin(), i);
            auto offset = _offsets[index];

            if (_types[index] == destType) {
                XlCopyMemory(dest, &_values[offset.second], destType.GetSize());
                return true;
            }
            else {
                return ImpliedTyping::Cast(
                    dest, destType.GetSize(), destType,
                    &_values[offset.second], _types[index]);
            }
        }
        return false;
    }

    bool ParameterBox::HasParameter(ParameterNameHash name) const
    {
        auto i = std::lower_bound(_parameterHashValues.cbegin(), _parameterHashValues.cend(), name);
        return i!=_parameterHashValues.cend() && *i == name;
    }

    ImpliedTyping::TypeDesc ParameterBox::GetParameterType(ParameterNameHash name) const
    {
        auto i = std::lower_bound(_parameterHashValues.cbegin(), _parameterHashValues.cend(), name);
        if (i!=_parameterHashValues.cend() && *i == name) {
            return _types[std::distance(_parameterHashValues.cbegin(), i)];
        }
        return ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::Void, 0);
    }

    template<typename CharType> std::basic_string<CharType> ParameterBox::GetString(ParameterNameHash name) const
    {
        auto type = GetParameterType(name);
        if (type._type == ImpliedTyping::TypeCat::Int8 || type._type == ImpliedTyping::TypeCat::UInt8) {
            std::basic_string<char> result;
            result.resize(std::max(1u, (unsigned)type._arrayCount));
            GetParameter(name, AsPointer(result.begin()), type);
            return Conversion::Convert<std::basic_string<CharType>>(result);
        }

        if (type._type == ImpliedTyping::TypeCat::Int16 || type._type == ImpliedTyping::TypeCat::UInt16) {
            std::basic_string<wchar_t> wideResult;
            wideResult.resize(std::max(1u, (unsigned)type._arrayCount));
            GetParameter(name, AsPointer(wideResult.begin()), type);
            return Conversion::Convert<std::basic_string<CharType>>(wideResult);
        }

        return std::basic_string<CharType>();
    }

    template<typename CharType> bool ParameterBox::GetString(ParameterNameHash name, CharType dest[], size_t destCount) const
    {
        auto type = GetParameterType(name);
        if (type._type == ImpliedTyping::TypeCat::Int8 || type._type == ImpliedTyping::TypeCat::UInt8) {
            std::basic_string<char> intermediate;
            intermediate.resize(std::max(1u, (unsigned)type._arrayCount));
            GetParameter(name, AsPointer(intermediate.begin()), type);

            bool result = Conversion::Convert(dest, destCount-1,
                AsPointer(intermediate.begin()), AsPointer(intermediate.end()));
            dest[std::min(destCount-1, intermediate.size())] = CharType(0);
            return result;
        }

        if (type._type == ImpliedTyping::TypeCat::Int16 || type._type == ImpliedTyping::TypeCat::UInt16) {
            std::basic_string<wchar_t> intermediate;
            intermediate.resize(std::max(1u, (unsigned)type._arrayCount));
            GetParameter(name, AsPointer(intermediate.begin()), type);

            bool result = Conversion::Convert(dest, destCount-1,
                AsPointer(intermediate.begin()), AsPointer(intermediate.end()));
            dest[std::min(destCount-1, intermediate.size())] = CharType(0);
            return result;
        }

        return false;
    }

    template void ParameterBox::SetParameter(const utf8 name[], uint32 value);
    template std::pair<bool, uint32> ParameterBox::GetParameter(const utf8 name[]) const;
    template std::pair<bool, uint32> ParameterBox::GetParameter(ParameterNameHash name) const;

    template void ParameterBox::SetParameter(const utf8 name[], int32 value);
    template std::pair<bool, int32> ParameterBox::GetParameter(const utf8 name[]) const;
    template std::pair<bool, int32> ParameterBox::GetParameter(ParameterNameHash name) const;

    template void ParameterBox::SetParameter(const utf8 name[], bool value);
    template std::pair<bool, bool> ParameterBox::GetParameter(const utf8 name[]) const;
    template std::pair<bool, bool> ParameterBox::GetParameter(ParameterNameHash name) const;

    template void ParameterBox::SetParameter(const utf8 name[], float value);
    template std::pair<bool, float> ParameterBox::GetParameter(const utf8 name[]) const;
    template std::pair<bool, float> ParameterBox::GetParameter(ParameterNameHash name) const;


    template void ParameterBox::SetParameter(const utf8 name[], Float2 value);
    template std::pair<bool, Float2> ParameterBox::GetParameter(const utf8 name[]) const;
    template std::pair<bool, Float2> ParameterBox::GetParameter(ParameterNameHash name) const;
    
    template void ParameterBox::SetParameter(const utf8 name[], Float3 value);
    template std::pair<bool, Float3> ParameterBox::GetParameter(const utf8 name[]) const;
    template std::pair<bool, Float3> ParameterBox::GetParameter(ParameterNameHash name) const;

    template void ParameterBox::SetParameter(const utf8 name[], Float4 value);
    template std::pair<bool, Float4> ParameterBox::GetParameter(const utf8 name[]) const;
    template std::pair<bool, Float4> ParameterBox::GetParameter(ParameterNameHash name) const;


    template void ParameterBox::SetParameter(const utf8 name[], Float3x3 value);
    template std::pair<bool, Float3x3> ParameterBox::GetParameter(const utf8 name[]) const;
    template std::pair<bool, Float3x3> ParameterBox::GetParameter(ParameterNameHash name) const;
    
    template void ParameterBox::SetParameter(const utf8 name[], Float3x4 value);
    template std::pair<bool, Float3x4> ParameterBox::GetParameter(const utf8 name[]) const;
    template std::pair<bool, Float3x4> ParameterBox::GetParameter(ParameterNameHash name) const;

    template void ParameterBox::SetParameter(const utf8 name[], Float4x4 value);
    template std::pair<bool, Float4x4> ParameterBox::GetParameter(const utf8 name[]) const;
    template std::pair<bool, Float4x4> ParameterBox::GetParameter(ParameterNameHash name) const;


    template void ParameterBox::SetParameter(const utf8 name[], UInt2 value);
    template std::pair<bool, UInt2> ParameterBox::GetParameter(const utf8 name[]) const;
    template std::pair<bool, UInt2> ParameterBox::GetParameter(ParameterNameHash name) const;
    
    template void ParameterBox::SetParameter(const utf8 name[], UInt3 value);
    template std::pair<bool, UInt3> ParameterBox::GetParameter(const utf8 name[]) const;
    template std::pair<bool, UInt3> ParameterBox::GetParameter(ParameterNameHash name) const;

    template void ParameterBox::SetParameter(const utf8 name[], UInt4 value);
    template std::pair<bool, UInt4> ParameterBox::GetParameter(const utf8 name[]) const;
    template std::pair<bool, UInt4> ParameterBox::GetParameter(ParameterNameHash name) const;

    template std::basic_string<char> ParameterBox::GetString(ParameterNameHash name) const;
    template std::basic_string<wchar_t> ParameterBox::GetString(ParameterNameHash name) const;
    template std::basic_string<utf8> ParameterBox::GetString(ParameterNameHash name) const;
    template std::basic_string<ucs2> ParameterBox::GetString(ParameterNameHash name) const;
    template std::basic_string<ucs4> ParameterBox::GetString(ParameterNameHash name) const;

    template bool ParameterBox::GetString(ParameterNameHash name, char dest[], size_t destCount) const;
    template bool ParameterBox::GetString(ParameterNameHash name, wchar_t dest[], size_t destCount) const;
    template bool ParameterBox::GetString(ParameterNameHash name, utf8 dest[], size_t destCount) const;
    template bool ParameterBox::GetString(ParameterNameHash name, ucs2 dest[], size_t destCount) const;
    template bool ParameterBox::GetString(ParameterNameHash name, ucs4 dest[], size_t destCount) const;

    uint64      ParameterBox::CalculateParameterNamesHash() const
    {
            //  Note that the parameter names are always in the same order (unless 
            //  two names resolve to the same 32 bit hash value). So, even though
            //  though the xor operation here doesn't depend on order, it should be
            //  ok -- because if the same parameter names appear in two different
            //  parameter boxes, they should have the same order.
        return Hash64(AsPointer(_names.cbegin()), AsPointer(_names.cend()));
    }

    uint64      ParameterBox::CalculateHash() const
    {
        return Hash64(AsPointer(_values.cbegin()), AsPointer(_values.cend()));
    }

    const void* ParameterBox::GetValue(size_t index) const
    {
        if (index < _offsets.size()) {
            auto offset = _offsets[index].second;
            return &_values[offset];
        }
        return 0;    
    }

    unsigned ParameterBox::GetParameterCount() const
    {
        return (unsigned)_parameterHashValues.size();
    }

    auto ParameterBox::GetParameterAtIndex(unsigned index) const -> ParameterNameHash
    {
        assert(index < _parameterHashValues.size());
        return _parameterHashValues[index];
    }

    const utf8* ParameterBox::GetFullNameAtIndex(unsigned index) const
    {
        assert(index < _offsets.size());
        return &_names[_offsets[index].first];
    }

    uint64      ParameterBox::GetHash() const
    {
        if (!_cachedHash) {
            _cachedHash = CalculateHash();
        }
        return _cachedHash;
    }

    uint64      ParameterBox::GetParameterNamesHash() const
    {
        if (!_cachedParameterNameHash) {
            _cachedParameterNameHash = CalculateParameterNamesHash();
        }
        return _cachedParameterNameHash;
    }

    uint64      ParameterBox::CalculateFilteredHashValue(const ParameterBox& source) const
    {
        if (_values.size() > 1024) {
            assert(0);
            return 0;
        }

        uint8 temporaryValues[1024];
        std::copy(_values.cbegin(), _values.cend(), temporaryValues);

        auto i  = _parameterHashValues.cbegin();
        auto i2 = source._parameterHashValues.cbegin();
        while (i < _parameterHashValues.cend() && i2 < source._parameterHashValues.cend()) {

            if (*i < *i2)       { ++i; } 
            else if (*i > *i2)  { ++i2; } 
            else if (*i == *i2) {
                auto offsetDest = _offsets[std::distance(_parameterHashValues.cbegin(), i)].second;
                auto typeDest   = _types[std::distance(_parameterHashValues.cbegin(), i)];
                auto offsetSrc  = source._offsets[std::distance(source._parameterHashValues.cbegin(), i2)].second;
                auto typeSrc    = source._types[std::distance(source._parameterHashValues.cbegin(), i2)];
                
                if (typeDest == typeSrc) {
                    XlCopyMemory(
                        PtrAdd(temporaryValues, offsetDest), 
                        PtrAdd(AsPointer(source._values.cbegin()), offsetSrc),
                        typeDest.GetSize());
                } else {
                        // sometimes we get trival casting situations (like "unsigned int" to "int")
                        //  -- even in those cases, we execute the casting function, which will effect performance
                    bool castSuccess = ImpliedTyping::Cast(
                        PtrAdd(temporaryValues, offsetDest), sizeof(temporaryValues)-offsetDest, typeDest,
                        PtrAdd(AsPointer(source._values.cbegin()), offsetSrc), typeSrc);

                    assert(castSuccess);  // type mis-match when attempting to build filtered hash value
                    (void)castSuccess;
                }

                ++i; ++i2;
            }

        }

        return Hash64(temporaryValues, PtrAdd(temporaryValues, _values.size()));
    }

    class StringTableComparison
    {
    public:
        bool operator()(const utf8* lhs, const std::pair<const utf8*, std::string>& rhs) const 
        {
            return XlCompareString(lhs, rhs.first) < 0;
        }

        bool operator()(const std::pair<const utf8*, std::string>& lhs, const std::pair<const utf8*, std::string>& rhs) const 
        {
            return XlCompareString(lhs.first, rhs.first) < 0;
        }

        bool operator()(const std::pair<const utf8*, std::string>& lhs, const utf8* rhs) const 
        {
            return XlCompareString(lhs.first, rhs) < 0;
        }
    };

    void ParameterBox::BuildStringTable(std::vector<std::pair<const utf8*, std::string>>& defines) const
    {
        for (auto i=_offsets.cbegin(); i!=_offsets.cend(); ++i) {
            const auto* name = &_names[i->first];
            const void* value = &_values[i->second];
            const auto& type = _types[std::distance(_offsets.begin(), i)];
            auto stringFormat = ImpliedTyping::AsString(value, _values.size() - i->second, type);

            auto insertPosition = std::lower_bound(
                defines.begin(), defines.end(), name, StringTableComparison());
            if (insertPosition!=defines.cend() && !XlCompareString(insertPosition->first, name)) {
                insertPosition->second = stringFormat;
            } else {
                defines.insert(insertPosition, std::make_pair(name, stringFormat));
            }
        }
    }

    void ParameterBox::OverrideStringTable(std::vector<std::pair<const utf8*, std::string>>& defines) const
    {
        for (auto i=_offsets.cbegin(); i!=_offsets.cend(); ++i) {
            const auto* name = &_names[i->first];
            const void* value = &_values[i->second];
            const auto& type = _types[std::distance(_offsets.begin(), i)];

            auto insertPosition = std::lower_bound(
                defines.begin(), defines.end(), name, StringTableComparison());

            if (insertPosition!=defines.cend() && !XlCompareString(insertPosition->first, name)) {
                insertPosition->second = ImpliedTyping::AsString(value, _values.size()-i->second, type);
            }
        }
    }

    bool ParameterBox::ParameterNamesAreEqual(const ParameterBox& other) const
    {
            // return true iff both boxes have exactly the same parameter names, in the same order
        if (_names.size() != other._names.size()) {
            return false;
        }
        return GetParameterNamesHash() == other.GetParameterNamesHash();
    }

    void ParameterBox::MergeIn(const ParameterBox& source)
    {
            // simple implementation... 
            //  We could build a more effective implementation taking into account
            //  the fact that both parameter boxes are sorted.
        for (auto i=source._offsets.cbegin(); i!=source._offsets.cend(); ++i) {
            const auto* name = &source._names[i->first];
            SetParameter(
                name, 
                &source._values[i->second],
                source._types[std::distance(source._offsets.cbegin(), i)]);
        }
    }

    void ParameterBox::Serialize(Serialization::NascentBlockSerializer& serializer) const
    {
        Serialization::Serialize(serializer, _cachedHash);
        Serialization::Serialize(serializer, _cachedParameterNameHash);
        Serialization::Serialize(serializer, _parameterHashValues);
        Serialization::Serialize(serializer, _offsets);
        Serialization::Serialize(serializer, _names);
        Serialization::Serialize(serializer, _values);
        Serialization::Serialize(serializer, _types);
    }

    template<typename CharType>
        void    ParameterBox::Serialize(OutputStreamFormatter& stream) const
    {
        std::basic_string<CharType> tmpBuffer;
        std::basic_string<CharType> nameBuffer;

        for (auto i=_offsets.cbegin(); i!=_offsets.cend(); ++i) {
            const auto* name = &_names[i->first];
            const void* value = &_values[i->second];
            const auto& type = _types[std::distance(_offsets.begin(), i)];

            auto nameLen = XlStringLen(name);
            nameBuffer.resize(nameLen);     // (note; we're assuming this stl implementation won't reallocate when resizing to smaller size)
            bool result = Conversion::Convert(
                AsPointer(nameBuffer.begin()), nameBuffer.size(),
                name, &name[nameLen]);
               
            if (!result)
                ThrowException(::Exceptions::BasicLabel("Error during name conversion for member: %s", name));

                // We need special cases for string types. In these cases we might have to
                // do some conversion to get the value in the format we want.
            if (type._type == ImpliedTyping::TypeCat::Int8 || type._type == ImpliedTyping::TypeCat::UInt8) {
                auto start = (const utf8*)value;
                tmpBuffer.resize((unsigned)type._arrayCount);
                bool result = Conversion::Convert(
                    AsPointer(tmpBuffer.begin()), tmpBuffer.size(),
                    start, &start[type._arrayCount]);
                
                if (!result)
                    ThrowException(::Exceptions::BasicLabel("Error during string conversion for member: %s", name));

                stream.WriteAttribute(
                    nameBuffer.c_str(), 
                    AsPointer(tmpBuffer.begin()), AsPointer(tmpBuffer.end()));
                continue;
            }

            if (type._type == ImpliedTyping::TypeCat::Int16 || type._type == ImpliedTyping::TypeCat::UInt16) {
                auto start = (const ucs2*)value;
                tmpBuffer.resize((unsigned)type._arrayCount);
                bool result = Conversion::Convert(
                    AsPointer(tmpBuffer.begin()), tmpBuffer.size(),
                    start, &start[type._arrayCount]);
                
                if (!result)
                    ThrowException(::Exceptions::BasicLabel("Error during string conversion for member: %s", name));

                stream.WriteAttribute(
                    nameBuffer.c_str(),
                    AsPointer(tmpBuffer.begin()), AsPointer(tmpBuffer.end()));
                continue;
            }

            auto stringFormat = ImpliedTyping::AsString(value, _values.size() - i->second, type);
            auto convertedString = Conversion::Convert<std::basic_string<CharType>>(stringFormat);
            stream.WriteAttribute(
                Conversion::Convert<std::basic_string<CharType>>(std::basic_string<utf8>(name)).c_str(), 
                AsPointer(convertedString.begin()), AsPointer(convertedString.end()));
        }
    }

    ParameterBox::ParameterBox()
    {
        _cachedHash = _cachedParameterNameHash = 0;
    }

    ParameterBox::ParameterBox(
        std::initializer_list<std::pair<const utf8*, const char*>> init)
    {
        for (auto i=init.begin(); i!=init.end(); ++i) {
            SetParameter(i->first, i->second);
        }
    }

    ParameterBox::ParameterBox(
        const InputStreamFormatter& stream, 
        ImpliedTyping::TypeCat streamCharType)
    {
        using namespace ImpliedTyping;

            // note -- fixed size buffer here bottlenecks max size for native representations
            // of these values
        uint8 nativeTypeBuffer[NativeRepMaxSize];
        std::basic_string<utf8> nameBuffer;
        std::basic_string<char> valueBuffer;

        auto streamCharSize = TypeDesc(streamCharType).GetSize();

            // attempt to read attributes from a serialized text file
            // as soon as we hit something that is not another attribute
            // (it could be a sub-element, or the end of this element)
            // then we will stop reading and return
        while (stream.GetNext() == InputStreamFormatter::Blob::Attribute) {
            InputStreamFormatter::InteriorSection name, value;
            bool success = stream.TryReadAttribute(name, value);
            if (!success)
                throw ::Exceptions::BasicLabel("Parsing exception while reading attribute in parameter box deserialization");

            auto nameLen = (size_t(name._end) - size_t(name._start)) / streamCharSize;
            nameBuffer.resize(nameLen);
            bool nameConvResult = false;

            TypeDesc nativeType(TypeCat::Void);
            if (streamCharType == TypeCat::UInt8 || streamCharType == TypeCat::Int8) {
                nativeType = Parse(
                    (const char*)value._start,
                    (const char*)value._end,
                    nativeTypeBuffer, sizeof(nativeTypeBuffer));

                nameConvResult = Conversion::Convert(
                    AsPointer(nameBuffer.begin()), nameBuffer.size(),
                    (const utf8*)name._start, (const utf8*)name._end);
            } else if (streamCharType == TypeCat::UInt16 || streamCharType == TypeCat::Int16) {

                valueBuffer.resize((const ucs2*)value._end - (const ucs2*)value._start);
                bool valueConversion = Conversion::Convert(
                    AsPointer(valueBuffer.begin()), valueBuffer.size(),
                    (const ucs2*)value._start, (const ucs2*)value._end);
                if (!valueConversion)
                    throw ::Exceptions::BasicLabel("Error converting value string in parameter box deserialization");

                nativeType = Parse(
                    AsPointer(valueBuffer.begin()),
                    AsPointer(valueBuffer.end()),
                    nativeTypeBuffer, sizeof(nativeTypeBuffer));

                nameConvResult = Conversion::Convert(
                    AsPointer(nameBuffer.begin()), nameBuffer.size(),
                    (const ucs2*)name._start, (const ucs2*)name._end);
            } else if (streamCharType == TypeCat::UInt32 || streamCharType == TypeCat::Int32) {

                valueBuffer.resize((const ucs4*)value._end - (const ucs4*)value._start);
                bool valueConversion = Conversion::Convert(
                    AsPointer(valueBuffer.begin()), valueBuffer.size(),
                    (const ucs4*)value._start, (const ucs4*)value._end);
                if (!valueConversion)
                    throw ::Exceptions::BasicLabel("Error converting value string in parameter box deserialization");

                nativeType = Parse(
                    AsPointer(valueBuffer.begin()),
                    AsPointer(valueBuffer.end()),
                    nativeTypeBuffer, sizeof(nativeTypeBuffer));

                nameConvResult = Conversion::Convert(
                    AsPointer(nameBuffer.begin()), nameBuffer.size(),
                    (const ucs4*)name._start, (const ucs4*)name._end);
            }

            if (!nameConvResult)
                throw ::Exceptions::BasicLabel("Error converting string name in parameter box deserialization");

            if (nativeType._type != TypeCat::Void) {
                SetParameter(nameBuffer.c_str(), nativeTypeBuffer, nativeType);
            } else {
                    // this is just a string. We should store it as a string, in whatever character set it came in
                SetParameter(
                    nameBuffer.c_str(),
                    value._start, 
                    TypeDesc(streamCharType, uint16((size_t(value._end) - size_t(value._start)) / streamCharSize), TypeHint::String));
            }
        }
    }

    ParameterBox::ParameterBox(ParameterBox&& moveFrom)
    : _parameterHashValues(std::move(moveFrom._parameterHashValues))
    , _offsets(std::move(moveFrom._offsets))
    , _names(std::move(moveFrom._names))
    , _values(std::move(moveFrom._values))
    , _types(std::move(moveFrom._types))
    {
        _cachedHash = moveFrom._cachedHash;
        _cachedParameterNameHash = moveFrom._cachedParameterNameHash;
    }
        
    ParameterBox& ParameterBox::operator=(ParameterBox&& moveFrom)
    {
        _parameterHashValues = std::move(moveFrom._parameterHashValues);
        _offsets = std::move(moveFrom._offsets);
        _names = std::move(moveFrom._names);
        _values = std::move(moveFrom._values);
        _types = std::move(moveFrom._types);
        _cachedHash = moveFrom._cachedHash;
        _cachedParameterNameHash = moveFrom._cachedParameterNameHash;
        return *this;
    }

    ParameterBox::~ParameterBox()
    {
    }

    template void ParameterBox::Serialize<utf8>(OutputStreamFormatter& stream) const;
    template void ParameterBox::Serialize<ucs2>(OutputStreamFormatter& stream) const;
    template void ParameterBox::Serialize<ucs4>(OutputStreamFormatter& stream) const;

///////////////////////////////////////////////////////////////////////////////////////////////////

}



