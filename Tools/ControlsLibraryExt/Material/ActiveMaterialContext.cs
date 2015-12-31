// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.ComponentModel.Composition;

namespace ControlsLibraryExt
{
    [Export(typeof(ActiveMaterialContext))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ActiveMaterialContext
    {
        public string MaterialName
        {
            get { return m_materialName; }
            set
            {
                if (value != m_materialName)
                {
                    m_materialName = value;
                    OnChange();
                }
            }
        }
        public string PreviewModelName { get; set; }
        public ulong PreviewModelBinding { get; set; }
        public delegate void OnChangeDelegate();
        public event OnChangeDelegate OnChange;

        private string m_materialName = null;
    }
}
