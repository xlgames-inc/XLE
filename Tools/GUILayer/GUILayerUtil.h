// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "CLIXAutoPtr.h"

namespace RenderCore { namespace Techniques { class TechniqueContext; } }
namespace SceneEngine { class IntersectionTestContext; class IntersectionTestScene; }

namespace GUILayer
{
    public ref class TechniqueContextWrapper
    {
    public:
        clix::shared_ptr<RenderCore::Techniques::TechniqueContext> _techniqueContext;

        TechniqueContextWrapper(std::shared_ptr<RenderCore::Techniques::TechniqueContext> techniqueContext);
    };

    public ref class IntersectionTestContextWrapper
    {
    public:
        clix::shared_ptr<SceneEngine::IntersectionTestContext> _context;

		SceneEngine::IntersectionTestContext& GetNative();
        IntersectionTestContextWrapper(std::shared_ptr<SceneEngine::IntersectionTestContext> context);
    };

	public ref class IntersectionTestSceneWrapper
	{
	public:
		clix::shared_ptr<SceneEngine::IntersectionTestScene> _scene;

		SceneEngine::IntersectionTestScene& GetNative();
		IntersectionTestSceneWrapper(std::shared_ptr<SceneEngine::IntersectionTestScene> scene);
	};
}
