// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../PlatformRig/OverlaySystem.h"
#include "../../RenderCore/Techniques/TechniqueMaterial.h"
#include "../../RenderCore/Metal/Forward.h"
#include "../../Assets/AssetUtils.h"

namespace ToolsRig
{
	class VisCameraSettings;

    class MaterialVisSettings
    {
    public:
        std::shared_ptr<VisCameraSettings> _camera;
        
        struct GeometryType
        {
            enum Enum { Sphere, Cube, Plane2D, Model };
        };
        GeometryType::Enum _geometryType;

        struct LightingType
        {
            enum Enum { Deferred, Forward, NoLightingParser };
        };
        LightingType::Enum _lightingType;

        mutable bool _pendingCameraAlignToModel;

        MaterialVisSettings();
    };

    class MaterialVisObject
    {
    public:
        RenderCore::Techniques::Material	_parameters;
        ::Assets::DirectorySearchRules		_searchRules;
        // IMaterialBinder::SystemConstants _systemConstants;
        // std::shared_ptr<IMaterialBinder> _materialBinder;

        ::Assets::rstring	_previewModelFile;
        uint64				_previewMaterialBinding;

        MaterialVisObject();
        ~MaterialVisObject();
    };

    class VisEnvSettings;

    /// <summary>Renders a visualisation of a material</summary>
    /// Designed for tools, this layer will render a material on a 
    /// generic piece of geometry, with generic environment settings.
    class MaterialVisLayer : public PlatformRig::IOverlaySystem
    {
    public:
        virtual std::shared_ptr<IInputListener> GetInputListener();

        virtual void RenderToScene(
            RenderCore::IThreadContext& context, 
            RenderCore::Techniques::ParsingContext& parserContext,
            SceneEngine::LightingParserContext& lightingParserContext); 
        virtual void RenderWidgets(
            RenderCore::IThreadContext& context, 
            RenderCore::Techniques::ParsingContext& parsingContext);
        virtual void SetActivationState(bool newState);

        MaterialVisLayer(
            std::shared_ptr<MaterialVisSettings> settings,
            std::shared_ptr<VisEnvSettings> envSettings,
            std::shared_ptr<MaterialVisObject> object);
        ~MaterialVisLayer();

        static bool Draw(
            RenderCore::IThreadContext& context,
			RenderCore::Techniques::ParsingContext& parserContext,
            SceneEngine::LightingParserContext& lightingParser,
            const MaterialVisSettings& settings,
			const VisEnvSettings& envSettings,
            const MaterialVisObject& object);
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
    
}


