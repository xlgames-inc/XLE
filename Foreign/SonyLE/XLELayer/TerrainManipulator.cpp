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

namespace SceneEngine { class TerrainManager; }

namespace XLELayer
{
    [Export(LevelEditorCore::IManipulator::typeid)]
    [Export(IInitializable::typeid)]
    [PartCreationPolicy(CreationPolicy::Shared)]
    public ref class TerrainManipulator : public LevelEditorCore::IManipulator, public IInitializable
    {
    public:
        virtual bool Pick(LevelEditorCore::ViewControl^ vc, Point scrPt)        { return _nativeManip->MouseMove(vc, scrPt); }
        virtual void Render(LevelEditorCore::ViewControl^ vc)                   { _nativeManip->Render(vc); }
        virtual void OnBeginDrag()                                              { _nativeManip->OnBeginDrag(); }
        virtual void OnDragging(LevelEditorCore::ViewControl^ vc, Point scrPt)  { _nativeManip->OnDragging(vc, scrPt); }

        virtual void OnEndDrag(LevelEditorCore::ViewControl^ vc, Point scrPt) 
		{
            _nativeManip->OnEndDrag(vc, scrPt);

			// we need to create operations and turn them into a transaction:
			// string transName = string.Format("Apply {0} brush", brush.Name);
			// 
			// GameContext context = m_designView.Context.As<GameContext>();
			// context.DoTransaction(
			// 	delegate
			// {
			// 	foreach(var op in m_tmpOps)
			// 		context.TransactionOperations.Add(op);
			// }, transName);
			// m_tmpOps.Clear();
		}

        property LevelEditorCore::ManipulatorInfo^ ManipulatorInfo
        {
            virtual LevelEditorCore::ManipulatorInfo^ get() 
            {
                return gcnew LevelEditorCore::ManipulatorInfo(
                    Sce::Atf::Localizer::Localize("Terrain", String::Empty),
                    Sce::Atf::Localizer::Localize("Activate Terrain editing", String::Empty),
                    LevelEditorCore::Resources::TerrainManipImage,
                    Keys::None);
            }
        }

        virtual void Initialize()
        {
            _domChangeInspector = gcnew DomChangeInspector(m_contextRegistry);
            _domChangeInspector->OnActiveContextChanged += gcnew DomChangeInspector::OnChangedDelegate(this, &TerrainManipulator::UpdateManipulatorContext);
            _domChangeInspector->OnDOMObjectChanged += gcnew DomChangeInspector::OnChangedDelegate(this, &TerrainManipulator::OnDOMChange);
            _nativeManip = gcnew NativeManipulatorLayer(_manipContext);
        }

        TerrainManipulator()
        {
            _manipContext = gcnew ActiveManipulatorContext();
        }

        property ActiveManipulatorContext^ ManipulatorContext
        {
            ActiveManipulatorContext^ get() { return _manipContext; }
        }

    private:
        void OnDOMChange(System::Object^ object)
        {
            bool updateManipulators = false;
            if (!object) {
                updateManipulators = true;
            } else {
                auto node = Sce::Atf::Adaptation::Adapters::As<Sce::Atf::Dom::DomNodeAdapter^>(object);
                if (node && node->DomNode->Type->Name == "gap:terrainType") {
                    updateManipulators = true;
                }
            }

            if (updateManipulators) {
                UpdateManipulatorContext(object);
            }
        }

        void UpdateManipulatorContext(System::Object^ object)
        {
            auto sceneMan = NativeManipulatorLayer::SceneManager;
            if (sceneMan) {
                _manipContext->ManipulatorSet = sceneMan->CreateTerrainManipulators();
            } else {
                _manipContext->ManipulatorSet = nullptr;
            }
        }

        NativeManipulatorLayer^ _nativeManip;
        ActiveManipulatorContext^ _manipContext;
        DomChangeInspector^ _domChangeInspector;
        [Import(AllowDefault = false)] IContextRegistry^ m_contextRegistry;
    };
}


