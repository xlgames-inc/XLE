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

            if (_resourceLister) {
                _resourceLister->SelectionChanged += 
                    gcnew EventHandler(this, &PlacementManipulator::resourceLister_SelectionChanged);
            }
        }

    private:
        ref class Settings : GUILayer::IPlacementManipulatorSettingsLayer
        {
        public:
            virtual String^ GetSelectedModel() override { return _selectedModel; }
            virtual void SelectModel(String^ newModelName) override  { _selectedModel = newModelName; }
            virtual void EnableSelectedModelDisplay(bool newState) override {}
            virtual void SwitchToMode(unsigned newMode) override {}

            Settings()
            {
                _selectedModel = "game/model/nature/bushtree/BushE";
            }

            String^ _selectedModel;
        };

        NativeManipulatorLayer^ _nativeManip;
        ActiveManipulatorContext^ _manipContext;
        Settings^ _manipSettings;

        [Import(AllowDefault = false)] IPlacementControls^ _controls;
        [Import(AllowDefault =  true)] LevelEditorCore::ResourceLister^ _resourceLister;

        String^ AsResourcePathName(Uri^ uri);

        String^ StripExtension(String^ input)
        {
            int dot = input->LastIndexOf('.');
            int sep0 = input->LastIndexOf('/');
            int sep1 = input->LastIndexOf('\\');
            if (dot > 0 && dot > sep0 && dot > sep1) {
                return input->Substring(0, dot);
            }
            return input;
        }

        void resourceLister_SelectionChanged(Object^ sender, EventArgs^ e)
        {
            auto resourceUri = _resourceLister->LastSelected;
            if (resourceUri) {
                _manipSettings->_selectedModel = 
                    StripExtension(AsResourcePathName(resourceUri));
            }
        }
    };

    String^ PlacementManipulator::AsResourcePathName(Uri^ uri)
    {
            // covert this uri into a string filename that is fit for the assets system
        auto cwd = gcnew Uri(System::IO::Directory::GetCurrentDirectory()->TrimEnd('\\') + "\\");
        auto relUri = cwd->MakeRelativeUri(uri);
        return Uri::UnescapeDataString(relUri->ToString());
    }
}

