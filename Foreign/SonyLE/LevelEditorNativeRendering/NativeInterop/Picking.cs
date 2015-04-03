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
            Ray3F ray, float maxCollisionDistance, bool ignoreSelection)
        {
            System.Diagnostics.Debug.Assert(!ignoreSelection);

            var endPt = ray.Origin + maxCollisionDistance * ray.Direction;
            var results = GUILayer.EditorInterfaceUtils.RayIntersection(
                GameEngine.GetEngineDevice(), techniqueContext, sceneManager,
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

        public static HitRecord[] RayPick(Matrix4F viewxform, Matrix4F projxfrom, Ray3F rayW, bool skipSelected) { return null; }
    }
}
