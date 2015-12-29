// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using Sce.Atf.Controls.Adaptable;
using System.ComponentModel.Composition;

namespace MaterialTool
{
    interface IDiagramControl
    {
        void SetContext(DiagramEditingContext context);
    }

    [Export(typeof(IDiagramControl))]
    [PartCreationPolicy(CreationPolicy.NonShared)]
    class DiagramControl : AdaptableControl, IDiagramControl
    {
        public DiagramControl()
        {
            DoubleBuffered = false;

            Margin = new System.Windows.Forms.Padding(0);
            _child = new HyperGraph.GraphControl();
            _child.Padding = new System.Windows.Forms.Padding(0);
            _child.Dock = System.Windows.Forms.DockStyle.Fill;
            Controls.Add(_child);

            _child.Paint += child_Paint;
        }

        public void SetContext(DiagramEditingContext context)
        {
            Context = context;
            _child.Model = context.Model;
            _child.Selection = context.DiagramSelection;
        }

        void child_Paint(object sender, System.Windows.Forms.PaintEventArgs e)
        {
            _engine.ForegroundUpdate();
        }

        [Import]
        private GUILayer.EngineDevice _engine;
        private HyperGraph.GraphControl _child;
    }
}
