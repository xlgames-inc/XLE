using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;

using Sce.Atf;
using Sce.Atf.Applications;
using Sce.Atf.Adaptation;
using Sce.Atf.Controls;
using Sce.Atf.Controls.PropertyEditing;
using System.ComponentModel.Composition;

namespace MaterialTool.Controls
{
    [Export(typeof(IInitializable))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class DiagramLister : TreeListViewEditor, IInitializable, IControlHostClient
    {
        public DiagramLister() : base(TreeListView.Style.TreeList)
        {
            _uberControl = new UserControl { Dock = DockStyle.Fill };

            const int buttonHeight = 0;
            {
                TreeListView.Control.Location = new Point(0, buttonHeight + 2);
                TreeListView.Control.Anchor =
                    AnchorStyles.Left | AnchorStyles.Top |
                    AnchorStyles.Right | AnchorStyles.Bottom;

                TreeListView.Control.Width = _uberControl.Width;
                TreeListView.Control.Height = _uberControl.Height - buttonHeight - 2;

                _uberControl.Controls.Add(TreeListView);
            }
        }

        #region IInitializable Members

        /// <summary>
        /// Initializes the MEF component</summary>
        public void Initialize()
        {
            // on initialization, register our tree control with the hosting service
            _controlHostService.RegisterControl(
                _uberControl,
                new ControlInfo(
                   "Diagram Lister".Localize(),
                   "List of nodes and connections in the diagram".Localize(),
                   StandardControlGroup.Right),
               this);
            _contextRegistry.ActiveContextChanged += contextRegistry_ActiveContextChanged;
        }
        #endregion

        #region IControlHostClient Members
        public void Activate(Control control) {}
        public void Deactivate(Control control) {}
        public bool Close(Control control) { return true; }
        #endregion

        private void contextRegistry_ActiveContextChanged(object sender, EventArgs e)
        {
            // Note: obtain ITreeView from ILayeringContext not directly from GetActiveContext().
            var context = _contextRegistry.GetActiveContext<DiagramEditingContext>();
            var treeView = context.As<ITreeListView>();
            
            if (View != treeView)
                View = treeView;
        }

        [Import(AllowDefault = false)]
        private IControlHostService _controlHostService;

        [Import]
        private IContextRegistry _contextRegistry;

        private UserControl _uberControl;
    }
}
