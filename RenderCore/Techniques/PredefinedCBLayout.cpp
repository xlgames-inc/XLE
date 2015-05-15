// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PredefinedCBLayout.h"
#include "../RenderUtils.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/BitUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/StringUtils.h"
#include <regex>

namespace RenderCore { namespace Techniques
{
    static ImpliedTyping::TypeDesc HLSLTypeNameAsTypeDesc(const char hlslTypeName[])
    {
        using namespace ImpliedTyping;
        std::pair<const char*, ImpliedTyping::TypeCat> baseTypes[] = 
        {
            { "float", TypeCat::Float },
            { "uint", TypeCat::UInt32 },
            { "dword", TypeCat::UInt32 },
            { "int", TypeCat::Int32 },
            { "byte", TypeCat::UInt8 }
            // "half", "double" not supported
        };
        for (unsigned c=0; c<dimof(baseTypes); ++c) {
            auto len = XlStringLen(baseTypes[c].first);
            if (!XlComparePrefix(baseTypes[c].first, hlslTypeName, len)) {
                const auto matrixMarker = XlFindChar(&hlslTypeName[len], 'x');
                if (matrixMarker != nullptr) {
                    auto count0 = XlAtoUI32(&hlslTypeName[len]);
                    auto count1 = XlAtoUI32(matrixMarker+1);

                    TypeDesc result;
                    result._arrayCount = (uint16)std::max(1u, count0 * count1);
                    result._type = baseTypes[c].second;
                    result._typeHint = TypeHint::Matrix;
                    return result;
                } else {
                    auto count = XlAtoUI32(&hlslTypeName[len]);
                    if (count == 0 || count > 4) count = 1;
                    TypeDesc result;
                    result._arrayCount = (uint16)count;
                    result._type = baseTypes[c].second;
                    result._typeHint = (count > 1) ? TypeHint::Vector : TypeHint::None;
                    return result;
                }
            }
        }

        return TypeDesc();
    }

    PredefinedCBLayout::PredefinedCBLayout(const ::Assets::ResChar initializer[])
    {
        // Here, we will read a simple configuration file that will define the layout
        // of a constant buffer. Sometimes we need to get the layout of a constant 
        // buffer without compiling any shader code, or really touching the HLSL at all.
        std::regex parseStatement(R"--((\w*)\s+(\w*)\s*(?:=\s*([^;]*))?;?\s*)--");
            
        size_t size;
        auto file = LoadFileAsMemoryBlock(initializer, &size);

        unsigned cbIterator = 0;

        const char* iterator = (const char*)file.get();
        const char* end = PtrAdd(iterator, size);
        for (;;) {
            while (iterator < end && (*iterator == '\n' || *iterator == '\r' || *iterator == ' '|| *iterator == '\t')) ++iterator;
            const char* lineStart = iterator;
            if (lineStart >= end) break;

            while (iterator < end && *iterator != '\n' && *iterator != '\r') ++iterator;

                // double slash at the start of a line means ignore the reset of the line
            if (*lineStart == '/' && (lineStart+1) < iterator && *(lineStart+1) == '/')
                continue;

            std::match_results<const char*> match;
            bool a = std::regex_match(lineStart, iterator, match, parseStatement);
            if (a && match.size() >= 3) {
                Element e;
                e._name = match[2];
                e._hash = ParameterBox::MakeParameterNameHash(e._name);
                e._type = HLSLTypeNameAsTypeDesc(match[1].str().c_str());

                auto size = e._type.GetSize();
                if (!size) {
                    LogWarning << "Problem parsing type in PredefinedCBLayout. Type size is 0: " << std::string(lineStart, iterator);
                    continue;
                }

                    // HLSL adds padding so that vectors don't straddle 16 byte boundaries!
                    // let's detect that case, and add padding as necessary
                if (FloorToMultiplePow2(e._offset, 16) != FloorToMultiplePow2(e._offset + std::min(16u, e._type.GetSize()) - 1, 16)) {
                    cbIterator = CeilToMultiplePow2(cbIterator, 16);
                }

                e._offset = cbIterator;
                cbIterator += size;
                _elements.push_back(e);

                if (match.size() > 3 && match[3].matched) {
                    uint8 buffer0[256], buffer1[256];
                    auto defaultType = ImpliedTyping::Parse(
                        match[3].first, match[3].second,
                        buffer0, dimof(buffer0));

                    if (!(defaultType == e._type)) {
                            //  The initialiser isn't exactly the same type as the
                            //  defined variable. Let's try a casting operation.
                            //  Sometimes we can get int defaults for floats variables, etc.
                        bool castSuccess = ImpliedTyping::Cast(
                            buffer1, dimof(buffer1), e._type,
                            buffer0, defaultType);
                        if (castSuccess) {
                            _defaults.SetParameter(e._name.c_str(), buffer1, e._type);
                        } else {
                            LogWarning << "Default initialiser can't be cast to same type as variable in PredefinedCBLayout: " << std::string(lineStart, iterator);
                        }
                    } else {
                        _defaults.SetParameter(e._name.c_str(), buffer0, defaultType);
                    }
                }
            } else {
                LogWarning << "Failed to parse line in PredefinedCBLayout: " << std::string(lineStart, iterator);
            }
        }

        _cbSize = cbIterator;
        _cbSize = CeilToMultiplePow2(_cbSize, 16);

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterFileDependency(_validationCallback, initializer);
    }

    PredefinedCBLayout::~PredefinedCBLayout() {}

    void PredefinedCBLayout::WriteBuffer(void* dst, const ParameterBox& parameters) const
    {
        for (auto c=_elements.cbegin(); c!=_elements.cend(); ++c) {
            bool gotValue = parameters.GetParameter(
                c->_hash, PtrAdd(dst, c->_offset),
                c->_type);

            if (!gotValue)
                _defaults.GetParameter(c->_hash, PtrAdd(dst, c->_offset), c->_type);
        }
    }

    std::vector<uint8> PredefinedCBLayout::BuildCBDataAsVector(const ParameterBox& parameters) const
    {
        std::vector<uint8> cbData(_cbSize, uint8(0));
        WriteBuffer(AsPointer(cbData.begin()), parameters);
        return std::move(cbData);
    }

    SharedPkt PredefinedCBLayout::BuildCBDataAsPkt(const ParameterBox& parameters) const
    {
        SharedPkt result = MakeSharedPktSize(_cbSize);
        XlSetMemory(result.begin(), 0, _cbSize);
        WriteBuffer(result.begin(), parameters);
        return std::move(result);
    }
}}
