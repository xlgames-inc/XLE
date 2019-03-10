// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#if 0

#include "UITypesBinding.h"
#include "IOverlaySystem.h"
#include "EngineForward.h"
#include "CLIXAutoPtr.h"
#include "../ToolsRig/MaterialVisualisation.h"
#include <memory>

using namespace System::Collections::Generic;

namespace ToolsRig { class VisCameraSettings; }

namespace GUILayer
{
    

    ref class EnvironmentSettingsSet;
	ref class VisCameraSettings;

    public ref class MaterialVisLayer : public IOverlaySystem
    {
    public:
        virtual void Render(
            RenderCore::IThreadContext& context,
			const RenderTargetWrapper& renderTarget,
            RenderCore::Techniques::ParsingContext& parserContext) override;

        void SetConfig(
            IEnumerable<RawMaterial^>^ config, 
            System::String^ previewModel, uint64 materialBinding);

        void SetEnvironment(
            EnvironmentSettingsSet^ settingsSet,
            System::String^ name);

		VisCameraSettings^ GetCamera();

        MaterialVisLayer(MaterialVisSettings^ settings);
        ~MaterialVisLayer();
        
    protected:
        clix::shared_ptr<ToolsRig::VisEnvSettings> _envSettings;
        IEnumerable<RawMaterial^>^ _config;
        System::String^ _previewModel;
        uint64 _materialBinding;
        MaterialVisSettings^ _settings;
		bool _nativeVisSettingsDirty;

		VisCameraSettings^ _cameraSettings;

        void ChangeHandler(System::Object^ sender, System::EventArgs^ args);
        void ListChangeHandler(System::Object^ sender, ListChangedEventArgs^ args);
        void PropChangeHandler(System::Object^ sender, PropertyChangedEventArgs^ args);
    };
}

#endif
