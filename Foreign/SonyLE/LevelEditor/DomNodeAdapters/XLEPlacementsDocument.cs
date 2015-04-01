using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Linq;
using System.Text;
using System.IO;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;

using LevelEditorCore;

namespace LevelEditor.DomNodeAdapters
{
    [Export(typeof(GenericDocumentRegistry))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class GenericDocumentRegistry
    {
        public T FindDocument<T>(Uri ur) where T : class
        {
            foreach (var doc in m_documents)
                if (doc.Uri == ur)
                {
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

    public class XLEPlacementDocument : DomDocument, IListable, IHierarchical
    {
        #region IListable Members
        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = "Placements";
        }
        #endregion
        #region IHierarchical Members
        public bool CanAddChild(object child)
        {
            return child.Is<XLEPlacementObject>();
        }

        public bool AddChild(object child)
        {
            var placement = child.As<XLEPlacementObject>();
            if (placement != null)
            {
                GetChildList<XLEPlacementObject>(Schema.placementsDocumentType.placementsChild).Add(placement);
                return true;
            }
            return false;
        }
        #endregion

        internal static GenericDocumentRegistry GetDocRegistry()
        {
                //  There are some problems related to using a document registry for
                //  these placement documents. Using a registry allow us to have multiple
                //  references to the same document... But in the case of placement cells, that
                //  isn't normal. We may get a better result by just creating and destroying
                //  the document for every reference
            // return Globals.MEFContainer.GetExportedValue<GenericDocumentRegistry>();
            return null;
        }

        public static XLEPlacementDocument OpenOrCreate(Uri uri, SchemaLoader schemaLoader)
        {
            if (!uri.IsAbsoluteUri)
                return null;

            var docRegistry = GetDocRegistry();
            if (docRegistry != null)
            {
                var existing = docRegistry.FindDocument<XLEPlacementDocument>(uri);
                if (existing != null) return existing;
            }

            string filePath = uri.LocalPath;

            DomNode rootNode = null;
            if (File.Exists(filePath))
            {
                // read existing document using custom dom XML reader
                using (FileStream stream = File.OpenRead(filePath))
                {
                    var reader = new CustomDomXmlReader(Globals.ResourceRoot, schemaLoader);
                    rootNode = reader.Read(stream, uri);
                }
            }
            else
            {
                // create new document by creating a Dom node of the root type defined by the schema                 
                rootNode = new DomNode(Schema.placementsDocumentType.Type, Schema.placementsDocumentRootElement);
            }

            var doc = rootNode.As<XLEPlacementDocument>();
            doc.Uri = uri;

            // Initialize Dom extensions now that the data is complete
            rootNode.InitializeExtensions();

            if (docRegistry!=null) docRegistry.Add(doc);
            doc.Dirty = false;
            return doc;
        }

        public static void Release(XLEPlacementDocument doc)
        {
            // We can't remove, because a single document can have multiple references upon it
            // We need strict reference counting to do this properly. But how do we do that 
            // in C#? We need to drop references in many cases:
            //  * close master document
            //  * delete cell ref
            //  * removal of parent from the tree
            //  * change uri
            // It might turn out difficult to catch all of those cases reliably
            var gameDocRegistry = GetDocRegistry();
            if (gameDocRegistry != null)
            {
                gameDocRegistry.Remove(doc.As<IDocument>());
            } 
            else
            {
                // doc.Dispose();
            }
        }

        public override string Type { get { return GameEditor.s_placementDocInfo.FileType; } }
    }
}


