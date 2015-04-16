//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.ComponentModel.Composition;

using Sce.Atf;
using Sce.Atf.VectorMath;
using Sce.Atf.Adaptation;
using Sce.Atf.Dom;
using Sce.Atf.Applications;
using LevelEditorCore;

namespace RenderingInterop
{
    /// <summary>
    /// Abstract base class for 3d Manipulators.</summary>
    public abstract class Manipulator : IManipulator
    {
        public Manipulator()
        {
            HitMatrix = new Matrix4F();
            DesignView = null;
        }

        #region IManipulator Members

        public virtual bool Pick(ViewControl vc, Point scrPt)
        {
            Matrix4F normWorld = GetManipulatorMatrix();
            if (normWorld == null) return false;
            HitRayV = vc.GetRay(scrPt, vc.Camera.ProjectionMatrix);            
            HitMatrix.Set(normWorld);            
            return true;
        }
        public abstract void Render(ViewControl vc);
        public abstract void OnBeginDrag();
        public abstract void OnDragging(ViewControl vc, Point scrPt);
        public abstract void OnEndDrag(ViewControl vc, Point scrPt);

        public ManipulatorInfo ManipulatorInfo
        {
            get;
            protected set;
        }

        #endregion
        protected abstract Matrix4F GetManipulatorMatrix();
        
        protected ITransformable GetManipulatorNode(TransformationTypes xformType)
        {
            ITransformable manipNode = null;
            var selectionCntx = DesignView.Context.As<ISelectionContext>();
            var visibilityContext = DesignView.Context.As<IVisibilityContext>();
            if (selectionCntx.LastSelected != null)
            {
                Path<object> path = selectionCntx.LastSelected.As<Path<object>>();
                foreach (object obj in path)
                {
                    DomNode pathnode = obj.As<DomNode>();
                    if (pathnode == null) break;
                    object item = Util.AdaptDomPath(pathnode);
                    if (selectionCntx.SelectionContains(item))
                    {
                        var xformable = pathnode.As<ITransformable>();
                        if (xformable != null 
                            && (xformable.TransformationType & xformType) != 0
                            && visibilityContext.IsVisible(pathnode))
                        {
                            manipNode = xformable;                           
                        }
                        break;
                    }
                }
            }
            return manipNode;
        }

        protected Matrix4F HitMatrix
        {
            get;
            private set;
        }

        [Import(AllowDefault = false)]
        protected IDesignView DesignView
        {
            get;
            private set;
        }        
        protected Ray3F HitRayV;  // hit ray in view space.


        // common properties
        public static readonly Color XAxisColor = Color.FromArgb(240, 40, 20);
        public static readonly Color YAxisColor = Color.FromArgb(75, 240, 0);
        public static readonly Color ZAxisColor = Color.FromArgb(15, 57, 240);
        public const float AxisLength = 80; // in pixels
        public const float AxisThickness = 1.0f / 26.0f;
        public const float AxisHandle = 1.0f / 6.0f;
    }


    internal class ManipulatorActiveOperation
    {
        public delegate bool FilterDelegate(ITransformable node);

        public ManipulatorActiveOperation(
            string name, ISelectionContext selectionContext,
            FilterDelegate filter,
            bool duplicate)
        {
            TransactionContextList = new List<ITransactionContext>();
            NodeList = new List<ITransformable>();
            IsDuplicating = duplicate;

            var selection = selectionContext.Selection;
            IEnumerable<DomNode> rootDomNodes = DomNode.GetRoots(selection.AsIEnumerable<DomNode>());

            SetupTransactionContexts(rootDomNodes);

            if (duplicate)
            {
                List<DomNode> originals = new List<DomNode>();
                foreach (DomNode node in rootDomNodes)
                {
                    ITransformable transformable = node.As<ITransformable>();
                    if (!CanManipulate(transformable, filter)) continue;

                    originals.Add(node);
                }

                if (originals.Count > 0)
                {
                    DomNode[] copies = DomNode.Copy(originals);

                    foreach (var t in TransactionContextList)
                        t.Begin(("Copy And" + name).Localize());

                    List<object> newSelection = new List<object>();
                    // re-parent copy
                    for (int i = 0; i < copies.Length; i++)
                    {
                        DomNode copy = copies[i];
                        DomNode original = originals[i];

                        ChildInfo chInfo = original.ChildInfo;
                        if (chInfo.IsList)
                            original.Parent.GetChildList(chInfo).Add(copy);
                        else
                            original.Parent.SetChild(chInfo, copy);

                        newSelection.Add(Util.AdaptDomPath(copy));
                        copy.InitializeExtensions();
                    }

                    selectionContext.SetRange(newSelection);
                    NodeList.AddRange(copies.AsIEnumerable<ITransformable>());
                }

            }
            else
            {
                foreach (DomNode node in rootDomNodes)
                {
                    ITransformable transformable = node.As<ITransformable>();
                    if (!CanManipulate(transformable, filter)) continue;
                    NodeList.Add(transformable);
                }

                if (NodeList.Count > 0)
                    foreach (var t in TransactionContextList)
                        t.Begin(name.Localize());
            }

            for (int k = 0; k < NodeList.Count; k++)
            {
                IManipulatorNotify notifier = NodeList[k].As<IManipulatorNotify>();
                if (notifier != null) notifier.OnBeginDrag();
            }
        }

        public void FinishTransaction()
        {
            if (NodeList.Count > 0)
            {
                for (int k = 0; k < NodeList.Count; k++)
                {
                    IManipulatorNotify notifier = NodeList[k].As<IManipulatorNotify>();
                    if (notifier != null) notifier.OnEndDrag();
                }

                var transactionContexts = TransactionContextList;
                foreach (var t in transactionContexts)
                {
                    try
                    {
                        if (t.InTransaction)
                            t.End();
                    }
                    catch (InvalidTransactionException ex)
                    {
                        if (t.InTransaction)
                            t.Cancel();
                        if (ex.ReportError)
                            Outputs.WriteLine(OutputMessageType.Error, ex.Message);
                    }
                }

                NodeList.Clear();
            }
        }

        private bool CanManipulate(ITransformable node, FilterDelegate filter)
        {
            bool result = node != null
                && node.Cast<IVisible>().Visible
                && node.Cast<ILockable>().IsLocked == false
                && filter(node);
            return result;
        }

        public List<ITransformable> NodeList
        {
            get;
            private set;
        }

        public bool IsDuplicating
        {
            get;
            private set;
        }

        protected IList<ITransactionContext> TransactionContextList
        {
            get;
            private set;
        }

        protected void SetupTransactionContexts(IEnumerable<DomNode> nodes)
        {
            TransactionContextList.Clear();
            if (nodes == null) return;

            foreach (var n in nodes)
            {
                var context = n.GetRoot().As<ITransactionContext>();
                if (context != null && !TransactionContextList.Contains(context))
                {
                    TransactionContextList.Add(context);
                }
            }
        }
    }
}
