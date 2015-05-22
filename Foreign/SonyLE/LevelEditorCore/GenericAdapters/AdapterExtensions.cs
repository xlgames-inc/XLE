using System;
using System.Collections.Generic;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;
using Sce.Atf.VectorMath;

namespace LevelEditorCore.GenericAdapters.Extensions
{
    public static class AdapterExtensions
    {
        public static Vec3F GetVec3(this DomNodeAdapter node, AttributeInfo attributeInfo)
        {
            return LevelEditorCore.DomNodeUtil.GetVector(node.DomNode, attributeInfo);

            // object value = node.DomNode.GetAttribute(attributeInfo);
            // 
            // // if value is not null, attempt the cast; an invalid type will then cause
            // //  an IllegalCastException; all value type attributes have a default
            // //  value, so will return here
            // if (value != null)
            //     return new Vec3F((float[])value);
            // 
            // return default(Vec3F);
        }

        public static void SetVec3(this DomNodeAdapter node, AttributeInfo attributeInfo, Vec3F v)
        {
            LevelEditorCore.DomNodeUtil.SetVector(node.DomNode, attributeInfo, v);
        }

        public static Matrix4F GetMatrix4x4(this DomNodeAdapter node, AttributeInfo attributeInfo)
        {
            object value = node.DomNode.GetAttribute(attributeInfo);
            
            // if value is not null, attempt the cast; an invalid type will then cause
            //  an IllegalCastException; all value type attributes have a default
            //  value, so will return here
            if (value != null)
                return new Matrix4F((float[])value);

            return default(Matrix4F);
        }

        public static void SetMatrix4x4(this DomNodeAdapter node, AttributeInfo attributeInfo, Matrix4F mat)
        {
            node.DomNode.SetAttribute(attributeInfo, mat.ToArray());
        }
    }
}
