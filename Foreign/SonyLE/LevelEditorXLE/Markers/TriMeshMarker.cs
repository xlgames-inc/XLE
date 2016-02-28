// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System.Collections.Generic;
using System.Linq;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;
using Sce.Atf.VectorMath;

using LevelEditorCore;
using LevelEditorCore.VectorMath;
using LevelEditorCore.GenericAdapters.Extensions;

namespace LevelEditorXLE.Markers
{
    public class TriMeshMarker : DomNodeAdapter, IHierarchical, IListable
    {
        public bool CanAddChild(object child)
        {
            var domNode = child.As<DomNode>();
            if (domNode == null) return false;

            return domNode.Type.Lineage.FirstOrDefault(
                t => t == Schema.markerPointType.Type) != null;
        }

        public bool AddChild(object child)
        {
            var domNode = child.As<DomNode>();
            if (domNode == null) return false;

            foreach (var type in domNode.Type.Lineage)
            {
                if (type == Schema.markerPointType.Type)
                {
                    MarkerPoints.Add(domNode);
                    return true;
                }
            }
            return false;
        }

        IList<DomNode> MarkerPoints
        {
            get { return GetChildList<DomNode>(Schema.triMeshMarkerType.pointsChild); }
        }

        int[] IndexList
        {
            get { return GetAttribute<int[]>(Schema.triMeshMarkerType.indexlistAttribute); }
            set { SetAttribute(Schema.triMeshMarkerType.indexlistAttribute, value); }
        }

        string Name
        {
            get { return GetAttribute<string>(Schema.triMeshMarkerType.nameAttribute); }
        }

        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = "TriMesh: " + Name;
        }

        public void AddPoint(Vec3F position)
        {
            AddChild(PointMarker.Create(position));
        }

        public static IAdaptable Create()
        {
            var node = new DomNode(Schema.triMeshMarkerType.Type);
            if (node == null) return null;

            var result = node.As<TriMeshMarker>();
            if (result==null) return null;

            result.AddPoint(new Vec3F(-100.0f, -100.0f, 0.0f));
            result.AddPoint(new Vec3F( 100.0f, -100.0f, 0.0f));
            result.AddPoint(new Vec3F(-100.0f,  100.0f, 0.0f));
            result.AddPoint(new Vec3F( 100.0f,  100.0f, 0.0f));
            result.IndexList = new int[] { 0, 1, 2, 2, 1, 3 };
            // node.InitializeExtensions();
            return result;
        }
    }

    public class PointMarker : DomNodeAdapter, ITransformable, IVisible, ILockable, IBoundable
    {
        public void UpdateTransform() { }

        public Matrix4F Transform
        {
            get
            {
                var result = new Matrix4F();
                result.Translation = Translation;
                return result;
            }
        }

        public Vec3F Translation
        {
            get { return this.GetVec3(Schema.markerPointType.translateAttribute); }
            set { this.SetVec3(Schema.markerPointType.translateAttribute, value); }
        }

        public Vec3F Rotation
        {
            get { return Vec3F.ZeroVector; }
            set { }
        }

        public Vec3F Scale
        {
            get { return new Vec3F(1, 1, 1); }
            set { }
        }

        public Vec3F Pivot
        {
            get { return Vec3F.ZeroVector; }
            set { }
        }

        public TransformationTypes TransformationType
        {
            get { return TransformationTypes.Translation; }
            set { }
        }

        public bool Visible
        {
            get { return true; }
            set { }
        }

        public bool IsLocked
        {
            get { return false; }
            set { }
        }

        public AABB BoundingBox
        {
            get { return new AABB(Translation + new Vec3F(-1, -1, -1), Translation + new Vec3F(1, 1, 1)); }
        }

        public AABB LocalBoundingBox
        {
            get { return new AABB(new Vec3F(-1, -1, -1), new Vec3F(1, 1, 1)); }
        }

        public static IAdaptable Create(Vec3F position)
        {
            var node = new DomNode(Schema.markerPointType.Type);
            if (node == null) return null;

            var result = node.As<PointMarker>();
            if (result == null) return null;

            result.Translation = position;
            // node.InitializeExtensions();
            return result;
        }
    }
}