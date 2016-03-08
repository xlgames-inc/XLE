//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Windows.Forms;

using Sce.Atf;
using Sce.Atf.Applications;
using Sce.Atf.Controls.PropertyEditing;
using Sce.Atf.Adaptation;


namespace LevelEditorCore.Commands
{
    [Export(typeof (IInitializable))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class PickFilterCommands : IInitializable
    {

        #region IInitializable Members

        void IInitializable.Initialize()
        {

            if (m_pickFilters == null)
            {
                m_designView.PickFilter = new DefaultPickFilter();
                return;

            }
                
            m_pickFilterComboBox = new ToolStripComboBox();
            m_pickFilterComboBox.BeginUpdate();
            m_pickFilterComboBox.DropDownStyle = ComboBoxStyle.DropDownList;
            m_pickFilterComboBox.Name = "pickCombo";
            m_pickFilterComboBox.ToolTipText = "Picking Filter".Localize();


            var defFilter = new DefaultPickFilter();
            m_filters.Add(defFilter.Name, defFilter);
            m_pickFilterComboBox.Items.Add(defFilter.Name);

            var defFilterIgnoreGroups = new DefaultPickFilterIgnoreGroups();
            m_filters.Add(defFilterIgnoreGroups.Name, defFilterIgnoreGroups);
            m_pickFilterComboBox.Items.Add(defFilterIgnoreGroups.Name);
                                  
            foreach (IPickFilter pickFilter in m_pickFilters)
            {
                m_filters.Add(pickFilter.Name, pickFilter);
                m_pickFilterComboBox.Items.Add(pickFilter.Name);

            }
            m_designView.PickFilter = defFilter;
            m_pickFilterComboBox.SelectedIndex = 0;

            m_pickFilterComboBox.EndUpdate();            
            MenuInfo.Edit.GetToolStrip().Items.Add(m_pickFilterComboBox);
            m_pickFilterComboBox.SelectedIndexChanged += PickpickFilterComboBox_SelectedIndexChanged;

            m_settingsService.RegisterSettings(this,new BoundPropertyDescriptor(this, () => ActivePickFilter, "ActiveFilter",null, null));

        }
        
        #endregion
        
        private void PickpickFilterComboBox_SelectedIndexChanged(object sender, EventArgs e)
        {
            ToolStripComboBox pickFilterComboBox = (ToolStripComboBox) sender;
            string filterName =(string) pickFilterComboBox.SelectedItem;
            m_designView.PickFilter = m_filters[filterName];
        }


        private string ActivePickFilter
        {
            get { return (string) m_pickFilterComboBox.SelectedItem; }
            set
            {                
                if(m_filters.ContainsKey(value))
                {
                    m_pickFilterComboBox.SelectedItem = value;
                }
                
            }
        }

        

        [Import(AllowDefault = false)] 
        private IDesignView m_designView = null;

        [ImportMany] 
        private IEnumerable<IPickFilter> m_pickFilters = null;

        [Import(AllowDefault = false)] 
        private ISettingsService m_settingsService = null;

        private ToolStripComboBox m_pickFilterComboBox;

        private Dictionary<string, IPickFilter> m_filters
            = new Dictionary<string, IPickFilter>();

        private class DefaultPickFilter : IPickFilter
        {
            #region IPickFilter Members
            public string Name { get { return "Any Object"; } }
            public object Filter(object obj, MouseEventArgs e) 
            {
                var path = obj.As<Path<object>>();
                if (path != null)
                {
                    // Search through the path to look for a group of some kind.
                    // If we find it, we must select the group, not the original object.
                    for (int c = 0; c < path.Count; ++c)
                    {
                        if (path[c].Is<ITransformableGroup>())
                            return path[c];
                    }
                }
                return obj; 
            }
            #endregion
        }

        private class DefaultPickFilterIgnoreGroups : IPickFilter
        {
            #region IPickFilter Members
            public string Name { get { return "Any Object (ignore groups)"; } }
            public object Filter(object obj, MouseEventArgs e) { return obj; }
            #endregion
        }
    }
}
