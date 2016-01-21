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
using LevelEditorXLE.Extensions;

namespace LevelEditorXLE.Terrain
{
    class TerrainMaterial : DomNodeAdapter, IExportable
    {
        public string ExportDirectory
        {
            get 
            {
                var game = DomNode.GetRoot().As<Game.GameExtensions>();
                if (game != null) return game.ExportDirectory;
                var rootTerrain = DomNode.Parent.As<XLETerrainGob>();
                if (rootTerrain != null) return rootTerrain.CellsDirectory;
                return "";
            }
        }

        #region IExportable
        public string ExportTarget
        {
            get { return ExportDirectory + "TerrainMaterial.cfg"; }
        }

        public string ExportCategory
        {
            get { return "Terrain"; }
        }

        public IEnumerable<PendingExport> BuildPendingExports()
        {
            var result = new List<PendingExport>();
            result.Add(new PendingExport(ExportTarget, this.GetSceneManager().ExportTerrainMaterialData()));
            return result;
        }
        #endregion
    }
}
