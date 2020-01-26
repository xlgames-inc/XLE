// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PreviewSceneRegistry.h"

namespace ToolsRig
{

	class MainPreviewSceneRegistry : public IPreviewSceneRegistry
	{
	public:
		std::vector<std::string> EnumerateScenes()
		{
			std::vector<std::string> result;
			for (auto i=_registrySet.begin(); i!=_registrySet.end(); ++i) {
				auto setScenes = i->second->EnumerateScenes();
				result.insert(result.end(), setScenes.begin(), setScenes.end());
			}
			return result;
		}

		std::shared_ptr<SceneEngine::IScene> CreateScene(StringSection<> sceneName, const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAccelerators)
		{
			for (auto i=_registrySet.begin(); i!=_registrySet.end(); ++i) {
				auto s = i->second->CreateScene(sceneName, pipelineAccelerators);
				if (s)
					return s;
			}
			return nullptr;
		}

		RegistrySetId Register(const std::shared_ptr<IPreviewSceneRegistrySet>& registrySet)
		{
			auto result = _nextRegistrySetId+1;
			_registrySet.push_back(std::make_pair(result, registrySet));
			return result;
		}

		void Deregister(RegistrySetId setId)
		{
			for (auto i=_registrySet.begin(); i!=_registrySet.end(); ++i)
				if (i->first == setId) {
					_registrySet.erase(i);
					break;
				}
		}

		MainPreviewSceneRegistry::MainPreviewSceneRegistry() {}
		MainPreviewSceneRegistry::~MainPreviewSceneRegistry() {}

		std::vector<std::pair<RegistrySetId, std::shared_ptr<IPreviewSceneRegistrySet>>> _registrySet;
		RegistrySetId _nextRegistrySetId = 1;
	};

	ConsoleRig::AttachablePtr<IPreviewSceneRegistry> GetPreviewSceneRegistry()
	{
		auto ptr = ::ConsoleRig::GetAttachablePtr<MainPreviewSceneRegistry>();
		if (!ptr)
			ptr = ::ConsoleRig::MakeAttachablePtr<MainPreviewSceneRegistry>();
		return std::move(*reinterpret_cast<ConsoleRig::AttachablePtr<IPreviewSceneRegistry>*>(&ptr));
	}

	IPreviewSceneRegistrySet::~IPreviewSceneRegistrySet() {}
	IPreviewSceneRegistry::~IPreviewSceneRegistry() {}

}

