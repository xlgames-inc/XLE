// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once
#include "AutoToShared.h"

namespace RenderCore { namespace Techniques { class TechniqueContext; } }
namespace SceneEngine { class IntersectionTestContext; class IntersectionTestScene; }

namespace GUILayer
{
    public ref class TechniqueContextWrapper
    {
    public:
        AutoToShared<RenderCore::Techniques::TechniqueContext> _techniqueContext;

        TechniqueContextWrapper(std::shared_ptr<RenderCore::Techniques::TechniqueContext> techniqueContext);
    };

    public ref class IntersectionTestContextWrapper
    {
    public:
        AutoToShared<SceneEngine::IntersectionTestContext> _context;

		SceneEngine::IntersectionTestContext& GetNative();
        IntersectionTestContextWrapper(std::shared_ptr<SceneEngine::IntersectionTestContext> context);
    };

	public ref class IntersectionTestSceneWrapper
	{
	public:
		AutoToShared<SceneEngine::IntersectionTestScene> _scene;

		SceneEngine::IntersectionTestScene& GetNative();
		IntersectionTestSceneWrapper(std::shared_ptr<SceneEngine::IntersectionTestScene> scene);
	};
}
