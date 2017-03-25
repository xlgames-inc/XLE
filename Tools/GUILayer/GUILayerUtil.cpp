// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "GUILayerUtil.h"
#include "MarshalString.h"
#include "ExportedNativeTypes.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../Assets/AssetUtils.h"
#include "../../ConsoleRig/IProgress.h"
#include "../../Utility/MemoryUtils.h"

namespace GUILayer
{
    System::UInt64 Utils::HashID(System::String^ string)
    {
        return Hash64(clix::marshalString<clix::E_UTF8>(string));
    }

	System::String^ Utils::MakeAssetName(System::String^ value)
	{
		auto nativeName = clix::marshalString<clix::E_UTF8>(value);
        ::Assets::ResolvedAssetFile resName;
        ::Assets::MakeAssetName(resName, nativeName.c_str());
		return clix::marshalString<clix::E_UTF8>(resName._fn);
	}

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
        std::shared_ptr<SceneEngine::PlacementCellSet> placements,
        std::shared_ptr<SceneEngine::PlacementsEditor> placementsEditor,
        std::initializer_list<std::shared_ptr<SceneEngine::IIntersectionTester>> extraTesters)
    {
		_scene = std::make_shared<SceneEngine::IntersectionTestScene>(
            terrainManager, 
            placements, placementsEditor, extraTesters);
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

///////////////////////////////////////////////////////////////////////////////////////////////////

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

    PlacementsRendererWrapper::PlacementsRendererWrapper(
		std::shared_ptr<SceneEngine::PlacementsRenderer> scene)
	{
		_renderer = std::move(scene);
	}

    PlacementsRendererWrapper::~PlacementsRendererWrapper()
    {
        _renderer.reset();
    }

    PlacementsRendererWrapper::!PlacementsRendererWrapper()
    {
        System::Diagnostics::Debug::Assert(false, "PlacementsEditorWrapper finalizer used");
    }

	SceneEngine::PlacementsRenderer& PlacementsRendererWrapper::GetNative()
	{
		return *_renderer.get();
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

    StepAdapter::~StepAdapter() 
    {
        _adapted->EndStep();
    }

    class ProgressAdapter : public ConsoleRig::IProgress
    {
    public:
        virtual std::shared_ptr<ConsoleRig::IStep> BeginStep(const char name[], unsigned progressMax, bool cancellable);

        ProgressAdapter(GUILayer::IProgress^ adapted);
        ~ProgressAdapter();
    protected:
        gcroot<GUILayer::IProgress^> _adapted;
    };

    std::shared_ptr<ConsoleRig::IStep> ProgressAdapter::BeginStep(const char name[], unsigned progressMax, bool cancellable)
    {
        auto n = clix::marshalString<clix::E_UTF8>(name);
        return std::shared_ptr<StepAdapter>(new StepAdapter(_adapted->BeginStep(n, progressMax, cancellable)));
    }

    ProgressAdapter::ProgressAdapter(GUILayer::IProgress^ adapted)
    : _adapted(adapted) {}

    ProgressAdapter::~ProgressAdapter() {}


    auto IProgress::CreateNative(IProgress^ managed) -> ProgressPtr
    {
        return ProgressPtr(new ProgressAdapter(managed), IProgress::DeleteNative);
    }

    void IProgress::DeleteNative(ConsoleRig::IProgress* native)
    {
        delete native;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	System::String^ DirectorySearchRules::ResolveFile(System::String^ baseName)
	{
		char buffer[MaxPath];
		auto nativeDirName = clix::marshalString<clix::E_UTF8>(baseName);
		_searchRules->ResolveFile(buffer, nativeDirName.c_str());
		return clix::marshalString<clix::E_UTF8>(buffer);
	}

	void DirectorySearchRules::AddSearchDirectory(System::String^ dirName)
	{
		auto nativeDirName = clix::marshalString<clix::E_UTF8>(dirName);
		_searchRules->AddSearchDirectory(MakeStringSection(nativeDirName));
	}

	const ::Assets::DirectorySearchRules& DirectorySearchRules::GetNative() { return *_searchRules.get(); }

    DirectorySearchRules::DirectorySearchRules(std::shared_ptr<::Assets::DirectorySearchRules> searchRules)
	{
		_searchRules = std::move(searchRules);
	}

	DirectorySearchRules::DirectorySearchRules()
	{
		_searchRules = std::make_shared<::Assets::DirectorySearchRules>();
	}

	DirectorySearchRules::DirectorySearchRules(const ::Assets::DirectorySearchRules& searchRules)
	{
		_searchRules = std::make_shared<::Assets::DirectorySearchRules>(searchRules);
	}

	DirectorySearchRules::~DirectorySearchRules() {}

}

