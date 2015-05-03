using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using Sce.Atf;
using Sce.Atf.VectorMath;

namespace RenderingInterop.NativeInterop
{
    public static unsafe class Picking
    {
        [Flags]
        public enum Flags : uint { 
            Terrain = 1 << 0, Objects = 1 << 1, Helpers = 1 << 6, 
            AllWorldObjects = Terrain | Objects,
            IgnoreSelection = 1 << 10 
        };

        public static HitRecord[] FrustumPick(
            GUILayer.TechniqueContextWrapper techniqueContext,
            Matrix4F pickingFrustum, 
            Sce.Atf.Rendering.Camera camera,
            System.Drawing.Size viewportSize,
            Flags flags)
        {
            System.Diagnostics.Debug.Assert((flags & Flags.IgnoreSelection) == 0);

            var sceneManager = GameEngine.GetEditorSceneManager();

            ICollection<GUILayer.HitRecord> results; 
            using (var context = XLELayer.XLELayerUtils.CreateIntersectionTestContext(
                GameEngine.GetEngineDevice(), techniqueContext, camera, 
                (uint)viewportSize.Width, (uint)viewportSize.Height))
            {
                using (var scene = sceneManager.GetIntersectionScene())
                {
                    fixed (float* ptr = &pickingFrustum.M11)
                    {
                        results = GUILayer.EditorInterfaceUtils.FrustumIntersection(
                            scene, context, ptr, (uint)flags);
                    }
                }
            }

            if (results == null) { return null; }
            return AsHitRecordArray(results);
        }

        public static HitRecord[] AsHitRecordArray(ICollection<GUILayer.HitRecord> results)
        {
            HitRecord[] hitRecords = new HitRecord[results.Count];
            uint index = 0;
            foreach (var r in results)
            {
                hitRecords[index].documentId = r._document;
                hitRecords[index].instanceId = r._object;
                hitRecords[index].index = 0;
                hitRecords[index].distance = r._distance;
                hitRecords[index].hitPt = XLELayer.XLELayerUtils.AsVec3F(r._worldSpaceCollision);
                hitRecords[index].normal = new Vec3F(0.0f, 0.0f, 0.0f);
                hitRecords[index].nearestVertex = new Vec3F(0.0f, 0.0f, 0.0f);
                hitRecords[index].hasNormal = hitRecords[index].hasNearestVert = false;
                hitRecords[index].drawCallIndex = r._drawCallIndex;
                hitRecords[index].materialGuid = r._materialGuid;
                hitRecords[index].materialName = r._materialName;
                hitRecords[index].modelName = r._modelName;
                index++;
            }

            return hitRecords;
        }

        public static HitRecord[] RayPick(
            GUILayer.TechniqueContextWrapper techniqueContext,
            Ray3F ray, Sce.Atf.Rendering.Camera camera, 
            System.Drawing.Size viewportSize, Flags flags)
        {
            System.Diagnostics.Debug.Assert((flags & Flags.IgnoreSelection)==0);

            var sceneManager = GameEngine.GetEditorSceneManager();

            ICollection<GUILayer.HitRecord> results; 
            var endPt = ray.Origin + camera.FarZ * ray.Direction;
            using (var context = XLELayer.XLELayerUtils.CreateIntersectionTestContext(
                GameEngine.GetEngineDevice(), techniqueContext, camera, (uint)viewportSize.Width, (uint)viewportSize.Height))
            {
                using (var scene = sceneManager.GetIntersectionScene())
                {
                    results = GUILayer.EditorInterfaceUtils.RayIntersection(
                        scene,
                        context,
                        XLELayer.XLELayerUtils.AsVector3(ray.Origin),
                        XLELayer.XLELayerUtils.AsVector3(endPt), (uint)flags);
                }
            }

            if (results == null) { return new HitRecord[0]; }
            return AsHitRecordArray(results);
        }
    }
}
