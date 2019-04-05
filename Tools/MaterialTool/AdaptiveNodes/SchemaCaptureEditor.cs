using System;
using System.ComponentModel.Composition;
using System.Collections.Generic;
using NodeEditorCore;
using HyperGraph;
using AuthoringConcept.AdaptiveEditing;

namespace MaterialTool.AdaptiveNodes
{
    [Export(typeof(INodeAmender))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class PreviewsNodeAmender : INodeAmender
    {
        public void AmendNode(Node node, ProcedureNodeType type, IEnumerable<object> dataPackets)
        {
            var tag = node.Tag as NodeEditorCore.ShaderProcedureNodeTag;
            if (tag == null) return;

            tag.MaterialProperties = null;

            var archiveName = tag.ArchiveName.Split(':');
            if (archiveName.Length < 2) return;

            var ext = archiveName[0].IndexOf('.');
            if (ext != -1) archiveName[0] = archiveName[0].Substring(0, ext);
            var script = SchemaSource.GetScript(archiveName[0] + ".py");
            if (script.Script == null) return;

            if (!script.Script.DataBlockDeclarations.TryGetValue(archiveName[1], out IDataBlockDeclaration dataBlockDecl))
                return;

            var storage = dataBlockDecl.CreateStorage(null, null);

            var frame = new AuthoringConcept.AdaptiveEditing.EditorFrame { Declaration = dataBlockDecl, Storage = storage };
            node.AddItem(new AuthoringConcept.EditorFrameItem(frame), HyperGraph.Node.Dock.Center);

            tag.MaterialProperties = () => (storage as RawMaterialStorage).Material;
        }

        [Import]
        private PythonAdaptiveSchemaSource SchemaSource;
    }
}


