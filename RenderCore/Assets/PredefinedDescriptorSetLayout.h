// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Utility/StringUtils.h"
#include <string>
#include <vector>
#include <memory>

namespace Assets { class DirectorySearchRules; class DependencyValidation; }
namespace Utility { class ConditionalProcessingTokenizer; }
namespace RenderCore { enum class DescriptorType; }

namespace RenderCore { namespace Assets 
{
	class PredefinedCBLayout;

	class PredefinedDescriptorSetLayout
	{
	public:

		// We should ideally support all of the descriptor slot type below
		// There's a bit of ambiguity, though, because the shader language
		// itself may not need to be so explicit about the slot type
		// Sampler = VK_DESCRIPTOR_TYPE_SAMPLER
		// VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
		// Texture = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
		// UnorderedAccessTexture = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
		// VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
		// VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
		// ConstantBuffer = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
		// UnorderedAccessBuffer = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
		// VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
		// VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
		// VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT

		struct ConditionalDescriptorSlot
		{
			std::string _name;
			DescriptorType _type;
			unsigned _arrayElementCount = 0u;
			unsigned _cbIdx = ~0u;		// this is an idx into the _constantBuffers array for constant buffer types
			std::string _conditions;
		};
		std::vector<ConditionalDescriptorSlot> _slots;
		std::vector<std::shared_ptr<RenderCore::Assets::PredefinedCBLayout>> _constantBuffers;

		PredefinedDescriptorSetLayout(
			StringSection<> inputData,
			const ::Assets::DirectorySearchRules& searchRules,
			const std::shared_ptr<::Assets::DependencyValidation>& depVal);
		PredefinedDescriptorSetLayout();
		~PredefinedDescriptorSetLayout();

		const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _depVal; }

	protected:
		void ParseSlot(ConditionalProcessingTokenizer& iterator, DescriptorType type);

		std::shared_ptr<::Assets::DependencyValidation> _depVal;
	};

}}

