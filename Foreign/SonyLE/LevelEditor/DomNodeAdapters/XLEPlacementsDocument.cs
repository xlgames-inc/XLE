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
    public class XLEPlacementDocument : DomDocument, IListable, IHierarchical, IGameDocument, IGameObjectFolder
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
                GetChildList<XLEPlacementObject>(Schema.placementsDocumentType.placementChild).Add(placement);
                return true;
            }
            return false;
        }
        #endregion
        #region INameable Members

        /// <summary>
        /// Gets and sets the name</summary>
        public string Name
        {
            get { return GetAttribute<string>(Schema.placementsDocumentType.nameAttribute); }
            set { SetAttribute(Schema.placementsDocumentType.nameAttribute, value); }
        }

        #endregion
        #region IVisible Members
        public bool Visible
        {
            get { return true; }
            set { }
        }
        #endregion
        #region ILockable Members
        public bool IsLocked
        {
            get
            {
                ILockable lockable = GetParentAs<ILockable>();
                return (lockable != null) ? lockable.IsLocked : false;
            }
            set { SetAttribute(Schema.gameObjectFolderType.lockedAttribute, value); }
        }
        #endregion
        #region IGameObjectFolder Members

        /// <summary>
        /// Gets the list of game objects</summary>
        public IList<IGameObject> GameObjects
        {
            get { return GetChildList<IGameObject>(Schema.placementsDocumentType.placementChild); }
        }

        /// <summary>
        /// Gets the list of child game object folders</summary>
        public IList<IGameObjectFolder> GameObjectFolders
        {
            get { return null; }
        }
        #endregion
        #region IGameDocument Members
            //  XLEPlacementDocument is derived from IGameDocument for the convenience of using the GameDocumentRegistry
            //  But there are some properties that aren't valid for this type
        public IGameObjectFolder RootGameObjectFolder { get { return this; } }
        public bool IsMasterGameDocument { get { return false; } }
        public IEnumerable<IReference<IGameDocument>> GameDocumentReferences { get { return null; } }

        public event EventHandler<ItemChangedEventArgs<IEditableResourceOwner>> EditableResourceOwnerDirtyChanged = delegate { };
        public void NotifyEditableResourceOwnerDirtyChanged(IEditableResourceOwner resOwner)
        {
            EditableResourceOwnerDirtyChanged(this, new ItemChangedEventArgs<IEditableResourceOwner>(resOwner));
        }
        #endregion

        internal static GameDocumentRegistry GetDocRegistry()
        {
                //  There are some problems related to using a document registry for
                //  these placement documents. Using a registry allow us to have multiple
                //  references to the same document... But in the case of placement cells, that
                //  isn't normal. We may get a better result by just creating and destroying
                //  the document for every reference
            return Globals.MEFContainer.GetExportedValue<GameDocumentRegistry>();
        }

        public static XLEPlacementDocument OpenOrCreate(Uri uri, SchemaLoader schemaLoader)
        {
            if (!uri.IsAbsoluteUri)
                return null;

            var docRegistry = GetDocRegistry();
            if (docRegistry != null)
            {
                var existing = docRegistry.FindDocument(uri);
                if (existing != null) return null;      // prevent a second reference here
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
                gameDocRegistry.Remove(doc.As<IGameDocument>());
            } 
            else
            {
                // doc.Dispose();
            }
        }

        public override string Type { get { return GameEditor.s_placementDocInfo.FileType; } }
    }
}


