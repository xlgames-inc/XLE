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
            { "byte", TypeCat::UInt8 }
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
}


