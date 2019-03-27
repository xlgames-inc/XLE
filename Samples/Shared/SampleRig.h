// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../PlatformRig/OverlaySystem.h"
#include <memory>

namespace RenderCore
{
	class IDevice;
	class IPresentationChain;
	namespace Techniques { class TechniqueContext; }
}

namespace RenderOverlays { namespace DebuggingDisplay { class DebugScreensSystem; }}
namespace PlatformRig { class MainInputHandler; }

namespace Sample
{
	class SampleGlobals
	{
	public:
		std::shared_ptr<RenderCore::IDevice>				_renderDevice;
		std::shared_ptr<RenderCore::IPresentationChain>		_presentationChain;
		
		std::shared_ptr<RenderCore::Techniques::TechniqueContext>	_techniqueContext;

		std::shared_ptr<RenderOverlays::DebuggingDisplay::DebugScreensSystem>	_debugScreens;
		std::shared_ptr<PlatformRig::MainInputHandler>							_mainInputHander;
	};

	class ISampleOverlay : public PlatformRig::IOverlaySystem
	{
	public:
		virtual void OnUpdate(float deltaTime);
		virtual void OnStartup(const SampleGlobals& globals);
	};

	void ExecuteSample(std::shared_ptr<ISampleOverlay>&& sampleOverlay);
}
