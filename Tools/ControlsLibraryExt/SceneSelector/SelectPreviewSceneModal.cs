using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace ControlsLibraryExt.SceneSelector
{
    public partial class SelectPreviewSceneModal : Form
    {
        public SelectPreviewSceneModal(ControlsLibraryExt.ModelView.PreviewerContext previewerContext)
        {
            InitializeComponent();

            _targetPreviewerContext = previewerContext;
            if (previewerContext.ModelSettings != null)
            {
                _modelSettings = previewerContext.ModelSettings.ShallowCopy();
            }
            else
            {
                _modelSettings = GUILayer.ModelVisSettings.CreateDefault();
            }
            _previewRegistryScene = previewerContext.PreviewRegistryScene;

            {
                var dropDownCtrl = new PropertyGrid();
                dropDownCtrl.SelectedObject = _modelSettings;
                dropDownCtrl.ToolbarVisible = false;
                dropDownCtrl.HelpVisible = false;
                dropDownCtrl.Dock = DockStyle.Fill;

                dropDownCtrl.PropertyValueChanged += DropDownCtrl_PropertyValueChanged;

                var page = new TabPage("Basic");
                page.UseVisualStyleBackColor = true;
                page.Controls.Add(dropDownCtrl);
                tabControl1.TabPages.Add(page);
                if (string.IsNullOrEmpty(_previewRegistryScene))
                    tabControl1.SelectedTab = page;
            }

            {
                var ctrl = new PreviewSceneSelector();
                ctrl.Dock = DockStyle.Fill;
                ctrl.OnSelectionChanged += Ctrl_OnSelectionChanged;
                var page = new TabPage("Preview Scenes");
                page.Padding = new System.Windows.Forms.Padding(3);
                page.UseVisualStyleBackColor = true;
                page.Controls.Add(ctrl);
                tabControl1.TabPages.Add(page);
                if (!string.IsNullOrEmpty(_previewRegistryScene))
                    tabControl1.SelectedTab = page;
            }

            {
                this._layerController = new GUILayer.VisLayerController();
                this._view = new ControlsLibrary.LayerControl();
                this._view.Dock = DockStyle.Fill;
                this._view.Padding = new System.Windows.Forms.Padding(3);
                this.splitContainer1.Panel2.Controls.Add(this._view);

                if (string.IsNullOrEmpty(_previewRegistryScene))
                {
                    this._layerController.SetScene(_modelSettings);
                }
                else
                {
                    this._layerController.SetPreviewRegistryScene(_previewRegistryScene);
                }
                this._layerController.AttachToView(this._view.Underlying);
            }
        }

        private void Ctrl_OnSelectionChanged(object sender, PreviewSceneSelector.PreviewSceneSelectionChanged e)
        {
            this._layerController.SetPreviewRegistryScene(e.PreviewScene);
            _previewRegistryScene = e.PreviewScene;
            this._view.Invalidate();
        }

        private void DropDownCtrl_PropertyValueChanged(object s, PropertyValueChangedEventArgs e)
        {
            this._layerController.SetScene(_modelSettings);
            _previewRegistryScene = null;
            this._view.Invalidate();
        }

        private ControlsLibrary.LayerControl _view;
        private GUILayer.VisLayerController _layerController;
        private GUILayer.ModelVisSettings _modelSettings;
        private string _previewRegistryScene;
        private ControlsLibraryExt.ModelView.PreviewerContext _targetPreviewerContext;

        private void cancelButton_Click(object sender, EventArgs e)
        {
            DialogResult = DialogResult.Cancel;
        }

        private void okButton_Click(object sender, EventArgs e)
        {
            DialogResult = DialogResult.OK;
            if (!string.IsNullOrEmpty(_previewRegistryScene))
            {
                _targetPreviewerContext.PreviewRegistryScene = _previewRegistryScene;
            }
            else
            {
                _targetPreviewerContext.ModelSettings = _modelSettings;
            }
        }
    }
}
