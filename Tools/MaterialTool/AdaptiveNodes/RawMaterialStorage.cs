using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Sce.Atf.VectorMath;

namespace MaterialTool.AdaptiveNodes
{
    class RawMaterialStorage : AuthoringConcept.AdaptiveEditing.IDataBlock
    {
        public void SetInt(string label, int newValue)
        {
            SetValue(label, newValue.ToString() + "i");
        }

        public void SetFloat(string label, float newValue)
        {
            SetValue(label, newValue.ToString() + "f");
        }

        public void SetBool(string label, bool newValue)
        {
            SetValue(label, newValue ? "true" : "false");
        }

        public void SetFloat2(string label, Vec2F newValue)
        {
            SetValue(label, "{" + newValue.X.ToString() + "f, " + newValue.Y.ToString() + "f}");
        }

        public void SetFloat3(string label, Vec3F newValue)
        {
            SetValue(label, "{" + newValue.X.ToString() + "f, " + newValue.Y.ToString() + "f, " + newValue.Z.ToString() + "f}");
        }

        public void SetFloat4(string label, Vec4F newValue)
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

        public bool GetBool(string label)
        {
            if (Material.TryGetConstantBool(label, out bool value))
            {
                return value;
            }
            return false;
        }

        public int GetInt(string label)
        {
            if (Material.TryGetConstantInt(label, out int value))
            {
                return value;
            }
            return 0;   // default value when no existing value
        }

        public float GetFloat(string label)
        {
            if (Material.TryGetConstantFloat(label, out float value))
            {
                return value;
            }
            return 0.0f;   // default value when no existing value
        }

        public Vec2F GetFloat2(string label)
        {
            float[] value = new float[2];
            if (Material.TryGetConstantFloat2(label, value))
            {
                return new Vec2F(value[0], value[1]);
            }
            return new Vec2F(0.0f, 0.0f);   // default value when no existing value
        }

        public Vec3F GetFloat3(string label)
        {
            float[] value = new float[3];
            if (Material.TryGetConstantFloat3(label, value))
            {
                return new Vec3F(value[0], value[1], value[2]);
            }
            return new Vec3F(0.0f, 0.0f, 0.0f);   // default value when no existing value
        }

        public Vec4F GetFloat4(string label)
        {
            float[] value = new float[4];
            if (Material.TryGetConstantFloat4(label, value))
            {
                return new Vec4F(value[0], value[1], value[2], value[3]);
            }
            return new Vec4F(0.0f, 0.0f, 0.0f, 0.0f);   // default value when no existing value
        }

        public bool HasValue(string label)
        {
            return Material.HasConstant(label);
        }

        public void RemoveValue(string label)
        {
            Material.RemoveConstant(label);
        }

        public GUILayer.RawMaterial Material;

        public string Identifier { get { return String.Empty; } }
        public string TypeIdentifier { get { return String.Empty; } }
    }
}
