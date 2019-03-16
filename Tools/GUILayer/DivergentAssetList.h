// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

using namespace System::Collections::Generic;

namespace Assets { class AssetSetManager; class IDefaultAssetHeap; }

namespace GUILayer
{
    public ref class PendingSaveList
    {
    public:
		enum class Action { Save, Abandon, Ignore };

        ref class Entry
        {
        public:
            System::String^ _filename;
            array<System::Byte>^ _oldFileData;
            array<System::Byte>^ _newFileData;
			Action _action;

            Entry() { _action = Action::Ignore; }
            Entry(System::String^ filename, array<System::Byte>^ oldFileData, array<System::Byte>^ newFileData)
                : _filename(filename), _oldFileData(oldFileData), _newFileData(newFileData), _action(Action::Save) {}
        };

        void Add(uint64_t typeCode, uint64_t id, Entry^ entry);
        Entry^ GetEntry(uint64_t typeCode, uint64_t id);

		ref class CommitResult
		{
		public:
			property System::String^ ErrorMessages;
		};

		virtual CommitResult^ Commit();

        PendingSaveList();
        ~PendingSaveList();

        static PendingSaveList^ Create();
        static bool HasModifiedAssets();
    internal:
        value class E
        {
        public:
            uint64_t _typeCode;
			uint64_t _id;
            Entry^ _entry;
        };
        
        List<E>^   _entries;
    };

	ref class EngineDevice;

    public ref class DivergentAssetList : public Aga::Controls::Tree::ITreeModel
    {
    public:
        typedef Aga::Controls::Tree::TreePath TreePath;
        typedef Aga::Controls::Tree::TreeModelEventArgs TreeModelEventArgs;
        typedef Aga::Controls::Tree::TreePathEventArgs TreePathEventArgs;

        virtual System::Collections::IEnumerable^ GetChildren(TreePath^ treePath);
        virtual bool IsLeaf(TreePath^ treePath);

        virtual event System::EventHandler<TreeModelEventArgs^>^ NodesChanged;
        virtual event System::EventHandler<TreeModelEventArgs^>^ NodesInserted;
        virtual event System::EventHandler<TreeModelEventArgs^>^ NodesRemoved;
        virtual event System::EventHandler<TreePathEventArgs^>^ StructureChanged;

        DivergentAssetList(EngineDevice^ engine, PendingSaveList^ saveList);
        ~DivergentAssetList();

    protected:
        ::Assets::AssetSetManager* _assetSets;
        PendingSaveList^ _saveList;
    };
}
