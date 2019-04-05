using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Sce.Atf.VectorMath;

namespace MaterialTool.AdaptiveNodes
{
    class RawMaterialStorage : AuthoringConcept.AdaptiveEditing.VanillaStorage, IDisposable
    {
        public override void SetInt(string label, int newValue)
        {
            base.SetInt(label, newValue);
            SetValue(label, newValue.ToString() + "i");
        }

        public override void SetFloat(string label, float newValue)
        {
            base.SetFloat(label, newValue);
            SetValue(label, newValue.ToString() + "f");
        }

        public override void SetBool(string label, bool newValue)
        {
            base.SetBool(label, newValue);
            SetValue(label, newValue ? "true" : "false");
        }

        public override void SetFloat2(string label, Vec2F newValue)
        {
            base.SetFloat2(label, newValue);
            SetValue(label, "{" + newValue.X.ToString() + "f, " + newValue.Y.ToString() + "f}");
        }

        public override void SetFloat3(string label, Vec3F newValue)
        {
            base.SetFloat3(label, newValue);
            SetValue(label, "{" + newValue.X.ToString() + "f, " + newValue.Y.ToString() + "f, " + newValue.Z.ToString() + "f}");
        }

        public override void SetFloat4(string label, Vec4F newValue)
        {
            base.SetFloat4(label, newValue);
            SetValue(label, "{" + newValue.X.ToString() + "f, " + newValue.Y.ToString() + "f, " + newValue.Z.ToString() + "f, " + newValue.W.ToString() + "f}");
        }

        private void SetValue(string label, string newValue)
        {
            foreach (var i in Material.ShaderConstants)
            {
                if (string.Compare(i.Name, label) == 0)
                {
                    i.Value = newValue;
                    return;
                }
            }
            Material.ShaderConstants.Add(GUILayer.RawMaterial.MakePropertyPair(label, newValue));
        }

        public RawMaterialStorage()
        {
            Material = GUILayer.RawMaterial.CreateUntitled();
        }

        ~RawMaterialStorage()
        {
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (Material != null)
            {
                Material.Dispose();
                Material = null;
            }
        }

        public GUILayer.RawMaterial Material;
    }
}
