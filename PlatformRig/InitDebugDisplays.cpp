// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DebuggingDisplays/BufferUploadDisplay.h"
#include "DebuggingDisplays/ConsoleDisplay.h"
#include "DebuggingDisplays/TestDisplays.h"

#include "../RenderCore/Assets/IModelFormat.h"
#include "../RenderCore/Assets/Services.h"

#include "../RenderOverlays/Overlays/Browser.h"
#include "../RenderOverlays/Overlays/OceanSettings.h"
#include "../RenderOverlays/Overlays/TestMaterialSettings.h"
#include "../RenderOverlays/Overlays/ToneMapSettings.h"
#include "../RenderOverlays/Overlays/VolFogSettings.h"

#include "../SceneEngine/Ocean.h"
#include "../SceneEngine/Tonemap.h"
#include "../SceneEngine/VolumetricFog.h"
#include "../SceneEngine/SceneEngineUtils.h"

#include "../ConsoleRig/Console.h"

namespace PlatformRig
{

    void InitDebugDisplays(RenderOverlays::DebuggingDisplay::DebugScreensSystem& debugSystem)
    {
        auto oceanSettingsDisplay           = std::make_shared<::Overlays::OceanSettingsDisplay>(std::ref(SceneEngine::GlobalOceanSettings));
        auto oceanLightingSettingsDisplay   = std::make_shared<::Overlays::OceanLightingSettingsDisplay>(std::ref(SceneEngine::GlobalOceanLightingSettings));
        // auto tonemapSettingsDisplay         = std::make_shared<::Overlays::ToneMapSettingsDisplay>(std::ref(SceneEngine::GlobalToneMapSettings));
        auto colorGradingSettingsDisplay    = std::make_shared<::Overlays::ColorGradingSettingsDisplay>(std::ref(SceneEngine::GlobalColorGradingSettings));
        auto testMaterialSettings           = std::make_shared<::Overlays::TestMaterialSettings>(std::ref(SceneEngine::GlobalMaterialOverride));
        auto modelBrowser                   = std::make_shared<::Overlays::ModelBrowser>("game\\model", nullptr);
        auto textureBrowser                 = std::make_shared<::Overlays::TextureBrowser>("game\\textures\\aa_terrain");
        auto gridIteratorDisplay            = std::make_shared<PlatformRig::Overlays::GridIteratorDisplay>();
        auto volFogDisplay                  = std::make_shared<::Overlays::VolumetricFogSettings>(std::ref(SceneEngine::GlobalVolumetricFogMaterial));
        auto dualContouringTest             = std::make_shared<PlatformRig::Overlays::DualContouringTest>();
        debugSystem.Register(modelBrowser, "[Browser] Model browser");
        debugSystem.Register(textureBrowser, "[Browser] Texture browser");

        debugSystem.Register(oceanSettingsDisplay, "[Settings] Ocean Settings");
        debugSystem.Register(oceanLightingSettingsDisplay, "[Settings] Ocean Lighting Settings");
        // debugSystem.Register(tonemapSettingsDisplay, "[Settings] Tone map settings");
        debugSystem.Register(colorGradingSettingsDisplay, "[Settings] Color grading settings");
        debugSystem.Register(testMaterialSettings, "[Settings] Material override settings");
        debugSystem.Register(volFogDisplay, "[Settings] Volumetric Fog Settings");

        debugSystem.Register(gridIteratorDisplay, "[Test] Grid iterator test");
        debugSystem.Register(dualContouringTest, "[Test] Dual Contouring Test");

        if (RenderCore::Assets::Services::HasInstance()) {
            auto bufferUploadDisplay = std::make_shared<PlatformRig::Overlays::BufferUploadDisplay>(&RenderCore::Assets::Services::GetBufferUploads());
            debugSystem.Register(bufferUploadDisplay, "[Profiler] Buffer Uploads Display");
        }
    }

}


