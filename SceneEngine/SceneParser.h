// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/IThreadContext_Forward.h"
#include "../RenderCore/Techniques/Drawables.h"
#include "../RenderCore/Techniques/TechniqueUtils.h"

namespace RenderCore { namespace Techniques { class CameraDesc; class ProjectionDesc; class ParsingContext; class DrawablesPacket; } }

namespace SceneEngine
{
    class LightingParserContext;
    class ShadowProjectionDesc;
    class GlobalLightingDesc;
    class LightDesc;
    class ToneMapSettings;
    class PreparedScene;

    enum class BatchFilter
    {
        General,                // general rendering batch
        PreDepth,               // objects that should get a pre-depth pass
        Transparent,            // transparent objects (particularly those that require some object based sorting)
        OITransparent,          // order independent transparent
        TransparentPreDepth,    // pre-depth pass for objects considered "transparent" (ie, opaque parts of transparent objects)
        DMShadows,              // depth map shadows
        RayTracedShadows        // objects enabled for rendering into ray traced shadows
    };

	class SceneView
	{
	public:
		enum class Type { Normal, Shadow, Other };

		RenderCore::Techniques::ProjectionDesc _projection;
		Type _type = SceneView::Type::Normal;
	};

	class IViewDelegate;

	class SceneExecuteContext
	{
	public:
		IteratorRange<const SceneView*> GetViews() const { return MakeIteratorRange(_views); }
		IteratorRange<const std::shared_ptr<IViewDelegate>*> GetViewDelegates();
		RenderCore::Techniques::DrawablesPacket& GetDrawablesPacket(unsigned viewIndex, BatchFilter batch);
		PreparedScene& GetPreparedScene();

		void AddView(
			const SceneView& view,
			const std::shared_ptr<IViewDelegate>& delegate);

		SceneExecuteContext();
		virtual ~SceneExecuteContext();
	private:
		class Pimpl;
		std::unique_ptr<Pimpl> _pimpl;

		std::vector<SceneView> _views;
	};

    class IScene
    {
    public:
        virtual void ExecuteScene(
            RenderCore::IThreadContext& threadCpntext,
			SceneExecuteContext& executeContext) const = 0;

		virtual ~IScene();
	};

}

