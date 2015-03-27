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

    [Export(typeof(GenericDocumentRegistry))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class GenericDocumentRegistry
    {
        public T FindDocument<T>(Uri ur) where T : class
        {
            foreach (var doc in m_documents)
                if (doc.Uri == ur) {
                    var adapt = doc as IAdaptable;
                    return doc.As<T>();
                }
            return default(T);
        }

        public bool Contains(IDocument doc)
        {
            return m_documents.Contains(doc);
        }

        public void Add(IDocument doc)
        {
            if (doc == null)
                throw new ArgumentNullException("doc");

            if (!m_documents.Contains(doc))
            {
                m_documents.Add(doc);
                doc.DirtyChanged += OnDocumentDirtyChanged;
                doc.UriChanged += OnDocumentUriChanged;
                DocumentAdded(this, new ItemInsertedEventArgs<IDocument>(m_documents.Count - 1, doc));
            }
        }

        public void Remove(IDocument doc)
        {
            if (doc == null || !m_documents.Contains(doc))
                return;

            m_documents.Remove(doc);
            doc.DirtyChanged -= OnDocumentDirtyChanged;
            doc.UriChanged -= OnDocumentUriChanged;
            DocumentRemoved(this, new ItemRemovedEventArgs<IDocument>(0, doc));
        }

        public event EventHandler<ItemInsertedEventArgs<IDocument>> DocumentAdded = delegate { };
        public event EventHandler<ItemRemovedEventArgs<IDocument>> DocumentRemoved = delegate { };
        public event EventHandler<ItemChangedEventArgs<IDocument>> DocumentDirtyChanged = delegate { };
        public event EventHandler<ItemChangedEventArgs<IDocument>> DocumentUriChanged = delegate { };

        public void Clear()
        {
            for (int i = m_documents.Count - 1; i >= 0; i--)
            {
                var doc = m_documents[i];
                m_documents.RemoveAt(i);
                DocumentRemoved(this, new ItemRemovedEventArgs<IDocument>(i, doc));
            }
        }

        private void OnDocumentDirtyChanged(object sender, EventArgs e)
        {
            DocumentDirtyChanged(this, new ItemChangedEventArgs<IDocument>((IDocument)sender));
        }

        private void OnDocumentUriChanged(object sender, UriChangedEventArgs e)
        {
            DocumentUriChanged(this, new ItemChangedEventArgs<IDocument>((IDocument)sender));
        }

        private readonly List<IDocument> m_documents = new List<IDocument>();
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
                }
            }            
        }
        public virtual void Unresolve()
        {
            if (m_target != null)
            {
                var gameDocRegistry = Globals.MEFContainer.GetExportedValue<GenericDocumentRegistry>();
                    // We can't remove, because a single document can have multiple references upon it
                    // We need strict reference counting to do this properly. But how do we do that 
                    // in C#? We need to drop references in many cases:
                    //  * close master document
                    //  * delete cell ref
                    //  * removal of parent from the tree
                    //  * change uri
                    // It might turn out difficult to catch all of those cases reliably
                // gameDocRegistry.Remove(m_target.As<IDocument>()); 
                m_target = null;
                m_error = "Not resolved";
            }
        }
        public virtual bool IsResolved() { return m_target != null; }

        public Vec3F Mins
        {
            get { return GetAttribute<Vec3F>(Schema.placementsCellReferenceType.minsAttribute); }
        }
        public Vec3F Maxs
        {
            get { return GetAttribute<Vec3F>(Schema.placementsCellReferenceType.maxsAttribute); }
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

