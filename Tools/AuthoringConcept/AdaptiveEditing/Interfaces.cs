using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Sce.Atf.VectorMath;

namespace AuthoringConcept.AdaptiveEditing
{
    public interface IDataBlock
    {
        int GetInt(string label);
        void SetInt(string label, int newValue);
        float GetFloat(string label);
        void SetFloat(string label, float newValue);
        bool GetBool(string label);
        void SetBool(string label, bool newValue);

        Vec2F GetFloat2(string label);
        void SetFloat2(string label, Vec2F newValue);
        Vec3F GetFloat3(string label);
        void SetFloat3(string label, Vec3F newValue);
        Vec4F GetFloat4(string label);
        void SetFloat4(string label, Vec4F newValue);

        bool HasValue(string label);
        void RemoveValue(string label);

        string Identifier { get; }
        string TypeIdentifier { get; }
    }

    public interface IDataBlockDeclaration
    {
        IDataBlock CreateStorage(Sce.Atf.Dom.DomDocument document, Sce.Atf.Dom.DomNode parent);
        void PerformLayout(ImmediateGUI.Arbiter gui, IDataBlock storage);
    }

    public class DataBlockDeclarationHelper
    {
        public class Member<T>
        {
            public T Default;
            public string NativeProperty;
        }

        public Dictionary<string, Member<bool>> Bools = new Dictionary<string, Member<bool>>();
        public Dictionary<string, Member<int>> Ints = new Dictionary<string, Member<int>>();
        public Dictionary<string, Member<float>> Floats = new Dictionary<string, Member<float>>();
        public Dictionary<string, Member<Vec2F>> Float2s = new Dictionary<string, Member<Vec2F>>();
        public Dictionary<string, Member<Vec3F>> Float3s = new Dictionary<string, Member<Vec3F>>();
        public Dictionary<string, Member<Vec4F>> Float4s = new Dictionary<string, Member<Vec4F>>();

        public Member<bool> Bool(string name, bool def)
        {
            var member = new Member<bool> { Default = def };
            Bools.Add(name, member);
            return member;
        }

        public Member<int> Int(string name, int def)
        {
            var member = new Member<int> { Default = def };
            Ints.Add(name, member);
            return member;
        }

        public Member<float> Float(string name, float def)
        {
            var member = new Member<float> { Default = def };
            Floats.Add(name, member);
            return member;
        }

        public Member<Vec2F> Float2(string name, Vec2F def)
        {
            var member = new Member<Vec2F> { Default = def };
            Float2s.Add(name, member);
            return member;
        }

        public Member<Vec3F> Float3(string name, Vec3F def)
        {
            var member = new Member<Vec3F> { Default = def };
            Float3s.Add(name, member);
            return member;
        }

        public Member<Vec4F> Float4(string name, Vec4F def)
        {
            var member = new Member<Vec4F> { Default = def };
            Float4s.Add(name, member);
            return member;
        }

        public void SetAllDefaults(IDataBlock dataBlock)
        {
            foreach (var b in Bools)
                dataBlock.SetBool(b.Key, b.Value.Default);
            foreach (var b in Ints)
                dataBlock.SetInt(b.Key, b.Value.Default);
            foreach (var b in Floats)
                dataBlock.SetFloat(b.Key, b.Value.Default);
            foreach (var b in Float2s)
                dataBlock.SetFloat2(b.Key, b.Value.Default);
            foreach (var b in Float3s)
                dataBlock.SetFloat3(b.Key, b.Value.Default);
            foreach (var b in Float4s)
                dataBlock.SetFloat4(b.Key, b.Value.Default);
        }
    }

    public abstract class AdaptiveSchemaSource
    {
        public IDataBlockDeclaration FindDataBlock(string name)
        {
            var split = name.Split(':');
            if (split.Length >= 2)
            {
                foreach (var script in SchemaScripts)
                {
                    if (string.Compare(split[0], script.Key) != 0) continue;
                    if (script.Value.DataBlockDeclarations.TryGetValue(split[1], out IDataBlockDeclaration dataBlockDecl))
                    {
                        return dataBlockDecl;
                    }
                }
            }
            return null;
        }

        public interface ISchemaScript
        {
            IDictionary<string, IDataBlockDeclaration> DataBlockDeclarations { get; }
        }

        protected virtual IEnumerable<KeyValuePair<string, ISchemaScript>> SchemaScripts { get; }
    }
}
