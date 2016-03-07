using System;
using System.Collections.Generic;
using System.Linq;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;
using Sce.Atf.VectorMath;

using LevelEditorCore;
using LevelEditorCore.GenericAdapters.Extensions;

namespace LevelEditor.DomNodeAdapters
{
    using TransformableST = Schema.transformObjectType;

    public class TransformObject : DomNodeAdapter, ITransformable
    {
        #region ITransformable Members

        public virtual void UpdateTransform()
        {
            this.SetMatrix4x4(TransformableST.transformAttribute, TransformUtils.CalcTransform(this));
        }

        /// <summary>
        /// Gets and sets the local transformation matrix</summary>
        public virtual Matrix4F Transform
        {
            get { return this.GetMatrix4x4(TransformableST.transformAttribute); }
            set
            {
                Translation = value.Translation;
                Scale = value.GetScale();
                Vec3F rot = new Vec3F();
                value.GetEulerAngles(out rot.X, out rot.Y, out rot.Z);
                Rotation = rot;
                UpdateTransform();
            }
        }

        /// <summary>
        /// Gets and sets the node translation</summary>
        public virtual Vec3F Translation
        {
            get { return this.GetVec3(TransformableST.translateAttribute); }
            set
            {
                if ((TransformationType & TransformationTypes.Translation) == 0)
                    return;
                this.SetVec3(TransformableST.translateAttribute, value);
            }
        }

        /// <summary>
        /// Gets and sets the node rotation</summary>
        public virtual Vec3F Rotation
        {
            get { return this.GetVec3(TransformableST.rotateAttribute); }
            set
            {
                if ((TransformationType & TransformationTypes.Rotation) == 0)
                    return;
                this.SetVec3(TransformableST.rotateAttribute, value);
            }
        }

        /// <summary>
        /// Gets and sets the node scale</summary>
        public virtual Vec3F Scale
        {
            get { return this.GetVec3(TransformableST.scaleAttribute); }
            set
            {
                if ((TransformationType & TransformationTypes.Scale) == 0
                    && (TransformationType & TransformationTypes.UniformScale) == 0)
                    return;

                this.SetVec3(TransformableST.scaleAttribute, value);
            }
        }

        /// <summary>
        /// Gets and sets the node scale pivot</summary>
        public virtual Vec3F Pivot
        {
            get { return this.GetVec3(TransformableST.pivotAttribute); }
            set
            {
                if ((TransformationType & TransformationTypes.Pivot) == 0)
                    return;
                this.SetVec3(TransformableST.pivotAttribute, value);
            }
        }

        /// <summary>
        /// Gets or sets the type of transformation this object can support. By default
        /// all transformation types are supported.</summary>
        public TransformationTypes TransformationType
        {
            get { return GetAttribute<TransformationTypes>(TransformableST.transformationTypeAttribute); }
            set
            {
                int v = (int)value;
                SetAttribute(TransformableST.transformationTypeAttribute, v);
            }
        }

        public Matrix4F LocalToWorld
        {
            get
            {
                Matrix4F world = Transform;
                var node = this.As<DomNode>();
                if (node != null)
                {
                    foreach (var n in node.Lineage.Skip(1))
                    {
                        var xformNode = n.As<ITransformable>();
                        if (xformNode != null)
                            world.Mul(world, xformNode.Transform);
                    }
                }
                return world;
            }
        }

        #endregion
    }
}
