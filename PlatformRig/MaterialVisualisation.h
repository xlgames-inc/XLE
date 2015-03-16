// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ModelVisualisation.h"
#include "../RenderCore/Assets/AssetUtils.h"
#include "../RenderCore/Metal/Forward.h"

namespace PlatformRig
{
    class MaterialVisSettings
    {
    public:
        std::shared_ptr<VisCameraSettings> _camera;
        
        struct GeometryType
        {
            enum Enum { Sphere, Cube, Plane2D };
        };
        GeometryType::Enum _geometryType;

        struct LightingType
        {
            enum Enum { Deferred, Forward, NoLightingParser };
        };
        LightingType::Enum _lightingType;

        MaterialVisSettings();
    };

    class MaterialVisObject
    {
    public:
        RenderCore::Metal::ShaderProgram* _shaderProgram;
        RenderCore::Assets::MaterialParameters _parameters;

        class SystemConstants
        {
        public:
            Float3      _lightNegativeDirection;
            Float3      _lightColour;
            SystemConstants();
        };
        SystemConstants _systemConstants;

        MaterialVisObject() { _shaderProgram = nullptr; }
    };

    /// <summary>Renders a visualisation of a material</summary>
    /// Designed for tools, this layer will render a material on a 
    /// generic piece of geometry, with generic environment settings.
    class MaterialVisLayer : public IOverlaySystem
    {
    public:
        virtual std::shared_ptr<IInputListener> GetInputListener();

        virtual void RenderToScene(
            RenderCore::IThreadContext* context, 
            SceneEngine::LightingParserContext& parserContext); 
        virtual void RenderWidgets(
            RenderCore::IThreadContext* context, 
            const RenderCore::Techniques::ProjectionDesc& projectionDesc);
        virtual void SetActivationState(bool newState);

        MaterialVisLayer(
            std::shared_ptr<MaterialVisSettings> settings,
            std::shared_ptr<MaterialVisObject> object);
        ~MaterialVisLayer();

        static bool Draw(
            RenderCore::IThreadContext& context,
            SceneEngine::LightingParserContext& lightingParser,
            const MaterialVisSettings& settings,
            const MaterialVisObject& object);
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
    
}


