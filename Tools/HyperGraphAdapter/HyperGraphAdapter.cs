// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using Sce.Atf;
using Sce.Atf.Controls.Adaptable;

namespace HyperGraphAdapter
{
    class HyperGraphAdapter : HyperGraph.GraphControl, IControlAdapter, ITransformAdapter
    {
        public AdaptableControl AdaptedControl { get; set; }

        public void Bind(AdaptableControl control)
        {
            Unbind(AdaptedControl);
            AdaptedControl = control;
            Attach(control);
        }
        public void BindReverse(AdaptableControl control) { }
        public void Unbind(AdaptableControl control)
        {
            if (control == null) return;
            Detach(control);
        }

        #region ITransformAdapter
        Matrix ITransformAdapter.Transform
        {
            get { return base.Transform; }
        }

        public PointF Translation
        {
            get { return new PointF(Transform.OffsetX, Transform.OffsetY); }
            set { SetTranslation(value); }
        }

        public PointF Scale
        {
            get
            {
                float[] m = Transform.Elements;
                return new PointF(m[0], m[3]);
            }
            set
            {
                SetScale(value);
            }
        }

        public bool EnforceConstraints
        {
            set { _enforceConstraints = value; }
            get { return _enforceConstraints; }
        }

        public PointF MinTranslation
        {
            get { return _minTranslation; }
            set
            {
                _minTranslation = value;
                SetTranslation(Translation); // check translation against new constraints
            }
        }

        public PointF MaxTranslation
        {
            get { return _maxTranslation; }
            set
            {
                _maxTranslation = value;
                SetTranslation(Translation); // check translation against new constraints
            }
        }

        public PointF MinScale
        {
            get { return _minScale; }
            set
            {
                if (value.X <= 0 ||
                    value.X > _maxScale.X ||
                    value.Y <= 0 ||
                    value.Y > _maxScale.Y)
                {
                    throw new ArgumentException("minimum components must be > 0 and less than maximum");
                }

                _minScale = value;
                SetScale(Scale); // check scale against new constraints
            }
        }

        public PointF MaxScale
        {
            get { return _maxScale; }
            set
            {
                if (value.X < _minScale.X ||
                    value.Y < _minScale.Y)
                {
                    throw new ArgumentException("maximum components must be greater than minimum");
                }

                _maxScale = value;
                SetScale(Scale); // check scale against new constraints
            }
        }

        public bool UniformScale
        {
            get { return _uniformScale; }
            set { _uniformScale = value; }
        }

        public void SetTransform(float xScale, float yScale, float xTranslation, float yTranslation)
        {
            PointF scale = EnforceConstraints ? this.ConstrainScale(new PointF(xScale, yScale)) : new PointF(xScale, yScale);
            PointF translation = EnforceConstraints ? this.ConstrainTranslation(new PointF(xTranslation, yTranslation)) : new PointF(xTranslation, yTranslation);
            SetTransformInternal((scale.X + scale.Y) * 0.5f, translation.X, translation.Y);
        }

        private void SetTranslation(PointF translation)
        {
            translation = EnforceConstraints ? this.ConstrainTranslation(translation) : translation;
            SetTransformInternal(_zoom, translation.X, translation.Y);
        }

        private void SetScale(PointF scale)
        {
            scale = EnforceConstraints ? this.ConstrainScale(scale) : scale;
            SetTransformInternal((scale.X + scale.Y) * 0.5f, _translation.X, _translation.Y);
        }

        public void SetTransformInternal(float zoom, float xTranslation, float yTranslation)
        {
            bool transformChanged = false;
            if (_zoom != zoom)
            {
                _zoom = zoom;
                transformChanged = true;
            }

            if (_translation.X != xTranslation || _translation.Y != xTranslation)
            {
                _translation = new PointF(xTranslation, yTranslation);
                transformChanged = true;
            }

            if (transformChanged)
            {
                UpdateMatrices();
                TransformChanged.Raise(this, EventArgs.Empty);
                if (AdaptedControl != null)
                    AdaptedControl.Invalidate();
            }
        }

        public event EventHandler TransformChanged;

        private PointF _minTranslation = new PointF(float.MinValue, float.MinValue);
        private PointF _maxTranslation = new PointF(float.MaxValue, float.MaxValue);
        private PointF _minScale = new PointF(float.MinValue, float.MinValue);
        private PointF _maxScale = new PointF(float.MaxValue, float.MaxValue);
        private bool _uniformScale = true;
        private bool _enforceConstraints = false;
        #endregion
    }
}
