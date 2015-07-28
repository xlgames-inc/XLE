// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.ComponentModel;

using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;

using LevelEditorCore;

namespace LevelEditorXLE.Terrain
{
    class TerrainMaterial : DomNodeAdapter, IExportable
    {
        public string CellsDirectory
        {
            get 
            {
                var rootTerrain = DomNode.Parent.As<XLETerrainGob>();
                if (rootTerrain==null) return "";
                return rootTerrain.CellsDirectory;
            }
        }

        #region IExportable
        public string ExportTarget
        {
            get { return CellsDirectory + "/TerrainTextures/Textures.txt"; }
        }

        public string ExportCategory
        {
            get { return "Terrain"; }
        }

        public PendingExports BuildPendingExports()
        {
            var sceneMan = XLEBridgeUtils.NativeManipulatorLayer.SceneManager;
            List<GUILayer.EditorSceneManager.PendingExport> result;
            result.Add(
                Tuple.Create(ExportTarget, sceneMan.ExportTerrainMaterialData())));
            return result;
        }
        #endregion
    }
}
