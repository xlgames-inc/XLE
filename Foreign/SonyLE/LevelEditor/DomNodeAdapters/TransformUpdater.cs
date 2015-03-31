//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System;


using Sce.Atf.Adaptation;
using Sce.Atf.Dom;
using Sce.Atf.VectorMath;

using LevelEditorCore;

namespace LevelEditor.DomNodeAdapters
{
    public class TransformUpdater : DomNodeAdapter
    {
        protected override void OnNodeSet()
        {
            base.OnNodeSet();

            m_transformable = DomNode.As<ITransformable>();            
            DomNode.AttributeChanged += OnAttributeChanged;
        }

        private void OnAttributeChanged(object sender, AttributeEventArgs e)
        {

            if (e.AttributeInfo.Equivalent(TransformAttributes.pivotAttribute))
            {// Update translation to keep object pinned when moving pivot                
                Matrix4F L0 = m_transformable.Transform;
                Matrix4F L1 = TransformUtils.CalcTransform(
                    m_transformable.Translation,
                    m_transformable.Rotation,
                    m_transformable.Scale,
                    m_transformable.Pivot);

                Vec3F deltaTranslation = L0.Translation - L1.Translation;
                m_transformable.Translation = m_transformable.Translation + deltaTranslation;
            }

            if (IsTransformAttribute(e.AttributeInfo))
            {
                ComputeTransform();
            }
        }

        /// <summary>
        /// Computes transformation matrix from transformation related atrributes.</summary>
        public void ComputeTransform()
        {
            Matrix4F xform = TransformUtils.CalcTransform(m_transformable);
            SetAttribute(TransformAttributes.transformAttribute, xform.ToArray());
        }
      
        /// <summary>
        /// Returns true iff the specified attribute is translation, rotation or scale</summary>
        /// <param name="attributeInfo"></param>
        /// <returns>True iff the specified attribute is translation, rotation or scale</returns>
        private bool IsTransformAttribute(AttributeInfo attributeInfo)
        {
            // Always use the Equivalent method when comparing two AttributeInfos
            // because using simple equality (==) doesn't respect inheritance
            // i.e. (Schema.baseType.someAttribute == Schema.derivedType.someAttribute) returns false
            // whereas (Schema.baseType.someAttribue.Equivalent(Schema.derivedType.someAttribute)) returns true
            var transformAttrib = TransformAttributes;
            return (attributeInfo.Equivalent(transformAttrib.translateAttribute)
                    || attributeInfo.Equivalent(transformAttrib.rotateAttribute)
                    || attributeInfo.Equivalent(transformAttrib.scaleAttribute)
                    || attributeInfo.Equivalent(transformAttrib.pivotAttribute));
        }

        private Schema.transformAttributes TransformAttributes
        {
            get 
            {
                    //  To use this adapter with different types of nodes,
                    //  we need to query the transform attributes here.
                    //  This class cannot take any parameters to it's constructor,
                    //  and there is no way to map from the node type object to the
                    //  precalculated static objects in "Schema". So we have to query again.
                    //  I'm presuming that "GetAttributeInfo" in DomNodeType is not too
                    //  inefficient, but to avoid a lot of work when loading a huge
                    //  hierarchy of nodes, let's calculate it on demand
                if (m_transformAttrib == null)
                {
                    var node = DomNode;
                    if (node==null) return null;

                    m_transformAttrib = new Schema.transformAttributes(node.Type);
                }

                return m_transformAttrib;
            }
        }

        private ITransformable m_transformable;
        private Schema.transformAttributes m_transformAttrib = null;
    }
}
