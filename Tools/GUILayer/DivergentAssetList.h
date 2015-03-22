// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "EngineDevice.h"

namespace Assets { class AssetSetManager; class UndoQueue; }

namespace GUILayer
{
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

        DivergentAssetList(EngineDevice^ engine);

    protected:
        Assets::AssetSetManager* _assetSets;
        Assets::UndoQueue* _undoQueue;
    };
}
