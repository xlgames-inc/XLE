using System;
using System.Collections.Generic;
using System.IO;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;

using LevelEditorCore;

namespace LevelEditor.DomNodeAdapters
{
    interface IResolveable
    {
        void Resolve();
        void Unresolve();
        bool IsResolved();
    }

    public abstract class GenericReference<T> : DomNodeAdapter, IReference<T>, IListable, IResolveable
        where T : class
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

        public Uri Uri
        {
            get { return GetAttribute<Uri>(s_refAttribute); }
        }

        public T Target
        {
            get { return m_target; }
        }

        #region IResolveable Members
        public virtual void Resolve()
        {
            if (m_target == null)
            {
                try
                {
                    Uri ur = Uri;
                    if (ur == null) { m_error = "ref attribute is null"; }
                    else if (!File.Exists(ur.LocalPath)) { m_error = "File not found: " + ur.LocalPath; }
                    else
                    {
                        m_target = Attach(ur);
                    }
                }
                catch (Exception e) { m_error = "Error during resolve: " + e.Message; }
            }
        }

        public virtual void Unresolve()
        {
            if (m_target != null)
            {
                Detach(m_target);
                m_target = null;
                m_error = "Not resolved";
            }
        }

        public virtual bool IsResolved() { return m_target != null; }
        #endregion

        protected string m_error = string.Empty;
        protected T m_target = default(T);
        protected static AttributeInfo s_nameAttribute;
        protected static AttributeInfo s_refAttribute;

        protected abstract T Attach(Uri uri);
        protected abstract void Detach(T target);
    }
}

