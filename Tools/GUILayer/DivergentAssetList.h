// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "EngineDevice.h"
#include "../../Core/Types.h"

using namespace System;
using namespace System::Collections::Generic;

namespace Assets { class AssetSetManager; class UndoQueue; class IAssetSet; }

namespace GUILayer
{
    public ref class PendingSaveList
    {
    public:
        ref class Entry
        {
        public:
            String^ _filename;
            array<Byte>^ _oldFileData;
            array<Byte>^ _newFileData;
            bool _saveQueued;

            Entry() { _saveQueued = false; }
            Entry(String^ filename, array<Byte>^ oldFileData, array<Byte>^ newFileData)
                : _filename(filename), _oldFileData(oldFileData), _newFileData(newFileData), _saveQueued(true) {}
        };

        void Add(const Assets::IAssetSet& set, uint64 id, Entry^ entry);
        Entry^ GetEntry(const Assets::IAssetSet& set, uint64 id);

		ref class CommitResult
		{
		public:
			property String^ ErrorMessages;
		};

		CommitResult^ Commit();

        PendingSaveList();
        ~PendingSaveList();

        static PendingSaveList^ Create();
        static bool HasModifiedAssets();
    internal:
        value class E
        {
        public:
            const Assets::IAssetSet* _assetSet;
            uint64 _id;
            Entry^ _entry;
        };
        
        List<E>^   _entries;
    };

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
        Assets::AssetSetManager* _assetSets;
        Assets::UndoQueue* _undoQueue;
        PendingSaveList^ _saveList;
    };
}
