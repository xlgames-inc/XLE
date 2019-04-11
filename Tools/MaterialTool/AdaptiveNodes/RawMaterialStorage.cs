using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Sce.Atf.VectorMath;

namespace MaterialTool.AdaptiveNodes
{
    class RawMaterialStorage : AuthoringConcept.AdaptiveEditing.VanillaStorage
    {
        public override void SetInt(string label, int newValue)
        {
            SetValue(label, newValue.ToString() + "i");
        }

        public override void SetFloat(string label, float newValue)
        {
            SetValue(label, newValue.ToString() + "f");
        }

        public override void SetBool(string label, bool newValue)
        {
            SetValue(label, newValue ? "true" : "false");
        }

        public override void SetFloat2(string label, Vec2F newValue)
        {
            SetValue(label, "{" + newValue.X.ToString() + "f, " + newValue.Y.ToString() + "f}");
        }

        public override void SetFloat3(string label, Vec3F newValue)
        {
            SetValue(label, "{" + newValue.X.ToString() + "f, " + newValue.Y.ToString() + "f, " + newValue.Z.ToString() + "f}");
        }

        public override void SetFloat4(string label, Vec4F newValue)
        {
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

        public override bool GetBool(string label)
        {
            if (Material.TryGetConstantBool(label, out bool value))
            {
                return value;
            }
            return false;
        }

        public override int GetInt(string label)
        {
            if (Material.TryGetConstantInt(label, out int value))
            {
                return value;
            }
            return 0;   // default value when no existing value
        }

        public override float GetFloat(string label)
        {
            if (Material.TryGetConstantFloat(label, out float value))
            {
                return value;
            }
            return 0.0f;   // default value when no existing value
        }

        public override Vec2F GetFloat2(string label)
        {
            float[] value = new float[2];
            if (Material.TryGetConstantFloat2(label, value))
            {
                return new Vec2F(value[0], value[1]);
            }
            return new Vec2F(0.0f, 0.0f);   // default value when no existing value
        }

        public override Vec3F GetFloat3(string label)
        {
            float[] value = new float[3];
            if (Material.TryGetConstantFloat3(label, value))
            {
                return new Vec3F(value[0], value[1], value[2]);
            }
            return new Vec3F(0.0f, 0.0f, 0.0f);   // default value when no existing value
        }

        public override Vec4F GetFloat4(string label)
        {
            float[] value = new float[4];
            if (Material.TryGetConstantFloat4(label, value))
            {
                return new Vec4F(value[0], value[1], value[2], value[3]);
            }
            return new Vec4F(0.0f, 0.0f, 0.0f, 0.0f);   // default value when no existing value
        }

        public GUILayer.RawMaterial Material;
    }
}
