// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Services.h"
#include "SimpleModelDeform.h"
#include "SkinDeformer.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../BufferUploads/DataPacket.h"
#include "../../ConsoleRig/AttachablePtr.h"		// (for ConsoleRig::CrossModule::GetInstance)

namespace RenderCore { namespace Techniques
{
	Services* Services::s_instance = nullptr;

	class Services::Pimpl
	{
	public:
		std::vector<BufferUploads::TexturePlugin> _texturePlugins;
		std::vector<unsigned> _texturePluginIds;
		unsigned _nextTexturePluginId = 1;
	};

	IteratorRange<const BufferUploads::TexturePlugin*> Services::GetTexturePlugins()
	{
		return MakeIteratorRange(_pimpl->_texturePlugins);
	}

	unsigned Services::RegisterTexturePlugin(BufferUploads::TexturePlugin&& plugin)
	{
		auto res = _pimpl->_nextTexturePluginId;
		++_pimpl->_nextTexturePluginId;

		_pimpl->_texturePlugins.emplace_back(std::move(plugin));
		_pimpl->_texturePluginIds.push_back(res);
		return res;
	}

	void Services::DeregisterTexturePlugin(unsigned pluginId)
	{
		auto i = std::find(_pimpl->_texturePluginIds.begin(), _pimpl->_texturePluginIds.end(), pluginId);
		if (i != _pimpl->_texturePluginIds.end()) {
			_pimpl->_texturePlugins.erase(_pimpl->_texturePlugins.begin() + (i-_pimpl->_texturePluginIds.begin()));
			_pimpl->_texturePluginIds.erase(i);
		}
	}

	Services::Services(const std::shared_ptr<RenderCore::IDevice>& device)
	{
		_pimpl = std::make_unique<Pimpl>();
		_device = device;
		_deformOpsFactory = std::make_shared<DeformOperationFactory>();
		_deformOpsFactory->RegisterDeformOperation("skin", SkinDeformer::InstantiationFunction);

		if (device) {
            BufferUploads::AttachLibrary(ConsoleRig::CrossModule::GetInstance());
            _bufferUploads = BufferUploads::CreateManager(*device);
        }
	}

	Services::~Services()
	{
		if (_bufferUploads) {
            _bufferUploads.reset();
            BufferUploads::DetachLibrary();
        }
	}

    void Services::AttachCurrentModule()
	{
		assert(s_instance==nullptr);
        s_instance = this;
	}

    void Services::DetachCurrentModule()
	{
		assert(s_instance==this);
        s_instance = nullptr;
	}
}}

