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
    using TerrainST = Schema.terrainType;

    class XLETerrainGob : DomNodeAdapter, IListable, ICommandClient, IContextMenuCommandProvider
    {
        public XLETerrainGob() {}
        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = "Terrain";
        }

        public static DomNode Create()
        {
            return new DomNode(TerrainST.Type);
        }


        #region ICommandClient Members
        bool ICommandClient.CanDoCommand(object commandTag)
        {
            if (commandTag is Command)
            {
                switch ((Command)commandTag)
                {
                    case Command.CreateBaseTexture:
                        return DomNode.GetChild(TerrainST.baseTextureChild) == null;
                }
            }
            return false;
        }

        void ICommandClient.DoCommand(object commandTag)
        {
            if (!(commandTag is Command)) return;

            switch ((Command)commandTag)
            {
                case Command.CreateBaseTexture:
                    {
                        if (DomNode.GetChild(TerrainST.baseTextureChild) == null)
                        {
                            DomNode.SetChild(
                                TerrainST.baseTextureChild,
                                new DomNode(Schema.terrainBaseTextureType.Type));
                        }
                        break;
                    }
            }
        }

        void ICommandClient.UpdateCommand(object commandTag, CommandState commandState)
        { }
        #endregion

        private enum Command
        {
            [Description("Create Base Texture")]
            CreateBaseTexture
        }

        IEnumerable<object> IContextMenuCommandProvider.GetCommands(object context, object target)
        {
            foreach (Command command in Enum.GetValues(typeof(Command)))
            {
                yield return command;
            }
        }
    }
}
