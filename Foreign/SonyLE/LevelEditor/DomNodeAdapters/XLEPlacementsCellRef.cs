//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.


using System;
using System.ComponentModel.Composition;
using System.Collections.Generic;
using System.IO;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;
using Sce.Atf.VectorMath;

using LevelEditorCore;

namespace LevelEditor.DomNodeAdapters
{
    interface IResolveable
    {
        void Resolve();
        void Unresolve();
        bool IsResolved();
    }

    public class GenericReference<T> : DomNodeAdapter, IReference<T>, IListable
    {
        #region IReference<T> Members

        bool IReference<T>.CanReference(T item)
        {
            return false;
        }

        T IReference<T>.Target
        {
            get { return m_target; }
            set { throw new InvalidOperationException("Target cannot be set"); }
        }

        #endregion

        #region IListable Members

        /// <summary>
        /// Provides info for the ProjectLister tree view and other controls</summary>
        /// <param name="info">Item info passed in and modified by the method</param>
        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = (m_target != null)
                                  ? info.GetImageList().Images.IndexOfKey(LevelEditorCore.Resources.FolderRefImage)
                                  : info.GetImageList().Images.IndexOfKey(
                                      LevelEditorCore.Resources.MissingFolderRefImage);
            IDocument gameDoc = Adapters.As<IDocument>(m_target);

            string name = GetAttribute<string>(s_nameAttribute);
            if (name == null) name = string.Empty;
            if (gameDoc != null && gameDoc.Dirty)
                name += "*";

            if (m_target == null && !string.IsNullOrEmpty(m_error))
            {
                name += " [ Error: " + m_error + " ]";
            }

            info.Label = name;
            info.IsLeaf = m_target == null;
        }

        #endregion

        protected string m_error = string.Empty;
        protected T m_target = default(T);
        protected static AttributeInfo s_nameAttribute;
    }

    public class PlacementsCellRef : GenericReference<XLEPlacementDocument>, IHierarchical, IResolveable
    {
        static PlacementsCellRef() { s_nameAttribute = Schema.placementsCellReferenceType.nameAttribute; }

        /// <summary>
        /// Gets absolute uri of the target GameDocument.</summary>
        public Uri Uri
        {
            get { return GetAttribute<Uri>(Schema.placementsCellReferenceType.refAttribute); }
        }

        public virtual void Resolve()
        {
            if (m_target == null)
            {
                Uri ur = Uri;
                if (ur == null) { m_error = "ref attribute is null"; }
                else if (!File.Exists(ur.LocalPath)) { m_error = "File not found: " + ur.LocalPath; }
                else
                {
                    SchemaLoader schemaloader = Globals.MEFContainer.GetExportedValue<SchemaLoader>();
                    m_target = XLEPlacementDocument.OpenOrCreate(ur, schemaloader);

                        //  We can use "SubscribeToEvents" to feed changes to the sub-tree
                        //  into the tree that this cell belongs to.
                        //  This is important because the "GameContext" (which is attached to
                        //  the root node of the main document) needs to recieve change events.
                        //  Even though the placement document is a separate document, we still
                        //  need to use the same single selection and history context
                    // m_target.DomNode.SubscribeToEvents(DomNode);
                }
            }            
        }
        public virtual void Unresolve()
        {
            if (m_target != null)
            {
                m_target.DomNode.UnsubscribeFromEvents(DomNode);
                XLEPlacementDocument.Release(m_target);
                m_target = null;
                m_error = "Not resolved";
            }
        }
        public virtual bool IsResolved() { return m_target != null; }

        public Vec3F Mins
        {
            get { return new Vec3F(GetAttribute<float[]>(Schema.placementsCellReferenceType.minsAttribute)); }
        }
        public Vec3F Maxs
        {
            get { return new Vec3F(GetAttribute<float[]>(Schema.placementsCellReferenceType.maxsAttribute)); }
        }
        public XLEPlacementDocument Target
        {
            get { return m_target; }
        }
    
        public bool CanAddChild(object child) { return (m_target != null) && m_target.CanAddChild(child); }
        public bool AddChild(object child) { return (m_target != null) && m_target.AddChild(child); }
    }

    public class PlacementsFolder : DomNodeAdapter, IListable
    {
        public static DomNode CreateNew()
        {
            var doc = new DomNode(Schema.placementsFolderType.Type);

            var pref = new DomNode(Schema.placementsCellReferenceType.Type);
            pref.SetAttribute(Schema.placementsCellReferenceType.refAttribute, "game/demworld/p035_020.plcdoc");
            pref.SetAttribute(Schema.placementsCellReferenceType.nameAttribute, "35-20");
            doc.GetChildList(Schema.placementsFolderType.cellChild).Add(pref);
            return doc;
        }

        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = "Placements";
        }

        public System.Collections.Generic.IEnumerable<PlacementsCellRef> Cells
        {
            get
            {
                return GetChildList<PlacementsCellRef>(Schema.placementsFolderType.cellChild);
            }
        }
    }
}

