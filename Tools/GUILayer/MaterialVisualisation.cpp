// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialVisualisation.h"
#include "ExportedNativeTypes.h"
#include "EditorInterfaceUtils.h"
#include "../ToolsRig/VisualisationUtils.h"
#include "../../PlatformRig/BasicSceneParser.h"
#include "../../RenderCore/Assets/MaterialScaffold.h"
#include "../../RenderCore/Assets/RawMaterial.h"
#include "../../RenderCore/Assets/AssetUtils.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/TechniqueMaterial.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/AssetTraits.h"
#include "../../Utility/StringFormat.h"

namespace GUILayer
{
	static ToolsRig::DrawPreviewLightingType AsNative(
         MaterialVisSettings::LightingType input)
    {
        switch (input) {
        case MaterialVisSettings::LightingType::Deferred:
            return ToolsRig::DrawPreviewLightingType::Deferred;
        case MaterialVisSettings::LightingType::Forward:
            return ToolsRig::DrawPreviewLightingType::Forward;
        default:
            return ToolsRig::DrawPreviewLightingType::Direct;
        }
    }

    void MaterialVisLayer::Render(
        RenderCore::IThreadContext& context,
		const RenderTargetWrapper& renderTarget,
        RenderCore::Techniques::ParsingContext& parserContext)
    {
		if (_nativeVisSettingsDirty) {
			auto& visObject = *_settings->GetUnderlyingPtr();
			visObject._parameters = RenderCore::Techniques::Material{};
			visObject._searchRules = ::Assets::DirectorySearchRules{};
        
			auto previewModel = clix::marshalString<clix::E_UTF8>(_previewModel);
			visObject._searchRules.AddSearchDirectoryFromFilename(MakeStringSection(previewModel));

			if (_config)
				for each(auto c in _config)
					RenderCore::Assets::MergeIn_Stall(visObject._parameters, *c->GetUnderlying(), visObject._searchRules);

			visObject._previewModelFile = clix::marshalString<clix::E_UTF8>(_previewModel);
			visObject._previewMaterialBinding = _materialBinding;
		}

		auto scene = ToolsRig::CreateScene(_settings->GetUnderlyingPtr());
		auto future = ::Assets::AutoConstructAsset<PlatformRig::EnvironmentSettings>(_envSettings->_envConfigFile);
		PlatformRig::BasicLightingParserDelegate lightingParserDelegate(std::move(future));

        ToolsRig::MaterialVisLayer::Draw(
			context, renderTarget._renderTarget, parserContext, 
			AsNative(_settings->Lighting), 
			*scene, lightingParserDelegate,
			AsCameraDesc(*_cameraSettings->GetUnderlying()));
    }

    void MaterialVisLayer::ChangeHandler(System::Object^ sender, System::EventArgs^ args)
    {
        _nativeVisSettingsDirty = true;
    }

    void MaterialVisLayer::ListChangeHandler(System::Object^ sender, ListChangedEventArgs^ args)
    {
        _nativeVisSettingsDirty = true;
    }

    void MaterialVisLayer::PropChangeHandler(System::Object^ sender, PropertyChangedEventArgs^ args)
    {
        _nativeVisSettingsDirty = true;
    }

    void MaterialVisLayer::SetConfig(IEnumerable<RawMaterial^>^ config, System::String^ previewModel, uint64 materialBinding)
    {
        _nativeVisSettingsDirty = true;

        auto listChangeHandler = gcnew ListChangedEventHandler(this, &MaterialVisLayer::ListChangeHandler);
        auto propChangeHandler = gcnew PropertyChangedEventHandler(this, &MaterialVisLayer::PropChangeHandler);

        if (_config) {
            for each(auto mat in _config) {
                mat->MaterialParameterBox->ListChanged -= listChangeHandler;
                mat->ShaderConstants->ListChanged -= listChangeHandler;
                mat->ResourceBindings->ListChanged -= listChangeHandler;
                mat->StateSet->PropertyChanged -= propChangeHandler;
            }
        }
        _config = config;
        _previewModel = previewModel;
        _materialBinding = materialBinding;
        if (_config) {
            for each(auto mat in _config) {
                mat->MaterialParameterBox->ListChanged += listChangeHandler;
                mat->ShaderConstants->ListChanged += listChangeHandler;
                mat->ResourceBindings->ListChanged += listChangeHandler;
                mat->StateSet->PropertyChanged += propChangeHandler;
            }
        }
    }

    void MaterialVisLayer::SetEnvironment(
        EnvironmentSettingsSet^ settingsSet,
        System::String^ name)
    {
        assert(0);
			// = settingsSet->GetSettings(name);
    }

	VisCameraSettings^ MaterialVisLayer::GetCamera()
	{
		return _cameraSettings;
	}

    MaterialVisLayer::MaterialVisLayer(MaterialVisSettings^ settings)
    : _settings(settings)
    {
		_nativeVisSettingsDirty = true;
        _config = nullptr;
        _previewModel = "";
        _materialBinding = 0;
        _envSettings = std::make_shared<ToolsRig::VisEnvSettings>();
		_cameraSettings = gcnew VisCameraSettings;
    }

    MaterialVisLayer::~MaterialVisLayer() 
    {
		delete _cameraSettings;
		_cameraSettings = nullptr;
        SetConfig(nullptr, "", 0);
    }

    
    static MaterialVisSettings::GeometryType AsManaged(
        ToolsRig::MaterialVisSettings::GeometryType input)
    {
        switch (input) {
        case ToolsRig::MaterialVisSettings::GeometryType::Sphere:
            return MaterialVisSettings::GeometryType::Sphere;
        case ToolsRig::MaterialVisSettings::GeometryType::Cube:
            return MaterialVisSettings::GeometryType::Cube;
        case ToolsRig::MaterialVisSettings::GeometryType::Model:
            return MaterialVisSettings::GeometryType::Model;
        default:
            return MaterialVisSettings::GeometryType::Plane2D;
        }
    }

    static ToolsRig::MaterialVisSettings::GeometryType AsNative(
         MaterialVisSettings::GeometryType input)
    {
        switch (input) {
        case MaterialVisSettings::GeometryType::Sphere:
            return ToolsRig::MaterialVisSettings::GeometryType::Sphere;
        case MaterialVisSettings::GeometryType::Cube:
            return ToolsRig::MaterialVisSettings::GeometryType::Cube;
        case MaterialVisSettings::GeometryType::Model:
            return ToolsRig::MaterialVisSettings::GeometryType::Model;
        default:
            return ToolsRig::MaterialVisSettings::GeometryType::Plane2D;
        }
    }

    MaterialVisSettings::GeometryType MaterialVisSettings::Geometry::get()
    {
        return AsManaged(_object->_geometryType);
    }

    void MaterialVisSettings::Geometry::set(GeometryType value)
    {
        _object->_geometryType = AsNative(value);
    }

    void MaterialVisSettings::ResetCamera::set(bool value)
    {
        _object->_pendingCameraAlignToModel = value;
    }

    MaterialVisSettings^ MaterialVisSettings::CreateDefault()
    {
        return gcnew MaterialVisSettings(std::make_shared<ToolsRig::MaterialVisSettings>());
    }

}

