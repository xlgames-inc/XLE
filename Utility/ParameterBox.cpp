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
#include "MemoryUtils.h"
#include "StringFormat.h"
#include "Conversion.h"
#include "Streams/StreamFormatter.h"
#include <algorithm>
#include <utility>
#include <regex>
#include <sstream>

#define HAS_XLE_MATH
#if defined(HAS_XLE_MATH)
    #include "../Math/Vector.h"
    #include "../Math/Matrix.h"
#endif

namespace Utility
{
    static const unsigned NativeRepMaxSize = MaxPath * 4;

    namespace ImpliedTyping
    {
        uint32 TypeDesc::GetSize() const
        {
            switch (_type) {
            case TypeCat::Bool: return sizeof(bool)*unsigned(_arrayCount);

            case TypeCat::Int8:
            case TypeCat::UInt8: return sizeof(uint8)*unsigned(_arrayCount);

            case TypeCat::Int16:
            case TypeCat::UInt16: return sizeof(uint16)*unsigned(_arrayCount);

            case TypeCat::Int32:
            case TypeCat::UInt32:
            case TypeCat::Float: return sizeof(uint32)*unsigned(_arrayCount);

            case TypeCat::Int64:
            case TypeCat::UInt64:
            case TypeCat::Double: return sizeof(uint64)*unsigned(_arrayCount);

            case TypeCat::Void:
            default: return 0;
            }
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

        template<> TypeDesc TypeOf<uint64>()        { return TypeDesc(TypeCat::UInt64); }
        template<> TypeDesc TypeOf<int64>()         { return TypeDesc(TypeCat::Int64); }
        template<> TypeDesc TypeOf<uint32>()        { return TypeDesc(TypeCat::UInt32); }
        template<> TypeDesc TypeOf<int32>()         { return TypeDesc(TypeCat::Int32); }
        template<> TypeDesc TypeOf<uint16>()        { return TypeDesc(TypeCat::UInt16); }
        template<> TypeDesc TypeOf<int16>()         { return TypeDesc(TypeCat::Int16); }
        template<> TypeDesc TypeOf<uint8>()         { return TypeDesc(TypeCat::UInt8); }
        template<> TypeDesc TypeOf<int8>()          { return TypeDesc(TypeCat::Int8); }
        template<> TypeDesc TypeOf<bool>()          { return TypeDesc(TypeCat::Bool); }
        template<> TypeDesc TypeOf<float>()         { return TypeDesc(TypeCat::Float); }
        template<> TypeDesc TypeOf<double>()        { return TypeDesc(TypeCat::Double); }
        template<> TypeDesc TypeOf<void>()          { return TypeDesc(TypeCat::Void); }
        #if defined(HAS_XLE_MATH)
            template<> TypeDesc TypeOf<Float2>()        { return TypeDesc(TypeCat::Float, 2, TypeHint::Vector); }
            template<> TypeDesc TypeOf<Float3>()        { return TypeDesc(TypeCat::Float, 3, TypeHint::Vector); }
            template<> TypeDesc TypeOf<Float4>()        { return TypeDesc(TypeCat::Float, 4, TypeHint::Vector); }
            template<> TypeDesc TypeOf<Float3x3>()      { return TypeDesc(TypeCat::Float, 9, TypeHint::Matrix); }
            template<> TypeDesc TypeOf<Float3x4>()      { return TypeDesc(TypeCat::Float, 12, TypeHint::Matrix); }
            template<> TypeDesc TypeOf<Float4x4>()      { return TypeDesc(TypeCat::Float, 16, TypeHint::Matrix); }
            template<> TypeDesc TypeOf<UInt2>()         { return TypeDesc(TypeCat::UInt32, 2, TypeHint::Vector); }
            template<> TypeDesc TypeOf<UInt3>()         { return TypeDesc(TypeCat::UInt32, 3, TypeHint::Vector); }
            template<> TypeDesc TypeOf<UInt4>()         { return TypeDesc(TypeCat::UInt32, 4, TypeHint::Vector); }
            template<> TypeDesc TypeOf<Int2>()          { return TypeDesc(TypeCat::Int32, 2, TypeHint::Vector); }
            template<> TypeDesc TypeOf<Int3>()          { return TypeDesc(TypeCat::Int32, 3, TypeHint::Vector); }
            template<> TypeDesc TypeOf<Int4>()          { return TypeDesc(TypeCat::Int32, 4, TypeHint::Vector); }
        #endif
        template<> TypeDesc TypeOf<const char*>()   { return TypeDesc(TypeCat::UInt8, (uint16)~uint16(0), TypeHint::String); }
        template<> TypeDesc TypeOf<const utf8*>()   { return TypeDesc(TypeCat::UInt8, (uint16)~uint16(0), TypeHint::String); }
		template<> TypeDesc TypeOf<const utf16*>()	{ return TypeDesc(TypeCat::UInt16, (uint16)~uint16(0), TypeHint::String); }
		template<> TypeDesc TypeOf<utf16>()			{ return TypeDesc(TypeCat::UInt16); }

        TypeDesc TypeOf(const char expression[]) 
        {
                // not implemented
            assert(0);
            return TypeDesc();
        }

        bool Cast(
            IteratorRange<void*> dest, TypeDesc destType,
            IteratorRange<const void*> rawSrc, TypeDesc srcType)
        {
            assert(rawSrc.size() >= srcType.GetSize());
            assert(dest.size() >= destType.GetSize());
            IteratorRange<const void*> src = rawSrc;
            if (destType._arrayCount <= 1) {
#if defined(__arm__) && !defined(__aarch64__)
                // Only 32-bit ARM, we may get unaligned access reading directly from rawSrc
                // Therefore, we check if it is 4-byte unaligned and copy if so

                // Setup the stack buffer if we need it
                // Right now maximum size of a type is 8 bytes, adjust if necessary
                const size_t MAXIMUM_SRC_SIZE = 8;
                const size_t srcSize = srcType.GetSize();
                assert(srcSize > 0);
                assert(srcSize <= MAXIMUM_SRC_SIZE);
                uint8_t srcBuffer[MAXIMUM_SRC_SIZE];

                // Check if unaligned
                if (uintptr_t(rawSrc.begin()) & 3u) {
                    // If unaligned, copy to the srcBuffer (memcpy is safe to do unaligned access)
                    memcpy(&srcBuffer[0], src.begin(), srcSize);
                    // Set src to the srcBuffer
                    src = { &srcBuffer[0], &srcBuffer[srcSize] };
                }
#endif
                    // casting single element. Will we read the first element
                    // of the 
                switch (destType._type) {
                case TypeCat::Bool:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(bool*)dest.begin() = *(bool*)src.begin(); return true;
                        case TypeCat::Int8: *(bool*)dest.begin() = !!*(int8*)src.begin(); return true;
                        case TypeCat::UInt8: *(bool*)dest.begin() = !!*(uint8*)src.begin(); return true;
                        case TypeCat::Int16: *(bool*)dest.begin() = !!*(int16*)src.begin(); return true;
                        case TypeCat::UInt16: *(bool*)dest.begin() = !!*(uint16*)src.begin(); return true;
                        case TypeCat::Int32: *(bool*)dest.begin() = !!*(int32*)src.begin(); return true;
                        case TypeCat::UInt32: *(bool*)dest.begin() = !!*(uint32*)src.begin(); return true;
                        case TypeCat::Int64: *(bool*)dest.begin() = !!*(int64*)src.begin(); return true;
                        case TypeCat::UInt64: *(bool*)dest.begin() = !!*(uint64*)src.begin(); return true;
                        case TypeCat::Float: *(bool*)dest.begin() = !!*(float*)src.begin(); return true;
                        case TypeCat::Double: *(bool*)dest.begin() = !!*(double*)src.begin(); return true;
                        case TypeCat::Void: break;
                        }
                    }
                    break;

                case TypeCat::Int8:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(int8*)dest.begin() = int8(*(bool*)src.begin()); return true;
                        case TypeCat::Int8: *(int8*)dest.begin() = int8(*(int8*)src.begin()); return true;
                        case TypeCat::UInt8: *(int8*)dest.begin() = int8(*(uint8*)src.begin()); return true;
                        case TypeCat::Int16: *(int8*)dest.begin() = int8(*(int16*)src.begin()); return true;
                        case TypeCat::UInt16: *(int8*)dest.begin() = int8(*(uint16*)src.begin()); return true;
                        case TypeCat::Int32: *(int8*)dest.begin() = int8(*(int32*)src.begin()); return true;
                        case TypeCat::UInt32: *(int8*)dest.begin() = int8(*(uint32*)src.begin()); return true;
                        case TypeCat::Int64: *(int8*)dest.begin() = int8(*(int64*)src.begin()); return true;
                        case TypeCat::UInt64: *(int8*)dest.begin() = int8(*(uint64*)src.begin()); return true;
                        case TypeCat::Float: *(int8*)dest.begin() = int8(*(float*)src.begin()); return true;
                        case TypeCat::Double: *(int8*)dest.begin() = int8(*(double*)src.begin()); return true;
                        case TypeCat::Void: break;
                        }
                    }
                    break;

                case TypeCat::UInt8:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(uint8*)dest.begin() = uint8(*(bool*)src.begin()); return true;
                        case TypeCat::Int8: *(uint8*)dest.begin() = uint8(*(int8*)src.begin()); return true;
                        case TypeCat::UInt8: *(uint8*)dest.begin() = uint8(*(uint8*)src.begin()); return true;
                        case TypeCat::Int16: *(uint8*)dest.begin() = uint8(*(int16*)src.begin()); return true;
                        case TypeCat::UInt16: *(uint8*)dest.begin() = uint8(*(uint16*)src.begin()); return true;
                        case TypeCat::Int32: *(uint8*)dest.begin() = uint8(*(int32*)src.begin()); return true;
                        case TypeCat::UInt32: *(uint8*)dest.begin() = uint8(*(uint32*)src.begin()); return true;
                        case TypeCat::Int64: *(uint8*)dest.begin() = uint8(*(int64*)src.begin()); return true;
                        case TypeCat::UInt64: *(uint8*)dest.begin() = uint8(*(uint64*)src.begin()); return true;
                        case TypeCat::Float: *(uint8*)dest.begin() = uint8(*(float*)src.begin()); return true;
                        case TypeCat::Double: *(uint8*)dest.begin() = uint8(*(double*)src.begin()); return true;
                        case TypeCat::Void: break;
                        }
                    }
                    break;
                
                case TypeCat::Int16:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(int16*)dest.begin() = int16(*(bool*)src.begin()); return true;
                        case TypeCat::Int8: *(int16*)dest.begin() = int16(*(int8*)src.begin()); return true;
                        case TypeCat::UInt8: *(int16*)dest.begin() = int16(*(uint8*)src.begin()); return true;
                        case TypeCat::Int16: *(int16*)dest.begin() = int16(*(int16*)src.begin()); return true;
                        case TypeCat::UInt16: *(int16*)dest.begin() = int16(*(uint16*)src.begin()); return true;
                        case TypeCat::Int32: *(int16*)dest.begin() = int16(*(int32*)src.begin()); return true;
                        case TypeCat::UInt32: *(int16*)dest.begin() = int16(*(uint32*)src.begin()); return true;
                        case TypeCat::Int64: *(int16*)dest.begin() = int16(*(int64*)src.begin()); return true;
                        case TypeCat::UInt64: *(int16*)dest.begin() = int16(*(uint64*)src.begin()); return true;
                        case TypeCat::Float: *(int16*)dest.begin() = int16(*(float*)src.begin()); return true;
                        case TypeCat::Double: *(int16*)dest.begin() = int16(*(double*)src.begin()); return true;
                        case TypeCat::Void: break;
                        }
                    }
                    break;

                case TypeCat::UInt16:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(uint16*)dest.begin() = uint16(*(bool*)src.begin()); return true;
                        case TypeCat::Int8: *(uint16*)dest.begin() = uint16(*(int8*)src.begin()); return true;
                        case TypeCat::UInt8: *(uint16*)dest.begin() = uint16(*(uint8*)src.begin()); return true;
                        case TypeCat::Int16: *(uint16*)dest.begin() = uint16(*(int16*)src.begin()); return true;
                        case TypeCat::UInt16: *(uint16*)dest.begin() = uint16(*(uint16*)src.begin()); return true;
                        case TypeCat::Int32: *(uint16*)dest.begin() = uint16(*(int32*)src.begin()); return true;
                        case TypeCat::UInt32: *(uint16*)dest.begin() = uint16(*(uint32*)src.begin()); return true;
                        case TypeCat::Int64: *(uint16*)dest.begin() = uint16(*(int64*)src.begin()); return true;
                        case TypeCat::UInt64: *(uint16*)dest.begin() = uint16(*(uint64*)src.begin()); return true;
                        case TypeCat::Float: *(uint16*)dest.begin() = uint16(*(float*)src.begin()); return true;
                        case TypeCat::Double: *(uint16*)dest.begin() = uint16(*(double*)src.begin()); return true;
                        case TypeCat::Void: break;
                        }
                    }
                    break;
                
                case TypeCat::Int32:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(int32*)dest.begin() = int32(*(bool*)src.begin()); return true;
                        case TypeCat::Int8: *(int32*)dest.begin() = int32(*(int8*)src.begin()); return true;
                        case TypeCat::UInt8: *(int32*)dest.begin() = int32(*(uint8*)src.begin()); return true;
                        case TypeCat::Int16: *(int32*)dest.begin() = int32(*(int16*)src.begin()); return true;
                        case TypeCat::UInt16: *(int32*)dest.begin() = int32(*(uint16*)src.begin()); return true;
                        case TypeCat::Int32: *(int32*)dest.begin() = *(int32*)src.begin(); return true;
                        case TypeCat::UInt32: *(int32*)dest.begin() = int32(*(uint32*)src.begin()); return true;
                        case TypeCat::Int64: *(int32*)dest.begin() = int32(*(int64*)src.begin()); return true;
                        case TypeCat::UInt64: *(int32*)dest.begin() = int32(*(uint64*)src.begin()); return true;
                        case TypeCat::Float: *(int32*)dest.begin() = int32(*(float*)src.begin()); return true;
                        case TypeCat::Double: *(int32*)dest.begin() = int32(*(double*)src.begin()); return true;
                        case TypeCat::Void: break;
                        }
                    }
                    break;

                case TypeCat::UInt32:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(uint32*)dest.begin() = uint32(*(bool*)src.begin()); return true;
                        case TypeCat::Int8: *(uint32*)dest.begin() = uint32(*(int8*)src.begin()); return true;
                        case TypeCat::UInt8: *(uint32*)dest.begin() = uint32(*(uint8*)src.begin()); return true;
                        case TypeCat::Int16: *(uint32*)dest.begin() = uint32(*(int16*)src.begin()); return true;
                        case TypeCat::UInt16: *(uint32*)dest.begin() = uint32(*(uint16*)src.begin()); return true;
                        case TypeCat::Int32: *(uint32*)dest.begin() = uint32(*(int32*)src.begin()); return true;
                        case TypeCat::UInt32: *(uint32*)dest.begin() = *(uint32*)src.begin(); return true;
                        case TypeCat::Int64: *(uint32*)dest.begin() = uint32(*(int64*)src.begin()); return true;
                        case TypeCat::UInt64: *(uint32*)dest.begin() = uint32(*(uint64*)src.begin()); return true;
                        case TypeCat::Float: *(uint32*)dest.begin() = uint32(*(float*)src.begin()); return true;
                        case TypeCat::Double: *(uint32*)dest.begin() = uint32(*(double*)src.begin()); return true;
                        case TypeCat::Void: break;
                        }
                    }
                    break;

                case TypeCat::Int64:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(int64*)dest.begin() = int64(*(bool*)src.begin()); return true;
                        case TypeCat::Int8: *(int64*)dest.begin() = int64(*(int8*)src.begin()); return true;
                        case TypeCat::UInt8: *(int64*)dest.begin() = int64(*(uint8*)src.begin()); return true;
                        case TypeCat::Int16: *(int64*)dest.begin() = int64(*(int16*)src.begin()); return true;
                        case TypeCat::UInt16: *(int64*)dest.begin() = int64(*(uint16*)src.begin()); return true;
                        case TypeCat::Int32: *(int64*)dest.begin() = int64(*(int32*)src.begin()); return true;
                        case TypeCat::UInt32: *(int64*)dest.begin() = int64(*(uint32*)src.begin()); return true;
                        case TypeCat::Int64: *(int64*)dest.begin() = int64(*(int64*)src.begin()); return true;
                        case TypeCat::UInt64: *(int64*)dest.begin() = int64(*(uint64*)src.begin()); return true;
                        case TypeCat::Float: *(int64*)dest.begin() = int64(*(float*)src.begin()); return true;
                        case TypeCat::Double: *(int64*)dest.begin() = int64(*(double*)src.begin()); return true;
                        case TypeCat::Void: break;
                        }
                    }
                    break;

                case TypeCat::UInt64:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(uint64*)dest.begin() = uint64(*(bool*)src.begin()); return true;
                        case TypeCat::Int8: *(uint64*)dest.begin() = uint64(*(int8*)src.begin()); return true;
                        case TypeCat::UInt8: *(uint64*)dest.begin() = uint64(*(uint8*)src.begin()); return true;
                        case TypeCat::Int16: *(uint64*)dest.begin() = uint64(*(int16*)src.begin()); return true;
                        case TypeCat::UInt16: *(uint64*)dest.begin() = uint64(*(uint16*)src.begin()); return true;
                        case TypeCat::Int32: *(uint64*)dest.begin() = uint64(*(int32*)src.begin()); return true;
                        case TypeCat::UInt32: *(uint64*)dest.begin() = uint64(*(uint32*)src.begin()); return true;
                        case TypeCat::Int64: *(uint64*)dest.begin() = uint64(*(int64*)src.begin()); return true;
                        case TypeCat::UInt64: *(uint64*)dest.begin() = uint64(*(uint64*)src.begin()); return true;
                        case TypeCat::Float: *(uint64*)dest.begin() = uint64(*(float*)src.begin()); return true;
                        case TypeCat::Double: *(uint64*)dest.begin() = uint64(*(double*)src.begin()); return true;
                        case TypeCat::Void: break;
                        }
                    }
                    break;

                case TypeCat::Float:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(float*)dest.begin() = float(*(bool*)src.begin()); return true;
                        case TypeCat::Int8: *(float*)dest.begin() = float(*(int8*)src.begin()); return true;
                        case TypeCat::UInt8: *(float*)dest.begin() = float(*(uint8*)src.begin()); return true;
                        case TypeCat::Int16: *(float*)dest.begin() = float(*(int16*)src.begin()); return true;
                        case TypeCat::UInt16: *(float*)dest.begin() = float(*(uint16*)src.begin()); return true;
                        case TypeCat::Int32: *(float*)dest.begin() = float(*(int32*)src.begin()); return true;
                        case TypeCat::UInt32: *(float*)dest.begin() = float(*(uint32*)src.begin()); return true;
                        case TypeCat::Int64: *(float*)dest.begin() = float(*(int64*)src.begin()); return true;
                        case TypeCat::UInt64: *(float*)dest.begin() = float(*(uint64*)src.begin()); return true;
                        case TypeCat::Float: *(float*)dest.begin() = float(*(float*)src.begin()); return true;
                        case TypeCat::Double: *(float*)dest.begin() = float(*(double*)src.begin()); return true;
                        case TypeCat::Void: break;
                        }
                    }
                    break;

                case TypeCat::Double:
                    {
                        switch (srcType._type) {
                        case TypeCat::Bool: *(double*)dest.begin() = double(*(bool*)src.begin()); return true;
                        case TypeCat::Int8: *(double*)dest.begin() = double(*(int8*)src.begin()); return true;
                        case TypeCat::UInt8: *(double*)dest.begin() = double(*(uint8*)src.begin()); return true;
                        case TypeCat::Int16: *(double*)dest.begin() = double(*(int16*)src.begin()); return true;
                        case TypeCat::UInt16: *(double*)dest.begin() = double(*(uint16*)src.begin()); return true;
                        case TypeCat::Int32: *(double*)dest.begin() = double(*(int32*)src.begin()); return true;
                        case TypeCat::UInt32: *(double*)dest.begin() = double(*(uint32*)src.begin()); return true;
                        case TypeCat::Int64: *(double*)dest.begin() = double(*(int64*)src.begin()); return true;
                        case TypeCat::UInt64: *(double*)dest.begin() = double(*(uint64*)src.begin()); return true;
                        case TypeCat::Float: *(double*)dest.begin() = double(*(float*)src.begin()); return true;
                        case TypeCat::Double: *(double*)dest.begin() = double(*(double*)src.begin()); return true;
                        case TypeCat::Void: break;
                        }
                    }
                    break;
                        
                case TypeCat::Void: break;
                }
            } else {

                    // multiple array elements. We might need to remap elements
                    // First -- trival cases can be completed with a memcpy
                if (    srcType._arrayCount == destType._arrayCount
                    &&  srcType._type == destType._type) {
                    XlCopyMemory(dest.begin(), src.begin(), std::min(dest.size(), size_t(srcType.GetSize())));
                    return true;
                }
                    
                auto destIterator = dest;
                auto srcIterator = src;
                for (unsigned c=0; c<destType._arrayCount; ++c) {
                    if (destIterator.size() < TypeDesc(destType._type).GetSize()) {
                        return false;
                    }
                    if (c < srcType._arrayCount) {
                        if (!Cast(destIterator, TypeDesc(destType._type),
                            srcIterator, TypeDesc(srcType._type))) {
                            return false;
                        }

                        destIterator.first = PtrAdd(destIterator.first, TypeDesc(destType._type).GetSize());
                        srcIterator.first = PtrAdd(srcIterator.first, TypeDesc(srcType._type).GetSize());
                    } else {
                            // using HLSL rules for filling in blanks:
                            //  element 3 is 1, but others are 0
                        unsigned value = (c==3)?1:0;
                        if (!Cast(destIterator, TypeDesc(destType._type),
                            AsOpaqueIteratorRange(value), TypeDesc(TypeCat::UInt32))) {
                            return false;
                        }
                        destIterator.first = PtrAdd(destIterator.first, TypeDesc(destType._type).GetSize());
                    }
                }
                return true;
            }

            return false;
        }
        
        
        enum CastType CastType(TypeCat testType, TypeCat againstType) {
            if (testType == againstType) {
                return CastType::Equal;
            }
            
            bool isWidening = false;
            switch (againstType) {
                case TypeCat::Bool:
                case TypeCat::UInt8:
                case TypeCat::UInt16:
                case TypeCat::UInt32:
                case TypeCat::UInt64:
                    isWidening = (testType == TypeCat::Bool ||
                            testType == TypeCat::UInt8 ||
                            testType == TypeCat::UInt16 ||
                            testType == TypeCat::UInt32 ||
                            testType == TypeCat::UInt64) && (testType < againstType);
                    break;
                case TypeCat::Float:
                case TypeCat::Double:
                    isWidening = testType < againstType;
                    break;
                case TypeCat::Int8:
                    isWidening = testType <= TypeCat::UInt8;
                    break;
                case TypeCat::Int16:
                    isWidening = testType <= TypeCat::UInt16;
                    break;
                case TypeCat::Int32:
                    isWidening = testType <= TypeCat::UInt32;
                    break;
                case TypeCat::Int64:
                    isWidening = testType <= TypeCat::UInt64;
                    break;
                default:
                    assert(false); // Unknown type
                    isWidening = false;
                    break;
            }
            
            return isWidening ? CastType::Widening : CastType::Narrowing;
        }
        
        template<typename CharType>
            class ParsingRegex
        {
        public:
            std::basic_regex<CharType> s_booleanTrue;
            std::basic_regex<CharType> s_booleanFalse;
            std::basic_regex<CharType> s_unsignedPattern;
            std::basic_regex<CharType> s_signedPattern;
            std::basic_regex<CharType> s_floatPattern;
            std::basic_regex<CharType> s_arrayPattern;
            std::basic_regex<CharType> s_arrayElementPattern;
        
            ParsingRegex()
            : s_booleanTrue((const CharType*)R"(^(true)|(True)|(y)|(Y)|(yes)|(Yes)|(TRUE)|(Y)|(YES)$)")
            , s_booleanFalse((const CharType*)R"(^(false)|(False)|(n)|(N)|(no)|(No)|(FALSE)|(N)|(NO)$)")
            , s_unsignedPattern((const CharType*)R"(^\+?(([\d]+)|(0x[\da-fA-F]+))(u|U|(u8)|(u16)|(u32)|(u64)|(U8)|(U16)|(U32)|(U64))?$)")
            , s_signedPattern((const CharType*)R"(^[-\+]?(([\d]+)|(0x[\da-fA-F]+))(i|I|(i8)|(i16)|(i32)|(i64)|(I8)|(I16)|(I32)|(I64))?$)")
            , s_floatPattern((const CharType*)R"(^[-\+]?(([\d]*\.?[\d]+)|([\d]+\.))([eE][-\+]?[\d]+)?(f|F|(f32)|(F32)|(f64)|(F64))?$)")
            , s_arrayPattern((const CharType*)R"(\{\s*([^,\s]+(?:\s*,\s*[^,\s]+)*)\s*\}([vcVC]?))")
            , s_arrayElementPattern((const CharType*)R"(\s*([^,\s]+)\s*(?:,|$))")
            {}
            ~ParsingRegex() {}
        };
        
        static std::unique_ptr<ParsingRegex<char>> s_parsingChar;
        
        template<typename CharType>
            TypeDesc Parse(
                StringSection<CharType> expression,
                void* dest, size_t destSize)
        {
            if (!s_parsingChar) {
                s_parsingChar = std::make_unique<ParsingRegex<char>>();
            }
                // parse string expression into native types.
                // We'll write the native object into the buffer and return a type desc
            if (std::regex_match(expression.begin(), expression.end(), s_parsingChar->s_booleanTrue)) {
                assert(destSize >= sizeof(bool));
                *(bool*)dest = true;
                return TypeDesc(TypeCat::Bool);
            } else if (std::regex_match(expression.begin(), expression.end(), s_parsingChar->s_booleanFalse)) {
                assert(destSize >= sizeof(bool));
                *(bool*)dest = false;
                return TypeDesc(TypeCat::Bool);
            }

            std::match_results<const CharType*> cm; 
            if (std::regex_match(expression.begin(), expression.end(), s_parsingChar->s_unsignedPattern)) {
                unsigned precision = 32;
                if (cm.size() >= 4 && cm[4].length() > 1)
                    precision = XlAtoUI32(&cm[2].str()[1]);

                uint64 value;
                auto len = expression.size();
                if (len > 2 && (expression.begin()[0] == '0' && expression.begin()[1] == 'x')) {
                        // hex form
                    value = XlAtoUI64(&expression.begin()[2], nullptr, 16);
                } else {
                    value = XlAtoUI64(expression.begin());
                }

                if (precision == 8) {
                    assert(destSize >= sizeof(uint8));
                    *(uint8*)dest = (uint8)value;
                    return TypeDesc(TypeCat::UInt8);
                } else if (precision == 16) {
                    assert(destSize >= sizeof(uint16));
                    *(uint16*)dest = (uint16)value;
                    return TypeDesc(TypeCat::UInt16);
                } else if (precision == 32) {
                    assert(destSize >= sizeof(uint32));
                    *(uint32*)dest = (uint32)value;
                    return TypeDesc(TypeCat::UInt32);
                } else if (precision == 64) {
                    assert(destSize >= sizeof(uint64));
                    *(uint64*)dest = (uint64)value;
                    return TypeDesc(TypeCat::UInt64);
                }

                assert(0);
            }

            if (std::regex_match(expression.begin(), expression.end(), cm, s_parsingChar->s_signedPattern)) {
                unsigned precision = 32;
                if (cm.size() >= 4 && cm[4].length() > 1)
                    precision = XlAtoUI32(&cm[2].str()[1]);

                int64 value;
                auto len = expression.end() - expression.begin();
                if (len > 2 && (expression.begin()[0] == '0' && expression.begin()[1] == 'x')) {
                        // hex form
                    value = XlAtoI64(&expression.begin()[2], nullptr, 16);
                } else {
                    value = XlAtoI64(expression.begin());
                }

                if (precision == 8) {
                    assert(destSize >= sizeof(int8));
                    *(int8*)dest = (int8)value;
                    return TypeDesc(TypeCat::Int8);
                } else if (precision == 16) {
                    assert(destSize >= sizeof(int16));
                    *(int16*)dest = (int16)value;
                    return TypeDesc(TypeCat::Int16);
                } else if (precision == 32) {
                    assert(destSize >= sizeof(int32));
                    *(int32*)dest = (int32)value;
                    return TypeDesc(TypeCat::Int32);
                } else if (precision == 64) {
                    assert(destSize >= sizeof(int64));
                    *(int64*)dest = (int64)value;
                    return TypeDesc(TypeCat::Int64);
                }

                assert(0);
            }

            if (std::regex_match(expression.begin(), expression.end(), s_parsingChar->s_floatPattern)) {
                bool doublePrecision = false;
                if (cm.size() >= 4 && cm[4].length() > 1) {
                    auto precision = XlAtoUI32(&cm[2].str()[1]);
                    doublePrecision = precision == 64;
                }

                if (doublePrecision) {
                    assert(destSize >= sizeof(double));
                    *(double*)dest = Conversion::Convert<double>(expression);
                    return TypeDesc(TypeCat::Double);
                } else {
                    assert(destSize >= sizeof(float));
                    *(float*)dest = Conversion::Convert<float>(expression);
                    return TypeDesc(TypeCat::Float);
                }
            }

            {
                    // match for float array:
                // R"(^\{\s*[-\+]?(([\d]*\.?[\d]+)|([\d]+\.))([eE][-\+]?[\d]+)?[fF]\s*(\s*,\s*([-\+]?(([\d]*\.?[\d]+)|([\d]+\.))([eE][-\+]?[\d]+)?[fF]))*\s*\}[vc]?$)"

                // std::match_results<typename std::basic_string<CharType>::const_iterator> cm;
                if (std::regex_match(expression.begin(), expression.end(), cm, s_parsingChar->s_arrayPattern)) {
                    const auto& subMatch = cm[1];
                    std::regex_iterator<const CharType*> rit(
                        subMatch.first, subMatch.second,
                        s_parsingChar->s_arrayElementPattern);
                    std::regex_iterator<const CharType*> rend;
                    
                    auto startIt = rit;

                    auto dstIterator = dest;
                    auto dstIteratorSize = ptrdiff_t(destSize);

                    TypeCat cat = TypeCat::Void;
                    unsigned count = 0;
                    if (rit != rend) {
                        assert(rit->size() >= 1);
                        const auto& eleMatch = (*rit)[1];

                        auto subType = Parse(
                            MakeStringSection(eleMatch.first, eleMatch.second),
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
                            MakeStringSection(eleMatch.first, eleMatch.second),
                            dstIterator, dstIteratorSize);

                        if (CastType(subType._type, cat) != CastType::Narrowing) {
                            bool castSuccess = Cast(   
                                { dstIterator, PtrAdd(dstIterator, subType.GetSize()) }, TypeDesc(cat),
                                { dstIterator, PtrAdd(dstIterator, subType.GetSize()) }, subType);
                            (void)castSuccess;
                            subType._type = cat;
                        } else {
                            // If the cast would narrow the type, we would corrupt the input
                            // Therefore, instead we modify the type we are reading and cast
                            // the previously read values to the new type
                            assert(CastType(cat, subType._type) == CastType::Widening);
                            const auto catType = TypeDesc(cat);
                            const size_t cpySize = catType.GetSize() * count;
                            std::unique_ptr<uint8[]> tempCpy = std::make_unique<uint8[]>(cpySize);
                            auto tempCpyIterator = tempCpy.get();
                            std::memcpy(tempCpy.get(), dest, cpySize);
                            
                            dstIterator = dest;
                            assert(ptrdiff_t(destSize) - dstIteratorSize == ptrdiff_t(cpySize));
                            dstIteratorSize = ptrdiff_t(destSize);
                            for (auto redoIt = startIt; redoIt != rit; ++redoIt) {
                                bool castSuccess = Cast(
                                    { dstIterator, PtrAdd(dstIterator, subType.GetSize()) }, subType, 
                                    { tempCpyIterator, PtrAdd(tempCpyIterator, catType.GetSize()) }, catType);
                                assert(castSuccess);
                                (void)castSuccess;
                                
                                dstIterator = PtrAdd(dstIterator, subType.GetSize());
                                dstIteratorSize -= subType.GetSize();
                                
                                tempCpyIterator = tempCpyIterator + catType.GetSize();
                            }
                            
                            cat = subType._type;
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

		template<>
            TypeDesc Parse(
                StringSection<utf8> expression,
                void* dest, size_t destSize)
		{
			return Parse(expression.Cast<char>(), dest, destSize);
		}

        template <typename Type> std::pair<bool, Type> Parse(StringSection<char> expression) 
        {
            char buffer[NativeRepMaxSize];
            auto parseType = Parse(expression, buffer, sizeof(buffer));
            if (parseType == TypeOf<Type>()) {
                return std::make_pair(true, *(Type*)buffer);
            } else {
                Type casted;
                if (Cast(AsOpaqueIteratorRange(casted), TypeOf<Type>(),
                    MakeIteratorRange(buffer), parseType)) {
                    return std::make_pair(true, casted);
                }
            }
            return std::make_pair(false, Type());
        }

        /*template <typename Type> std::pair<bool, Type> Parse(const char expression[]) 
        {
            return Parse<Type>(MakeStringSection(expression));
        }*/

        template <typename Type>
            std::pair<bool, Type> Parse(StringSection<utf8> expression)
        {
            return Parse<Type>(expression.Cast<char>());
        }

        std::string AsString(const void* data, size_t dataSize, const TypeDesc& desc, bool strongTyping)
        {
            if (desc._typeHint == TypeHint::String) {
                if (desc._type == TypeCat::UInt8 || desc._type == TypeCat::Int8) {
                    return std::string((const char*)data, (const char*)PtrAdd(data, desc._arrayCount * sizeof(char)));
                }
                if (desc._type == TypeCat::UInt16 || desc._type == TypeCat::Int16) {
                    return Conversion::Convert<std::string>(std::basic_string<utf16>((const utf16*)data, (const utf16*)PtrAdd(data, desc._arrayCount * sizeof(utf16))));
                }
            }

            std::stringstream result;
            assert(dataSize >= desc.GetSize());
            auto arrayCount = unsigned(desc._arrayCount);
            if (arrayCount > 1) result << "{";

            for (auto i=0u; i<arrayCount; ++i) {
                if (i!=0) result << ", ";

                if (strongTyping) {
                    switch (desc._type) {
                    case TypeCat::Bool:     if (*(bool*)data) { result << "true"; } else { result << "false"; }; break;
                    case TypeCat::Int8:     result << (int32)*(int8*)data << "i8"; break;
                    case TypeCat::UInt8:    result << (uint32)*(uint8*)data << "u8"; break;
                    case TypeCat::Int16:    result << *(int16*)data << "i16"; break;
                    case TypeCat::UInt16:   result << *(uint16*)data << "u16"; break;
                    case TypeCat::Int32:    result << *(int32*)data << "i"; break;
                    case TypeCat::UInt32:   result << *(uint32*)data << "u"; break;
                    case TypeCat::Int64:    result << *(int64*)data << "i64"; break;
                    case TypeCat::UInt64:   result << *(uint64*)data << "u64"; break;
                    case TypeCat::Float:    result << *(float*)data << "f"; break;
                    case TypeCat::Double:   result << *(double*)data << "f64"; break;
                    case TypeCat::Void:     result << ""; break;
                    default:                result << "<<error>>"; break;
                    }
                } else {
                    switch (desc._type) {
                    case TypeCat::Bool:     result << *(bool*)data; break;
                    case TypeCat::Int8:     result << (int32)*(int8*)data; break;
                    case TypeCat::UInt8:    result << (uint32)*(uint8*)data; break;
                    case TypeCat::Int16:    result << *(int16*)data; break;
                    case TypeCat::UInt16:   result << *(uint16*)data; break;
                    case TypeCat::Int32:    result << *(int32*)data; break;
                    case TypeCat::UInt32:   result << *(uint32*)data; break;
                    case TypeCat::Int64:    result << *(int64*)data; break;
                    case TypeCat::UInt64:   result << *(uint64*)data; break;
                    case TypeCat::Float:    result << *(float*)data; break;
                    case TypeCat::Double:   result << *(double*)data; break;
                    case TypeCat::Void:     result << ""; break;
                    default:                result << "<<error>>"; break;
                    }
                }

                    // skip forward one element
                data = PtrAdd(data, TypeDesc(desc._type).GetSize());
            }

            if (arrayCount > 1) {
                result << "}";
                switch (desc._typeHint) {
                case TypeHint::Color: result << "c"; break;
                case TypeHint::Vector: result << "v"; break;
                default: break;
                }
            }

            return result.str();
        }

        std::string AsString(IteratorRange<const void*> data, const TypeDesc& type, bool strongTyping)
        {
            return AsString(data.begin(), data.size(), type, strongTyping);
		}

		void Cleanup()
		{
            s_parsingChar.reset();
        }


        template std::pair<bool, bool> Parse(StringSection<utf8>);
        template std::pair<bool, unsigned> Parse(StringSection<utf8>);
        template std::pair<bool, signed> Parse(StringSection<utf8>);
        template std::pair<bool, uint64> Parse(StringSection<utf8>);
        template std::pair<bool, int64> Parse(StringSection<utf8>);
        template std::pair<bool, float> Parse(StringSection<utf8>);
		template std::pair<bool, double> Parse(StringSection<utf8>);
        #if defined(HAS_XLE_MATH)
            template std::pair<bool, Float2> Parse(StringSection<utf8>);
            template std::pair<bool, Float3> Parse(StringSection<utf8>);
            template std::pair<bool, Float4> Parse(StringSection<utf8>);
            template std::pair<bool, Float3x3> Parse(StringSection<utf8>);
            template std::pair<bool, Float3x4> Parse(StringSection<utf8>);
            template std::pair<bool, Float4x4> Parse(StringSection<utf8>);
            template std::pair<bool, UInt2> Parse(StringSection<utf8>);
            template std::pair<bool, UInt3> Parse(StringSection<utf8>);
            template std::pair<bool, UInt4> Parse(StringSection<utf8>);
            template std::pair<bool, Int2> Parse(StringSection<utf8>);
            template std::pair<bool, Int3> Parse(StringSection<utf8>);
            template std::pair<bool, Int4> Parse(StringSection<utf8>);
        #endif

    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	ParameterBox::ParameterNameHash ParameterBox::MakeParameterNameHash(StringSection<utf8> name)
    {
        return MakeParameterNameHash(name.Cast<char>());
    }

    ParameterBox::ParameterNameHash    ParameterBox::MakeParameterNameHash(StringSection<char> name)
    {
		// If the variable name has array indexor syntax, we strip off that syntax and use
        // the indexor as a offset for the hash value. This makes it possible to store arrays,
		// and has a couple of interesting side effects.
		//		- array elements always get stored subsequentially
		//		- only single dimensional arrays are supported, because this syntax has no hints for how to arrange multi dimensional arrays in the hash space
		//		- "something[0]" and "something" evaluate to the same hash value
		//		- only positive integer array indexors are supported, but octal or hex numbers can be used
        if (name.size() >= 2 && *(name.end()-1) == ']') {
            auto i = &name[name.size()-2];
            while (i > name.begin() && ((*i >= '0' && *i <= '9') || *i == 'x' || *i == '+')) --i;
			if (*i == '[') {
				char* end = nullptr;
				auto indexor = strtoul(i+1, &end, 0);
				if (end == name.end()-1) {
					// successful parse, we can use the array syntax interpretation
					return MakeParameterNameHash(MakeStringSection(name.begin(), i)) + indexor;
				}
			}
        }

        return Hash64(name.begin(), name.end());
    }

    void ParameterBox::SetParameter(StringSection<utf8> name, StringSection<char> stringData)
    {
        using namespace ImpliedTyping;
        if (stringData.IsEmpty()) {
                // null values or empty strings become "void" type parameters
			SetParameter(name, {}, TypeDesc(TypeCat::Void, 0));
            return;
        }

        uint8 buffer[NativeRepMaxSize];
		assert(stringData.size() < NativeRepMaxSize);
        auto typeDesc = Parse(stringData, buffer, sizeof(buffer));
        if (typeDesc._type != TypeCat::Void) {
			SetParameter(name, {buffer, PtrAdd(buffer, std::min(sizeof(buffer), (size_t)typeDesc.GetSize()))}, typeDesc);
        } else {
            // no conversion... just store a string
            SetParameter(
				name, MakeIteratorRange(stringData.begin(), stringData.end()),
                TypeDesc(TypeCat::UInt8, (uint16)(stringData.size()), TypeHint::String));
        }
    }

	template<>
        void ParameterBox::SetParameter(StringSection<utf8> name, const char* value)
    {
		SetParameter(name, value ? MakeStringSection(value) : StringSection<>{});
    }

	template<>
        void ParameterBox::SetParameter(StringSection<utf8> name, const utf8* value)
    {
        SetParameter(name, MakeStringSection((const char*)value));
    }

    template<typename Type>
        void ParameterBox::SetParameter(StringSection<utf8> name, Type value)
    {
        const auto insertType = ImpliedTyping::TypeOf<Type>();
        auto size = insertType.GetSize();
        assert(size == sizeof(Type)); (void)size;
        SetParameter(name, AsOpaqueIteratorRange(value), insertType);
    }

    static uint8* ValueTableOffset(SerializableVector<uint8>& values, size_t offset)
    {
        return PtrAdd(AsPointer(values.begin()), offset);
    }

    static const uint8* ValueTableOffset(const SerializableVector<uint8>& values, size_t offset)
    {
        return PtrAdd(AsPointer(values.begin()), offset);
    }

    void ParameterBox::SetParameter(
        StringSection<utf8> name, IteratorRange<const void*> value, 
        const ImpliedTyping::TypeDesc& insertType)
    {
        SetParameter(MakeParameterNameHash(name), name, value, insertType);
    }

    void ParameterBox::SetParameter(ParameterNameHash nameHash, IteratorRange<const void*> data, const TypeDesc& type)
    {
        SetParameter(nameHash, {}, data, type);
    }

    void ParameterBox::SetParameter(
        ParameterNameHash hash, StringSection<utf8> name, IteratorRange<const void*> value,
        const ImpliedTyping::TypeDesc& insertType)
    {
        SetParameterHint(
            std::lower_bound(_hashNames.cbegin(), _hashNames.cend(), hash),
            hash, name,
            value, insertType);
    }

    auto ParameterBox::SetParameterHint(
        SerializableVector<ParameterNameHash>::const_iterator i,
        ParameterNameHash hash, StringSection<utf8> name, IteratorRange<const void*> value,
        const ImpliedTyping::TypeDesc& insertType) -> SerializableVector<ParameterNameHash>::const_iterator
    {
		assert(value.size() == insertType.GetSize());
        if (i==_hashNames.cend()) {
                // push new value onto the end (including name & type info)
            _hashNames.push_back(hash);

            auto valueOffset = _values.size();
            auto nameOffset = _names.size();
            
            _values.insert(_values.end(), (const uint8*)value.begin(), (const uint8*)value.end());

            if (!name.IsEmpty())
                _names.insert(_names.end(), name.begin(), name.end());
            _names.push_back(0);

			_offsets.push_back(OffsetsEntry{unsigned(nameOffset), unsigned(valueOffset), unsigned(name.size()), unsigned(value.size())});
            _types.push_back(insertType);

            _cachedHash = 0;
            _cachedParameterNameHash = 0;
            return _hashNames.end()-1;
        }

        size_t index = std::distance(_hashNames.cbegin(), i);
        if (*i!=hash) {
                // insert new value in the middle somewhere
            i = _hashNames.insert(i, hash);

            const auto nameLength = name.size()+1;
            auto dstOffsets = _offsets[index];
			dstOffsets._nameSize = (unsigned)name.size();
			dstOffsets._valueSize = (unsigned)value.size();

            _offsets.insert(_offsets.begin()+index, dstOffsets);
            for (auto i2=_offsets.begin()+index+1; i2<_offsets.end(); ++i2) {
                i2->_nameBegin += unsigned(nameLength);
                i2->_valueBegin += unsigned(value.size());
            }

            _values.insert(
                _values.cbegin()+dstOffsets._valueBegin, 
                (uint8*)value.begin(), (uint8*)value.end());
            if (!name.IsEmpty())
                _names.insert(_names.cbegin()+dstOffsets._nameBegin, name.begin(), name.end());
            _names.insert(_names.cbegin()+dstOffsets._nameBegin+name.size(), 0);
            _types.insert(_types.begin() + index, insertType);

            _cachedHash = 0;
            _cachedParameterNameHash = 0;
            return i;
        }

            // just update the value
        const auto offset = _offsets[index];

        assert(name.IsEmpty()|| !XlCompareString(&_names[offset._nameBegin], name));

        if (offset._valueSize == value.size()) {

                // same type, or type with the same size...
            XlCopyMemory(ValueTableOffset(_values, offset._valueBegin), (uint8*)value.begin(), value.size());
            _types[index] = insertType;

        } else {

                // if the size of the type changes, we need to adjust the values table a bit
                // hopefully this should be an uncommon case
			auto prevSize = _offsets[index]._valueSize;
            signed sizeChange = signed(value.size()) - signed(prevSize);
			_offsets[index]._valueSize = (unsigned)value.size();
            auto dstOffsets = _offsets[index];

            for (auto i2=_offsets.begin()+index+1; i2<_offsets.end(); ++i2) {
                i2->_valueBegin += sizeChange;
            }

			if (prevSize != 0) {
				_values.erase(
					_values.cbegin()+dstOffsets._valueBegin,
					_values.cbegin()+dstOffsets._valueBegin+prevSize);
			}
            _values.insert(
                _values.cbegin()+dstOffsets._valueBegin, 
                (uint8*)value.begin(), (uint8*)value.end());
            _types[index] = insertType;

        }

        _cachedHash = 0;
        return i;
    }

	void ParameterBox::RemoveParameter(ParameterName name)
	{
		auto i = std::lower_bound(_hashNames.begin(), _hashNames.end(), name._hash);
        if (i==_hashNames.end() || *i != name._hash)
			return;

		auto index = std::distance(_hashNames.begin(), i);

		{
			auto prevSize = _offsets[index]._valueSize;
			if (prevSize != 0) {
				_values.erase(
					_values.cbegin() + _offsets[index]._valueBegin,
					_values.cbegin() + _offsets[index]._valueBegin + prevSize);
			}

            signed sizeChange = 0 - signed(prevSize);
            for (auto i2=_offsets.begin()+index+1; i2<_offsets.end(); ++i2)
                i2->_valueBegin += sizeChange;
        }

		{
			auto prevSize = _offsets[index]._nameSize;
			assert(prevSize != 0);
			_names.erase(
                _names.cbegin() + _offsets[index]._nameBegin,
                _names.cbegin() + _offsets[index]._nameBegin + prevSize);

            signed sizeChange = 0 - signed(prevSize);
			for (auto i2=_offsets.begin()+index+1; i2<_offsets.end(); ++i2)
                i2->_nameBegin += sizeChange;
        }

		_hashNames.erase(_hashNames.begin() + index);
		_offsets.erase(_offsets.begin() + index);
		_types.erase(_types.begin() + index);
	}

    template<typename Type>
        std::optional<Type> ParameterBox::GetParameter(ParameterName name) const
    {
        auto i = std::lower_bound(_hashNames.cbegin(), _hashNames.cend(), name._hash);
        if (i!=_hashNames.cend() && *i == name._hash) {
            size_t index = std::distance(_hashNames.cbegin(), i);
            auto offset = _offsets[index];

            if (_types[index] == ImpliedTyping::TypeOf<Type>()) {
                return *(Type*)&_values[offset._valueBegin];
            } else {
                Type result;
                if (ImpliedTyping::Cast(
                    AsOpaqueIteratorRange(result), ImpliedTyping::TypeOf<Type>(),
                    { ValueTableOffset(_values, offset._valueBegin), ValueTableOffset(_values, offset._valueBegin+offset._valueSize) },
                    _types[index])) {
					return result;
                }
            }
        }
		return {};
    }
    
    bool ParameterBox::GetParameter(ParameterName name, void* dest, const ImpliedTyping::TypeDesc& destType) const
    {
        auto i = std::lower_bound(_hashNames.cbegin(), _hashNames.cend(), name._hash);
        if (i!=_hashNames.cend() && *i == name._hash) {
            size_t index = std::distance(_hashNames.cbegin(), i);
            auto offset = _offsets[index];

            if (_types[index] == destType) {
                XlCopyMemory(dest, ValueTableOffset(_values, offset._valueBegin), offset._valueSize);
                return true;
            }
            else {
                return ImpliedTyping::Cast(
                    { dest, PtrAdd(dest, destType.GetSize()) }, destType,
                    { ValueTableOffset(_values, offset._valueBegin), ValueTableOffset(_values, offset._valueBegin+offset._valueSize) },
                    _types[index]);
            }
        }
        return false;
    }

    bool ParameterBox::HasParameter(ParameterName name) const
    {
        auto i = std::lower_bound(_hashNames.cbegin(), _hashNames.cend(), name._hash);
        return i!=_hashNames.cend() && *i == name._hash;
    }

    ImpliedTyping::TypeDesc ParameterBox::GetParameterType(ParameterName name) const
    {
        auto i = std::lower_bound(_hashNames.cbegin(), _hashNames.cend(), name._hash);
        if (i!=_hashNames.cend() && *i == name._hash) {
            return _types[std::distance(_hashNames.cbegin(), i)];
        }
        return ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::Void, 0);
    }

	IteratorRange<const void*> ParameterBox::GetParameterRawValue(ParameterName name) const
	{
		auto i = std::lower_bound(_hashNames.cbegin(), _hashNames.cend(), name._hash);
		if (i != _hashNames.cend() && *i == name._hash) {
			size_t index = std::distance(_hashNames.cbegin(), i);
			auto offset = _offsets[index];
			return {ValueTableOffset(_values, offset._valueBegin), ValueTableOffset(_values, offset._valueBegin+offset._valueSize)};
		}
		return {};
	}

    template<typename CharType> std::basic_string<CharType> ParameterBox::GetString(ParameterName name) const
    {
        auto type = GetParameterType(name);
        if (type._type == ImpliedTyping::TypeCat::Int8 || type._type == ImpliedTyping::TypeCat::UInt8) {
            std::basic_string<char> result;
            result.resize((unsigned)type._arrayCount);
            GetParameter(name, AsPointer(result.begin()), type);
            return Conversion::Convert<std::basic_string<CharType>>(result);
        }

        if (type._type == ImpliedTyping::TypeCat::Int16 || type._type == ImpliedTyping::TypeCat::UInt16) {
            std::basic_string<utf16> wideResult;
            wideResult.resize((unsigned)type._arrayCount);
            GetParameter(name, AsPointer(wideResult.begin()), type);
            return Conversion::Convert<std::basic_string<CharType>>(wideResult);
        }

        return std::basic_string<CharType>();
    }

    template<typename CharType> bool ParameterBox::GetString(ParameterName name, CharType dest[], size_t destCount) const
    {
        auto type = GetParameterType(name);
        if (type._type == ImpliedTyping::TypeCat::Int8 || type._type == ImpliedTyping::TypeCat::UInt8) {
            std::basic_string<char> intermediate;
            intermediate.resize((unsigned)type._arrayCount);
            GetParameter(name, AsPointer(intermediate.begin()), type);

            auto finalLength = Conversion::Convert(dest, destCount-1,
                AsPointer(intermediate.begin()), AsPointer(intermediate.end()));
            if (finalLength < 0) return false;

            dest[std::min(destCount-1, (size_t)finalLength)] = CharType(0);
            return true;
        }

        if (type._type == ImpliedTyping::TypeCat::Int16 || type._type == ImpliedTyping::TypeCat::UInt16) {
            std::basic_string<utf16> intermediate;
            intermediate.resize((unsigned)type._arrayCount);
            GetParameter(name, AsPointer(intermediate.begin()), type);

            auto finalLength = Conversion::Convert(dest, destCount-1,
                AsPointer(intermediate.begin()), AsPointer(intermediate.end()));
            if (finalLength < 0) return false;

            dest[std::min(destCount-1, (size_t)finalLength)] = CharType(0);
            return true;
        }

        return false;
    }

    template void ParameterBox::SetParameter(StringSection<utf8> name, uint32 value);
    template std::optional<uint32> ParameterBox::GetParameter(ParameterName name) const;

    template void ParameterBox::SetParameter(StringSection<utf8> name, int32 value);
    template std::optional<int32> ParameterBox::GetParameter(ParameterName name) const;

    template void ParameterBox::SetParameter(StringSection<utf8> name, uint64 value);
    template std::optional<uint64> ParameterBox::GetParameter(ParameterName name) const;

    template void ParameterBox::SetParameter(StringSection<utf8> name, int64 value);
    template std::optional<int64> ParameterBox::GetParameter(ParameterName name) const;

    template void ParameterBox::SetParameter(StringSection<utf8> name, bool value);
    template std::optional<bool> ParameterBox::GetParameter(ParameterName name) const;

    template void ParameterBox::SetParameter(StringSection<utf8> name, float value);
    template std::optional<float> ParameterBox::GetParameter(ParameterName name) const;

    template void ParameterBox::SetParameter(StringSection<utf8> name, double value);
    template std::optional<double> ParameterBox::GetParameter(ParameterName name) const;


    template void ParameterBox::SetParameter(StringSection<utf8> name, Float2 value);
    template std::optional<Float2> ParameterBox::GetParameter(ParameterName name) const;
    
    template void ParameterBox::SetParameter(StringSection<utf8> name, Float3 value);
    template std::optional<Float3> ParameterBox::GetParameter(ParameterName name) const;

    template void ParameterBox::SetParameter(StringSection<utf8> name, Float4 value);
    template std::optional<Float4> ParameterBox::GetParameter(ParameterName name) const;


    template void ParameterBox::SetParameter(StringSection<utf8> name, Float3x3 value);
    template std::optional<Float3x3> ParameterBox::GetParameter(ParameterName name) const;
    
    template void ParameterBox::SetParameter(StringSection<utf8> name, Float3x4 value);
    template std::optional<Float3x4> ParameterBox::GetParameter(ParameterName name) const;

    template void ParameterBox::SetParameter(StringSection<utf8> name, Float4x4 value);
    template std::optional<Float4x4> ParameterBox::GetParameter(ParameterName name) const;


    template void ParameterBox::SetParameter(StringSection<utf8> name, UInt2 value);
    template std::optional<UInt2> ParameterBox::GetParameter(ParameterName name) const;
    
    template void ParameterBox::SetParameter(StringSection<utf8> name, UInt3 value);
    template std::optional<UInt3> ParameterBox::GetParameter(ParameterName name) const;

    template void ParameterBox::SetParameter(StringSection<utf8> name, UInt4 value);
    template std::optional<UInt4> ParameterBox::GetParameter(ParameterName name) const;
    
    
    template void ParameterBox::SetParameter(StringSection<utf8> name, Int2 value);
    template std::optional<Int2> ParameterBox::GetParameter(ParameterName name) const;
    
    template void ParameterBox::SetParameter(StringSection<utf8> name, Int3 value);
    template std::optional<Int3> ParameterBox::GetParameter(ParameterName name) const;
    
    template void ParameterBox::SetParameter(StringSection<utf8> name, Int4 value);
    template std::optional<Int4> ParameterBox::GetParameter(ParameterName name) const;
    
    
    template std::basic_string<char> ParameterBox::GetString(ParameterName name) const;
    template std::basic_string<utf8> ParameterBox::GetString(ParameterName name) const;
	template std::basic_string<utf16> ParameterBox::GetString(ParameterName name) const;
	// template std::basic_string<ucs2> ParameterBox::GetString(ParameterName name) const;
    template std::basic_string<ucs4> ParameterBox::GetString(ParameterName name) const;

    template bool ParameterBox::GetString(ParameterName name, char dest[], size_t destCount) const;
    template bool ParameterBox::GetString(ParameterName name, utf8 dest[], size_t destCount) const;
	template bool ParameterBox::GetString(ParameterName name, utf16 dest[], size_t destCount) const;
	// template bool ParameterBox::GetString(ParameterName name, ucs2 dest[], size_t destCount) const;
    template bool ParameterBox::GetString(ParameterName name, ucs4 dest[], size_t destCount) const;

    uint64      ParameterBox::CalculateParameterNamesHash() const
    {
            //  Note that the parameter names are always in the same order (unless 
            //  two different names resolve to the same 32 hash value). So we should be
			//	ok if the same parameter names are added in 2 different orders.
        return Hash64(AsPointer(_hashNames.cbegin()), AsPointer(_hashNames.cend()));
    }

    uint64      ParameterBox::CalculateHash() const
    {
        return Hash64(AsPointer(_values.cbegin()), AsPointer(_values.cend()));
    }

    size_t ParameterBox::GetCount() const
    {
        return (unsigned)_offsets.size();
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

        auto i  = _hashNames.cbegin();
        auto i2 = source._hashNames.cbegin();
        while (i < _hashNames.cend() && i2 < source._hashNames.cend()) {

            if (*i < *i2)       { ++i; } 
            else if (*i > *i2)  { ++i2; } 
            else if (*i == *i2) {
                auto offsetDest = _offsets[std::distance(_hashNames.cbegin(), i)]._valueBegin;
                auto typeDest   = _types[std::distance(_hashNames.cbegin(), i)];
                auto offsetSrc  = source._offsets[std::distance(source._hashNames.cbegin(), i2)];
                auto typeSrc    = source._types[std::distance(source._hashNames.cbegin(), i2)];
                
                if (typeDest == typeSrc) {
                    XlCopyMemory(
                        PtrAdd(temporaryValues, offsetDest), 
                        ValueTableOffset(source._values, offsetSrc._valueBegin),
                        offsetSrc._valueSize);
                } else {
                        // sometimes we get trival casting situations (like "unsigned int" to "int")
                        //  -- even in those cases, we execute the casting function, which will effect performance
                    bool castSuccess = ImpliedTyping::Cast(
                        { PtrAdd(temporaryValues, offsetDest), PtrAdd(temporaryValues, sizeof(temporaryValues)) }, typeDest,
                        { ValueTableOffset(source._values, offsetSrc._valueBegin), ValueTableOffset(source._values, offsetSrc._valueBegin+offsetSrc._valueSize) },
                        typeSrc);

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

    bool ParameterBox::AreParameterNamesEqual(const ParameterBox& other) const
    {
            // return true iff both boxes have exactly the same parameter names, in the same order
        if (_names.size() != other._names.size()) {
            return false;
        }
        return GetParameterNamesHash() == other.GetParameterNamesHash();
    }

    void ParameterBox::MergeIn(const ParameterBox& source)
    {
        auto srcHashNameI = source._hashNames.cbegin();
        auto hashNameI = _hashNames.cbegin();

        for (;;) {
            if (srcHashNameI == source._hashNames.cend()) return;

            // Skip over any parameters in "this" that should come before the
            // parameter we need to merge in
            while (hashNameI < _hashNames.cend() && *hashNameI < *srcHashNameI) ++hashNameI;

            auto srcIdx = std::distance(source._hashNames.cbegin(), srcHashNameI);
            auto srcOffsets = source._offsets[srcIdx];
            hashNameI = SetParameterHint(
                hashNameI,
                *srcHashNameI,
                { PtrAdd(source._names.begin(), srcOffsets._nameBegin), PtrAdd(source._names.begin(), srcOffsets._nameBegin+srcOffsets._nameSize) },
                { ValueTableOffset(source._values, srcOffsets._valueBegin), ValueTableOffset(source._values, srcOffsets._valueBegin+srcOffsets._valueSize) },
                source._types[srcIdx]);
            ++srcHashNameI;
            ++hashNameI;
        }
    }

    template<typename CharType>
        std::string AsString(const std::vector<CharType>& buffer, size_t len)
    {
        return Conversion::Convert<std::string>(
            std::basic_string<CharType>(AsPointer(buffer.cbegin()), AsPointer(buffer.cbegin()) + len));
    }

    template<typename CharType>
        void    ParameterBox::Serialize(OutputStreamFormatter& stream) const
    {
        std::vector<CharType> tmpBuffer;
        std::vector<CharType> nameBuffer;

        for (auto i=_offsets.cbegin(); i!=_offsets.cend(); ++i) {
            const auto* name = &_names[i->_nameBegin];
            const void* value = ValueTableOffset(_values, i->_valueBegin);
            const auto& type = _types[std::distance(_offsets.begin(), i)];

            auto nameLen = i->_nameSize;
            nameBuffer.resize((nameLen*2)+1);     // (note; we're assuming this stl implementation won't reallocate when resizing to smaller size)
            auto finalNameLen = Conversion::Convert(
                AsPointer(nameBuffer.begin()), nameBuffer.size(),
                name, &name[nameLen]);
               
                // attributes with empty name strings will throw an exception here
            if (finalNameLen <= 0) {
                // Throw(::Exceptions::BasicLabel("Empty name string or error during name conversion"));
                nameBuffer.resize(64);
                XlUI64toA(_hashNames[std::distance(_offsets.cbegin(), i)], (char*)nameBuffer.data(), nameBuffer.size(), 16);
                finalNameLen = (unsigned)(std::find(nameBuffer.begin(), nameBuffer.end(), (utf8)'\0') - nameBuffer.begin());
            }

                // We need special cases for string types. In these cases we might have to
                // do some conversion to get the value in the format we want.
            if (type._type == ImpliedTyping::TypeCat::Int8 || type._type == ImpliedTyping::TypeCat::UInt8) {
                auto start = (const utf8*)value;
                tmpBuffer.resize((type._arrayCount*2)+1);
                auto valueLen = Conversion::Convert(
                    AsPointer(tmpBuffer.begin()), tmpBuffer.size(),
                    start, &start[type._arrayCount]);
                
                if (valueLen < 0)
                    Throw(::Exceptions::BasicLabel("Error during string conversion for member: %s", AsString(nameBuffer, finalNameLen).c_str()));

                stream.WriteAttribute(
                    AsPointer(nameBuffer.begin()), AsPointer(nameBuffer.begin()) + finalNameLen,
                    AsPointer(tmpBuffer.begin()), AsPointer(tmpBuffer.begin()) + valueLen);
                continue;
            }

            if (type._type == ImpliedTyping::TypeCat::Int16 || type._type == ImpliedTyping::TypeCat::UInt16) {
                auto start = (const ucs2*)value;
                tmpBuffer.resize((type._arrayCount*2)+1);
                auto valueLen = Conversion::Convert(
                    AsPointer(tmpBuffer.begin()), tmpBuffer.size(),
                    start, &start[type._arrayCount]);
                
                if (valueLen < 0)
                    Throw(::Exceptions::BasicLabel("Error during string conversion for member: %s", AsString(nameBuffer, finalNameLen).c_str()));

                stream.WriteAttribute(
                    AsPointer(nameBuffer.begin()), AsPointer(nameBuffer.begin()) + finalNameLen,
                    AsPointer(tmpBuffer.begin()), AsPointer(tmpBuffer.begin()) + valueLen);
                continue;
            }

            auto stringFormat = ImpliedTyping::AsString(value, _values.size() - i->_valueBegin, type, true);
            auto convertedString = Conversion::Convert<std::basic_string<CharType>>(stringFormat);
            stream.WriteAttribute(
                AsPointer(nameBuffer.begin()), AsPointer(nameBuffer.begin()) + finalNameLen,
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

    template<typename CharType>
        ParameterBox::ParameterBox(
            InputStreamFormatter<CharType>& stream, 
            IteratorRange<const void*> defaultValue, const ImpliedTyping::TypeDesc& defaultValueType)
    {
        using namespace ImpliedTyping;

            // note -- fixed size buffer here bottlenecks max size for native representations
            // of these values
        uint8 nativeTypeBuffer[NativeRepMaxSize];
        std::vector<utf8> nameBuffer;
        std::vector<char> valueBuffer;

            // attempt to read attributes from a serialized text file
            // as soon as we hit something that is not another attribute
            // (it could be a sub-element, or the end of this element)
            // then we will stop reading and return
        while (stream.PeekNext() == InputStreamFormatter<CharType>::Blob::AttributeName) {
            typename InputStreamFormatter<CharType>::InteriorSection name, value;
            bool success = stream.TryAttribute(name, value);
            if (!success)
                Throw(::Exceptions::BasicLabel("Parsing exception while reading attribute in parameter box deserialization"));

            auto nameLen = (size_t(name._end) - size_t(name._start)) / sizeof(CharType);
			{
                nameBuffer.resize(nameLen*2+1);
                
                auto nameConvResult = Conversion::Convert(
                    AsPointer(nameBuffer.begin()), nameBuffer.size(),
                    name._start, name._end);

                if (nameConvResult <= 0)
                    Throw(::Exceptions::BasicLabel("Empty name or error converting string name in parameter box deserialization"));

				nameLen = std::min(nameBuffer.size()-1, (size_t)nameConvResult);
                nameBuffer[nameLen] = '\0';
            }

            if (!value._start || !value._end) {
                    // if there is no value attached, we default to the value given us
                    // (usually jsut void)
                SetParameter(AsPointer(nameBuffer.cbegin()), defaultValue, defaultValueType);
                continue;
            }

            TypeDesc nativeType(TypeCat::Void);
            if (constant_expression<sizeof(CharType) == sizeof(utf8)>::result()) {

                nativeType = Parse(
                    MakeStringSection((const char*)value._start, (const char*)value._end),
                    nativeTypeBuffer, sizeof(nativeTypeBuffer));

            } else {

                valueBuffer.resize((value._end - value._start)*2+1);
                auto valueLen = Conversion::Convert(
                    AsPointer(valueBuffer.begin()), valueBuffer.size(),
                    value._start, value._end);

                // a failed conversion here is valid, but it means we must treat the value as a string
                if (valueLen>=0) {
                    nativeType = Parse(
                        MakeStringSection(AsPointer(valueBuffer.begin()), AsPointer(valueBuffer.begin()) + valueLen),
                        nativeTypeBuffer, sizeof(nativeTypeBuffer));
                }

            }

            if (nativeType._type != TypeCat::Void) {
                SetParameter(
					MakeStringSection(AsPointer(nameBuffer.cbegin()), PtrAdd(AsPointer(nameBuffer.cbegin()), nameLen)), 
					MakeIteratorRange(nativeTypeBuffer, PtrAdd(nativeTypeBuffer, nativeType.GetSize())), 
					nativeType);
            } else {
                    // this is just a string. We should store it as a string, in whatever character set it came in
                SetParameter(
                    MakeStringSection(AsPointer(nameBuffer.cbegin()), PtrAdd(AsPointer(nameBuffer.cbegin()), nameLen)),
                    MakeIteratorRange(value.begin(), value.end()), 
                    TypeDesc(TypeOf<CharType>()._type, uint16(value._end - value._start), TypeHint::String));
            }
        }
    }

    ParameterBox::ParameterBox(ParameterBox&& moveFrom) never_throws
    : _hashNames(std::move(moveFrom._hashNames))
    , _offsets(std::move(moveFrom._offsets))
    , _names(std::move(moveFrom._names))
    , _values(std::move(moveFrom._values))
    , _types(std::move(moveFrom._types))
    {
        _cachedHash = moveFrom._cachedHash;
        _cachedParameterNameHash = moveFrom._cachedParameterNameHash;
    }
        
    ParameterBox& ParameterBox::operator=(ParameterBox&& moveFrom) never_throws
    {
        _hashNames = std::move(moveFrom._hashNames);
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

    template ParameterBox::ParameterBox(InputStreamFormatter<utf8>& stream, IteratorRange<const void*>, const ImpliedTyping::TypeDesc&);
    template ParameterBox::ParameterBox(InputStreamFormatter<ucs2>& stream, IteratorRange<const void*>, const ImpliedTyping::TypeDesc&);
    template ParameterBox::ParameterBox(InputStreamFormatter<ucs4>& stream, IteratorRange<const void*>, const ImpliedTyping::TypeDesc&);

///////////////////////////////////////////////////////////////////////////////////////////////////

    void BuildStringTable(StringTable& defines, const ParameterBox& box)
    {
        for (const auto&i:box) {
            const auto name = i.Name();
            auto value = i.RawValue();
            const auto& type = i.Type();
            auto stringFormat = ImpliedTyping::AsString(
                value.begin(), value.size(), type);

            auto insertPosition = std::lower_bound(
                defines.begin(), defines.end(), name.begin(), StringTableComparison());
            if (insertPosition!=defines.cend() && !XlCompareString(insertPosition->first, name)) {
                insertPosition->second = stringFormat;
            } else {
                defines.insert(insertPosition, std::make_pair(name.begin(), stringFormat));
            }
        }
    }

    void OverrideStringTable(StringTable& defines, const ParameterBox& box)
    {
        for (const auto&i:box) {
            const auto name = i.Name();
            auto value = i.RawValue();
            const auto& type = i.Type();

            auto insertPosition = std::lower_bound(
                defines.begin(), defines.end(), name.begin(), StringTableComparison());

            if (insertPosition!=defines.cend() && !XlCompareString(insertPosition->first, name)) {
                insertPosition->second = ImpliedTyping::AsString(
                    value.begin(), value.size(), type);
            }
        }
    }

    std::string FlattenStringTable(const StringTable& stringTable)
    {
        std::string combinedStrings;
        
            // Calculate size of the concatenated string first, so we can avoid allocations during the 
            // concatenation process.
        size_t size = 0;
        std::for_each(stringTable.cbegin(), stringTable.cend(), 
            [&size](const std::pair<const utf8*, std::string>& object) { size += 2 + XlStringLen(object.first) + object.second.size(); });
        combinedStrings.reserve(size+1);

        std::for_each(stringTable.cbegin(), stringTable.cend(), 
            [&combinedStrings](const std::pair<const utf8*, std::string>& object) 
            {
                combinedStrings.insert(combinedStrings.end(), (const char*)object.first, (const char*)XlStringEnd(object.first));
                combinedStrings.push_back('=');
                combinedStrings.insert(combinedStrings.end(), object.second.cbegin(), object.second.cend()); 
                combinedStrings.push_back(';');
            });
        return combinedStrings;
    }

	std::string BuildFlatStringTable(const ParameterBox& box)
	{
		std::stringstream str;
		for (const auto&i:box)
			str << i.Name().Cast<char>() << '=' << i.ValueAsString() << ';';
		return str.str();
	}

    IteratorRange<const void*> ParameterBox::Iterator::Value::RawValue() const
    {
		const auto& offsets = _box->_offsets[_index];
		return {ValueTableOffset(_box->_values, offsets._valueBegin), ValueTableOffset(_box->_values, offsets._valueBegin + offsets._valueSize)};
    }

}



