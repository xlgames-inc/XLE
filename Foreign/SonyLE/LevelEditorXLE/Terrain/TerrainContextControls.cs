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
using Sce.Atf.Adaptation;


namespace LevelEditorXLE.Terrain
{
    public partial class TerrainContextControls : UserControl
    {
        public TerrainContextControls(IContextRegistry contextRegistry)
        {
            InitializeComponent();
            contextRegistry.ActiveContextChanged += OnActiveContextChanged;
        }

        protected override void Dispose(bool disposing)
        {
            DetachOldTerrain();
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        GUILayer.TerrainManipulatorContext Context
        {
            set
            {
                _attached = value;
                if (_attached != null)
                {
                    _showLockedArea.CheckState = _attached.ShowLockedArea ? CheckState.Checked : CheckState.Unchecked;
                    SyncActiveLayer();
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
                DetachOldTerrain();
                if (value != null)
                {
                    BuildLayerList(value);
                        // todo --  We need to catch change events for this coverage layers list
                    value.DomNode.ChildInserted += AttachedTerrainChanged;
                    value.DomNode.ChildRemoved += AttachedTerrainChanged;
                    _attachedTerrain = new WeakReference(value);
                }
            }
        }

        void AttachedTerrainChanged(object sender, Sce.Atf.Dom.ChildEventArgs e)
        {
            var terrain = sender.As<XLETerrainGob>();
            if (terrain != null) BuildLayerList(terrain);
        }

        void DetachOldTerrain()
        {
            if (_attachedTerrain == null) return;
            var oldTerrain = _attachedTerrain.Target as Sce.Atf.Dom.DomNodeAdapter;
            if (oldTerrain != null)
            {
                oldTerrain.DomNode.ChildInserted -= AttachedTerrainChanged;
                oldTerrain.DomNode.ChildRemoved -= AttachedTerrainChanged;
            }
            _attachedTerrain = null;
        }

        private GUILayer.TerrainManipulatorContext _attached;
        private WeakReference _attachedTerrain;
        private WeakReference _attachedSceneManager;

        private void BuildLayerList(XLETerrainGob terrain)
        {
            _coverageLayer.DisplayMember = "Value";
            _coverageLayer.Items.Clear();
            _coverageLayer.Items.Add(new KeyValuePair<uint, string>(1, "Heights"));
            foreach (var l in terrain.CoverageLayers)
                _coverageLayer.Items.Add(new KeyValuePair<uint, string>(l.LayerId, l.LayerName));
            SyncActiveLayer();
        }

        private void SyncActiveLayer()
        {
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
                    _attachedSceneManager = new WeakReference(gameExt.SceneManager);
                }
            }
        }

        private void _showLockedArea_CheckedChanged(object sender, EventArgs e)
        {
            if (_attached != null)
                _attached.ShowLockedArea = _showLockedArea.CheckState == CheckState.Checked;
        }

        private void _coverageLayer_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (_coverageLayer.SelectedIndex >= 0 && _coverageLayer.SelectedIndex < _coverageLayer.Items.Count) 
            {
                if (_coverageLayer.Items[_coverageLayer.SelectedIndex] is KeyValuePair<uint, string>)
                {
                    var pair = (KeyValuePair<uint, string>)_coverageLayer.Items[_coverageLayer.SelectedIndex];
                    _attached.ActiveLayer = pair.Key;
                }
            }
        }

        private void _saveButton_Click(object sender, EventArgs e)
        {
            if (_attachedSceneManager == null || _attached == null) return;
            var sceneManager = _attachedSceneManager.Target as GUILayer.EditorSceneManager;
            if (sceneManager == null) return;

            try
            {
                sceneManager.SaveTerrainLock(_attached.ActiveLayer);
            } 
            catch (Exception ex)
            {
                ControlsLibrary.BasicControls.ExceptionReport.Show(ex, "Saving terrain lock");
            }
        }

        private void _abandonButton_Click(object sender, EventArgs e)
        {
            if (_attachedSceneManager == null || _attached == null) return;
            var sceneManager = _attachedSceneManager.Target as GUILayer.EditorSceneManager;
            if (sceneManager == null) return;

            try
            {
                sceneManager.AbandonTerrainLock(_attached.ActiveLayer);
            }
            catch (Exception ex)
            {
                ControlsLibrary.BasicControls.ExceptionReport.Show(ex, "Abandoning terrain lock");
            }
        }
    }
}
