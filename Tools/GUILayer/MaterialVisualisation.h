// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "UITypesBinding.h"
#include "IOverlaySystem.h"
#include "EngineForward.h"
#include "CLIXAutoPtr.h"
#include "../ToolsRig/MaterialVisualisation.h"

using namespace System::Collections::Generic;

namespace ToolsRig { class MaterialVisObject; }

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
            VisCameraSettings^ get() { return _camSettings; }
        }

        static MaterialVisSettings^ CreateDefault();

        MaterialVisSettings(std::shared_ptr<ToolsRig::MaterialVisSettings> attached)
        {
            _object = std::move(attached);
            _camSettings = gcnew VisCameraSettings(_object->_camera);
        }

        ~MaterialVisSettings() { delete _camSettings; _object.reset(); }

        !MaterialVisSettings()
        {
            System::Diagnostics::Debug::Assert(false, "Non deterministic delete of MaterialVisSettings");
        }

        const ToolsRig::MaterialVisSettings& GetUnderlying() { return *_object.get(); }

    protected:
        clix::shared_ptr<ToolsRig::MaterialVisSettings> _object;
        VisCameraSettings^ _camSettings;
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

        void SetConfig(IEnumerable<RawMaterial^>^ config);

        MaterialVisLayer(
            MaterialVisSettings^ settings,
            IEnumerable<RawMaterial^>^ config);
        ~MaterialVisLayer();
        
    protected:
        clix::shared_ptr<ToolsRig::MaterialVisObject> _visObject;
        IEnumerable<RawMaterial^>^ _config;
        MaterialVisSettings^ _settings;

        void Resolve();
        void ChangeHandler(System::Object^ sender, System::EventArgs^ args);
        void ListChangeHandler(System::Object^ sender, ListChangedEventArgs^ args);
        void PropChangeHandler(System::Object^ sender, PropertyChangedEventArgs^ args);
    };
}

