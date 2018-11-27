// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PredefinedCBLayout.h"
#include "../RenderUtils.h"
#include "../ShaderLangUtil.h"
#include "../Format.h"
#include "../../Assets/ConfigFileContainer.h"
#include "../../Assets/IFileSystem.h"
#include "../../Assets/DepVal.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/BitUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/Conversion.h"
#include <regex>

namespace RenderCore { namespace Techniques
{
    PredefinedCBLayout::PredefinedCBLayout(StringSection<::Assets::ResChar> initializer)
    {
		_validationCallback = std::make_shared<::Assets::DependencyValidation>();
		::Assets::RegisterFileDependency(_validationCallback, initializer);

		TRY {
			// Here, we will read a simple configuration file that will define the layout
			// of a constant buffer. Sometimes we need to get the layout of a constant 
			// buffer without compiling any shader code, or really touching the HLSL at all.
			size_t size;
			auto file = ::Assets::TryLoadFileAsMemoryBlock(initializer, &size);
			StringSection<char> configSection((const char*)file.get(), (const char*)PtrAdd(file.get(), size));

			// if it's a compound document, we're only going to extra the cb layout part
			auto compoundDoc = ::Assets::ReadCompoundTextDocument(configSection);
			if (!compoundDoc.empty()) {
				auto i = std::find_if(
					compoundDoc.cbegin(), compoundDoc.cend(),
					[](const ::Assets::TextChunk<char>& chunk)
					{ return XlEqString(chunk._type, "CBLayout"); });
				if (i != compoundDoc.cend())
					configSection = i->_content;
			}

			Parse(configSection);
		} CATCH(const ::Assets::Exceptions::ConstructionError& e) {
			Throw(::Assets::Exceptions::ConstructionError(e, _validationCallback));
		} CATCH (const std::exception& e) {
			Throw(::Assets::Exceptions::ConstructionError(e, _validationCallback));
		} CATCH_END
	}

    PredefinedCBLayout::PredefinedCBLayout(StringSection<char> source, bool)
    {
        Parse(source);
        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
    }

    void PredefinedCBLayout::Parse(StringSection<char> source)
    {
        std::regex parseStatement(R"--((\w*)\s+(\w*)\s*(?:\[(\d*)\])?\s*(?:=\s*([^;]*))?;.*)--");

        unsigned cbIterator = 0;
        const char* iterator = source.begin();
        const char* end = source.end();
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
            if (a && match.size() >= 4) {
                Element e;
                e._name = match[2].str();
                e._hash = ParameterBox::MakeParameterNameHash(e._name);
                e._hash64 = Hash64(AsPointer(e._name.begin()), AsPointer(e._name.end()));
                e._type = ShaderLangTypeNameAsTypeDesc(MakeStringSection(match[1].str()));

                auto size = e._type.GetSize();
                if (!size) {
                    Log(Warning) << "Problem parsing type in PredefinedCBLayout. Type size is 0: " << std::string(lineStart, iterator) << std::endl;
                    continue;
                }

                    // HLSL adds padding so that vectors don't straddle 16 byte boundaries!
                    // let's detect that case, and add padding as necessary
                if (FloorToMultiplePow2(e._offset, 16) != FloorToMultiplePow2(e._offset + std::min(16u, e._type.GetSize()) - 1, 16)) {
                    cbIterator = CeilToMultiplePow2(cbIterator, 16);
                }

                unsigned arrayElementCount = 1;
                if (match.size() > 3 && match[3].matched) {
                    arrayElementCount = Conversion::Convert<unsigned>(match[3].str());
                }

                e._offset = cbIterator;
                e._arrayElementCount = arrayElementCount;
                e._arrayElementStride = (arrayElementCount>1) ? CeilToMultiplePow2(size, 16) : size;
                if (arrayElementCount != 0)
                    cbIterator += (arrayElementCount-1) * e._arrayElementStride + size;
                _elements.push_back(e);

                if (match.size() > 4 && match[4].matched) {

                    if (arrayElementCount > 1)
                        Log(Warning) << "Attempting to provide an default for an array type in PredefinedCBLayout (this isn't supported): " << std::string(lineStart, iterator) << std::endl;

                    uint8 buffer0[256], buffer1[256];
                    auto defaultType = ImpliedTyping::Parse(
                        MakeStringSection(match[4].first, match[4].second),
                        buffer0, dimof(buffer0));

                    if (!(defaultType == e._type)) {
                            //  The initialiser isn't exactly the same type as the
                            //  defined variable. Let's try a casting operation.
                            //  Sometimes we can get int defaults for floats variables, etc.
                        bool castSuccess = ImpliedTyping::Cast(
                            buffer1, dimof(buffer1), e._type,
                            buffer0, defaultType);
                        if (castSuccess) {
							_defaults.SetParameter(MakeStringSection(e._name).Cast<utf8>(), {buffer1, PtrAdd(buffer1, std::min(sizeof(buffer1), (size_t)e._type.GetSize()))}, e._type);
                        } else {
                            Log(Warning) << "Default initialiser can't be cast to same type as variable in PredefinedCBLayout: " << std::string(lineStart, iterator) << std::endl;
                        }
                    } else {
                        _defaults.SetParameter(MakeStringSection(e._name).Cast<utf8>(), {buffer0, PtrAdd(buffer0, std::min(sizeof(buffer0), (size_t)defaultType.GetSize()))}, defaultType);
                    }
                }
            } else {
                Log(Warning) << "Failed to parse line in PredefinedCBLayout: " << std::string(lineStart, iterator) << std::endl;
            }
        }

        _cbSize = cbIterator;
        _cbSize = CeilToMultiplePow2(_cbSize, 16);
    }

    void PredefinedCBLayout::WriteBuffer(void* dst, const ParameterBox& parameters) const
    {
        for (auto c=_elements.cbegin(); c!=_elements.cend(); ++c) {
            for (auto e=0u; e<c->_arrayElementCount; e++) {
                bool gotValue = parameters.GetParameter(
                    c->_hash + e, PtrAdd(dst, c->_offset + e * c->_arrayElementStride),
                    c->_type);

                if (!gotValue)
                    _defaults.GetParameter(c->_hash + e, PtrAdd(dst, c->_offset), c->_type);
            }
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
        std::memset(result.begin(), 0, _cbSize);
        WriteBuffer(result.begin(), parameters);
        return std::move(result);
    }
    
    uint64_t PredefinedCBLayout::CalculateHash() const
    {
        return HashCombine(Hash64(AsPointer(_elements.begin()), AsPointer(_elements.end())), _defaults.GetHash());
    }
    
    auto PredefinedCBLayout::MakeConstantBufferElements() const -> std::vector<ConstantBufferElementDesc>
    {
        std::vector<ConstantBufferElementDesc> result;
        result.reserve(_elements.size());
        for (auto i=_elements.begin(); i!=_elements.end(); ++i) {
            result.push_back(ConstantBufferElementDesc {
                i->_hash64, AsFormat(i->_type),
                i->_offset, i->_arrayElementCount });
        }
        return result;
    }

    PredefinedCBLayout::PredefinedCBLayout() 
	: _cbSize(0)
	{}    
    PredefinedCBLayout::~PredefinedCBLayout() {}
}}
