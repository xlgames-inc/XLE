// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Techniques/Drawables.h"
#include "../RenderCore/Techniques/TechniqueUtils.h"

namespace RenderCore { class IThreadContext; }
namespace RenderCore { namespace Techniques { class ParsingContext; class DrawablesPacket; enum class BatchFilter; } }

namespace SceneEngine
{
    class PreparedScene;
	class IViewDelegate;

	class SceneExecuteContext
	{
	public:
		IteratorRange<const SceneView*> GetViews() const { return MakeIteratorRange(_views); }
		IteratorRange<const std::shared_ptr<IViewDelegate>*> GetViewDelegates();
		RenderCore::Techniques::DrawablesPacket* GetDrawablesPacket(unsigned viewIndex, RenderCore::Techniques::BatchFilter batch);
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

}

