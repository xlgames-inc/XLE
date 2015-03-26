// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "UITypesBinding.h"
#include "IOverlaySystem.h"
#include "EngineForward.h"
#include "../../PlatformRig/MaterialVisualisation.h"

namespace GUILayer
{
    public ref class MaterialVisSettings
    {
    public:
        enum class GeometryType { Sphere, Cube, Plane2D };
        enum class LightingType { Deferred, Forward, NoLightingParser };

        property GeometryType Geometry { GeometryType get(); void set(GeometryType); }
        property LightingType Lighting { LightingType get(); void set(LightingType); }

        property VisCameraSettings^ Camera
        {
            VisCameraSettings^ get() { return gcnew VisCameraSettings((*_object)->_camera); }
        }

        static MaterialVisSettings^ CreateDefault();

        MaterialVisSettings(std::shared_ptr<PlatformRig::MaterialVisSettings> attached)
        {
            _object.reset(new std::shared_ptr<PlatformRig::MaterialVisSettings>(std::move(attached)));
        }

        ~MaterialVisSettings() { _object.reset(); }

        const PlatformRig::MaterialVisSettings& GetUnderlying() { return *_object->get(); }

    protected:
        AutoToShared<PlatformRig::MaterialVisSettings> _object;
    };

    public ref class MaterialVisLayer : public IOverlaySystem
    {
    public:
        virtual void RenderToScene(
            RenderCore::IThreadContext* context, 
            SceneEngine::LightingParserContext& parserContext) override;
        virtual void RenderWidgets(
            RenderCore::IThreadContext* device, 
            const RenderCore::Techniques::ProjectionDesc& projectionDesc) override;
        virtual void SetActivationState(bool newState) override;
        // virtual std::shared_ptr<IInputListener> GetInputListener() override;

        void SetConfig(RawMaterial^ config);

        MaterialVisLayer(
            MaterialVisSettings^ settings,
            RawMaterial^ config);
        ~MaterialVisLayer();
        
    protected:
        RawMaterial^ _config;
        MaterialVisSettings^ _settings;
    };
}

