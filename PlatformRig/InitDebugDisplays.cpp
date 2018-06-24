// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DebuggingDisplays/BufferUploadDisplay.h"
#include "DebuggingDisplays/ConsoleDisplay.h"
#include "DebuggingDisplays/TestDisplays.h"

#include "../RenderCore/Assets/Services.h"

#include "../RenderOverlays/Overlays/Browser.h"
#include "../RenderOverlays/Overlays/OceanSettings.h"
#include "../RenderOverlays/Overlays/TestMaterialSettings.h"
#include "../RenderOverlays/Overlays/ToneMapSettings.h"
#include "../RenderOverlays/Overlays/VolFogSettings.h"

#if defined(XLE_HAS_SCENE_ENGINE)		// getting link-time errors when linking into executables that don't use the scene engine
#include "../SceneEngine/Ocean.h"
#include "../SceneEngine/DeepOceanSim.h"
#include "../SceneEngine/Tonemap.h"
#include "../SceneEngine/VolumetricFog.h"
// #include "../SceneEngine/SceneEngineUtils.h"
#endif

#include "../ConsoleRig/Console.h"

#if defined(XLE_HAS_SCENE_ENGINE)
namespace SceneEngine
{
    extern DeepOceanSimSettings GlobalOceanSettings;
    extern OceanLightingSettings GlobalOceanLightingSettings;
}
#endif

namespace PlatformRig
{
    #if defined(XLE_HAS_SCENE_ENGINE)
		static auto GlobalVolumetricFogMaterial = SceneEngine::VolumetricFogMaterial_Default();
	#endif

    void InitDebugDisplays(RenderOverlays::DebuggingDisplay::DebugScreensSystem& debugSystem)
    {
		#if defined(XLE_HAS_SCENE_ENGINE)
			auto oceanSettingsDisplay           = std::make_shared<::Overlays::OceanSettingsDisplay>(std::ref(SceneEngine::GlobalOceanSettings));
			auto oceanLightingSettingsDisplay   = std::make_shared<::Overlays::OceanLightingSettingsDisplay>(std::ref(SceneEngine::GlobalOceanLightingSettings));
			// auto tonemapSettingsDisplay         = std::make_shared<::Overlays::ToneMapSettingsDisplay>(std::ref(SceneEngine::GlobalToneMapSettings));
			// auto colorGradingSettingsDisplay    = std::make_shared<::Overlays::ColorGradingSettingsDisplay>(std::ref(SceneEngine::GlobalColorGradingSettings));
			// auto testMaterialSettings           = std::make_shared<::Overlays::TestMaterialSettings>(std::ref(SceneEngine::GlobalMaterialOverride));
			auto volFogDisplay                  = std::make_shared<::Overlays::VolumetricFogSettings>(std::ref(GlobalVolumetricFogMaterial));
		#endif
        auto modelBrowser                   = std::make_shared<::Overlays::ModelBrowser>("game\\model");
        auto textureBrowser                 = std::make_shared<::Overlays::TextureBrowser>("game\\textures\\aa_terrain");
        auto gridIteratorDisplay            = std::make_shared<PlatformRig::Overlays::GridIteratorDisplay>();
        auto dualContouringTest             = std::make_shared<PlatformRig::Overlays::DualContouringTest>();
        auto constRasterTest                = std::make_shared<PlatformRig::Overlays::ConservativeRasterTest>();
        auto rectPackingTest                = std::make_shared<PlatformRig::Overlays::RectanglePackerTest>();
        debugSystem.Register(modelBrowser, "[Browser] Model browser");
        debugSystem.Register(textureBrowser, "[Browser] Texture browser");

        #if defined(XLE_HAS_SCENE_ENGINE)
			debugSystem.Register(oceanSettingsDisplay, "[Settings] Ocean Settings");
			debugSystem.Register(oceanLightingSettingsDisplay, "[Settings] Ocean Lighting Settings");
			// debugSystem.Register(tonemapSettingsDisplay, "[Settings] Tone map settings");
			// debugSystem.Register(colorGradingSettingsDisplay, "[Settings] Color grading settings");
			// debugSystem.Register(testMaterialSettings, "[Settings] Material override settings");
			debugSystem.Register(volFogDisplay, "[Settings] Volumetric Fog Settings");
		#endif

        debugSystem.Register(gridIteratorDisplay, "[Test] Grid iterator test");
        debugSystem.Register(dualContouringTest, "[Test] Dual Contouring Test");
        debugSystem.Register(constRasterTest, "[Test] Conservative Raster");
        debugSystem.Register(rectPackingTest, "[Test] Rectangle Packing");

        if (RenderCore::Assets::Services::HasInstance()) {
            auto bufferUploadDisplay = std::make_shared<PlatformRig::Overlays::BufferUploadDisplay>(&RenderCore::Assets::Services::GetBufferUploads());
            debugSystem.Register(bufferUploadDisplay, "[Profiler] Buffer Uploads Display");
        }
    }

}


