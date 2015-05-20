//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.


using System;
using System.ComponentModel.Composition;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;
using Sce.Atf.VectorMath;

using LevelEditorCore;

using LevelEditor.DomNodeAdapters.Extensions;

namespace LevelEditor.DomNodeAdapters
{
    using CellRefST = Schema.placementsCellReferenceType;
    using FolderST = Schema.placementsFolderType;

    public class PlacementsCellRef : GenericReference<XLEPlacementDocument>, IHierarchical
    {
        public Vec3F Mins
        {
            get { return this.GetVec3(CellRefST.minsAttribute); }
        }

        public Vec3F Maxs
        {
            get { return this.GetVec3(CellRefST.maxsAttribute); }
        }

        #region IHierachical Members
        public bool CanAddChild(object child) { return (m_target != null) && m_target.CanAddChild(child); }
        public bool AddChild(object child) { return (m_target != null) && m_target.AddChild(child); }
        #endregion

        #region GenericReference<> Members
        static PlacementsCellRef()
        {
            s_nameAttribute = CellRefST.nameAttribute;
            s_refAttribute = CellRefST.refAttribute;
        }

        protected override XLEPlacementDocument Attach(Uri uri)
        {
            return XLEPlacementDocument.OpenOrCreate(
                uri, Globals.MEFContainer.GetExportedValue<SchemaLoader>());
        }

        protected override void Detach(XLEPlacementDocument target)
        {
            XLEPlacementDocument.Release(target);
        }
        #endregion
    }

    public interface IDomNodeCommandClient : ICommandClient
    {
        void RegisterCommand(ICommandService service, ICommandClient parentClient);
        bool OwnsCommand(object command);
    }

    public class PlacementsFolder : DomNodeAdapter, IListable, ICommandClient, IContextMenuCommandProvider
    {
        public static DomNode CreateNew()
        {
            return new DomNode(FolderST.Type);
        }

        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = "Placements";
        }

        public System.Collections.Generic.IEnumerable<PlacementsCellRef> Cells
        {
            get { return GetChildList<PlacementsCellRef>(FolderST.cellChild); }
        }

        #region ICommandClient Members
        bool ICommandClient.CanDoCommand(object commandTag)
        {
            return commandTag is Command;
        }

        void ICommandClient.DoCommand(object commandTag)
        {
            if (!(commandTag is Command)) return;

            switch ((Command)commandTag)
            {
            case Command.Configure:
                {
                        // open the configuration dialog
                    break;
                }
            }
        }

        void ICommandClient.UpdateCommand(object commandTag, CommandState commandState)
        {}
        #endregion

        private enum Command
        {
            [Description("Configure Placements...")]
            Configure
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

