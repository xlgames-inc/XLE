// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using Sce.Atf;

namespace MaterialTool
{
    /// <summary>
    /// Adapts the circuit to IDocument and synchronizes URI and dirty bit changes to the
    /// ControlInfo instance used to register the viewing control in the UI</summary>
    public class DiagramDocument : DiagramEditingContext, IDocument
    {
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

        public DiagramDocument(HyperGraph.IGraphModel model, Uri uri) : base(model) 
        {
            _uri = uri;

                // tracking for dirty flag --
            Model.NodeAdded += Model_NodeAdded;
            Model.NodeRemoved += Model_NodeRemoved;
            Model.ConnectionAdded += Model_ConnectionAdded;
            Model.ConnectionRemoved += Model_ConnectionRemoved;
            Model.MiscChange += Model_MiscChange;
        }

        void Model_MiscChange(object sender, EventArgs e) { SetDirty(true); }
        void Model_ConnectionRemoved(object sender, HyperGraph.NodeConnectionEventArgs e) { SetDirty(true); }
        void Model_ConnectionAdded(object sender, HyperGraph.AcceptNodeConnectionEventArgs e) { SetDirty(true); }
        void Model_NodeRemoved(object sender, HyperGraph.NodeEventArgs e) { SetDirty(true); }
        void Model_NodeAdded(object sender, HyperGraph.AcceptNodeEventArgs e) { SetDirty(true); }
    }
}
