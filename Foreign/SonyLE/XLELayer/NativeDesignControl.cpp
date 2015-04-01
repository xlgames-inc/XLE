// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../PlatformRig/ModelVisualisation.h"
#include "../../PlatformRig/OverlaySystem.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../Tools/GUILayer/NativeEngineDevice.h"
#include "../../RenderCore/IDevice.h"
#include <memory>

using namespace System;
using namespace System::Windows::Forms;
using namespace System::Drawing;
using namespace System::Collections::Generic;
using namespace LevelEditorCore::VectorMath;
using namespace Sce::Atf;
using namespace Sce::Atf::Adaptation;
using namespace Sce::Atf::Applications;
using namespace Sce::Atf::Dom;
using namespace Sce::Atf::VectorMath;
using namespace LevelEditorCore;

#pragma warning(disable:4564)   // method 'Copy' of class 'Sce::Atf::Dom::DomNode' defines unsupported default parameter 'originalToCopyMap'

namespace XLELayer
{
    static Float3 AsFloat3(Vec3F^ input) { return Float3(input->X, input->Y, input->Z); }

///////////////////////////////////////////////////////////////////////////////////////////////////

    public delegate void RenderCallback(LevelEditorCore::DesignView^ designView, Sce::Atf::Rendering::Camera^ camera);

    private ref class ManipulatorOverlay : public GUILayer::IOverlaySystem
    {
    public:
        virtual void RenderToScene(
            RenderCore::IThreadContext* device, 
            SceneEngine::LightingParserContext& parserContext) override;

        virtual void RenderWidgets(
            RenderCore::IThreadContext* device, 
            const RenderCore::Techniques::ProjectionDesc& projectionDesc) override {}
        virtual void SetActivationState(bool) override {}

        event RenderCallback^ OnRender;

        ManipulatorOverlay(
            LevelEditorCore::DesignView^ designView,
            LevelEditorCore::ViewControl^ viewControl)
        : _designView(designView), _viewControl(viewControl) {}

        ~ManipulatorOverlay() 
        {
            delete _designView; _designView = nullptr;
            delete _viewControl; _designView = nullptr;
        }

        !ManipulatorOverlay() 
        {
            delete _designView; _designView = nullptr;
            delete _viewControl; _designView = nullptr;
        }

    protected:
        LevelEditorCore::DesignView^ _designView;
        LevelEditorCore::ViewControl^ _viewControl;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    public ref class NativeDesignControl : public DesignViewControl
    {
    public:
        static RenderCore::Techniques::ParsingContext* s_currentParsingContext = nullptr;
        static GUILayer::SimpleRenderingContext^ CreateSimpleRenderingContext(GUILayer::SavedRenderResources^ savedRes)
        {
            if (!s_currentParsingContext) return nullptr;

            return gcnew GUILayer::SimpleRenderingContext(
                savedRes, 
                *GUILayer::EngineDevice::GetInstance()->GetNative().GetRenderDevice()->GetImmediateContext(),
                s_currentParsingContext);
        }

        NativeDesignControl(
            LevelEditorCore::DesignView^ designView, 
            GUILayer::EditorSceneManager^ sceneManager)
        : DesignViewControl(designView)
        {
            _layerControl = gcnew GUILayer::LayerControl(this);
            _cameraSettings = gcnew GUILayer::VisCameraSettings();
            _sceneManager = sceneManager;
            _layerControl->AddSystem(sceneManager->CreateOverlaySystem(_cameraSettings));
            _manipulatorOverlay = gcnew ManipulatorOverlay(designView, this);
            _layerControl->AddSystem(_manipulatorOverlay);
            
            if (s_marqueePen == nullptr) {
                using namespace System::Drawing;
                s_marqueePen = gcnew Pen(Color::FromArgb(30, 30, 30), 2);
                s_marqueePen->DashPattern = gcnew array<float, 1> { 3.f, 3.f };
            }

                //  use the camera "AxisSystem" to convert from the native SonyWWS
                //  camera coordinates into the coordinates we need for XLE.
                //      SonyWWS native has +Y as up
                //      But XLE uses +Z up
                //  note -- handiness untested. XLE defaults to right handed coordinates in
                //          view space.
                //
                //  When using AxisSystem like this, we must use the "World..." properties 
                //  in the Camera object. eg, "WorldEye" gives us the native XLE coordinates,
                //  "Eye" gives us the native SonyWWS coordinates.
            Camera->AxisSystem = gcnew Matrix4F(
                1.f, 0.f, 0.f, 0.f, 
                0.f, 0.f, -1.f, 0.f, 
                0.f, 1.f, 0.f, 0.f, 
                0.f, 0.f, 0.f, 1.f);
        }

        ~NativeDesignControl() { delete _layerControl; _layerControl = nullptr; delete _cameraSettings; _cameraSettings = nullptr; }

        void Render() override 
        {
                //  "_cameraSettings" should match the camera set in 
                //  the view control
            auto camera = Camera;
            _cameraSettings->GetUnderlyingRaw()->_position = AsFloat3(camera->WorldEye);
            _cameraSettings->GetUnderlyingRaw()->_focus = AsFloat3(camera->WorldLookAtPoint);
            _cameraSettings->GetUnderlyingRaw()->_verticalFieldOfView = camera->YFov * 180.f / gPI;
            _cameraSettings->GetUnderlyingRaw()->_nearClip = camera->NearZ;
            _cameraSettings->GetUnderlyingRaw()->_farClip = camera->FarZ;
            _layerControl->Render();

                // testing for agreement between SonyWWS camera code and XLE camera code
            // float viewportAspect = Width / float(Height);
            // auto camDesc = PlatformRig::AsCameraDesc(*_cameraSettings->GetUnderlyingRaw());
            // auto proj = RenderCore::Techniques::PerspectiveProjection(
            //     camDesc, viewportAspect);
            // auto viewMat = InvertOrthonormalTransform(camDesc._cameraToWorld);
            // auto compViewMat = Camera->ViewMatrix;
            // auto compProjMat = Camera->ProjectionMatrix;
            // (void)compViewMat; (void)compProjMat; (void)proj; (void)viewMat;

            if (IsPicking)
            {// todo: use Directx to draw marque.                
                Graphics^ g = CreateGraphics();
                auto rect = MakeRect(FirstMousePoint, CurrentMousePoint);
                if (rect.Width > 0 && rect.Height > 0) {
                    g->DrawRectangle(s_marqueePen, rect);
                }
                delete g;
            }
        }

        void AddRenderCallback(RenderCallback^ callback)
        {
            _manipulatorOverlay->OnRender += callback;
        }

    protected:
        IList<Object^>^ Pick(MouseEventArgs^ e) override 
        { 
            bool multiSelect = DragOverThreshold;
            if (multiSelect) {
                return gcnew List<Object^>();   // multi-select not supported yet
            }

            Ray3F ray = GetWorldRay(CurrentMousePoint);
            float maxCollisionDistance = 2048.f;

                //  We need to find a list of objects that intersect this ray.
                //  When we find the objects, we can use GameEngine::GetAdapterFromId
                //  to try to match the picked objects to NativeObjectAdapter objects
            
            auto endPt = ray.Origin + maxCollisionDistance * ray.Direction;
            /*auto results =*/ GUILayer::EditorInterfaceUtils::RayIntersection(
                GUILayer::EngineDevice::GetInstance(), _layerControl, _sceneManager,
                ray.Origin.X, ray.Origin.Y, ray.Origin.Z,
                endPt.X, endPt.Y, endPt.Z);

            // if (results) {
            //     for each(auto r in results)
            //     {
            // 
            //     }
            // }
            return gcnew List<Object^>();
        }

        void OnDragEnter(DragEventArgs^ drgevent) override {}
        void OnDragOver(DragEventArgs^ drgevent) override {}
        void OnDragDrop(DragEventArgs^ drgevent) override {}
        void OnDragLeave(EventArgs^ drgevent) override {}

        void OnPaint(PaintEventArgs^ e) override
        {
            try
            {
                if (DesignView->Context == nullptr || GameLoop == nullptr) {
                    e->Graphics->Clear(DesignView->BackColor);
                    return;
                }

                Render();
            }
            catch(Exception^ ex)
            {
                e->Graphics->DrawString(ex->Message, Font, Brushes::Red, 1, 1);
            }            
        }

        void OnResize(System::EventArgs^ e) override
        {
            _layerControl->OnResize(e);
            auto sz = ClientSize;
            if (sz.Width > 0 && sz.Height > 0)
            {
                Camera->Aspect = (float)sz.Width / (float)sz.Height;
            }
            __super::OnResize(e);
        }

        GUILayer::LayerControl^ _layerControl;
        GUILayer::VisCameraSettings^ _cameraSettings;
        GUILayer::EditorSceneManager^ _sceneManager;

    private:
        // IGame^ TargetGame()
        // {
        //     auto selection = Adapters::As<ISelectionContext^>(DesignView->Context);
        //     auto node = selection->GetLastSelected<DomNode^>();
        //               
        //     IReference<IGame^>^ gameref = Adapters::As<IReference<IGame^>^>(node);
        //     if (gameref != nullptr && gameref->Target != nullptr)
        //         return gameref->Target;  
        //               
        //     if(node != nullptr)
        //         return Adapters::As<IGame^>(node->GetRoot()); 
        //     
        //     return Adapters::As<IGame^>(DesignView->Context);
        // }
        
        ManipulatorOverlay^ _manipulatorOverlay;
        
        static System::Drawing::Rectangle MakeRect(Point p1, Point p2)
        {
            int minx = System::Math::Min(p1.X, p2.X);
            int miny = System::Math::Min(p1.Y, p2.Y);
            int maxx = System::Math::Max(p1.X, p2.X);
            int maxy = System::Math::Max(p1.Y, p2.Y);
            int w = maxx - minx;
            int h = maxy - miny;
            return System::Drawing::Rectangle(minx, miny, w, h);
        }

        static System::Drawing::Pen^ s_marqueePen;

        // ref class GameDocRange : IEnumerable<DomNode^> 
        // {
        // private:
        //     ref class GameDocRangeIterator : IEnumerator<DomNode^> 
        //     {
        //     public:
        //         property DomNode^ Current 
        //      {
        //          virtual DomNode^ get()
        //          { 
        //              if (_stage == 0) { return _folderNodeIterator->Current; }
        //              if (_stage == 1) { return _subDocFolder->Current; }
        //              return nullptr;
        //          }
        //      }
        //         property Object^ Current2
        //      {
        //          virtual Object^ get() = System::Collections::IEnumerator::Current::get
        //          {
        //              return Current;
        //          }
        //      };
        //         virtual bool MoveNext() 
        //      { 
        //          if (_stage == 0) {
        //              if (!_folderNodeIterator || !_folderNodeIterator->MoveNext()) {
        //                  _stage = 1;
        //
        //                  _subDocs = _gameDocRegistry->SubDocuments->GetEnumerator();
        //                  if (!_subDocs || !_subDocs->Current) { _stage = 2; return false; }
        //
        //                  for (;;) {
        //                      auto folderNode = Adapters::Cast<DomNode^>(_subDocs->Current);
        //                      _subDocFolder = folderNode->Subtree->GetEnumerator();
        //                      if (_subDocFolder && _subDocFolder->Current) break;
        //
        //                      if (!_subDocs->MoveNext()) { _stage = 2; return false; }
        //                  }
        //              }
        //          } else if (_stage == 1) {
        //              if (_subDocFolder->MoveNext()) { return true; }
        //
        //              for (;;) {
        //                  if (!_subDocs->MoveNext()) { _stage = 2; return false; }
        //                  
        //                  auto folderNode = Adapters::Cast<DomNode^>(_subDocs->Current);
        //                  _subDocFolder = folderNode->Subtree->GetEnumerator();
        //                  if (_subDocFolder && _subDocFolder->Current) break;
        //              }
        //
        //              return true;
        //          }
        //
        //          return false;
        //      }
        //         virtual void Reset() = IEnumerator<DomNode^>::Reset { throw gcnew NotImplementedException(); }
        // 
        //         GameDocRangeIterator()
        //      {
        //          _gameDocRegistry = Globals::MEFContainer->GetExportedValue<IGameDocumentRegistry^>();
        //          _folderNode = Adapters::Cast<DomNode^>(_gameDocRegistry->MasterDocument->RootGameObjectFolder);
        //          _folderNodeIterator = _folderNode->Subtree->GetEnumerator();
        //      }
        //         ~GameDocRangeIterator() {}
        //         !GameDocRangeIterator() {}
        // 
        //     protected:
        //         unsigned _stage;
        //         IEnumerator<DomNode^>^ _folderNodeIterator;
        //         IEnumerator<LevelEditorCore::IGameDocument^>^ _subDocs;
        //         IEnumerator<DomNode^>^ _subDocFolder;
        // 
        //         LevelEditorCore::IGameDocumentRegistry^ _gameDocRegistry;
        //         DomNode^ _folderNode;
        //     };
        // 
        // public:
        //     virtual IEnumerator<DomNode^>^ GetEnumerator() { return gcnew GameDocRangeIterator(); }
        //     virtual System::Collections::IEnumerator^ GetEnumerator2() = System::Collections::IEnumerable::GetEnumerator
        //     { return GetEnumerator(); }
        // };
        // property IEnumerable<DomNode^>^ Items
        // {
        //     IEnumerable<DomNode^>^ get()
        //     {
        //             // C# version of this just uses "yield"... which makes it much easier
        //         return gcnew GameDocRange();
        //     }
        // }
    };


    void ManipulatorOverlay::RenderToScene(
        RenderCore::IThreadContext* device, 
        SceneEngine::LightingParserContext& parserContext)
    {
        using namespace LevelEditorCore;
        NativeDesignControl::s_currentParsingContext = &parserContext;
            
        try
        {
            // auto game = Sce::Atf::Adaptation::Adapters::As<IGame^>(_designView->Context);
            // GridRenderer gridRender = game->Grid->Cast<GridRenderer>();
            // gridRender.Render(_viewControl->Camera);
            OnRender(_designView, _viewControl->Camera);

            // GameEngine::SetRendererFlag(RenderingInterop::BasicRendererFlags::Foreground | RenderingInterop::BasicRendererFlags::Lit);
            if (_designView->Manipulator != nullptr)
                _designView->Manipulator->Render(_viewControl);
        }
        catch (...)
        {
            NativeDesignControl::s_currentParsingContext = nullptr;
            throw;
        }

        NativeDesignControl::s_currentParsingContext = nullptr;
    }
}

