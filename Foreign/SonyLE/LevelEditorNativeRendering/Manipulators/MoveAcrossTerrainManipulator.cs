//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System;
using System.Windows.Forms;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Drawing;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;
using Sce.Atf.VectorMath;

using LevelEditorCore;
using LevelEditorCore.VectorMath;

using Camera = Sce.Atf.Rendering.Camera;
using ViewTypes = Sce.Atf.Rendering.ViewTypes;
using AxisSystemType = Sce.Atf.Rendering.Dom.AxisSystemType;


namespace RenderingInterop
{
    using HitRegion = TranslatorControl.HitRegion;

    [Export(typeof(IManipulator))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class MoveAcrossTerrainManipulator : Manipulator
    {
        public MoveAcrossTerrainManipulator()
        {
            ManipulatorInfo = new ManipulatorInfo(
                "Move Across Terrain".Localize(),
                "Move objects across the surface of the terrain".Localize(),
                LevelEditorCore.Resources.TranslateImage,
                Keys.None);
        }

        #region Implementation of IManipulator

        public override bool Pick(ViewControl vc, Point scrPt)
        {
            m_hitRegion = HitRegion.None;
            if (base.Pick(vc, scrPt) == false)
                return false;

            Camera camera = vc.Camera;
            
            Matrix4F view = camera.ViewMatrix;
            Matrix4F vp = view  * camera.ProjectionMatrix;
            Matrix4F wvp = HitMatrix * vp;
            
            Ray3F rayL = vc.GetRay(scrPt,wvp);

            float s = Util.CalcAxisScale(vc.Camera, HitMatrix.Translation, AxisLength, vc.Height);

                // There's only one hot-spot for this manipulator:
                //      a square at the manipulator origin.
            Vec3F min = new Vec3F(-0.5f, -0.5f, -0.5f);
            Vec3F max = new Vec3F(0.5f, 0.5f, 0.5f);
            AABB box = new AABB(min, max);

            float centerCubeScale = s * CenterCubeSize;
            Matrix4F centerCubeXform = new Matrix4F();
            centerCubeXform.Scale(centerCubeScale);
            centerCubeXform.Invert(centerCubeXform);
            Ray3F ray = rayL;
            ray.Transform(centerCubeXform);
            if (box.Intersect(ray))
            {
                m_hitRegion = HitRegion.XYSquare;
                return true;
            }

            m_hitRegion = HitRegion.None;
            return false;
        }

        public override void Render(ViewControl vc)
        {                                                
            Matrix4F normWorld = GetManipulatorMatrix();
            if (normWorld == null) return;

            Vec3F pos = normWorld.Translation;
            float s = Util.CalcAxisScale(vc.Camera, pos, AxisLength, vc.Height);

            Color centerCubeColor = (m_hitRegion == HitRegion.XYSquare) ? Color.Gold : Color.White;

            Vec3F sv = new Vec3F(s, s, s);
            Vec3F centerCubeScale = sv * CenterCubeSize;
            Matrix4F scale = new Matrix4F(); 
            scale.Scale(centerCubeScale);
            Matrix4F centerCubeXform = scale * normWorld;
            Util3D.DrawCube(centerCubeXform, centerCubeColor);
        }

        public override void OnBeginDrag()
        {
            if (m_hitRegion == HitRegion.None)
                return;

            Clear(); // cached values.

            var op = new ManipulatorActiveOperation(
                "Move Across Terrain", DesignView.Context.As<ISelectionContext>(),
                (ITransformable node) => (node.TransformationType & TransformationTypes.Translation) != 0,
                Control.ModifierKeys == m_duplicateKey);

            m_originalTranslations = new Vec3F[op.NodeList.Count];
            m_originalHeights = new float[op.NodeList.Count];

            using (var intersectionScene = GameEngine.GetEditorSceneManager().GetIntersectionScene())
            {
                for (int k = 0; k < op.NodeList.Count; k++)
                {
                    Path<DomNode> path = new Path<DomNode>(op.NodeList[k].Cast<DomNode>().GetPath());
                    Matrix4F localToWorld = TransformUtils.CalcPathTransform(path, path.Count - 1);

                    m_originalTranslations[k] = localToWorld.Translation;
                    
                    float heightAboveTerrain = 0.0f;
                    float terrainHeight = 0.0f;
                    if (GUILayer.EditorInterfaceUtils.GetTerrainHeight(
                        out terrainHeight, intersectionScene, m_originalTranslations[k].X, m_originalTranslations[k].Y))
                    {
                        heightAboveTerrain = m_originalTranslations[k].Z - terrainHeight;
                    }
                    m_originalHeights[k] = heightAboveTerrain;
                }
            }

            m_pendingStartPt = true;
            m_activeOp = op;
        }

        public override void OnDragging(ViewControl vc, Point scrPt)
        {
            if (m_hitRegion == HitRegion.None || m_activeOp == null || m_activeOp.NodeList.Count == 0)
                return;

                // create ray in view space.            
            Ray3F rayV = vc.GetWorldRay(scrPt);

            using (var intersectionScene = GameEngine.GetEditorSceneManager().GetIntersectionScene())
            {
                Vec3F intersectionPt;
                if (!CalculateTerrainIntersection(vc, rayV, intersectionScene, out intersectionPt)) 
                    return;

                if (m_pendingStartPt)
                {
                    m_startPt = intersectionPt;
                    m_pendingStartPt = false;
                }
                else
                {
                    bool clampToSurface = Control.ModifierKeys == Keys.Shift;
                    Vec3F translate = new Vec3F(intersectionPt.X - m_startPt.X, intersectionPt.Y - m_startPt.Y, 0.0f);
                    for (int i = 0; i < m_activeOp.NodeList.Count; i++)
                    {
                        ITransformable node = m_activeOp.NodeList[i];

                        Path<DomNode> path = new Path<DomNode>(Adapters.Cast<DomNode>(node).GetPath());
                        Matrix4F parentLocalToWorld = TransformUtils.CalcPathTransform(path, path.Count - 2);
                        Matrix4F parentWorldToLocal = new Matrix4F();
                        parentWorldToLocal.Invert(parentLocalToWorld);

                        Vec3F newWorldPos = m_originalTranslations[i] + translate;
                        float terrainHeight = 0.0f;
                        if (GUILayer.EditorInterfaceUtils.GetTerrainHeight(
                            out terrainHeight, intersectionScene, newWorldPos.X, newWorldPos.Y)) {

                            newWorldPos.Z = terrainHeight + (clampToSurface ? 0.0f : m_originalHeights[i]);
                            Vec3F localTranslation;
                            parentWorldToLocal.TransformVector(newWorldPos, out localTranslation);
                            node.Translation = localTranslation;
                        }
                    }
                }
            }
        }

        public override void OnEndDrag(ViewControl vc, Point scrPt)
        {
            if (m_activeOp != null)
            {
                m_activeOp.FinishTransaction();
            }
            m_hitRegion = HitRegion.None;
            Clear();
        }

        public override void OnMouseWheel(ViewControl vc, Point scrPt, int delta) { }

        #endregion

        protected override Matrix4F GetManipulatorMatrix()
        {
            ITransformable node = GetManipulatorNode(TransformationTypes.Translation);
            if (node == null) return null;

            ISnapSettings snapSettings = (ISnapSettings)DesignView;            
            Path<DomNode> path = new Path<DomNode>(node.Cast<DomNode>().GetPath());
            Matrix4F localToWorld = TransformUtils.CalcPathTransform(path, path.Count - 1);
            
            Matrix4F toworld = new Matrix4F();
            if (snapSettings.ManipulateLocalAxis)
            {
                toworld.Set(localToWorld);
                toworld.Normalize(toworld);                
            }
            else
            {
                toworld.Translation = localToWorld.Translation;                
            }

                //  DavidJ --   note -- this "pivot" behaviour was inherited from another manipulator
            //                  but appears to be broken! Check coordinate space of value returned from CalcSnapFromOffset
            // Vec3F offset = TransformUtils.CalcSnapFromOffset(node, snapSettings.SnapFrom);
            // 
            // // Offset by pivot
            // Matrix4F P = new Matrix4F();
            // P.Translation = offset;
            // toworld.Mul(toworld,P);
                        
            return toworld;
        }

        /// <summary>
        /// Clear local cache</summary>
        private void Clear()
        {
            m_activeOp = null;
            m_originalTranslations = null;
            m_originalHeights = null;
        }

        private bool CalculateTerrainIntersection(ViewControl vc, Ray3F ray, GUILayer.IntersectionTestSceneWrapper testScene, out Vec3F result)
        {
            var pick = XLEBridgeUtils.Picking.RayPick(vc, ray, XLEBridgeUtils.Picking.Flags.Terrain);
            if (pick != null && pick.Length > 0)
            {
                result = pick[0].hitPt;
                return true;
            }
            
            result = new Vec3F(0.0f, 0.0f, 0.0f);
            return false;
        }

        private const float CenterCubeSize = 1.0f / 6.0f;
        private HitRegion m_hitRegion = HitRegion.None;        
        private Vec3F[] m_originalTranslations = null;
        private float[] m_originalHeights = null;
        private Keys m_duplicateKey = Keys.Control | Keys.Shift;
        private bool m_pendingStartPt = false;
        private Vec3F m_startPt;

        private ManipulatorActiveOperation m_activeOp = null;
    }    
}
