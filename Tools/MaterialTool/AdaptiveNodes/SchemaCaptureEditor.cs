using System;
using System.ComponentModel.Composition;
using System.Collections.Generic;
using System.Linq;
using NodeEditorCore;
using HyperGraph;
using AuthoringConcept.AdaptiveEditing;
using System.Drawing;
using System.Drawing.Drawing2D;

namespace MaterialTool.AdaptiveNodes
{
    public class EditorFrameItem : AuthoringConcept.EditorFrameItem
    {
        public override SizeF Measure(Graphics graphics, object context)
        {
            var mat = (context as NodeEditorCore.IEditingContext).Document.GraphMetaData.Material;
            var matContext = new RawMaterialStorage { Material = mat };
            return base.Measure(graphics, matContext);
        }

        public override void Render(Graphics graphics, RectangleF rectangle, object context)
        {
            var mat = (context as NodeEditorCore.IEditingContext).Document.GraphMetaData.Material;
            var matContext = new RawMaterialStorage { Material = mat };
            base.Render(graphics, rectangle, matContext);
        }

        public EditorFrameItem(IDataBlockDeclaration declaration) : base(declaration) {}
    }

    [Export(typeof(INodeAmender))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class PreviewsNodeAmender : INodeAmender
    {
        public void AmendNode(GUILayer.NodeGraphFile diagramContext, Node node, ProcedureNodeType type, IEnumerable<object> dataPackets)
        {
            var tag = node.Tag as NodeEditorCore.ShaderProcedureNodeTag;
            if (tag == null) return;

            if (diagramContext == null) return;

            var attachedSchemaFiles = diagramContext.FindAttachedSchemaFilesForNode(tag.ArchiveName);
            if (attachedSchemaFiles ==  null || !attachedSchemaFiles.Any()) return;

            foreach (var script in attachedSchemaFiles)
            {
                var schemaSourceScript = SchemaSource.GetScript(script.SchemaFileName);
                if (schemaSourceScript.Script == null) continue;

                if (!schemaSourceScript.Script.DataBlockDeclarations.TryGetValue(script.SchemaName, out IDataBlockDeclaration dataBlockDecl))
                    continue;

                node.AddItem(new EditorFrameItem(dataBlockDecl), HyperGraph.Node.Dock.Center);
            }
        }

        [Import]
        private PythonAdaptiveSchemaSource SchemaSource;
    }
}


