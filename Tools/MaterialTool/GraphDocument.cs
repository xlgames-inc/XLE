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
    public class GraphDocument : GraphEditingContext, IDocument
    {
        #region IDocument Members
        bool IDocument.IsReadOnly
        {
            get { return false; }
        }

        bool IDocument.Dirty
        {
            get { return _dirty; }
            set
            {
                if (value != _dirty)
                {
                    _dirty = value;
                    OnDirtyChanged(EventArgs.Empty);
                }
            }
        }

        event EventHandler IDocument.DirtyChanged
        {
            add
            {
                lock (DirtyChangedEvent)
                {
                    DirtyChangedEvent += value;
                }
            }
            remove
            {
                lock (DirtyChangedEvent)
                {
                    DirtyChangedEvent -= value;
                }
            }
        }

        private event EventHandler DirtyChangedEvent;

        protected void OnDirtyChanged(EventArgs e)
        {
            DirtyChangedEvent.Raise(this, e);
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

        public GraphDocument(HyperGraph.IGraphModel model) : base(model) {}
    }
}
