// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DescriptorSetInstantiation.h"
#include "NodeGraphSignature.h"
#include "../RenderCore/Assets/PredefinedCBLayout.h"
#include "../RenderCore/ShaderLangUtil.h"
#include "../Utility/StringUtils.h"
#include "../Utility/IteratorUtils.h"
#include <set>
#include <unordered_map>

namespace ShaderSourceParser
{
	TypeDescriptor CalculateTypeDescriptor(StringSection<> type)
    {
        // HLSL keywords are not case sensitive. We could assume that
        // a type name that does not begin with one of the scalar type
        // prefixes is a global resource (like a texture, etc). This
        // should provide some flexibility with new DX12 types, and perhaps
        // allow for manipulating custom "interface" types...?
        //
        // However, that isn't going to work well with struct types (which
        // can be contained with cbuffers and be passed to functions like
        // scalars)
        //
        // const char* scalarTypePrefixes[] =
        // {
        //     "bool", "int", "uint", "half", "float", "double"
        // };
        //
        // Note that we only check the prefix -- to catch variations on
        // texture and RWTexture (and <> arguments like Texture2D<float4>)

        const char* resourceTypePrefixes[] =
        {
            "cbuffer", "tbuffer",
            "StructuredBuffer", "Buffer", "ByteAddressBuffer", "AppendStructuredBuffer",
            "RWBuffer", "RWByteAddressBuffer", "RWStructuredBuffer",

            "RWTexture", // RWTexture1D, RWTexture1DArray, RWTexture2D, RWTexture2DArray, RWTexture3D
            "texture", // Texture1D, Texture1DArray, Texture2D, Texture2DArray, Texture2DMS, Texture2DMSArray, Texture3D, TextureCube, TextureCubeArray

            // special .fx file types:
            // "BlendState", "DepthStencilState", "DepthStencilView", "RasterizerState", "RenderTargetView",
        };
        // note -- some really special function signature-only: InputPatch, OutputPatch, LineStream, TriangleStream, PointStream

        for (unsigned c=0; c<dimof(resourceTypePrefixes); ++c)
            if (XlBeginsWithI(type, MakeStringSection(resourceTypePrefixes[c])))
                return TypeDescriptor::Resource;

		const char* samplerTypePrefixes[] =
        {
			"sampler", // SamplerState, SamplerComparisonState
		};
		for (unsigned c=0; c<dimof(samplerTypePrefixes); ++c)
            if (XlBeginsWithI(type, MakeStringSection(samplerTypePrefixes[c])))
                return TypeDescriptor::Sampler;

        return TypeDescriptor::Constant;
    }

	static std::string MakeGlobalName(const std::string& str)
	{
		auto i = str.find('.');
		if (i != std::string::npos)
			return str.substr(i+1);
		return str;
	}

	std::shared_ptr<MaterialDescriptorSet> MakeMaterialDescriptorSet(
		IteratorRange<const GraphLanguage::NodeGraphSignature::Parameter*> captures,
		std::ostream& warningStream)
	{
		std::vector<std::string> srvs;
		std::vector<std::string> samplers;

		using NameAndType = RenderCore::Assets::PredefinedCBLayout::NameAndType;
		struct WorkingCB
		{
			std::vector<NameAndType> _cbElements;
			ParameterBox _defaults;
		};
		std::unordered_map<std::string, WorkingCB> workingCBs;
		std::set<std::string> texturesAlreadyStored;

		// hack -- skip DiffuseTexture and NormalsTexture, because these are provided by the system headers
		// texturesAlreadyStored.insert("DiffuseTexture");
		// texturesAlreadyStored.insert("NormalsTexture");

		for (const auto&c : captures) {
			auto type = CalculateTypeDescriptor(c._type);
			if (type != TypeDescriptor::Constant) {
				auto globalName = MakeGlobalName(c._name);
				if (texturesAlreadyStored.find(globalName) == texturesAlreadyStored.end()) {
					texturesAlreadyStored.insert(globalName);
					// This capture must be either an srv or a sampler
					if (c._direction == GraphLanguage::ParameterDirection::In) {
						if (type == TypeDescriptor::Resource) {
							srvs.push_back(globalName);
						} else
							samplers.push_back(globalName);					
					}
				}
				continue;
			}

			auto fmt = RenderCore::ShaderLangTypeNameAsTypeDesc(c._type);
			if (fmt._type == ImpliedTyping::TypeCat::Void) {
				warningStream << "\t// Could not convert type (" << c._type << ") to shader language type for capture (" << c._name << "). Skipping cbuffer entry." << std::endl;
				continue;
			}

			std::string cbName, memberName;
			auto i = c._name.find('.');
			if (i != std::string::npos) {
				cbName = c._name.substr(0, i);
				memberName = c._name.substr(i+1);
			} else {
				cbName = "BasicMaterialConstants";
				memberName = c._name;
			}

			auto cbi = workingCBs.find(cbName);
			if (cbi == workingCBs.end())
				cbi = workingCBs.insert(std::make_pair(cbName, WorkingCB{})).first;

			cbi->second._cbElements.push_back(NameAndType{ memberName, fmt });
			if (!c._default.empty())
				cbi->second._defaults.SetParameter(
					MakeStringSection(memberName).Cast<utf8>(),
					MakeStringSection(c._default));
		}

		auto result = std::make_shared<MaterialDescriptorSet>();
		result->_srvs = std::move(srvs);
		result->_samplers = std::move(samplers);

		for (auto&cb:workingCBs) {
			// Sort first in alphabetical order, and then optimize for
			// type packing. This ensures that we get the same output layout for a given
			// input, regardless of the input's original ordering.
			std::sort(
				cb.second._cbElements.begin(), cb.second._cbElements.end(),
				[](const NameAndType& lhs, const NameAndType& rhs) {
					return lhs._name < rhs._name;
				});
			RenderCore::Assets::PredefinedCBLayout::OptimizeElementOrder(MakeIteratorRange(cb.second._cbElements));

			auto layout = std::make_shared<RenderCore::Assets::PredefinedCBLayout>();
			layout->AppendElements(MakeIteratorRange(cb.second._cbElements));

			if (!layout->_elements.empty())
				result->_constantBuffers.emplace_back(
					MaterialDescriptorSet::ConstantBuffer {
						cb.first,
						std::move(layout)
					});
		}

		return result;
	}
}
