using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using Sce.Atf;
using Sce.Atf.VectorMath;

namespace RenderingInterop.NativeInterop
{
    static class Picking
    {
        public static HitRecord[] RayPick(
            GUILayer.EditorSceneManager sceneManager,
            GUILayer.TechniqueContextWrapper techniqueContext,
            Ray3F ray, Sce.Atf.Rendering.Camera camera, 
            int viewportWidth, int viewportHeight,
            bool ignoreSelection)
        {
            System.Diagnostics.Debug.Assert(!ignoreSelection);

            var endPt = ray.Origin + camera.FarZ * ray.Direction;
            using (var context = XLELayer.XLELayerUtils.CreateIntersectionTestContext(
                GameEngine.GetEngineDevice(), techniqueContext, camera, (uint)viewportWidth, (uint)viewportHeight))
            {
                using (var scene = sceneManager.GetIntersectionScene())
                {
                    var results = GUILayer.EditorInterfaceUtils.RayIntersection(
                        scene,
                        context,
                        ray.Origin.X, ray.Origin.Y, ray.Origin.Z,
                        endPt.X, endPt.Y, endPt.Z, ~0u);

                    if (results == null) { return null; }

                    HitRecord[] hitRecords = new HitRecord[results.Count];
                    uint index = 0;
                    foreach (var r in results)
                    {
                        hitRecords[index].documentId = r._document;
                        hitRecords[index].instanceId = r._object;
                        hitRecords[index].index = 0;
                        hitRecords[index].distance = r._distance;
                        hitRecords[index].hitPt = new Vec3F(r._worldSpaceCollisionX, r._worldSpaceCollisionY, r._worldSpaceCollisionZ);
                        hitRecords[index].normal = new Vec3F(0.0f, 0.0f, 0.0f);
                        hitRecords[index].nearestVertex = new Vec3F(0.0f, 0.0f, 0.0f);
                        hitRecords[index].hasNormal = hitRecords[index].hasNearestVert = false;
                        index++;
                    }

                    return hitRecords;
                }
            }
        }
    }
}
