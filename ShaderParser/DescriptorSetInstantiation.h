// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "NodeGraphSignature.h"
#include "../RenderCore/ShaderLangUtil.h"
#include "../Utility/StringUtils.h"
#include "../Utility/IteratorUtils.h"
#include <memory>
#include <string>
#include <vector>
#include <ios>

namespace RenderCore { namespace Assets { class PredefinedCBLayout; }}

namespace ShaderSourceParser
{
	class MaterialDescriptorSet
	{
	public:
		struct ConstantBuffer
		{
			std::string _name;
			std::shared_ptr<RenderCore::Assets::PredefinedCBLayout> _layout;
		};

		std::vector<ConstantBuffer> _constantBuffers;
		std::vector<std::string> _srvs;
		std::vector<std::string> _samplers;
	};

	std::shared_ptr<MaterialDescriptorSet> MakeMaterialDescriptorSet(
		IteratorRange<const GraphLanguage::NodeGraphSignature::Parameter*> captures,
		RenderCore::ShaderLanguage shaderLanguage,
		std::ostream& warningStream);

	enum class TypeDescriptor { Constant, Resource, Sampler };
	TypeDescriptor CalculateTypeDescriptor(StringSection<> type);
}
