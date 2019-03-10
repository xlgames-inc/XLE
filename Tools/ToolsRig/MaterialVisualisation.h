// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

// #include "../../PlatformRig/OverlaySystem.h"
// #include "../../RenderCore/Assets/MaterialScaffold.h"
#include "../../Assets/AssetsCore.h"
#include <memory>

// namespace RenderCore { namespace Techniques { class TechniqueContext; class AttachmentPool; class FrameBufferPool; class ITechniqueDelegate; class IMaterialDelegate; class CameraDesc; } }
namespace SceneEngine { class IScene; }

namespace ToolsRig
{
	class VisCameraSettings;

    class MaterialVisSettings
    {
    public:
        enum class GeometryType { Sphere, Cube, Plane2D, Model };
        GeometryType _geometryType = GeometryType::Sphere;

        ::Assets::rstring					_previewModelFile;
        uint64_t							_previewMaterialBinding = 0;
    };

	// RenderCore::Assets::MaterialScaffoldMaterial		_parameters;
	// ::Assets::DirectorySearchRules		_searchRules;

	::Assets::FuturePtr<SceneEngine::IScene> MakeScene(const MaterialVisSettings& visObject);

    class VisEnvSettings;
	

#if 0
////////////////////////////////////////////////////////////////////////////////////////////////

	/// <summary>Renders a visualisation of a material</summary>
    /// Designed for tools, this layer will render a material on a 
    /// generic piece of geometry, with generic environment settings.
    class MaterialVisLayer : public PlatformRig::IOverlaySystem
    {
    public:
        virtual void Render(
            RenderCore::IThreadContext& context, 
			const RenderCore::IResourcePtr& renderTarget,
            RenderCore::Techniques::ParsingContext& parserContext);
		void SetLightingType(DrawPreviewLightingType newType);

        MaterialVisLayer(
            const std::shared_ptr<SceneEngine::IScene>& scene,
			const std::shared_ptr<SceneEngine::ILightingParserDelegate>& lightingParserDelegate,
			const std::shared_ptr<VisCameraSettings>& camera);
        ~MaterialVisLayer();

        static bool Draw(
            RenderCore::IThreadContext& context,
			const RenderCore::IResourcePtr& renderTarget,
			RenderCore::Techniques::ParsingContext& parserContext,
            DrawPreviewLightingType lightingType,
			SceneEngine::IScene& sceneParser,
			const SceneEngine::ILightingParserDelegate& lightingParserDelegate,
			const RenderCore::Techniques::CameraDesc& cameraDesc);
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
#endif

}


