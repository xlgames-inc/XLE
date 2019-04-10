using System;
using System.ComponentModel.Composition;
using System.Collections.Generic;
using System.Linq;
using NodeEditorCore;
using HyperGraph;
using AuthoringConcept.AdaptiveEditing;

namespace MaterialTool.AdaptiveNodes
{
    [Export(typeof(INodeAmender))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class PreviewsNodeAmender : INodeAmender
    {
        public void AmendNode(GUILayer.NodeGraphFile diagramContext, Node node, ProcedureNodeType type, IEnumerable<object> dataPackets)
        {
            var tag = node.Tag as NodeEditorCore.ShaderProcedureNodeTag;
            if (tag == null) return;

            tag.MaterialProperties = null;
            if (diagramContext == null) return;

            var attachedSchemaFiles = diagramContext.FindAttachedSchemaFilesForNode(tag.ArchiveName);
            if (attachedSchemaFiles ==  null || !attachedSchemaFiles.Any()) return;

            foreach (var script in attachedSchemaFiles)
            {
                var schemaSourceScript = SchemaSource.GetScript(script.SchemaFileName);
                if (schemaSourceScript.Script == null) continue;

                if (!schemaSourceScript.Script.DataBlockDeclarations.TryGetValue(script.SchemaName, out IDataBlockDeclaration dataBlockDecl))
                    continue;

                var storage = dataBlockDecl.CreateStorage(null, null);

                var frame = new AuthoringConcept.AdaptiveEditing.EditorFrame { Declaration = dataBlockDecl, Storage = storage };
                node.AddItem(new AuthoringConcept.EditorFrameItem(frame), HyperGraph.Node.Dock.Center);

                tag.MaterialProperties = () => (storage as RawMaterialStorage).Material;
            }
        }

        [Import]
        private PythonAdaptiveSchemaSource SchemaSource;
    }
}


