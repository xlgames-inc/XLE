// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Services.h"
#include "SimpleModelDeform.h"
#include "SkinDeformer.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../ConsoleRig/AttachablePtr.h"
#include <vector>
#include <regex>

namespace RenderCore { namespace Techniques
{
	class Services::Pimpl
	{
	public:
		struct TexturePlugin
		{
			std::regex _initializerMatcher;
			std::function<TextureLoaderSignature> _loader;
			unsigned _id;
		};
		std::vector<TexturePlugin> _texturePlugins;
		unsigned _nextTexturePluginId = 1;
	};

	unsigned Services::RegisterTextureLoader(
		const std::basic_regex<char, std::regex_traits<char>>& initializerMatcher, 
		std::function<TextureLoaderSignature>&& loader)
	{
		auto res = _pimpl->_nextTexturePluginId++;

		Pimpl::TexturePlugin plugin;
		plugin._initializerMatcher = initializerMatcher;
		plugin._loader = std::move(loader);
		plugin._id = res;
		_pimpl->_texturePlugins.push_back(std::move(plugin));
		return res;
	}

	void Services::DeregisterTextureLoader(unsigned pluginId)
	{
		auto i = std::find_if(_pimpl->_texturePlugins.begin(), _pimpl->_texturePlugins.end(), [pluginId](const auto& c) { return c._id == pluginId; });
		if (i != _pimpl->_texturePlugins.end())
			_pimpl->_texturePlugins.erase(i);
	}

	std::shared_ptr<BufferUploads::IAsyncDataSource> Services::CreateTextureDataSource(StringSection<> identifier, TextureLoaderFlags::BitField flags)
	{
		for (const auto& plugin:_pimpl->_texturePlugins)
			if (std::regex_match(identifier.begin(), identifier.end(), plugin._initializerMatcher))
				return plugin._loader(identifier, flags);
		return nullptr;
	}

	Services::Services(const std::shared_ptr<RenderCore::IDevice>& device)
	{
		_pimpl = std::make_unique<Pimpl>();
		_device = device;
		_deformOpsFactory = std::make_shared<DeformOperationFactory>();
		_deformOpsFactory->RegisterDeformOperation("skin", SkinDeformer::InstantiationFunction);

		if (device) {
            _bufferUploads = BufferUploads::CreateManager(*device);
        }
	}

	Services::~Services()
	{
		if (_bufferUploads) {
            _bufferUploads.reset();
        }
	}

	static ConsoleRig::AttachablePtr<Services> s_servicesInstance;
	bool Services::HasInstance() { return s_servicesInstance != nullptr; }
	Services& Services::GetInstance() { return *s_servicesInstance; }
}}

