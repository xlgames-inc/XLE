//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System;
using System.Collections.Generic;
using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Dom;
using Sce.Atf.Applications;

using LevelEditorCore;

namespace LevelEditor.DomNodeAdapters
{
    /// <summary>
    /// Prefab instance.</summary>
    public class PrefabInstance : TransformableGroup, IPrefabInstance, IListable
    {
        public static PrefabInstance Create(IPrefab prefab)
        {
            DomNode instNode = new DomNode(Schema.prefabInstanceType.Type);
            PrefabInstance inst = instNode.As<PrefabInstance>();
            inst.m_prefab = prefab;
            instNode.InitializeExtensions();
            inst.Resolve(null);
            return inst;
        }

        protected IEnumerable<object> GameObjects
        {
            get { return GetChildList<object>(Schema.prefabInstanceType.gameObjectChild);}
        }

        protected override void OnNodeSet()
        {
            base.OnNodeSet();
            DomNode.ChildRemoving += new EventHandler<ChildEventArgs>(DomNodeStructureChanged);
            DomNode.ChildInserting += new EventHandler<ChildEventArgs>(DomNodeStructureChanged);
            DomNode.AttributeChanged += new EventHandler<AttributeEventArgs>(DomNode_AttributeChanged);
       
            m_overrideList = GetChildList<ObjectOverride>(Schema.prefabInstanceType.objectOverrideChild);
            foreach (var objectOverride in m_overrideList)
            {
                m_overridesMap.Add(objectOverride.ObjectName, objectOverride);
            }

            if(DomNode.Parent == null)
                Resolve(null);
        }

        private IList<ObjectOverride> m_overrideList;
        private void DomNode_AttributeChanged(object sender, AttributeEventArgs e)
        {
            if (e.DomNode == this.DomNode)
                return;

            if (!e.AttributeInfo.Equivalent(e.DomNode.Type.IdAttribute))
            {
                string originalName;
                if (m_intsToOriginal.TryGetValue(e.DomNode, out originalName))
                {
                    ObjectOverride objectOverride;
                    m_overridesMap.TryGetValue(originalName, out objectOverride);
                    if (objectOverride == null)
                    {
                        objectOverride = ObjectOverride.Create(originalName);
                        m_overrideList.Add(objectOverride);
                        m_overridesMap.Add(originalName, objectOverride);
                    }

                    AttributeOverride attrOverride = objectOverride.GetOrCreateByName(e.AttributeInfo.Name);
                    attrOverride.AttribValue = e.AttributeInfo.Type.Convert(e.NewValue);
                }
            }
        }

        private void DomNodeStructureChanged(object sender, ChildEventArgs e)
        {
            if (!m_updating && e.ChildInfo.Equivalent(Schema.prefabInstanceType.gameObjectChild))
            {
                throw new InvalidTransactionException("Structure of PrefabInstance cannot be changesd");
            }
        }

        private string GetName(object obj, int index)
        {
            var nameable = obj.As<INameable>();
            if (nameable != null) return nameable.Name;
                // Without a name, we just call back to the index in the list
                // We could do better -- this can create a lot of confusion if the objects in the
                // prefab file change order.
            return index.ToString();
        }

        private bool m_updating;
        public void Resolve(UniqueNamer namer)
        {
            try
            {
                m_updating = true;
                Uri resUri = GetAttribute<Uri>(Schema.prefabInstanceType.prefabRefAttribute);
                if (resUri != null)
                    m_prefab = Globals.ResourceService.Load(resUri) as IPrefab;
                if (m_prefab == null) return;

                // update name and uri                
                if(resUri == null)
                    SetAttribute(Schema.prefabInstanceType.prefabRefAttribute, m_prefab.Uri);

                // Remove the children that are stored within this group (these remain in the file
                // so that they are used when the resolve fails)
                var childrenCopy = new List<DomNode>(base.Objects.AsIEnumerable<DomNode>());
                foreach (var c in childrenCopy) c.RemoveFromParent();

                // Create new children from the prefab, applying the overriden attributes as we go
                DomNode[] gobs = DomNode.Copy(m_prefab.GameObjects.AsIEnumerable<DomNode>());
                HashSet<string> gobIds = new HashSet<string>(); 
                for (int c = 0; c < gobs.Length; ++c)
                {
                    var gobNode = gobs[c];
                    gobNode.InitializeExtensions();
                    var name = GetName(gobNode, c);

                    m_intsToOriginal.Add(gobNode, name);
                    gobIds.Add(name);
                    ObjectOverride objectOverride;
                    m_overridesMap.TryGetValue(name, out objectOverride);
                    updateNode(gobNode, objectOverride);

                    if (namer != null)
                    {
                        var nameable = gobNode.As<INameable>();
                        if (nameable != null)
                            nameable.Name = namer.Name(nameable.Name);
                    }

                    AddChild(gobNode);
                }

                // cleanup m_overridesmap
                List<string> overrideIds = new List<string>(m_overridesMap.Keys);
                foreach (string id in overrideIds)
                {
                    if (!gobIds.Contains(id))
                    {
                        ObjectOverride objectOverride = m_overridesMap[id];
                        objectOverride.DomNode.RemoveFromParent();
                        m_overridesMap.Remove(id);
                        m_overrideList.Remove(objectOverride);
                    }
                }
            }
            finally
            {
                m_updating = false;
            }
        }

        private void updateNode(DomNode node, ObjectOverride objectOverride)
        {
            if (node == null || objectOverride == null)
                return;
            foreach (AttributeOverride attrOverride in objectOverride.AttributeOverrides)
            {
                AttributeInfo attrInfo = node.Type.GetAttributeInfo(attrOverride.Name);
                node.SetAttribute(attrInfo, attrInfo.Type.Convert(attrOverride.AttribValue));
            }
        }

        #region IListable Members
        public new void GetInfo(ItemInfo info)
        {
            base.GetInfo(info);
            info.Label = "Prefab: " + ((m_prefab != null) ? m_prefab.Name : "<<unattached>>");
        }
        #endregion

        // maps the id of the instance to object override
        private Dictionary<string, ObjectOverride>
            m_overridesMap = new Dictionary<string, ObjectOverride>();
            
        // maps instances node to the Id of the original node
        private Dictionary<DomNode, string> m_intsToOriginal
            = new Dictionary<DomNode, string>();
        private IPrefab m_prefab;
    }
    
    public class AttributeOverride : DomNodeAdapter
    {
        public static AttributeOverride Create(string name, string value)
        {
            if (string.IsNullOrWhiteSpace(name))
                throw new InvalidOperationException("name cannot be null or empty");

            DomNode node = new DomNode(Schema.attributeOverrideType.Type);
            AttributeOverride attrOverride = node.As<AttributeOverride>();
            attrOverride.Name = name;
            attrOverride.AttribValue = value;
            return attrOverride;
        }
        public string Name
        {
            get { return GetAttribute<string>(Schema.attributeOverrideType.nameAttribute); }
            private set 
            {
                if (string.IsNullOrWhiteSpace(value))
                    throw new InvalidTransactionException("name cannot be null");
                SetAttribute(Schema.attributeOverrideType.nameAttribute, value);
            }
        }

        public string AttribValue
        {
            get { return GetAttribute<string>(Schema.attributeOverrideType.valueAttribute); }
            set { SetAttribute(Schema.attributeOverrideType.valueAttribute, value); }
        }
    }
    public class ObjectOverride : DomNodeAdapter
    {
        public static ObjectOverride Create(string name)
        {
            DomNode node = new DomNode(Schema.objectOverrideType.Type);
            ObjectOverride objectOverride = node.As<ObjectOverride>();
            objectOverride.ObjectName = name;
            return objectOverride;
        }

        protected override void OnNodeSet()
        {
            base.OnNodeSet();
            m_overrides = GetChildList<AttributeOverride>(Schema.objectOverrideType.attributeOverrideChild); 
        }
        public string ObjectName
        {
            get{return GetAttribute<string>(Schema.objectOverrideType.objectNameAttribute);}
            private set
            {
                if (string.IsNullOrWhiteSpace(value))
                    throw new InvalidTransactionException("type name cannot be empty or null");
                SetAttribute(Schema.objectOverrideType.objectNameAttribute, value);
            }
        }

        /// <summary>
        /// Gets AttributeOverride by name
        /// if not found then it will create new instacne</summary>        
        public AttributeOverride GetOrCreateByName(string name)
        {
            if (string.IsNullOrWhiteSpace(name))
                throw new InvalidOperationException("name cannot be null or empty");

            //note:linear search is OK there are not many items.
            // find AttributeOverride by name.
            foreach (var attrib in m_overrides)
            {
                if (attrib.Name == name)
                    return attrib;
            }

            // if not found  create new instance and add it to the list.
            AttributeOverride attrOverride = AttributeOverride.Create(name, "");
            m_overrides.Add(attrOverride);
            return attrOverride;
        }
        public IEnumerable<AttributeOverride> AttributeOverrides
        {
            get { return m_overrides; }
        }

        private IList<AttributeOverride> m_overrides;        
    }
}
