// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using Sce.Atf.Adaptation;

namespace LevelEditorXLE.Extensions
{
    internal static class ExtensionsClass
    {
        internal static GUILayer.EditorSceneManager GetSceneManager(this Sce.Atf.Dom.DomNodeAdapter adapter)
        {
            var root = adapter.DomNode.GetRoot();
            System.Diagnostics.Debug.Assert(root != null);
            if (root == null) return null;
            var gameExt = root.As<Game.GameExtensions>();
            System.Diagnostics.Debug.Assert(gameExt != null);
            if (gameExt == null) return null;
            return gameExt.SceneManager;
        }
    }
}
