// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "GUILayerUtil.h"
#include "MarshalString.h"
#include "ExportedNativeTypes.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../Utility/MemoryUtils.h"

namespace GUILayer
{
    public ref class Util
    {
    public:
        static System::UInt64 HashID(System::String^ string)
        {
            return Hash64(clix::marshalString<clix::E_UTF8>(string));
        }
    };

    TechniqueContextWrapper::TechniqueContextWrapper(
        std::shared_ptr<RenderCore::Techniques::TechniqueContext> techniqueContext)
    {
        _techniqueContext = std::move(techniqueContext);
    }

    TechniqueContextWrapper::~TechniqueContextWrapper()
    {
        _techniqueContext.reset();
    }

    IntersectionTestContextWrapper::IntersectionTestContextWrapper(
        std::shared_ptr<SceneEngine::IntersectionTestContext> context)
    {
        _context = std::move(context);
    }

    IntersectionTestContextWrapper::~IntersectionTestContextWrapper()
    {
        _context.reset();
    }

	SceneEngine::IntersectionTestContext& IntersectionTestContextWrapper::GetNative()
	{
		return *_context.get();
	}

	IntersectionTestSceneWrapper::IntersectionTestSceneWrapper(
		std::shared_ptr<SceneEngine::IntersectionTestScene> scene)
	{
		_scene = std::move(scene);
	}

    IntersectionTestSceneWrapper::IntersectionTestSceneWrapper(
        std::shared_ptr<SceneEngine::TerrainManager> terrainManager,
        std::shared_ptr<SceneEngine::PlacementsEditor> placements)
    {
		_scene = std::make_shared<SceneEngine::IntersectionTestScene>(terrainManager, placements);
    }

    IntersectionTestSceneWrapper::~IntersectionTestSceneWrapper()
    {
        _scene.reset();
    }

	SceneEngine::IntersectionTestScene& IntersectionTestSceneWrapper::GetNative()
	{
		return *_scene.get();
	}
}

