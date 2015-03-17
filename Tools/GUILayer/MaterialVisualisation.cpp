// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialVisualisation.h"
#include "../../RenderCore/Assets/Material.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../Assets/AssetUtils.h"

namespace GUILayer
{
    void MaterialVisLayer::RenderToScene(
        RenderCore::IThreadContext* context, 
        SceneEngine::LightingParserContext& parserContext)
    {
        PlatformRig::MaterialVisObject obj;
        obj._parameters = _config->GetUnderlying().Resolve(
            ::Assets::DefaultDirectorySearchRules(_config->GetUnderlying()._filename.c_str()));
        // obj._systemConstants = 

            // We must build a shader program to render this preview
            // We can assume input vertices have position, normal and texture coordinates
            // Let's use a technique so we can generate shaders that work with
            // different lighting models

        {
            using namespace RenderCore;
            Techniques::TechniqueInterface techniqueInterface(Metal::GlobalInputLayouts::PNT);
            Techniques::TechniqueContext::BindGlobalUniforms(techniqueInterface);
            techniqueInterface.BindConstantBuffer(Hash64("LocalTransform"), 0, 1);

            ParameterBox materialParameters;
            ParameterBox geoParameters;
            geoParameters.SetParameter("GEO_HAS_NORMAL", 1);
            geoParameters.SetParameter("GEO_HAS_TEXCOORD", 1);
            const ParameterBox* state[] = {
                &geoParameters, &parserContext.GetTechniqueContext()._globalEnvironmentState,
                &parserContext.GetTechniqueContext()._runtimeState, &obj._parameters._matParams
            };

            const unsigned techniqueIndex = 0;

            auto& shaderType = ::Assets::GetAssetDep<Techniques::ShaderType>("game/xleres/illum.txt");
            auto variation = shaderType.FindVariation(techniqueIndex, state, techniqueInterface);
            if (variation._shaderProgram == nullptr) {
                return; // we can't render because we couldn't resolve a good shader variation
            }

            obj._shaderProgram = variation._shaderProgram;
        }

        PlatformRig::MaterialVisLayer::Draw(
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

    auto MaterialVisLayer::GetInputListener() -> std::shared_ptr<IInputListener>
    {
        return nullptr;
    }

    void MaterialVisLayer::SetConfig(RawMaterial^ config)
    {
        _config = config;
    }

    MaterialVisLayer::MaterialVisLayer(
        MaterialVisSettings^ settings,
        RawMaterial^ config)
    : _settings(settings), _config(config)
    {}

    MaterialVisLayer::~MaterialVisLayer()
    {
        delete _config;
        delete _settings;
    }

    
    static MaterialVisSettings::GeometryType AsManaged(
        PlatformRig::MaterialVisSettings::GeometryType::Enum input)
    {
        switch (input) {
        case PlatformRig::MaterialVisSettings::GeometryType::Sphere:
            return MaterialVisSettings::GeometryType::Sphere;
        case PlatformRig::MaterialVisSettings::GeometryType::Cube:
            return MaterialVisSettings::GeometryType::Cube;
        default:
            return MaterialVisSettings::GeometryType::Plane2D;
        }
    }

    static PlatformRig::MaterialVisSettings::GeometryType::Enum AsNative(
         MaterialVisSettings::GeometryType input)
    {
        switch (input) {
        case MaterialVisSettings::GeometryType::Sphere:
            return PlatformRig::MaterialVisSettings::GeometryType::Sphere;
        case MaterialVisSettings::GeometryType::Cube:
            return PlatformRig::MaterialVisSettings::GeometryType::Cube;
        default:
            return PlatformRig::MaterialVisSettings::GeometryType::Plane2D;
        }
    }

    static MaterialVisSettings::LightingType AsManaged(
        PlatformRig::MaterialVisSettings::LightingType::Enum input)
    {
        switch (input) {
        case PlatformRig::MaterialVisSettings::LightingType::Deferred:
            return MaterialVisSettings::LightingType::Deferred;
        case PlatformRig::MaterialVisSettings::LightingType::Forward:
            return MaterialVisSettings::LightingType::Forward;
        default:
            return MaterialVisSettings::LightingType::NoLightingParser;
        }
    }

    static PlatformRig::MaterialVisSettings::LightingType::Enum AsNative(
         MaterialVisSettings::LightingType input)
    {
        switch (input) {
        case MaterialVisSettings::LightingType::Deferred:
            return PlatformRig::MaterialVisSettings::LightingType::Deferred;
        case MaterialVisSettings::LightingType::Forward:
            return PlatformRig::MaterialVisSettings::LightingType::Forward;
        default:
            return PlatformRig::MaterialVisSettings::LightingType::NoLightingParser;
        }
    }

    MaterialVisSettings::GeometryType MaterialVisSettings::Geometry::get()
    {
        return AsManaged(_object->get()->_geometryType);
    }

    void MaterialVisSettings::Geometry::set(GeometryType value)
    {
        _object->get()->_geometryType = AsNative(value);
    }

    MaterialVisSettings::LightingType MaterialVisSettings::Lighting::get()
    {
        return AsManaged(_object->get()->_lightingType);
    }

    void MaterialVisSettings::Lighting::set(LightingType value)
    {
        _object->get()->_lightingType = AsNative(value);
    }

    MaterialVisSettings^ MaterialVisSettings::CreateDefault()
    {
        return gcnew MaterialVisSettings(std::make_shared<PlatformRig::MaterialVisSettings>());
    }

}

