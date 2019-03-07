// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "CLIXAutoPtr.h"
#include <functional>

namespace RenderCore { namespace Techniques { class TechniqueContext; } }
namespace SceneEngine 
{
    class IntersectionTestScene; 
    class TerrainManager;
    class PlacementsEditor;
    class PlacementsRenderer;
    class PlacementCellSet;
    class IIntersectionTester;
}

namespace ConsoleRig { class IProgress; }
namespace Assets { class DirectorySearchRules; }

namespace GUILayer
{
	public ref class Utils
	{
	public:
		static System::String^ MakeAssetName(System::String^ input);
		static System::UInt64 HashID(System::String^ string);

		ref struct AssetExtension
		{
			System::String^ Extension;
			System::String^ Description;
		};
		static System::Collections::Generic::IEnumerable<AssetExtension^>^ GetModelExtensions();
		static System::Collections::Generic::IEnumerable<AssetExtension^>^ GetAnimationSetExtensions();
	};

    public ref class TechniqueContextWrapper
    {
    public:
        clix::shared_ptr<RenderCore::Techniques::TechniqueContext> _techniqueContext;

        TechniqueContextWrapper(std::shared_ptr<RenderCore::Techniques::TechniqueContext> techniqueContext);
        ~TechniqueContextWrapper();
    };

	public ref class IntersectionTestSceneWrapper
	{
	public:
		clix::shared_ptr<SceneEngine::IntersectionTestScene> _scene;

		SceneEngine::IntersectionTestScene& GetNative();
		IntersectionTestSceneWrapper(std::shared_ptr<SceneEngine::IntersectionTestScene> scene);
        IntersectionTestSceneWrapper(
            std::shared_ptr<SceneEngine::TerrainManager> terrainManager,
            std::shared_ptr<SceneEngine::PlacementCellSet> placements,
            std::shared_ptr<SceneEngine::PlacementsEditor> placementsEditor,
            std::initializer_list<std::shared_ptr<SceneEngine::IIntersectionTester>> extraTesters);
        ~IntersectionTestSceneWrapper();
        !IntersectionTestSceneWrapper();
	};

    public ref class PlacementsEditorWrapper
	{
	public:
		clix::shared_ptr<SceneEngine::PlacementsEditor> _editor;

		SceneEngine::PlacementsEditor& GetNative();
		PlacementsEditorWrapper(std::shared_ptr<SceneEngine::PlacementsEditor> scene);
        ~PlacementsEditorWrapper();
        !PlacementsEditorWrapper();
	};

    public ref class PlacementsRendererWrapper
	{
	public:
		clix::shared_ptr<SceneEngine::PlacementsRenderer> _renderer;

		SceneEngine::PlacementsRenderer& GetNative();
		PlacementsRendererWrapper(std::shared_ptr<SceneEngine::PlacementsRenderer> scene);
        ~PlacementsRendererWrapper();
        !PlacementsRendererWrapper();
	};

    public interface class IStep
    {
        virtual void SetProgress(unsigned progress);
        virtual void Advance();
        virtual bool IsCancelled();
        virtual void EndStep();
    };

    public interface class IProgress
    {
    public:
        virtual IStep^ BeginStep(System::String^ name, unsigned progressMax, bool cancellable);

        typedef std::unique_ptr<ConsoleRig::IProgress, std::function<void(ConsoleRig::IProgress*)>> ProgressPtr;
        static ProgressPtr CreateNative(IProgress^ managed);
        static void DeleteNative(ConsoleRig::IProgress* native);
    };

	public ref class DirectorySearchRules
    {
    public:
        clix::shared_ptr<::Assets::DirectorySearchRules> _searchRules;

		System::String^ ResolveFile(System::String^ baseName);
		void AddSearchDirectory(System::String^ dirName);

		const ::Assets::DirectorySearchRules& GetNative();

        DirectorySearchRules(std::shared_ptr<::Assets::DirectorySearchRules> searchRules);
		DirectorySearchRules(const ::Assets::DirectorySearchRules& searchRules);
		DirectorySearchRules();
        ~DirectorySearchRules();
    };
}
