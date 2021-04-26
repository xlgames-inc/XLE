// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LightingEngineApparatus.h"
#include "../Techniques/TechniqueDelegates.h"
#include "../Techniques/Apparatuses.h"
#include "../Techniques/Techniques.h"
#include "../../Assets/AssetTraits.h"
#include "../../Assets/Assets.h"
#include "../../xleres/FileList.h"

namespace RenderCore { namespace LightingEngine
{
	SharedTechniqueDelegateBox::SharedTechniqueDelegateBox(const std::shared_ptr<Techniques::TechniqueSharedResources>& sharedResources)
	: _sharedResources(sharedResources)
	{
		_techniqueSetFile = ::Assets::MakeAsset<RenderCore::Techniques::TechniqueSetFile>(ILLUM_TECH);
		_forwardIllumDelegate_DisableDepthWrite = RenderCore::Techniques::CreateTechniqueDelegate_Forward(_techniqueSetFile, _sharedResources, RenderCore::Techniques::TechniqueDelegateForwardFlags::DisableDepthWrite);
		_depthOnlyDelegate = RenderCore::Techniques::CreateTechniqueDelegate_Forward(_techniqueSetFile, _sharedResources);
	}

	SharedTechniqueDelegateBox::SharedTechniqueDelegateBox(Techniques::DrawingApparatus& drawingApparatus)
	: SharedTechniqueDelegateBox(drawingApparatus._techniqueSharedResources)
	{}

	LightingEngineApparatus::LightingEngineApparatus(std::shared_ptr<Techniques::DrawingApparatus> drawingApparatus)
	{
		_device = drawingApparatus->_device;
		_sharedDelegates = std::make_shared<SharedTechniqueDelegateBox>(*drawingApparatus);
	}

	LightingEngineApparatus::~LightingEngineApparatus() {}
}}

namespace RenderCore
{
	uint64_t Hash64(CullMode cullMode, uint64_t seed)
	{
		return HashCombine((uint64_t)cullMode, seed);
	}

	namespace Techniques 
	{ 
		uint64_t Hash64(RSDepthBias depthBias, uint64_t seed)
		{
			unsigned t0 = *(unsigned*)&depthBias._depthBias;
			unsigned t1 = *(unsigned*)&depthBias._depthBiasClamp;
			unsigned t2 = *(unsigned*)&depthBias._slopeScaledBias;
			return HashCombine(((uint64_t(t0) << 32ull) | uint64_t(t1)) ^ (uint64_t(t2) << 16ull), seed);
		} 
	}
}

