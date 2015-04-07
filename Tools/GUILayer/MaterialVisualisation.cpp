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
            Techniques::TechniqueInterface techniqueInterface(Metal::GlobalInputLayouts::PNTT);

            static const auto DefaultNormalsTextureBindingHash = Hash64("NormalsTexture");
            ParameterBox materialParameters = obj._parameters._matParams;
            for (auto i=obj._parameters._bindings.cbegin(); i!=obj._parameters._bindings.cend(); ++i) {
                materialParameters.SetParameter(StringMeld<64>() << "RES_HAS_" << std::hex << i->_bindHash, 1);
                if (i->_bindHash == DefaultNormalsTextureBindingHash) {
                    ResChar resolvedName[MaxPath];
                    obj._searchRules.ResolveFile(resolvedName, dimof(resolvedName), i->_resourceName.c_str());
                    materialParameters.SetParameter("RES_HAS_NormalsTexture_DXT", 
                        RenderCore::Assets::IsDXTNormalMap(resolvedName));
                }
            }

            ParameterBox geoParameters;
            geoParameters.SetParameter("GEO_HAS_NORMAL", 1);
            geoParameters.SetParameter("GEO_HAS_TEXCOORD", 1);
            geoParameters.SetParameter("GEO_HAS_TANGENT_FRAME", 1);
            const ParameterBox* state[] = {
                &geoParameters, &parserContext.GetTechniqueContext()._globalEnvironmentState,
                &parserContext.GetTechniqueContext()._runtimeState, &materialParameters
            };

            const unsigned techniqueIndex = 
                (_settings->Lighting == MaterialVisSettings::LightingType::Deferred) ? 2 : 0;

            auto& shaderType = ::Assets::GetAssetDep<Techniques::ShaderType>("game/xleres/illum.txt");
            auto variation = shaderType.FindVariation(techniqueIndex, state, techniqueInterface);
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
        _config = config;
    }

    MaterialVisLayer::MaterialVisLayer(
        MaterialVisSettings^ settings,
        RawMaterial^ config)
    : _settings(settings), _config(config)
    {}

    MaterialVisLayer::~MaterialVisLayer() {}

    
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

