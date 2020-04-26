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

namespace RenderCore { namespace Assets 
{
	class PredefinedCBLayout;

	class PredefinedDescriptorSetLayout
	{
	public:
		struct ConstantBuffer
		{
			std::string _name;
			std::shared_ptr<RenderCore::Assets::PredefinedCBLayout> _layout;
			std::string _conditions;
		};
		std::vector<ConstantBuffer> _constantBuffers;

		struct Resource
		{
			std::string _name;
			std::string _conditions;
			unsigned _arrayElementCount = 0u;
		};
		std::vector<Resource> _resources;

		struct Sampler
		{
			std::string _name;
			std::string _conditions;
			unsigned _arrayElementCount = 0u;
		};
		std::vector<Sampler> _samplers;

		PredefinedDescriptorSetLayout(
			StringSection<> inputData,
			const ::Assets::DirectorySearchRules& searchRules,
			const std::shared_ptr<::Assets::DependencyValidation>& depVal);
		PredefinedDescriptorSetLayout();
		~PredefinedDescriptorSetLayout();

		const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _depVal; }

	protected:
		static ConstantBuffer ParseCBLayout(ConditionalProcessingTokenizer& iterator);
		static Resource ParseResource(ConditionalProcessingTokenizer& iterator);
		static Sampler ParseSampler(ConditionalProcessingTokenizer& iterator);

		std::shared_ptr<::Assets::DependencyValidation> _depVal;
	};

}}

