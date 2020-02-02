using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace ControlsLibraryExt.SceneSelector
{
    public partial class SimpleModelSelector : UserControl
    {
        public SimpleModelSelector(ModelView.PreviewerContext previewerContext)
        {
            _previewerContext = previewerContext;
            InitializeComponent();

            model.Text = _previewerContext.ModelSettings.ModelName;
            material.Text = _previewerContext.ModelSettings.MaterialName;
            skeleton.Text = _previewerContext.ModelSettings.SkeletonFileName;
            animation.Text = _previewerContext.ModelSettings.AnimationFileName;
        }

        private ModelView.PreviewerContext _previewerContext;

        public void ApplyChanges()
        {
            _previewerContext.ModelSettings.ModelName = model.Text;
            _previewerContext.ModelSettings.MaterialName = material.Text;
            _previewerContext.ModelSettings.SkeletonFileName = skeleton.Text;
            _previewerContext.ModelSettings.AnimationFileName = animation.Text;
        }

        private void selectModel_Click(object sender, EventArgs e)
        {
            using (OpenFileDialog ofd = new OpenFileDialog())
            {
                ofd.FileName = model.Text;
                if (OpenFilesWithFilters(ofd, GUILayer.Utils.GetModelExtensions()) == DialogResult.OK)
                {
                    model.Text = ofd.FileName;
                }
            }
        }

        private void selectMaterial_Click(object sender, EventArgs e)
        {
            using (OpenFileDialog ofd = new OpenFileDialog())
            {
                ofd.FileName = material.Text;
                if (OpenFilesWithFilters(ofd, GUILayer.Utils.GetModelExtensions()) == DialogResult.OK)
                {
                    material.Text = ofd.FileName;
                }
            }
        }

        private void selectSkeleton_Click(object sender, EventArgs e)
        {
            using (OpenFileDialog ofd = new OpenFileDialog())
            {
                ofd.FileName = skeleton.Text;
                if (OpenFilesWithFilters(ofd, GUILayer.Utils.GetModelExtensions()) == DialogResult.OK)
                {
                    skeleton.Text = ofd.FileName;
                }
            }
        }

        private void selectAnimation_Click(object sender, EventArgs e)
        {
            using (OpenFileDialog ofd = new OpenFileDialog())
            {
                ofd.FileName = animation.Text;
                if (OpenFilesWithFilters(ofd, GUILayer.Utils.GetModelExtensions()) == DialogResult.OK)
                {
                    animation.Text = ofd.FileName;
                }
            }
        }

        private void model_TextChanged(object sender, EventArgs e)
        {
            
        }

        private void material_TextChanged(object sender, EventArgs e)
        {
            
        }

        private void skeleton_TextChanged(object sender, EventArgs e)
        {
            
        }

        private void animation_TextChanged(object sender, EventArgs e)
        {

        }
        
        private DialogResult OpenFilesWithFilters(OpenFileDialog ofd, System.Collections.Generic.IEnumerable<GUILayer.Utils.AssetExtension> filters)
        {
            var sb = new System.Text.StringBuilder("", 256);
            bool first = true;
            bool folderIsAnOption = false;
            foreach (var f in filters)
            {
                if (f.Extension == "folder")
                {
                    folderIsAnOption = true;
                    continue;
                }

                if (!first) sb.Append("|");
                first = false;
                sb.Append(f.Description);
                sb.Append("|*.");
                sb.Append(f.Extension);
            }
            if (!first) sb.Append("|");
            first = false;
            sb.Append("All Files|*.*");

            ofd.Filter = sb.ToString();
            return ofd.ShowDialog();
        }
    }
}
