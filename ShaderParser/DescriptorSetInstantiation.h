// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "NodeGraphSignature.h"
#include "../RenderCore/Assets/PredefinedDescriptorSetLayout.h"
#include "../RenderCore/ShaderLangUtil.h"
#include "../Utility/StringUtils.h"
#include "../Utility/IteratorUtils.h"
#include <memory>
#include <ios>

namespace RenderCore { namespace Assets { class PredefinedDescriptorSetLayout; }}

namespace ShaderSourceParser
{
	std::shared_ptr<RenderCore::Assets::PredefinedDescriptorSetLayout> MakeMaterialDescriptorSet(
		IteratorRange<const GraphLanguage::NodeGraphSignature::Parameter*> captures,
		RenderCore::ShaderLanguage shaderLanguage,
		std::ostream& warningStream);

	enum class TypeDescriptor { Constant, Resource, Sampler };
	TypeDescriptor CalculateTypeDescriptor(StringSection<> type);
}
