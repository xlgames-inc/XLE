using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.ComponentModel.Composition;

using Sce.Atf.Applications;


namespace LevelEditorXLE.Terrain
{
    public partial class TerrainContextControls : UserControl
    {
        public TerrainContextControls(IContextRegistry contextRegistry)
        {
            InitializeComponent();
            contextRegistry.ActiveContextChanged += OnActiveContextChanged;
        }

        GUILayer.TerrainManipulatorContext Context
        {
            set
            {
                _attached = value;
                if (_attached != null)
                {
                    _showLockedArea.CheckState = _attached.ShowLockedArea ? CheckState.Checked : CheckState.Unchecked;
                }
                else
                {
                    _showLockedArea.CheckState = CheckState.Unchecked;
                }
            }
            get { return _attached; }
        }

        XLETerrainGob Terrain
        {
            set 
            {
                if (value != null)
                {
                    _coverageLayer.DisplayMember = "Value";
                    _coverageLayer.Items.Clear();
                    _coverageLayer.Items.Add(new KeyValuePair<uint, string>(1, "Heights"));
                    foreach (var l in value.CoverageLayers)
                        _coverageLayer.Items.Add(new KeyValuePair<uint, string>(l.LayerId, l.LayerName));

                    if (_attached != null)
                    {
                        for (int c = 0; c < _coverageLayer.Items.Count; ++c)
                            if (((KeyValuePair<uint, string>)_coverageLayer.Items[c]).Key == _attached.ActiveLayer)
                            {
                                _coverageLayer.SelectedIndex = c;
                                break;
                            }
                    }
                }
            }
        }

        private GUILayer.TerrainManipulatorContext _attached;

        private void OnActiveContextChanged(object obj, EventArgs args)
        {
            IContextRegistry registry = obj as IContextRegistry;
            if (registry != null)
            {
                var gameExt = registry.GetActiveContext<Game.GameExtensions>();
                if (gameExt != null)
                {
                    Context = gameExt.TerrainManipulatorContext;
                    Terrain = gameExt.Terrain;
                }
            }
        }

        private void _showLockedArea_CheckedChanged(object sender, EventArgs e)
        {
            if (_attached != null)
                _attached.ShowLockedArea = _showLockedArea.CheckState == CheckState.Checked;
        }
    }
}
