// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;

using LevelEditorCore;

namespace LevelEditorXLE.Environment
{
    public class EnvUtility : DomNodeAdapter, IListable
    {
        void OnAttributeChanged(object sender, AttributeEventArgs e)
        {
            if (e.AttributeInfo.Equivalent(Schema.envUtilityType.SunAngleAttribute))
            {
                float newAngle = (float)(SunAngle * Math.PI / 180.0f);

                    // We must get the axis of sun movement (which is actually a property of
                    // the terrain), and then move the sun light to a position along this
                    // axis).
                var sun = SunNode;
                if (sun == null) return;

                var sceneMan = GUILayer.NativeManipulatorLayer.SceneManager;
                float sunPathAngle = GUILayer.EditorInterfaceUtils.GetSunPathAngle(sceneMan);

                var x = (float)(Math.Sin(newAngle) * Math.Cos(sunPathAngle));
                var y = (float)(Math.Sin(newAngle) * Math.Sin(sunPathAngle));
                var z = (float)Math.Cos(newAngle);

                const float distance = 100.0f;  // (distance away from the origin -- actually doesn't matter)
                sun.SetAttribute(
                    Schema.transformObjectType.translateAttribute,
                    new float[3] { distance * x, distance * y, distance * z });
            }
        }

        public float SunAngle
        {
            get { return GetAttribute<float>(Schema.envUtilityType.SunAngleAttribute); }
        }

        public string SunName
        {
            get { return GetAttribute<string>(Schema.envUtilityType.SunNameAttribute); }
        }

        public DomNode SunNode
        {
            get 
            {
                    // Our parent should be an XLEEnvSettings. We will look for a child 
                    // object that has the name 
                var envSettings = DomNode.Parent.As<XLEEnvSettings>();
                if (envSettings == null) return null;
                return envSettings.FindObjectByName(SunName);
            }
        }

        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = "EnvUtility";
        }

        protected override void OnNodeSet()
        {
            if (DomNode != _attachedNode)
            {
                if (_attachedNode != null)
                    _attachedNode.AttributeChanged -= OnAttributeChanged;
                DomNode.AttributeChanged += OnAttributeChanged;
                _attachedNode = DomNode;
            }
        }
        
        private DomNode _attachedNode;
    }
}
