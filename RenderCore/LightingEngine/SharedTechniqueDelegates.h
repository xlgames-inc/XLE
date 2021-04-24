// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Techniques/TechniqueDelegates.h"
#include "../../Assets/AssetsCore.h"
#include "../../Assets/InitializerPack.h"
#include <memory>
#include <map>

namespace RenderCore { namespace Techniques { 
	class TechniqueSetFile;
	class TechniqueSharedResources;
	class ITechniqueDelegate;
	class DrawingApparatus;
}}
namespace RenderCore { class IDevice; }
namespace Assets { class InitializerPack; }

namespace RenderCore { namespace LightingEngine
{
	class SharedTechniqueDelegateBox
	{
	public:
		::Assets::FuturePtr<Techniques::TechniqueSetFile> _techniqueSetFile;
		std::shared_ptr<Techniques::ITechniqueDelegate> _forwardIllumDelegate_DisableDepthWrite;
		std::shared_ptr<Techniques::ITechniqueDelegate> _depthOnlyDelegate;
		std::shared_ptr<Techniques::ITechniqueDelegate> _deferredIllumDelegate;
		std::shared_ptr<Techniques::TechniqueSharedResources> _sharedResources;

		template<typename... Args>
			std::shared_ptr<Techniques::ITechniqueDelegate> GetShadowGenTechniqueDelegate(Args... args);

		SharedTechniqueDelegateBox(Techniques::DrawingApparatus& drawingApparatus);
		SharedTechniqueDelegateBox(const std::shared_ptr<Techniques::TechniqueSharedResources>& sharedResources);
	private:
		std::map<uint64_t, std::shared_ptr<Techniques::ITechniqueDelegate>> _shadowGenTechniqueDelegates;
	};

	class LightingEngineApparatus
	{
	public:
		std::shared_ptr<SharedTechniqueDelegateBox> _sharedDelegates;
		std::shared_ptr<IDevice> _device;

		LightingEngineApparatus(std::shared_ptr<Techniques::DrawingApparatus>);
		~LightingEngineApparatus();
		LightingEngineApparatus(LightingEngineApparatus&) = delete;
		LightingEngineApparatus& operator=(LightingEngineApparatus&) = delete;
	};

	template<typename... Args>
		std::shared_ptr<Techniques::ITechniqueDelegate> SharedTechniqueDelegateBox::GetShadowGenTechniqueDelegate(Args... args)
	{
		::Assets::InitializerPack pack{args...};
		auto hash = pack.ArchivableHash();
		auto i = _shadowGenTechniqueDelegates.find(hash);
		if (i != _shadowGenTechniqueDelegates.end())
			return i->second;

		auto delegate = Techniques::CreateTechniqueDelegate_ShadowGen(_techniqueSetFile, _sharedResources, args...);
		_shadowGenTechniqueDelegates.insert(std::make_pair(hash, delegate));
		return delegate;
	}

}}

namespace RenderCore
{
	uint64_t Hash64(CullMode, uint64_t=DefaultSeed64);
	namespace Techniques 
	{ 
		uint64_t Hash64(RSDepthBias, uint64_t=DefaultSeed64); 
	}
}
