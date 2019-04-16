using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Sce.Atf.VectorMath;

namespace AuthoringConcept.AdaptiveEditing
{
    public class VanillaStorage : IDataBlock
    {
        public virtual int GetInt(string label)
        {
            if (!_ints.TryGetValue(label, out int value))
                return 0;
            return value;
        }

        public virtual void SetInt(string label, int newValue)
        {
            _ints[label] = newValue;
        }

        public virtual float GetFloat(string label)
        {
            if (!_floats.TryGetValue(label, out float value))
                return 0.0f;
            return value;
        }

        public virtual void SetFloat(string label, float newValue)
        {
            _floats[label] = newValue;
        }

        public virtual bool GetBool(string label)
        {
            if (!_bools.TryGetValue(label, out bool value))
                return false;
            return value;
        }

        public virtual void SetBool(string label, bool newValue)
        {
            _bools[label] = newValue;
        }

        public virtual string GetString(string label)
        {
            if (!_strings.TryGetValue(label, out string value))
                return null;
            return value;
        }

        public virtual void SetString(string label, string newValue)
        {
            _strings[label] = newValue;
        }

        public virtual Vec2F GetFloat2(string label)
        {
            if (!_float2s.TryGetValue(label, out Vec2F value))
                return new Vec2F(0.0f, 0.0f);
            return value;
        }

        public virtual void SetFloat2(string label, Vec2F newValue)
        {
            _float2s[label] = newValue;
        }

        public virtual Vec3F GetFloat3(string label)
        {
            if (!_float3s.TryGetValue(label, out Vec3F value))
                return new Vec3F(0.0f, 0.0f, 0.0f);
            return value;
        }

        public virtual void SetFloat3(string label, Vec3F newValue)
        {
            _float3s[label] = newValue;
        }

        public virtual Vec4F GetFloat4(string label)
        {
            if (!_float4s.TryGetValue(label, out Vec4F value))
                return new Vec4F(0.0f, 0.0f, 0.0f, 0.0f);
            return value;
        }

        public virtual void SetFloat4(string label, Vec4F newValue)
        {
            _float4s[label] = newValue;
        }

        public virtual bool HasValue(string label)
        {
            return _bools.ContainsKey(label)
                || _ints.ContainsKey(label)
                || _strings.ContainsKey(label)
                || _floats.ContainsKey(label)
                || _float2s.ContainsKey(label)
                || _float3s.ContainsKey(label)
                || _float4s.ContainsKey(label)
                ;
        }

        public virtual void RemoveValue(string label)
        {
            _bools.Remove(label);
            _ints.Remove(label);
            _strings.Remove(label);
            _floats.Remove(label);
            _float2s.Remove(label);
            _float3s.Remove(label);
            _float4s.Remove(label);
        }

        private Dictionary<string, bool> _bools = new Dictionary<string, bool>();
        private Dictionary<string, int> _ints = new Dictionary<string, int>();
        private Dictionary<string, float> _floats = new Dictionary<string, float>();
        private Dictionary<string, string> _strings = new Dictionary<string, string>();

        private Dictionary<string, Vec2F> _float2s = new Dictionary<string, Vec2F>();
        private Dictionary<string, Vec3F> _float3s = new Dictionary<string, Vec3F>();
        private Dictionary<string, Vec4F> _float4s = new Dictionary<string, Vec4F>();

        public string Identifier { get { return String.Empty; } }
        public string TypeIdentifier { get { return String.Empty; } }
    }
}
