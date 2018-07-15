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
#include <memory>

namespace RenderCore { namespace Techniques { class TechniqueContext; } }

namespace ToolsRig
{
	class VisCameraSettings;

    class MaterialVisSettings
    {
    public:
        std::shared_ptr<VisCameraSettings> _camera;
        
        enum class GeometryType { Sphere, Cube, Plane2D, Model };
        GeometryType _geometryType = GeometryType::Sphere;

        enum class LightingType { Deferred, Forward, NoLightingParser };
        LightingType _lightingType = LightingType::NoLightingParser;

        mutable bool _pendingCameraAlignToModel = false;

        MaterialVisSettings();
    };

    class MaterialVisObject
    {
    public:
        RenderCore::Techniques::Material	_parameters;
        ::Assets::DirectorySearchRules		_searchRules;
        ::Assets::rstring					_previewModelFile;
        uint64_t							_previewMaterialBinding = 0;
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
            RenderCore::Techniques::ParsingContext& parserContext);
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
            const MaterialVisSettings& settings,
			const VisEnvSettings& envSettings,
            const MaterialVisObject& object);
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };


	enum class DrawPreviewResult
    {
        Error,
        Pending,
        Success
    };

	enum class PreviewGeometry
    {
        Chart, Plane2D, Box, Sphere, Model
    };

	std::pair<DrawPreviewResult, std::string> DrawPreview(
        RenderCore::IThreadContext& context,
        const RenderCore::Techniques::TechniqueContext& techContext,
        PreviewGeometry geometry,
		ToolsRig::MaterialVisObject& sourceVisObject);
    
}


