//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Windows.Forms;
using System.IO;
using System.Linq;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;
using Sce.Atf.VectorMath;

using LevelEditorCore;
using LevelEditorCore.VectorMath;

using LevelEditor.DomNodeAdapters;

namespace LevelEditor
{
    /// <summary>
    /// Prefab service</summary>
    /// <remarks>
    /// There are some common code between this class and 
    /// PrototypingService class need to be factored out</remarks>
    [Export(typeof(IInitializable))]
    [Export(typeof(IResourceResolver))]
    [Export(typeof(IResourceConverter))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class PrefabService : IResourceConverter, IResourceResolver, ICommandClient, IInitializable
    {
        #region IInitializable Members

        void IInitializable.Initialize()
        {
            m_commandService.RegisterCommand(
               Commands.CreatePrefab,
               StandardMenu.File,
               StandardCommandGroup.FileSave,
               "Create Prefab ...".Localize(),
               "Create prefab from selected GameObjects".Localize(),
               Sce.Atf.Input.Keys.None,
               null,
               CommandVisibility.Menu, this);

            if (m_scriptingService != null)
                m_scriptingService.SetVariable(this.GetType().Name, this);            

        }

        #endregion

        #region IResourceConverter Members

        IAdaptable IResourceConverter.Convert(IResource resource)
        {
            Prefab prefab = resource as Prefab;
            if (prefab != null)
            {
                return PrefabInstance.Create(prefab);
            }
            return null;
        }

        #endregion

        #region IResourceResolver Members

        IResource IResourceResolver.Resolve(Uri uri)
        {
            IResource resource = null;
            try
            {

                string fileName = uri.LocalPath;
                string ext = Path.GetExtension(fileName).ToLower();
                if (ext == m_ext)
                {
                    using (Stream stream = File.OpenRead(fileName))
                    {
                        var reader = new CustomDomXmlReader(Globals.ResourceRoot, m_schemaLoader);
                        DomNode node = reader.Read(stream, uri);                        
                        resource = Prefab.Create(node, uri);
                    }
                }
            }
            catch (System.IO.IOException e)
            {
                Outputs.WriteLine(OutputMessageType.Warning, "Could not load resource: " + e.Message);
            }

            return resource;
        }

        #endregion

        #region ICommandClient Members

        bool ICommandClient.CanDoCommand(object commandTag)
        {
            if (commandTag.Equals(Commands.CreatePrefab))
                return SelectedGobs.Any();
            return false;
        }

        void ICommandClient.DoCommand(object commandTag)
        {
            if (!commandTag.Equals(Commands.CreatePrefab))
                return;

            string filePath = Util.GetFilePath(m_fileFilter, Globals.ResourceRoot.LocalPath, true);
            if (!string.IsNullOrEmpty(filePath))
            {
                try
                {
                    // save selected gameobject to a new prototype file.
                    Uri ur = new Uri(filePath);
                    DomNode prefab = CreatePrefab(SelectedGobs);

                    string filePathLocal = ur.LocalPath;
                    FileMode fileMode = File.Exists(filePathLocal) ? FileMode.Truncate : FileMode.OpenOrCreate;
                    using (FileStream stream = new FileStream(filePathLocal, fileMode))
                    {
                        var writer = new CustomDomXmlWriter(Globals.ResourceRoot, m_schemaLoader.TypeCollection);
                        writer.Write(prefab, stream, ur);
                    }
                    m_resourceService.Unload(ur);
                }
                catch (Exception ex)
                {
                    MessageBox.Show(m_mainWindow.DialogOwner, ex.Message);
                }
            }
        }

        void ICommandClient.UpdateCommand(object commandTag, CommandState commandState)
        {
            
        }

        #endregion

        private IEnumerable<object> SelectedGobs
        {
            get
            {
                var selectionContext = m_contextRegistry.GetActiveContext<ISelectionContext>();
                var rootDomNodes = 
                    selectionContext != null 
                    ? DomNode.GetRoots(selectionContext.GetSelection<DomNode>()) 
                    : EmptyArray<DomNode>.Instance;
                return rootDomNodes;
            }
        }

        private DomNode CreatePrefab(IEnumerable<object> gobs)
        {
            UniqueNamer uniqueNamer = new UniqueNamer();
            DomNode[] temp = new DomNode[1];
            var copyList = new List<object>();
            AABB bound = new AABB();
            foreach (var gameObject in SelectedGobs)
            {
                IBoundable boundable = gameObject.As<IBoundable>();
                bound.Extend(boundable.BoundingBox);
                
                var trans = gameObject.As<ITransformable>();
                var world = (trans != null) ? TransformUtils.ComputeWorldTransform(trans) : Matrix4F.Identity;

                temp[0] = gameObject.As<DomNode>();
                DomNode[] copies = DomNode.Copy(temp);
                copies[0].InitializeExtensions();

                var nameable = copies[0].As<INameable>();
                if (nameable != null)
                    nameable.Name = uniqueNamer.Name(nameable.Name);

                var copyTrans = copies[0].As<ITransformable>();
                if (copyTrans != null)
                    TransformUtils.SetTransform(copyTrans, world);
                copyList.Add(copies[0]);
            }

            DomNode prefab = new DomNode(Schema.prefabType.Type, Schema.prefabRootElement);
            var list = prefab.GetChildList(Schema.prefabType.gameObjectChild);
            Vec3F center = bound.Center;
            foreach (var gob in copyList)
            {
                var trans = gob.As<ITransformable>();
                if (trans != null)
                {
                    trans.Translation = trans.Translation - center;
                    trans.UpdateTransform();
                }
                var node = gob.As<DomNode>();
                if (node != null)
                    list.Add(node);
            }
            return prefab;
        }
        
        [Import(AllowDefault = false)]
        private IMainWindow m_mainWindow = null;

        [Import(AllowDefault = false)]
        private ICommandService m_commandService = null;

        [Import(AllowDefault = false)]
        private IContextRegistry m_contextRegistry = null;

        [Import(AllowDefault = false)]
        private SchemaLoader m_schemaLoader = null;

        [Import(AllowDefault = false)]
        private IResourceService m_resourceService = null;

        [Import(AllowDefault = true)]
        private ScriptingService m_scriptingService = null;

        private enum Commands
        {
            CreatePrefab,
        }

        private const string m_ext = ".prefab";
        private string m_fileFilter = string.Format("Prefab (*{0})|*{0}", m_ext);
        
    }

    public class Prefab : DomNodeAdapter, IPrefab
    {
        public static Prefab Create(DomNode node, Uri ur)
        {
            if (node.Type != Schema.prefabType.Type)
                throw new InvalidOperationException("Invalid node type");
            Prefab prefab = node.As<Prefab>();
            prefab.m_uri = ur;
            prefab.Name = Path.GetFileNameWithoutExtension(ur.LocalPath);
            return prefab;
        }

        public IEnumerable<object> GameObjects 
        { 
            get
            {
                return DomNode.GetChildren(Schema.prefabType.gameObjectChild);
            }
        }

        public string Name
        {
            get;
            private set;
        }

        #region IResource Members

        public string Type
        {
            get { return "Prefab"; }
        }

        public Uri Uri
        {
            get { return m_uri; }
            set { throw new InvalidOperationException(); }
        }

        public event EventHandler<UriChangedEventArgs> UriChanged
            = delegate { };

        #endregion

        private Uri m_uri;
    }
}
