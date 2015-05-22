// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;

using LevelEditorCore;

namespace LevelEditorXLE.Environment
{
    public class XLEEnvSettings : DomNodeAdapter, IHierarchical, IListable
    {
        public bool CanAddChild(object child)
        {
            var domNode = child as DomNode;
            if (domNode != null)
            {
                foreach (var type in domNode.Type.Lineage)
                {
                    if (type == Schema.envObjectType.Type) return true;
                    if (type == Schema.envMiscType.Type) return true;
                    if (type == Schema.ambientSettingsType.Type) return true;
                    if (type == Schema.toneMapSettingsType.Type) return true;
                }
            }
            return false;
        }

        public bool AddChild(object child)
        {
            var domNode = child.As<DomNode>();
            if (domNode != null)
            {
                foreach (var type in domNode.Type.Lineage)
                {
                    if (type == Schema.envObjectType.Type)
                    {
                        GetChildList<DomNode>(Schema.envSettingsType.objectsChild).Add(domNode);
                        return true;
                    }
                    if (type == Schema.envMiscType.Type)
                    {
                        GetChildList<DomNode>(Schema.envSettingsType.settingsChild).Add(domNode);
                        return true;
                    }
                    if (type == Schema.ambientSettingsType.Type)
                    {
                        SetChild(Schema.envSettingsType.ambientChild, domNode);
                        return true;
                    }
                    if (type == Schema.toneMapSettingsType.Type)
                    {
                        SetChild(Schema.envSettingsType.tonemapChild, domNode);
                        return true;
                    }
                }
            }
            return false;
        }

        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = "Settings: " + Name;
        }

        public string Name
        {
            get { return GetAttribute<string>(Schema.envSettingsType.nameAttribute); }
        }

        public static DomNode Create(string name)
        {
            var result = new DomNode(Schema.envSettingsType.Type);
            result.SetAttribute(Schema.envSettingsType.nameAttribute, name);
            return result;
        }
    }

    public class XLEEnvSettingsFolder : DomNodeAdapter, IHierarchical, IListable
    {
        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = "EnvSettingsFolder";
        }

        public bool CanAddChild(object child)
        {
            return child.Is<XLEEnvSettings>();
        }

        public bool AddChild(object child)
        {
            var settings = child.As<XLEEnvSettings>();
            if (settings != null) {
                Settings.Add(settings);
                return true;
            }
            return false;
        }

        public System.Collections.Generic.ICollection<XLEEnvSettings> Settings
        {
            get
            {
                return GetChildList<XLEEnvSettings>(Schema.envSettingsFolderType.settingsChild);
            }
        }
        
        public string GetNameForNewChild()
        {
            var settings = Settings;
            string result = "environment";
            if (!settings.Any(s => s.Name.Equals(result)))
                return result;

            uint postfix = 1;
            for (; ; ++postfix)
            {
                result = String.Format("environment{0}", postfix);
                if (!settings.Any(s => s.Name.Equals(result)))
                    return result;
            }
        }

        public static DomNode Create()
        {
            return new DomNode(Schema.envSettingsFolderType.Type);
        }
    }

}