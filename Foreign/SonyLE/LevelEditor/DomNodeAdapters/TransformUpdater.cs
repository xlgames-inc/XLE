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
            if (e.AttributeInfo.Equivalent(Schema.transformObjectType.pivotAttribute))
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
            SetAttribute(Schema.transformObjectType.transformAttribute, xform.ToArray());
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
            return (attributeInfo.Equivalent(Schema.transformObjectType.translateAttribute)
                    || attributeInfo.Equivalent(Schema.transformObjectType.rotateAttribute)
                    || attributeInfo.Equivalent(Schema.transformObjectType.scaleAttribute)
                    || attributeInfo.Equivalent(Schema.transformObjectType.pivotAttribute));
        }

        private ITransformable m_transformable;
    }
}
