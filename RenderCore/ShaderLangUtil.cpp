// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderLangUtil.h"
#include <algorithm>

namespace RenderCore
{
    template<typename DestType = unsigned, typename CharType = char>
        DestType StringToUnsigned(const StringSection<CharType> source)
    {
        auto* start = source.begin();
        auto* end = source.end();
        if (start >= end) return 0;

        auto result = DestType(0);
        for (;;) {
            if (start >= end) break;
            if (*start < '0' || *start > '9') break;
            result = (result * 10) + DestType((*start) - '0');
            ++start;
        }
        return result;
    }

    ImpliedTyping::TypeDesc ShaderLangTypeNameAsTypeDesc(StringSection<char> hlslTypeName)
    {
            // Note that HLSL type names are not case sensitive!
            //  see "Keywords" in the HLSL docs
        using namespace ImpliedTyping;
        static std::pair<StringSection<char>, ImpliedTyping::TypeCat> baseTypes[] = 
        {
            { "float", TypeCat::Float },
            { "uint", TypeCat::UInt32 },
            { "dword", TypeCat::UInt32 },
            { "int", TypeCat::Int32 },
            { "byte", TypeCat::UInt8 },
            { "bool", TypeCat::Bool },

            { "vec", TypeCat::Float }          // GLSL-style naming
            // "half", "double" not supported
        };
        for (unsigned c=0; c<dimof(baseTypes); ++c) {
            auto len = baseTypes[c].first.Length();
            if (hlslTypeName.Length() >= len
                && !XlComparePrefixI(baseTypes[c].first.begin(), hlslTypeName.begin(), len)) {

                auto matrixMarker = hlslTypeName.begin() + len;
                while (matrixMarker != hlslTypeName.end() && *matrixMarker != 'x') ++matrixMarker;
                if (matrixMarker != hlslTypeName.end()) {
                    auto count0 = StringToUnsigned(MakeStringSection(&hlslTypeName[len], matrixMarker));
                    auto count1 = StringToUnsigned(MakeStringSection(matrixMarker+1, hlslTypeName.end()));

                    TypeDesc result;
                    result._arrayCount = (uint16)std::max(1u, count0 * count1);
                    result._type = baseTypes[c].second;
                    result._typeHint = TypeHint::Matrix;
                    return result;
                } else {
                    auto count = StringToUnsigned(MakeStringSection(hlslTypeName.begin() + len, hlslTypeName.end()));
                    if (count == 0 || count > 4) count = 1;
                    TypeDesc result;
                    result._arrayCount = (uint16)count;
                    result._type = baseTypes[c].second;
                    result._typeHint = (count > 1) ? TypeHint::Vector : TypeHint::None;
                    return result;
                }
            }
        }

        return TypeDesc(TypeCat::Void, 0);
    }

    std::string AsShaderLangTypeName(const ImpliedTyping::TypeDesc& type)
    {
        const char* baseName = nullptr;
        using namespace ImpliedTyping;
        switch (type._type) {
        case TypeCat::Bool: baseName = "bool"; break;
        case TypeCat::Int8: baseName = "int"; break;
        case TypeCat::UInt8: baseName = "byte"; break;
        case TypeCat::Int16: baseName = "int"; break;
        case TypeCat::UInt16: baseName = "uint"; break;
        case TypeCat::Int32: baseName = "int"; break;
        case TypeCat::UInt32: baseName = "uint"; break;
        case TypeCat::Int64: baseName = "uint"; break;
        case TypeCat::UInt64: baseName = "uint"; break;
        case TypeCat::Float: baseName = "float"; break;
        case TypeCat::Double: baseName = "float"; break;
        default:
        case TypeCat::Void: return "";
        }

        if (type._typeHint == TypeHint::Matrix) {
            if (type._arrayCount == 16)
                return baseName + std::string("4x4");
            if (type._arrayCount == 12)
                return baseName + std::string("3x4");
            if (type._arrayCount == 9)
                return baseName + std::string("3x3");
        }

        if (type._arrayCount <= 1)
            return baseName;
        return baseName + std::to_string(type._arrayCount);
    }
}


