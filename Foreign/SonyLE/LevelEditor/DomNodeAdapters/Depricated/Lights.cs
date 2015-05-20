//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.


using System.Drawing;


using Sce.Atf.VectorMath;

using LevelEditorCore;

namespace LevelEditor.DomNodeAdapters
{
    public class PointLight : GameObject
    {
        protected override void OnNodeSet()
        {
            base.OnNodeSet();
            DomNodeUtil.SetVector(DomNode, Schema.transformObjectType.scaleAttribute, new Vec3F(0.4f, 0.4f, 0.4f)); 
            UpdateTransform();
            TransformationType = TransformationTypes.Translation;            
        }
    }

    public class BoxLight : GameObject
    {
        protected override void OnNodeSet()
        {
            base.OnNodeSet();                        
            TransformationType = TransformationTypes.Translation | TransformationTypes.Scale;
        }
    }
}
