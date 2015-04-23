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
        if (!_config->GetUnderlying()) { return; }

        ToolsRig::MaterialVisObject obj;
        obj._searchRules = ::Assets::DefaultDirectorySearchRules(_config->GetUnderlying()->GetInitializerFilename().c_str());
        _config->GetUnderlying()->Resolve(obj._parameters, obj._searchRules);
        // obj._systemConstants = 

            // We must build a shader program to render this preview
            // We can assume input vertices have position, normal and texture coordinates
            // Let's use a technique so we can generate shaders that work with
            // different lighting models

        {
            using namespace RenderCore;
            using namespace RenderCore::Techniques;

            static const auto DefaultNormalsTextureBindingHash = ParameterBox::MakeParameterNameHash("NormalsTexture");

            ParameterBox materialParameters = obj._parameters._matParams;
            const auto& resBindings = obj._parameters._bindings;
            for (unsigned c=0; c<resBindings.GetParameterCount(); ++c) {
                materialParameters.SetParameter(StringMeld<64>() << "RES_HAS_" << resBindings.GetFullNameAtIndex(c), 1);
                if (resBindings.GetParameterAtIndex(c) == DefaultNormalsTextureBindingHash) {
                    auto resourceName = resBindings.GetString<::Assets::ResChar>(DefaultNormalsTextureBindingHash);
                    ResChar resolvedName[MaxPath];
                    obj._searchRules.ResolveFile(resolvedName, dimof(resolvedName), resourceName.c_str());
                    materialParameters.SetParameter("RES_HAS_NormalsTexture_DXT", 
                        RenderCore::Assets::IsDXTNormalMap(resolvedName));
                }
            }

            TechniqueMaterial material(
                Metal::GlobalInputLayouts::PNTT,
                {}, materialParameters);

            const unsigned techniqueIndex = 
                (_settings->Lighting == MaterialVisSettings::LightingType::Deferred) ? 2 : 0;

            auto variation = material.FindVariation(parserContext, techniqueIndex, "game/xleres/illum.txt");
            if (variation._shaderProgram == nullptr) {
                return; // we can't render because we couldn't resolve a good shader variation
            }

            obj._shaderProgram = variation._shaderProgram;
        }

        ToolsRig::MaterialVisLayer::Draw(
            *context, parserContext,
            _settings->GetUnderlying(), obj);
    }

    void MaterialVisLayer::RenderWidgets(
        RenderCore::IThreadContext* device, 
        const RenderCore::Techniques::ProjectionDesc& projectionDesc)
    {
    }

    void MaterialVisLayer::SetActivationState(bool newState)
    {
    }

    // auto MaterialVisLayer::GetInputListener() -> std::shared_ptr<IInputListener>
    // {
    //     return nullptr;
    // }

    void MaterialVisLayer::SetConfig(RawMaterial^ config)
    {
        delete _config;
        _config = config ? (gcnew RawMaterial(config)) : nullptr;
    }

    MaterialVisLayer::MaterialVisLayer(
        MaterialVisSettings^ settings,
        RawMaterial^ config)
    : _settings(settings), _config(gcnew RawMaterial(config))
    {}

    MaterialVisLayer::~MaterialVisLayer() 
    {
        delete _config;
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

