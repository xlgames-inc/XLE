// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.ComponentModel.Composition;
using Sce.Atf;

namespace MaterialTool
{
    /// <summary>
    /// Adapts the circuit to IDocument and synchronizes URI and dirty bit changes to the
    /// ControlInfo instance used to register the viewing control in the UI</summary>
    [Export(typeof(NodeEditorCore.IDiagramDocument))]
    [Export(typeof(DiagramDocument))]
    [PartCreationPolicy(CreationPolicy.NonShared)]
    public class DiagramDocument : NodeEditorCore.IDiagramDocument, IDocument, ControlsLibraryExt.ISerializableDocument
    {
        #region IDiagramDocument Members
        public ShaderPatcherLayer.NodeGraphMetaData GraphMetaData { get; set; }
        public ShaderPatcherLayer.NodeGraphFile NodeGraphFile
        {
            get {
                if ((ViewModel == null) || (_nodeGraphFileRepresentation != null && _nodeGraphFileRepresentationRevisionIndex == ViewModel.RevisionIndex))
                {
                    return _nodeGraphFileRepresentation;
                }
                _nodeGraphFileRepresentationRevisionIndex = ViewModel.RevisionIndex;
                _nodeGraphFileRepresentation = ViewModel.Rebuild();
                return _nodeGraphFileRepresentation;
            }
        }

        public uint GlobalRevisionIndex { get { return ViewModel.RevisionIndex; } }

        public void Save(Uri destination)
        {
            // Write to memory stream first, and if successful, flush out to a file stream
            // This avoid making any filesystem changes if NodeGraphFile.Serialize() throws an exception
            using (var memoryStream = new System.IO.MemoryStream())
            {
                NodeGraphFile.Serialize(memoryStream, System.IO.Path.GetFileNameWithoutExtension(destination.LocalPath), GraphMetaData);

                var fileMode = System.IO.File.Exists(destination.LocalPath) ? System.IO.FileMode.Truncate : System.IO.FileMode.OpenOrCreate;
                using (var fileStream = new System.IO.FileStream(destination.LocalPath, fileMode))
                {
                    memoryStream.WriteTo(fileStream);
                }
            }
        }

        public void Load(Uri source)
        {
            ShaderPatcherLayer.NodeGraphMetaData graphMetaData;
            ShaderPatcherLayer.NodeGraphFile.Load(source.LocalPath, out _nodeGraphFileRepresentation, out graphMetaData);
            GraphMetaData = graphMetaData;
            _nodeGraphFileRepresentationRevisionIndex = 0;
        }

        public void InitializeNew()
        {
            GraphMetaData = new ShaderPatcherLayer.NodeGraphMetaData();
            _nodeGraphFileRepresentation = new ShaderPatcherLayer.NodeGraphFile();
            _nodeGraphFileRepresentationRevisionIndex = 0;
        }
        #endregion

        #region IDocument Members
        bool IDocument.IsReadOnly
        {
            get { return false; }
        }

        bool IDocument.Dirty
        {
            get { return _dirty; }
            set { SetDirty(value); }
        }

        public event EventHandler DirtyChanged;

        private void SetDirty(bool newValue)
        {
            if (newValue != _dirty)
            {
                _dirty = newValue;
                DirtyChanged.Raise(this, EventArgs.Empty);
            }
        }

        private bool _dirty;
        #endregion

        #region IResource Members
        public virtual string Type
        {
            get { return "Shader Graph".Localize(); }
        }

        public virtual Uri Uri
        {
            get { return _uri; }
            set
            {
                if (value == null)
                    throw new ArgumentNullException("value");

                if (value != _uri)
                {
                    Uri oldUri = _uri;
                    _uri = value;
                    OnUriChanged(new UriChangedEventArgs(oldUri));
                }
            }
        }

        public event EventHandler<UriChangedEventArgs> UriChanged;

        protected virtual void OnUriChanged(UriChangedEventArgs e)
        {
            UriChanged.Raise(this, e);
        }

        private Uri _uri;

        #endregion

        #region ISerializableDocument
        public byte[] Serialize()
        {
            using (var memoryStream = new System.IO.MemoryStream())
            {
                NodeGraphFile.Serialize(memoryStream, System.IO.Path.GetFileNameWithoutExtension(Uri.LocalPath), GraphMetaData);
                return memoryStream.GetBuffer();
            }
        }
        #endregion

        public interface IViewModel
        {
            uint RevisionIndex { get; }
            ShaderPatcherLayer.NodeGraphFile Rebuild();
        }

        public IViewModel ViewModel { get; set; }

        private ShaderPatcherLayer.NodeGraphFile _nodeGraphFileRepresentation;
        private uint _nodeGraphFileRepresentationRevisionIndex;
    }
}
