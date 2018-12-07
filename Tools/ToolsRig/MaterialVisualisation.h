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

namespace RenderCore { namespace Techniques { class TechniqueContext; class AttachmentPool; class FrameBufferPool; class ITechniqueDelegate; class IMaterialDelegate; } }
namespace SceneEngine { class IScene; }

namespace ToolsRig
{
	class VisCameraSettings;

    class MaterialVisSettings
    {
    public:
        std::shared_ptr<VisCameraSettings> _camera = std::make_shared<VisCameraSettings>();
        
        enum class GeometryType { Sphere, Cube, Plane2D, Model };
        GeometryType _geometryType = GeometryType::Sphere;

        mutable bool _pendingCameraAlignToModel = false;

		RenderCore::Techniques::Material	_parameters;
        ::Assets::DirectorySearchRules		_searchRules;
        ::Assets::rstring					_previewModelFile;
        uint64_t							_previewMaterialBinding = 0;
    };

	// std::shared_ptr<RenderCore::Techniques::ITechniqueDelegate> _techniqueDelegate;
	// std::shared_ptr<RenderCore::Techniques::IMaterialDelegate> _materialDelegate;

    class VisEnvSettings;

	enum class DrawPreviewResult
    {
        Error,
        Pending,
        Success
    };

	enum class DrawPreviewLightingType { Deferred, Forward, NoLightingParser };

	std::pair<DrawPreviewResult, std::string> DrawPreview(
        RenderCore::IThreadContext& context,
        const RenderCore::Techniques::TechniqueContext& techContext,
		RenderCore::Techniques::AttachmentPool* attachmentPool,
		RenderCore::Techniques::FrameBufferPool* frameBufferPool,
		const std::shared_ptr<MaterialVisSettings>& visObject);

	/*std::shared_ptr<SceneEngine::ISceneParser> CreateMaterialVisSceneParser(
		const std::shared_ptr<MaterialVisSettings>& settings,
        const std::shared_ptr<VisEnvSettings>& envSettings);*/

////////////////////////////////////////////////////////////////////////////////////////////////

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

		void SetLightingType(DrawPreviewLightingType newType);

        MaterialVisLayer(
            std::shared_ptr<SceneEngine::ILightingParserDelegate> lightingParser);
        ~MaterialVisLayer();

        bool Draw(
            RenderCore::IThreadContext& context,
			const RenderCore::IResourcePtr& renderTarget,
			RenderCore::Techniques::ParsingContext& parserContext,
            DrawPreviewLightingType lightingType,
			SceneEngine::IScene& sceneParser);
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };


}


