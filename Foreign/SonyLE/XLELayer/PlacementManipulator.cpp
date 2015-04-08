// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4564)

#include "NativeManipulators.h"
#include "XLELayerUtils.h"

using namespace System::ComponentModel;
using namespace System::ComponentModel::Composition;
using namespace System::Windows::Forms;

namespace XLELayer
{
    public interface class IPlacementControls
    {
    public:
        property ActiveManipulatorContext^ ActiveContext 
            { virtual void set(ActiveManipulatorContext^); }
    };

    [Export(LevelEditorCore::IManipulator::typeid)]
    [Export(IInitializable::typeid)]
    [PartCreationPolicy(CreationPolicy::Shared)]
    public ref class PlacementManipulator : public LevelEditorCore::IManipulator, public IInitializable
    {
    public:
        virtual bool Pick(LevelEditorCore::ViewControl^ vc, Point scrPt)        { return _nativeManip->MouseMove(vc, scrPt); }
        virtual void Render(LevelEditorCore::ViewControl^ vc)                   { _nativeManip->Render(vc); }
        virtual void OnBeginDrag()                                              { _nativeManip->OnBeginDrag(); }
        virtual void OnDragging(LevelEditorCore::ViewControl^ vc, Point scrPt)  { _nativeManip->OnDragging(vc, scrPt); }

        virtual void OnEndDrag(LevelEditorCore::ViewControl^ vc, Point scrPt) 
		{
            _nativeManip->OnEndDrag(vc, scrPt);
		}

        property LevelEditorCore::ManipulatorInfo^ ManipulatorInfo
        {
            virtual LevelEditorCore::ManipulatorInfo^ get() 
            {
                return gcnew LevelEditorCore::ManipulatorInfo(
                    Sce::Atf::Localizer::Localize("Placements", String::Empty),
                    Sce::Atf::Localizer::Localize("Activate placements editing", String::Empty),
                    LevelEditorCore::Resources::TerrainManipImage,
                    Keys::None);
            }
        }

        virtual void Initialize()
        {
            _manipSettings = gcnew Settings();
            _manipContext = gcnew ActiveManipulatorContext();
            _manipContext->ManipulatorSet = NativeManipulatorLayer::SceneManager->CreatePlacementManipulators(_manipSettings);
            _controls->ActiveContext = _manipContext;
            _nativeManip = gcnew NativeManipulatorLayer(_manipContext);
        }

    private:
        NativeManipulatorLayer^ _nativeManip;
        ActiveManipulatorContext^ _manipContext;
        GUILayer::IPlacementManipulatorSettingsLayer^ _manipSettings;

        [Import(AllowDefault = false)] IPlacementControls^ _controls;

        ref class Settings : GUILayer::IPlacementManipulatorSettingsLayer
        {
        public:
            virtual String^ GetSelectedModel() override
            {
                return "game/model/nature/bushtree/BushE";
            }
            virtual void EnableSelectedModelDisplay(bool newState) override {}
            virtual void SelectModel(String^ newModelName) override {}
            virtual void SwitchToMode(unsigned newMode) override {}
        };
    };
}

