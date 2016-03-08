// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

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

namespace LevelEditorXLE.Placements
{
    public class XLEPlacementDocument 
        : DomDocument, 
            IGameDocument, IGameObjectFolder, IEnumerableContext,
            IHierarchical, INameable, IListable
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
            return child.Is<XLEPlacementObject>() || child.Is<IPrefabInstance>() || child.Is<ITransformableGroup>();
        }

        public bool AddChild(object child)
        {
            var placement = child.As<XLEPlacementObject>();
            if (placement != null)
            {
                GetChildList<XLEPlacementObject>(Schema.placementsDocumentType.placementChild).Add(placement);
                return true;
            }

            // we can also add other "transformable" objects (such as a group or a IPrefabInstance)
            var node = child.As<DomNode>();
            if (node != null && node.Type.Lineage.FirstOrDefault(t => t == Schema.transformObjectType.Type) != null)
            {
                GetChildList<DomNode>(Schema.placementsDocumentType.placementChild).Add(node);
                return true;
            }
            return false;
        }
        #endregion
        #region INameable Members
        public string Name
        {
            get { return GetAttribute<string>(Schema.placementsDocumentType.nameAttribute); }
            set { SetAttribute(Schema.placementsDocumentType.nameAttribute, value); }
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

        public ITransformableGroup CreateGroup()
        {
            var gobGroup = new DomNode(Schema.transformObjectGroupType.Type).As<ITransformableGroup>();
            // gobGroup.Name = "Group".Localize("this is the name of a folder in the project lister");
            return gobGroup;
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

        public static void SaveDomDocument(DomNode node, Uri uri, ISchemaLoader schemaLoader)
        {
            string filePath = uri.LocalPath;
            Directory.CreateDirectory(new FileInfo(filePath).Directory.FullName);       // (make sure the directory exists)
            FileMode fileMode = File.Exists(filePath) ? FileMode.Truncate : FileMode.OpenOrCreate;
            using (FileStream stream = new FileStream(filePath, fileMode))
            {
                // note --  "LevelEditor" project has a ComstDomXmlWriter object that contains some
                //          special case code for certain node types. We're just using the default
                //          DomXmlWriter, so we won't benefit from that special behaviour.
                var writer = new Sce.Atf.Dom.DomXmlWriter(schemaLoader.TypeCollection);
                writer.Write(node, stream, uri);
            }
        }

        public virtual void Save(Uri uri, ISchemaLoader schemaLoader)
        {
            if (Dirty || m_uriChanged)
            {
                SaveDomDocument(DomNode, uri, schemaLoader);
                m_uriChanged = false;
            }

            Dirty = false;
        }
        #endregion
        #region IEnumerableContext Members
        public IEnumerable<object> Items
        {
            get
            {
                // note -- basically identical to GameContext::Items
                var pendingChildren = new Queue<DomNode>(DomNode.Children);
                while (pendingChildren.Count != 0)
                {
                    var childNode = pendingChildren.Dequeue();
                    var e = childNode.As<IEnumerableContext>();
                    if (e != null)
                    {
                        foreach (var o in e.Items)
                            yield return o;
                    }
                    else
                    {
                        yield return Util.AdaptDomPath(childNode);
                        foreach (var c in childNode.Children)
                            pendingChildren.Enqueue(c);
                    }
                }

            }
        }
        #endregion

        internal static IGameDocumentRegistry GetDocRegistry()
        {
                //  There are some problems related to using a document registry for
                //  these placement documents. Using a registry allow us to have multiple
                //  references to the same document... But in the case of placement cells, that
                //  isn't normal. We may get a better result by just creating and destroying
                //  the document for every reference
            return Globals.MEFContainer.GetExportedValue<IGameDocumentRegistry>();
        }

        public static XLEPlacementDocument OpenOrCreate(Uri uri, ISchemaLoader schemaLoader)
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

            bool createdNewFile = false;
            DomNode rootNode = null;
            if (File.Exists(filePath))
            {
                // read existing document using custom dom XML reader
                using (FileStream stream = File.OpenRead(filePath))
                {
                    // Note --  Sony code uses "CustomDomXmlReader" to modify
                    //          the urls of relative assets in reference types at
                    //          load time.
                    //          However, we're going to prefer a method that does this
                    //          on demand, rather than at load time -- so we should be able
                    //          to use a standard xml reader.
                    // var reader = new CustomDomXmlReader(Globals.ResourceRoot, schemaLoader);
                    var reader = new DomXmlReader(schemaLoader as XmlSchemaTypeLoader);
                    rootNode = reader.Read(stream, uri);
                }
            }
            else
            {
                // create new document by creating a Dom node of the root type defined by the schema                 
                rootNode = new DomNode(Schema.placementsDocumentType.Type, Schema.placementsDocumentRootElement);
                createdNewFile = true;
            }

            var doc = rootNode.As<XLEPlacementDocument>();
            doc.Uri = uri;

            // Initialize Dom extensions now that the data is complete
            rootNode.InitializeExtensions();

            // sync all the prefab instances
            {
                UniqueNamer uniqueNamer = new UniqueNamer('_');
                foreach (var prefab in doc.As<DomNode>().Subtree.AsIEnumerable<IPrefabInstance>())
                    prefab.Resolve(uniqueNamer);
            }

            if (docRegistry != null) docRegistry.Add(doc);
            doc.Dirty = createdNewFile;
            return doc;
        }

        public static XLEPlacementDocument OpenUntitled()
        {
            var rootNode = new DomNode(Schema.placementsDocumentType.Type, Schema.placementsDocumentRootElement);

            var doc = rootNode.As<XLEPlacementDocument>();

            // Initialize Dom extensions now that the data is complete
            rootNode.InitializeExtensions();

            var docRegistry = GetDocRegistry(); 
            if (docRegistry != null) docRegistry.Add(doc);
            doc.Dirty = true;
            return doc;
        }

        private void DomNode_AttributeChanged(object sender, AttributeEventArgs e) { Dirty = true; }
        private void DomNode_ChildInserted(object sender, ChildEventArgs e) { Dirty = true; }
        private void DomNode_ChildRemoved(object sender, ChildEventArgs e) { Dirty = true; }

        public static void Release(XLEPlacementDocument doc)
        {
            // Multiple references to the same document might turn out to be awkward in C#.
            // We need strict reference counting to do this properly. But how do we do that 
            // in C#? Perhaps with an extra utility class? We need to drop references in many cases:
            //  * close master document
            //  * delete cell ref
            //  * removal of parent from the tree
            //  * change uri
            // It might turn out difficult to catch all of those cases reliably.
            // It's simplier just to demand that a given document can only be referenced once.
            var gameDocRegistry = GetDocRegistry();
            if (gameDocRegistry != null)
                gameDocRegistry.Remove(doc.As<IGameDocument>());
        }

        public override string Type { get { return s_placementDocInfo.FileType; } }

        public static readonly DocumentClientInfo s_placementDocInfo = new DocumentClientInfo(
            "PlacementDocument",
            new string[] { ".plcdoc" },
            Sce.Atf.Resources.DocumentImage,
            Sce.Atf.Resources.FolderImage,
            false);

        protected override void OnNodeSet()
        {
            base.OnNodeSet();
            DomNode.AttributeChanged += DomNode_AttributeChanged;
            DomNode.ChildInserted += DomNode_ChildInserted;
            DomNode.ChildRemoved += DomNode_ChildRemoved;

            UriChanged += delegate { m_uriChanged = true; };
        }

        private bool m_uriChanged;
    }
}


