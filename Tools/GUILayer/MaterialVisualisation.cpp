// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialVisualisation.h"
#include "ExportedNativeTypes.h"
#include "EditorInterfaceUtils.h"
#include "../ToolsRig/VisualisationUtils.h"
#include "../../RenderCore/Assets/MaterialScaffold.h"
#include "../../RenderCore/Assets/RawMaterial.h"
#include "../../RenderCore/Assets/AssetUtils.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/TechniqueMaterial.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../Assets/AssetUtils.h"
#include "../../Utility/StringFormat.h"

namespace GUILayer
{
    void MaterialVisLayer::RenderToScene(
        RenderCore::IThreadContext& context, 
        SceneEngine::LightingParserContext& parserContext)
    {
        if (!_visObject) { Resolve(); }
        if (!_visObject) { return; }

        ToolsRig::MaterialVisLayer::Draw(
            context, parserContext, _settings->GetUnderlying(), 
            *_envSettings.get(), *_visObject.get());
    }

    void MaterialVisLayer::RenderWidgets(
        RenderCore::IThreadContext& device, 
        RenderCore::Techniques::ParsingContext&)
    {}

    void MaterialVisLayer::SetActivationState(bool newState) {}

    void MaterialVisLayer::Resolve()
    {
        if (!_config) { _visObject.reset(); return; }

        clix::shared_ptr<ToolsRig::MaterialVisObject> visObject(
            std::make_shared<ToolsRig::MaterialVisObject>());

        auto& resMat = visObject->_parameters;
        auto& searchRules = visObject->_searchRules;
        
		auto previewModel = clix::marshalString<clix::E_UTF8>(_previewModel);
		searchRules.AddSearchDirectoryFromFilename(MakeStringSection(previewModel));

        if (_config)
            for each(auto c in _config)
				RenderCore::Assets::MergeIn_Stall(resMat, *c->GetUnderlying(), searchRules);

        const ::Assets::ResChar* shader = (resMat._techniqueConfig[0]) ? resMat._techniqueConfig : "illum";
        visObject->_materialBinder = std::make_shared<ToolsRig::MaterialBinder>(shader);
        visObject->_previewModelFile = clix::marshalString<clix::E_UTF8>(_previewModel);
        visObject->_previewMaterialBinding = _materialBinding;

        _visObject = visObject;
    }

    void MaterialVisLayer::ChangeHandler(System::Object^ sender, System::EventArgs^ args)
    {
        _visObject.reset();
    }

    void MaterialVisLayer::ListChangeHandler(System::Object^ sender, ListChangedEventArgs^ args)
    {
        _visObject.reset();
    }

    void MaterialVisLayer::PropChangeHandler(System::Object^ sender, PropertyChangedEventArgs^ args)
    {
        _visObject.reset();
    }

    void MaterialVisLayer::SetConfig(IEnumerable<RawMaterial^>^ config, System::String^ previewModel, uint64 materialBinding)
    {
        _visObject.reset();

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
        _envSettings->_activeSetting = settingsSet->GetSettings(name);
    }

    MaterialVisLayer::MaterialVisLayer(MaterialVisSettings^ settings)
    : _settings(settings)
    {
        _config = nullptr;
        _previewModel = "";
        _materialBinding = 0;
        _envSettings = std::make_shared<ToolsRig::VisEnvSettings>();
    }

    MaterialVisLayer::~MaterialVisLayer() 
    {
        SetConfig(nullptr, "", 0);
    }

    
    static MaterialVisSettings::GeometryType AsManaged(
        ToolsRig::MaterialVisSettings::GeometryType::Enum input)
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

    static ToolsRig::MaterialVisSettings::GeometryType::Enum AsNative(
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

    static MaterialVisSettings::LightingType AsManaged(
        ToolsRig::MaterialVisSettings::LightingType::Enum input)
    {
        switch (input) {
        case ToolsRig::MaterialVisSettings::LightingType::Deferred:
            return MaterialVisSettings::LightingType::Deferred;
        case ToolsRig::MaterialVisSettings::LightingType::Forward:
            return MaterialVisSettings::LightingType::Forward;
        default:
            return MaterialVisSettings::LightingType::NoLightingParser;
        }
    }

    static ToolsRig::MaterialVisSettings::LightingType::Enum AsNative(
         MaterialVisSettings::LightingType input)
    {
        switch (input) {
        case MaterialVisSettings::LightingType::Deferred:
            return ToolsRig::MaterialVisSettings::LightingType::Deferred;
        case MaterialVisSettings::LightingType::Forward:
            return ToolsRig::MaterialVisSettings::LightingType::Forward;
        default:
            return ToolsRig::MaterialVisSettings::LightingType::NoLightingParser;
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

    MaterialVisSettings::LightingType MaterialVisSettings::Lighting::get()
    {
        return AsManaged(_object->_lightingType);
    }

    void MaterialVisSettings::Lighting::set(LightingType value)
    {
        _object->_lightingType = AsNative(value);
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

