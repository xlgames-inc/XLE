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
            // Prefer to return the SceneManager object associated
            // with the root game document. If we can't find one, fall
            // back to the GlobalSceneManager
            var root = adapter.DomNode.GetRoot();
            if (root != null)
            {
                var gameExt = root.As<Game.GameExtensions>();
                if (gameExt != null)
                {
                    var man = gameExt.SceneManager;
                    if (man != null)
                        return man;
                }
            }
            return XLEBridgeUtils.Utils.GlobalSceneManager;
        }

        internal static Uri GetBaseExportUri(this Sce.Atf.Dom.DomNodeAdapter adapter)
        {
            var root = adapter.DomNode.GetRoot();
            var game = root.As<Game.GameExtensions>();
            if (game != null) return game.ExportDirectory;
            var resource = root.As<Sce.Atf.IResource>();
            if (resource != null) return resource.Uri;
            return new Uri(System.IO.Directory.GetCurrentDirectory().TrimEnd('\\') + "\\");
        }
    }
}
