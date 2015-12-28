// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using Sce.Atf.Controls.Adaptable;

namespace MaterialTool
{
    class GraphControl : AdaptableControl
    {
        public GraphControl(GraphEditingContext context)
        {
            Context = context;

            Margin = new System.Windows.Forms.Padding(0);
            var child = new HyperGraph.GraphControl();
            child.Model = context.Model;
            child.Padding = new System.Windows.Forms.Padding(0);
            child.Dock = System.Windows.Forms.DockStyle.Fill;
            Controls.Add(child);
        }
    }
}
