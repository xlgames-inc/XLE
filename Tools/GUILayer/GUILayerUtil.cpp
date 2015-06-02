// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "GUILayerUtil.h"
#include "MarshalString.h"
#include "ExportedNativeTypes.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../ConsoleRig/IProgress.h"
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
        std::shared_ptr<SceneEngine::PlacementsEditor> placements,
        std::initializer_list<std::shared_ptr<SceneEngine::IIntersectionTester>> extraTesters)
    {
		_scene = std::make_shared<SceneEngine::IntersectionTestScene>(terrainManager, placements, extraTesters);
    }

    IntersectionTestSceneWrapper::~IntersectionTestSceneWrapper()
    {
        _scene.reset();
    }

    IntersectionTestSceneWrapper::!IntersectionTestSceneWrapper()
    {
        System::Diagnostics::Debug::Assert(false, "IntersectionTestSceneWrapper finalizer used");
    }

	SceneEngine::IntersectionTestScene& IntersectionTestSceneWrapper::GetNative()
	{
		return *_scene.get();
	}

    PlacementsEditorWrapper::PlacementsEditorWrapper(
		std::shared_ptr<SceneEngine::PlacementsEditor> scene)
	{
		_editor = std::move(scene);
	}

    PlacementsEditorWrapper::~PlacementsEditorWrapper()
    {
        _editor.reset();
    }

    PlacementsEditorWrapper::!PlacementsEditorWrapper()
    {
        System::Diagnostics::Debug::Assert(false, "PlacementsEditorWrapper finalizer used");
    }

	SceneEngine::PlacementsEditor& PlacementsEditorWrapper::GetNative()
	{
		return *_editor.get();
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class StepAdapter : public ConsoleRig::IStep
    {
    public:
        void SetProgress(unsigned progress);
        void Advance();
        bool IsCancelled() const;

        StepAdapter(GUILayer::IStep^ adapted);
        ~StepAdapter();
    protected:
        gcroot<GUILayer::IStep^> _adapted;
    };

    void StepAdapter::SetProgress(unsigned progress)
    {
        _adapted->SetProgress(progress);
    }

    void StepAdapter::Advance()
    {
        _adapted->Advance();
    }

    bool StepAdapter::IsCancelled() const
    {
        return _adapted->IsCancelled();
    }

    StepAdapter::StepAdapter(GUILayer::IStep^ adapted)
    : _adapted(adapted) {}

    StepAdapter::~StepAdapter() {}

    class ProgressAdapter : public ConsoleRig::IProgress
    {
    public:
        virtual std::shared_ptr<ConsoleRig::IStep> BeginStep(const char name[], unsigned progressMax);

        ProgressAdapter(GUILayer::IProgress^ adapted);
        ~ProgressAdapter();
    protected:
        gcroot<GUILayer::IProgress^> _adapted;
    };

    std::shared_ptr<ConsoleRig::IStep> ProgressAdapter::BeginStep(const char name[], unsigned progressMax)
    {
        return std::shared_ptr<StepAdapter>(new StepAdapter(_adapted->BeginStep(name, progressMax)));
    }

    ProgressAdapter::ProgressAdapter(GUILayer::IProgress^ adapted)
    : _adapted(adapted) {}

    ProgressAdapter::~ProgressAdapter() {}


    ConsoleRig::IProgress* IProgress::CreateNative(IProgress^ managed)
    {
        return new ProgressAdapter(managed);
    }

    void IProgress::DeleteNative(ConsoleRig::IProgress* native)
    {
        delete native;
    }

}

