// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialVisualisation.h"
#include "ExportedNativeTypes.h"
#include "../ToolsRig/VisualisationUtils.h"
#include "../../RenderCore/Assets/Material.h"
#include "../../RenderCore/Assets/AssetUtils.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/TechniqueMaterial.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../Assets/AssetUtils.h"
#include "../../Utility/StringFormat.h"

namespace GUILayer
{
    void MaterialVisLayer::RenderToScene(
        RenderCore::IThreadContext* context, 
        SceneEngine::LightingParserContext& parserContext)
    {
        if (!_visObject) { Resolve(); }
        if (!_visObject) { return; }

        ToolsRig::MaterialVisLayer::Draw(
            *context, parserContext,
            _settings->GetUnderlying(), *_visObject.get());
    }

    void MaterialVisLayer::RenderWidgets(
        RenderCore::IThreadContext* device, 
        const RenderCore::Techniques::ProjectionDesc& projectionDesc)
    {}

    void MaterialVisLayer::SetActivationState(bool newState) {}

    void MaterialVisLayer::Resolve()
    {
        if (!_config) { _visObject.reset(); return; }

        clix::shared_ptr<ToolsRig::MaterialVisObject> visObject(
            std::make_shared<ToolsRig::MaterialVisObject>());

        auto& resMat = visObject->_parameters;
        auto& searchRules = visObject->_searchRules;
        
        if (_config) {
            for each(auto c in _config)
                searchRules.AddSearchDirectoryFromFilename(c->GetUnderlying()->GetInitializerFilename().c_str());

            for each(auto c in _config)
                c->GetUnderlying()->Resolve(resMat, searchRules);
        }

        visObject->_materialBinder = std::make_shared<ToolsRig::MaterialBinder>(
            "game/xleres/illum.txt");

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

    void MaterialVisLayer::SetConfig(IEnumerable<RawMaterial^>^ config)
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
        if (_config) {
            for each(auto mat in _config) {
                mat->MaterialParameterBox->ListChanged += listChangeHandler;
                mat->ShaderConstants->ListChanged += listChangeHandler;
                mat->ResourceBindings->ListChanged += listChangeHandler;
                mat->StateSet->PropertyChanged += propChangeHandler;
            }
        }
    }

    MaterialVisLayer::MaterialVisLayer(
        MaterialVisSettings^ settings,
        IEnumerable<RawMaterial^>^ config)
    : _settings(settings)
    {
        SetConfig(config);
    }

    MaterialVisLayer::~MaterialVisLayer() 
    {
        SetConfig(nullptr);
    }

    
    static MaterialVisSettings::GeometryType AsManaged(
        ToolsRig::MaterialVisSettings::GeometryType::Enum input)
    {
        switch (input) {
        case ToolsRig::MaterialVisSettings::GeometryType::Sphere:
            return MaterialVisSettings::GeometryType::Sphere;
        case ToolsRig::MaterialVisSettings::GeometryType::Cube:
            return MaterialVisSettings::GeometryType::Cube;
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

    MaterialVisSettings^ MaterialVisSettings::CreateDefault()
    {
        return gcnew MaterialVisSettings(std::make_shared<ToolsRig::MaterialVisSettings>());
    }

}

