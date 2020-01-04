// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <string>
#include <vector>

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
		};

		std::vector<ConstantBuffer> _constantBuffers;
		std::vector<std::string> _srvs;
		std::vector<std::string> _samplers;
	};

}}

