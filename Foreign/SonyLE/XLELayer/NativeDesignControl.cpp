// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

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
    public ref class NativeDesignControl : public DesignViewControl
    {
    public:
        NativeDesignControl(LevelEditorCore::DesignView^ designView)
        : DesignViewControl(designView)
        {}

        ~NativeDesignControl() {}
        !NativeDesignControl() {}

        void Render() override {}

    protected:
        IList<Object^>^ Pick(MouseEventArgs^ e) override { return gcnew List<Object^>(); }
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

    private:
        IGame^ TargetGame()
        {
            auto selection = Adapters::As<ISelectionContext^>(DesignView->Context);
            auto node = selection->GetLastSelected<DomNode^>();
                      
            IReference<IGame^>^ gameref = Adapters::As<IReference<IGame^>^>(node);
            if (gameref != nullptr && gameref->Target != nullptr)
                return gameref->Target;  
                      
            if(node != nullptr)
                return Adapters::As<IGame^>(node->GetRoot()); 
            
            return Adapters::As<IGame^>(DesignView->Context);
        }
        
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
}

