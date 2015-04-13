//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System;
using System.Windows.Forms;
using System.Drawing;
using System.Collections.Generic;
using LevelEditorCore.VectorMath;
using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;
using Sce.Atf.VectorMath;

using LevelEditorCore;

using ViewTypes = Sce.Atf.Rendering.ViewTypes;

namespace RenderingInterop
{
    public class NativeDesignControl : XLELayer.NativeDesignControl
    {
        public NativeDesignControl(DesignView designView, GUILayer.EditorSceneManager sceneManager) :
            base(designView, sceneManager)
        {
            if (s_marqueePen == null)
            {
                s_marqueePen = new Pen(Color.FromArgb(30, 30, 30), 2);
                s_marqueePen.DashPattern = new float[] { 3, 3 };
            }

            m_renderState = new RenderState();
            m_renderState.RenderFlag = GlobalRenderFlags.Solid | GlobalRenderFlags.Textured | GlobalRenderFlags.Lit | GlobalRenderFlags.Shadows;
            m_renderState.WireFrameColor = Color.DarkBlue;
            m_renderState.SelectionColor = Color.FromArgb(66, 255, 161);
            BackColor = SystemColors.ControlDark;
            m_renderState.Changed += (sender, e) => Invalidate();

            base.AddRenderCallback(RenderExtras);
        }

        public ulong SurfaceId
        {
            get;
            private set;
        }

        public RenderState RenderState
        {
            get { return m_renderState; }
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                m_renderState.Dispose();
            }
            base.Dispose(disposing);
        }
        
        protected override IList<object> Pick(MouseEventArgs e)
        {           
            bool multiSelect = DragOverThreshold;
            List<object> paths = new List<object>();

            HitRecord[] hits;


            if(multiSelect)
            {// frustum pick                
                RectangleF rect = MakeRect(FirstMousePoint, CurrentMousePoint);
                var frustum = XLELayer.XLELayerUtils.MakeFrustumMatrix(Camera, rect, ClientSize);
                hits = NativeInterop.Picking.FrustumPick(
                    SceneManager, TechniqueContext, frustum,
                    Camera, ClientSize.Width, ClientSize.Height, false);
            }
            else
            {// ray pick
                Ray3F rayW = GetWorldRay(CurrentMousePoint);
                hits = NativeInterop.Picking.RayPick(
                    SceneManager, TechniqueContext, rayW, 
                    Camera, ClientSize.Width, ClientSize.Height, false);
            }

            if (hits==null) return new List<object>();

            // create unique list of hits
            HashSet<ulong> instanceSet = new HashSet<ulong>();
            List<HitRecord> uniqueHits = new List<HitRecord>();
            // build 'path' objects for each hit record.
            foreach (HitRecord hit in hits)
            {
                bool added = instanceSet.Add(hit.instanceId);
                if (added) uniqueHits.Add(hit);
            }

            HitRecord firstHit = new HitRecord();
            

            // build 'path' objects for each hit record.
            foreach (HitRecord hit in uniqueHits)
            {
                NativeObjectAdapter nobj = GameEngine.GetAdapterFromId(hit.documentId, hit.instanceId);
                if (nobj == null) continue;

                DomNode dom = nobj.DomNode;
                object hitPath = Util.AdaptDomPath(dom);
                object obj = DesignView.PickFilter.Filter(hitPath, e);
                if (obj != null)
                {
                    if (paths.Count == 0)
                    {
                        firstHit = hit;
                    }
                    var newPath = obj as AdaptablePath<object> ?? Util.AdaptDomPath((DomNode)obj);
                    paths.Add(newPath);
                }
            }


            if (multiSelect == false && paths.Count > 0)                
            {
                var path = paths[0];
                ISelectionContext selection = DesignView.Context.As<ISelectionContext>();
                ILinear linear = path.As<ILinear>();
                if (linear != null
                    && Control.ModifierKeys == System.Windows.Forms.Keys.Shift
                    && selection.SelectionContains(path))
                {
                    ITransactionContext trans = DesignView.Context.As<ITransactionContext>();
                    trans.DoTransaction(
                        delegate
                        {
                            linear.InsertPoint(firstHit.index, firstHit.hitPt.X, firstHit.hitPt.Y, firstHit.hitPt.Z);
                        }, "insert control point".Localize()
                        );
                }
            }
            return paths;
        }
        
        private IGame TargetGame()
        {
            return DesignView.Context.As<IGame>();
        }

        private readonly List<DomNode> m_ghosts = new List<DomNode>();
        
        protected override void OnDragEnter(DragEventArgs drgevent)
        {
            base.OnDragEnter(drgevent);
            var dragDropTarget = TargetGame();

            m_ghosts.Clear();

            ResourceConverterService resourceConverter = Globals.MEFContainer.GetExportedValue<ResourceConverterService>();
            IEnumerable<object> nodes = Util.ConvertData(drgevent.Data, false);
            foreach (object iterNode in nodes)
            {
                DomNode node = iterNode as DomNode;
                if (node == null)
                {                 
                    if (resourceConverter != null)
                    {
                        IGameObject resGob = resourceConverter.Convert(iterNode as IResource);
                        node = resGob.As<DomNode>();                    
                    }                    
                }
                
                IGameObject gob = node.As<IGameObject>();
                if (gob == null || node.GetRoot().Is<IGame>())
                    continue;
                
                node.InitializeExtensions();
                if (dragDropTarget.AddChild(node))
                {
                    m_ghosts.Add(node);
                }
            }

            drgevent.Effect = (m_ghosts.Count > 0) ? (DragDropEffects.Move | DragDropEffects.Link) : DragDropEffects.None;
        }

        protected bool GetTerrainCollision(out Vec3F result, Point clientPt)
        {
            Ray3F ray = GetWorldRay(clientPt);
            using (var testScene = GameEngine.GetEditorSceneManager().GetIntersectionScene())
            {
                using (var testContext = XLELayer.XLELayerUtils.CreateIntersectionTestContext(
                    GameEngine.GetEngineDevice(), null,
                    Camera, (uint)ClientSize.Width, (uint)ClientSize.Height))
                {
                    var endPt = ray.Origin + Camera.FarZ * ray.Direction;
                    var results = GUILayer.EditorInterfaceUtils.RayIntersection(
                        testScene,
                        testContext,
                        ray.Origin.X, ray.Origin.Y, ray.Origin.Z,
                        endPt.X, endPt.Y, endPt.Z,
                        1);

                    if (results != null) 
                    {
                        using (var enumer = results.GetEnumerator()) 
                        {
                            if (enumer.MoveNext()) {
                                var first = enumer.Current;
                                result = new Vec3F(
                                    first._worldSpaceCollisionX,
                                    first._worldSpaceCollisionY,
                                    first._worldSpaceCollisionZ);
                                return true;
                            }
                        }
                    }
                }
            }

            result = new Vec3F(0.0f, 0.0f, 0.0f);
            return false;
        }
        
        protected override void OnDragOver(DragEventArgs drgevent)
        {        
            base.OnDragOver(drgevent);
            if (DesignView.Context == null || m_ghosts.Count == 0) return;
            
                //  Just do an intersection against the terrain to 
                //  calculate basic insertion position
            Vec3F terrainHit;
            if (GetTerrainCollision(out terrainHit, PointToClient(new Point(drgevent.X, drgevent.Y)))) {
                foreach (var ghost in m_ghosts) {
                    var gameObject = ghost.As<IGameObject>();
                    if (gameObject != null) {
                        gameObject.Translation = terrainHit;
                    }
                }

                DesignView.InvalidateViews();
            }
        }

        protected override void OnDragDrop(DragEventArgs drgevent)
        {
            base.OnDragDrop(drgevent);
            if (DesignView.Context == null) return;

            if (m_ghosts.Count > 0)
            {
                foreach (DomNode ghost in m_ghosts)
                    ghost.RemoveFromParent();

                var dragDropTarget = TargetGame();
                ApplicationUtil.Insert(
                    dragDropTarget,
                    dragDropTarget,
                    m_ghosts,
                    "Drag and Drop",
                    null);

                m_ghosts.Clear();                
                DesignView.InvalidateViews();
            }
        }

        protected override void OnDragLeave(EventArgs e)
        {
            base.OnDragLeave(e);
            if (DesignView.Context == null) return;

            if (m_ghosts.Count > 0)
            {
                foreach (DomNode ghost in m_ghosts) { ghost.RemoveFromParent(); }
                m_ghosts.Clear();                
                DesignView.InvalidateViews();
            }

        }
        protected override void OnPaint(PaintEventArgs e)
        {
            try
            {
                if (DesignView.Context == null || GameEngine.IsInError
                    || GameLoop == null)
                {
                    e.Graphics.Clear(DesignView.BackColor);
                    if (GameEngine.IsInError)
                        e.Graphics.DrawString(GameEngine.CriticalError, Font, Brushes.Red, 1, 1);
                    return;
                }
                GameLoop.Update();
                Render();
            }
            catch (Exception ex)
            {
                e.Graphics.DrawString(ex.Message, Font, Brushes.Red, 1, 1);
            }
        }

        // render the scene.
        public override void Render()
        {
            bool skipRender =
                GameEngine.IsInError
                || Width == 0
                || Height == 0
                || DesignView.Context == null;

            if (skipRender)
                return;

            m_clk.Start();
            base.Render();
            m_pendingCaption = string.Format("View Type: {0}   time per frame-render call: {1:0.00} ms", ViewType, m_clk.Milliseconds);

            if (IsPicking)
            {// todo: use Directx to draw marque.                
                using (Graphics g = CreateGraphics())
                {
                    Rectangle rect = MakeRect(FirstMousePoint, CurrentMousePoint);
                    if (rect.Width > 0 && rect.Height > 0)
                    {
                        g.DrawRectangle(s_marqueePen, rect);
                    }
                }
            }

        }
      
        private void RenderExtras(DesignView designView, Sce.Atf.Rendering.Camera camera)
        {
            bool renderSelected = RenderState.DisplayBound == DisplayFlagModes.Selection
                || RenderState.DisplayCaption == DisplayFlagModes.Selection
                || RenderState.DisplayPivot == DisplayFlagModes.Selection;

            if (renderSelected)
            {
                var selection = DesignView.Context.As<ISelectionContext>().Selection;
                IEnumerable<DomNode> rootDomNodes = DomNode.GetRoots(selection.AsIEnumerable<DomNode>());
                RenderProperties(rootDomNodes,
                    RenderState.DisplayCaption == DisplayFlagModes.Selection,
                    RenderState.DisplayBound == DisplayFlagModes.Selection,
                    RenderState.DisplayPivot == DisplayFlagModes.Selection);
            }

            RenderProperties(Items,
                RenderState.DisplayCaption == DisplayFlagModes.Always,
                RenderState.DisplayBound == DisplayFlagModes.Always,
                RenderState.DisplayPivot == DisplayFlagModes.Always);

            GameEngine.DrawText2D(m_pendingCaption, Util3D.CaptionFont, 1, 1, Color.White);
        }
        
        private void RenderProperties(IEnumerable<object> objects, bool renderCaption, bool renderBound, bool renderPivot)
        {                      
            if (renderCaption || renderBound)
            {
                Util3D.RenderFlag = BasicRendererFlags.WireFrame;
                Matrix4F vp = Camera.ViewMatrix * Camera.ProjectionMatrix;
                foreach (object obj in objects)
                {
                    IBoundable bnode = obj.As<IBoundable>();
                    if (bnode == null || bnode.BoundingBox.IsEmpty || obj.Is<IGameObjectFolder>()) continue;

                    INameable nnode = obj.As<INameable>();
                    ITransformable trans = obj.As<ITransformable>();

                    if (renderBound)
                    {
                        Util3D.DrawAABB(bnode.BoundingBox);
                    }
                    if (renderCaption && nnode != null)
                    {
                        Vec3F topCenter = bnode.BoundingBox.Center;
                        topCenter.Y = bnode.BoundingBox.Max.Y;
                        Point pt = Project(vp, topCenter);
                        GameEngine.DrawText2D(nnode.Name, Util3D.CaptionFont, pt.X, pt.Y, Color.White);
                    }
                }
            }

            if (renderPivot)
            {
                Util3D.RenderFlag = BasicRendererFlags.WireFrame | BasicRendererFlags.DisableDepthTest;

                // create few temp matrics to
                Matrix4F toWorld = new Matrix4F();
                Matrix4F PV = new Matrix4F();
                Matrix4F sc = new Matrix4F();
                Matrix4F bl = new Matrix4F();
                Matrix4F recXform = new Matrix4F();
                foreach (object obj in objects)
                {
                    ITransformable trans = obj.As<ITransformable>();
                    IBoundable bnode = obj.As<IBoundable>();
                    if (trans == null || bnode == null || bnode.BoundingBox.IsEmpty || obj.Is<IGameObjectFolder>()) continue;

                    Path<DomNode> path = new Path<DomNode>(trans.Cast<DomNode>().GetPath());
                    toWorld.Set(Vec3F.ZeroVector);
                    TransformUtils.CalcPathTransform(toWorld, path, path.Count - 1);

                    // Offset by pivot
                    PV.Set(trans.Pivot);
                    toWorld.Mul(PV, toWorld);
                    Vec3F pos = toWorld.Translation;

                    const float pivotDiameter = 16; // in pixels
                    float s = Util.CalcAxisScale(Camera, pos, pivotDiameter, Height);                    
                    sc.Scale(s);
                    Util.CreateBillboard(bl, pos, Camera.WorldEye, Camera.Up, Camera.LookAt);
                    recXform = sc * bl;
                    Util3D.DrawPivot(recXform, Color.Yellow);
                }
            }
        }
       
        private IEnumerable<DomNode> Items
        {
            get
            {
                IGameDocumentRegistry gameDocumentRegistry = Globals.MEFContainer.GetExportedValue<IGameDocumentRegistry>();
                DomNode folderNode = gameDocumentRegistry.MasterDocument.RootGameObjectFolder.Cast<DomNode>();
                foreach (DomNode childNode in folderNode.Subtree)
                {
                    yield return childNode;
                }

                foreach (IGameDocument subDoc in gameDocumentRegistry.SubDocuments)
                {
                    folderNode = subDoc.RootGameObjectFolder.Cast<DomNode>();
                    foreach (DomNode childNode in folderNode.Subtree)
                    {
                        yield return childNode;
                    }
                }
            }
        }
        
        private Rectangle MakeRect(Point p1, Point p2)
        {
            int minx = Math.Min(p1.X, p2.X);
            int miny = Math.Min(p1.Y, p2.Y);
            int maxx = Math.Max(p1.X, p2.X);
            int maxy = Math.Max(p1.Y, p2.Y);
            int w = maxx - minx;
            int h = maxy - miny;
            return new Rectangle(minx, miny, w, h);
        }
        
        private static Pen s_marqueePen;
        private Clock m_clk = new Clock();
        private RenderState m_renderState;
        private string m_pendingCaption;
    }
}
