// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.ComponentModel.Composition.Hosting;
using System.Linq;
using System.Text;
using System.Drawing;
using HyperGraph;

using ParamSourceType = ShaderFragmentArchive.Parameter.SourceType;

namespace NodeEditorCore
{
    public interface IShaderFragmentNodeCreator
    {
        Node CreateNode(ShaderFragmentArchive.Function fn, String archiveName);
        Node CreateEmptyParameterNode(ParamSourceType sourceType, String archiveName, String title);
        Node CreateParameterNode(ShaderFragmentArchive.ParameterStruct parameter, String archiveName, ParamSourceType type);
    }

    #region ShaderFragmentNodeItem

        //
    /////////////////////////////////////////////////////////////////////////////////////
        //
        //          ShaderFragmentNodeItem
        //
        //      Node Item for the hypergraph that represents an input or output
        //      of a shader graph node.
        //  
    
    internal class ShaderFragmentNodeItem : NodeItem
    {
        public ShaderFragmentNodeItem(string name, string type, string archiveName, bool inputEnabled=false, bool outputEnabled=false) :
            base(inputEnabled, outputEnabled)
        {
            this.Name = name;
            this.Type = type;
            this.Tag = type;
            this.ArchiveName = archiveName;
        }

        #region Properties
        string internalText = string.Empty;
        public string Name
        {
            get { return internalText; }
            set
            {
                if (internalText == value)
                    return;
                internalText = value;
                TextSize = Size.Empty;
            }
        }
        internal string _shortType;
        internal string _typeText;
        public string Type
        {
            get { return _typeText; }
            set
            {
                _typeText = value;
                if (value.Length > 0)
                {
                    _shortType = value[0].ToString();
                    if (value.Count() > 0 && Char.IsDigit(value[value.Count() - 1]))
                    {
                        if (value.Count() > 2 && value[value.Count() - 2] == 'x' && Char.IsDigit(value[value.Count() - 3]))
                        {
                            _shortType += value.Substring(value.Count() - 3);
                        }
                        else
                        {
                            _shortType += value.Substring(value.Count() - 1);
                        }
                    }
                }
                else
                {
                    _shortType = "";
                }
            }
        }
        public string ShortType { get { return _shortType; } }
        internal SizeF TextSize;
        public string ArchiveName { get; set; }
        #endregion

        public override SizeF Measure(Graphics graphics)
        {
                // Resize based on the length of the strings. Sometimes we get really
                // long names, so it's useful to resize the connector to match...!
            var mainStringSize = graphics.MeasureString(this.Name, SystemFonts.MenuFont);
            var shortTypeSize = graphics.MeasureString("(" + this.ShortType + ")", SystemFonts.MenuFont);
            const uint separation = 8;
            return new SizeF(
                mainStringSize.Width + separation + shortTypeSize.Width, 
                Math.Max(GraphConstants.MinimumItemHeight, mainStringSize.Height));
        }
        
        public override void Render(Graphics graphics, SizeF minimumSize, PointF location) {}

        public override void RenderConnector(Graphics graphics, RectangleF bounds)
        {
            graphics.DrawString(
                this.Name, SystemFonts.MenuFont, Brushes.White,
                bounds, GraphConstants.LeftTextStringFormat);

            var mainStringSize = graphics.MeasureString(this.Name, SystemFonts.MenuFont);
            const uint separation = 8;

            RectangleF newRect = new RectangleF(bounds.Location, bounds.Size);
            newRect.X += mainStringSize.Width + separation;
            newRect.Width -= mainStringSize.Width + separation;
            graphics.DrawString("(" + this.ShortType + ")", SystemFonts.MenuFont, Brushes.DarkGray,
                newRect, GraphConstants.LeftTextStringFormat);
        }
    }

    #endregion
    #region ShaderFragmentPreviewItem

        //
    /////////////////////////////////////////////////////////////////////////////////////
        //
        //          ShaderFragmentPreviewItem
        //
        //      Node item for shader fragment preview... Renders and image via D3D
        //      and displays it in a little preview
        //  

    internal class ShaderFragmentPreviewItem : NodeItem 
    {
        public ShaderFragmentPreviewItem()
        {
            _builder = null;
            _cachedBitmap = null;
            // _previewManager = exportProvider.GetExport<PreviewRender.IManager>().Value;
            // _converter = exportProvider.GetExport<IModelConversion>().Value;
            Geometry = PreviewRender.PreviewGeometry.Sphere;
            OutputToVisualize = "";
        }

        HyperGraph.IGraphModel Graph { set { _graph = value; } }
        ShaderDiagram.Document Document { set { _document = value; } }

        public override void    Render(Graphics graphics, SizeF minimumSize, PointF location)
        {
            if (Node.Tag is ShaderFragmentNodeTag)
            {
                SizeF size = Measure(graphics);
                if (!graphics.IsVisible(new Rectangle() {X = (int)location.X, Y = (int)location.Y, Width = (int)size.Width, Height = (int)size.Height }))
                    return;

                if (_graph == null) return;

                // PreviewRender.Manager.Instance.Update();

                if (_builder == null)
                {
                    var nodeGraph = _converter.ToShaderPatcherLayer(_graph);
                    var shader = ShaderPatcherLayer.NodeGraph.GeneratePreviewShader(
                        nodeGraph, ((ShaderFragmentNodeTag)Node.Tag).Id, OutputToVisualize);

                    _builder = _previewManager.CreatePreviewBuilder(shader);
                }

                if (_builder == null)
                    return;
                
                    // (assuming no rotation on this transformation -- scale is easy to find)
                Size idealSize = new Size((int)(graphics.Transform.Elements[0] * size.Width), (int)(graphics.Transform.Elements[3] * size.Height));
                if (_cachedBitmap != null)
                {
                        // compare the current bitmap size to the size we'd like
                    Size bitmapSize = _cachedBitmap.Size;
                    float difference = System.Math.Max(System.Math.Abs(1.0f - bitmapSize.Width / (float)(idealSize.Width)), System.Math.Abs(1.0f - bitmapSize.Height / (float)(idealSize.Height)));
                    if (difference > 0.1f)
                        _cachedBitmap = null;
                }

                if (_cachedBitmap == null)
                    _cachedBitmap = _builder.Build(_document, idealSize, Geometry);

                if (_cachedBitmap != null)
                    graphics.DrawImage(_cachedBitmap, new RectangleF() { X = location.X, Y = location.Y, Width = size.Width, Height = size.Height });
            }
        }

        public override SizeF   Measure(Graphics graphics) { return new SizeF(196, 196); }
        public override void    RenderConnector(Graphics graphics, RectangleF bounds) { }

        public void InvalidateShaderStructure() { _cachedBitmap = null; _builder = null; }
        public void InvalidateParameters() { _cachedBitmap = null; }
        public void InvalidateAttachedConstants() { _cachedBitmap = null; _builder = null;  /* required complete rebuild of shader */ }

        public override bool OnStartDrag(PointF location, out PointF original_location)
        {
            base.OnStartDrag(location, out original_location);
            _lastDragLocation = original_location = location;
            return true;
        }

        public override bool OnDrag(PointF location)
        {
            base.OnDrag(location);
            // PreviewRender.Manager.Instance.RotateLightDirection(_document, new PointF(location.X - _lastDragLocation.X, location.Y - _lastDragLocation.Y));
            _lastDragLocation = location;
            InvalidateParameters();
            return true;
        }

        public override bool OnEndDrag() { base.OnEndDrag(); return true; }

        public PreviewRender.PreviewGeometry Geometry { get { return _previewGeometry; } set { _previewGeometry = value; InvalidateParameters(); } }
        public string OutputToVisualize { get { return _outputToVisualize; } set { _outputToVisualize = value; InvalidateShaderStructure(); } }

        private HyperGraph.IGraphModel          _graph;
        private ShaderDiagram.Document          _document;
        private PreviewRender.IPreviewBuilder   _builder;
        private PointF                          _lastDragLocation;

        [Import]
        private PreviewRender.IManager          _previewManager;
        [Import]
        private IModelConversion                _converter;

        private System.Drawing.Bitmap           _cachedBitmap;
        private PreviewRender.PreviewGeometry   _previewGeometry;
        private string _outputToVisualize;
    }

    #endregion
    #region ShaderFragmentNodeCompatibility

        //
    /////////////////////////////////////////////////////////////////////////////////////
        //
        //          ShaderFragmentNodeCompatibility
        //
        //      Checks node inputs and outputs for type compatibility
        //  

    public class ShaderFragmentNodeCompatibility : HyperGraph.Compatibility.ICompatibilityStrategy
    {
        public HyperGraph.Compatibility.ConnectionType CanConnect(NodeConnector from, NodeConnector to)
        {
            if (null == from.Item.Tag && null == to.Item.Tag) return HyperGraph.Compatibility.ConnectionType.Compatible;
            if (null == from.Item.Tag || null == to.Item.Tag) return HyperGraph.Compatibility.ConnectionType.Incompatible;

            if (from.Item.Tag is string && to.Item.Tag is string)
            {
                string fromType = (string)from.Item.Tag;
                string toType = (string)to.Item.Tag;
                if (fromType.Equals(toType, StringComparison.CurrentCultureIgnoreCase))
                {
                    return HyperGraph.Compatibility.ConnectionType.Compatible;
                }
                else
                {
                    // Some types have automatic conversion operations
                    if (ShaderPatcherLayer.TypeRules.HasAutomaticConversion(fromType, toType))
                    {
                        return HyperGraph.Compatibility.ConnectionType.Conversion;
                    }
                }
            }

            return HyperGraph.Compatibility.ConnectionType.Incompatible;
        }
    }

    #endregion
    #region Node Creation

        //
    /////////////////////////////////////////////////////////////////////////////////////
        //
        //          Creating nodes & tag tags
        //  

    internal class ShaderFragmentNodeTag
    {
        public string ArchiveName { get; set; }
        public UInt32 Id { get; set; }
        public ShaderFragmentNodeTag(string archiveName) { ArchiveName = archiveName; Id = ++nodeAccumulatingId; }
        private static UInt32 nodeAccumulatingId = 1;
    }

    internal class ShaderProcedureNodeTag : ShaderFragmentNodeTag
    {
        public ShaderProcedureNodeTag(string archiveName) : base(archiveName) {}
    }

    internal class ShaderParameterNodeTag : ShaderFragmentNodeTag
    {
        public ShaderParameterNodeTag(string archiveName) : base(archiveName) {}
    }

    [Export(typeof(IShaderFragmentNodeCreator))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ShaderFragmentNodeCreator : IShaderFragmentNodeCreator
    {
        internal static IDictionary<Enum, string> PreviewGeoNames;
        internal static IDictionary<Enum, string> ParamSourceTypeNames;
        
        static ShaderFragmentNodeCreator()
        {
            PreviewGeoNames = new Dictionary<Enum, string>
            {
                { PreviewRender.PreviewGeometry.Chart, "Chart" },
                { PreviewRender.PreviewGeometry.Box, "Box" },
                { PreviewRender.PreviewGeometry.Sphere, "Sphere" },
                { PreviewRender.PreviewGeometry.Model, "Model" }
            };

            ParamSourceTypeNames = new Dictionary<Enum, string>
            {
                { ParamSourceType.Material, "Material Parameter" },
                { ParamSourceType.InterpolatorIntoVertex, "Interpolator Into Vertex Shader" },
                { ParamSourceType.InterpolatorIntoPixel, "Interpolator Into Pixel Shader" },
                { ParamSourceType.System, "System Parameter" },
                { ParamSourceType.Output, "Output" },
                { ParamSourceType.Constant, "Constant" }
            };
        }

        internal static String AsString(Enum e, IDictionary<Enum, string> table)
        {
            foreach (KeyValuePair<Enum, string> kvp in table)
                if (kvp.Key == e)
                    return kvp.Value;
            throw new InvalidOperationException(String.Format("Could not convert enum to string value {0}.", e));
        }

        internal static T AsEnumValue<T>(String input, IDictionary<Enum, string> table) where T : struct, IComparable, IConvertible, IFormattable
        {
            Type enumType = typeof(T);
            if (!enumType.IsEnum)
                throw new InvalidOperationException("Expecting enum type as parameter to AsEnumValue.");

            foreach (KeyValuePair<Enum, string> kvp in table)
                if (kvp.Value == input)
                    return (T)(object)kvp.Key;
            return default(T);
        }

        internal static Tuple<List<String>, int> AsEnumList(Enum value, IDictionary<Enum, string> table)
        {
            int selectedIndex = 0;
            List<String> typeNames = new List<String>();
            foreach (KeyValuePair<Enum, string> kvp in table)
            {
                if (kvp.Key.Equals(value))
                    selectedIndex = typeNames.Count;
                typeNames.Add(kvp.Value);
            }
            return Tuple.Create(typeNames, selectedIndex);
        }

        public Node CreateNode(ShaderFragmentArchive.Function fn, String archiveName)
        {
            var node = new Node(fn.Name);
            node.Tag = new ShaderProcedureNodeTag(archiveName);

            var previewItem = new ShaderFragmentPreviewItem();

            // use composition to access some exported types --
            {
                CompositionBatch compositionBatch = new CompositionBatch();
                compositionBatch.AddPart(previewItem);
                _composer.Compose(compositionBatch);
            }
            node.AddItem(previewItem);

            // Drop-down selection box for "preview mode"
            var enumList = AsEnumList(previewItem.Geometry, PreviewGeoNames);
            var previewModeSelection = new HyperGraph.Items.NodeDropDownItem(enumList.Item1.ToArray(), enumList.Item2, false, false);
            node.AddItem(previewModeSelection);
            previewModeSelection.SelectionChanged += 
                (object sender, HyperGraph.Items.AcceptNodeSelectionChangedEventArgs args) => 
                {
                    if (sender is HyperGraph.Items.NodeDropDownItem)
                    {
                        var item = (HyperGraph.Items.NodeDropDownItem)sender;
                        previewItem.Geometry = AsEnumValue<PreviewRender.PreviewGeometry>(item.Items[args.Index], PreviewGeoNames);
                    }
                };

                // Text item box for output visualization string
                // note --  should could potentially become a more complex editing control
                //          it might be useful to have some preset defaults and helpers rather than just
                //          requiring the user to construct the raw string
            var outputToVisualize = new HyperGraph.Items.NodeTextBoxItem(previewItem.OutputToVisualize, false, false);
            node.AddItem(outputToVisualize);
            outputToVisualize.TextChanged +=
                (object sender, HyperGraph.Items.AcceptNodeTextChangedEventArgs args) => { previewItem.OutputToVisualize = args.Text; };

            foreach (var param in fn.InputParameters)
                node.AddItem(new ShaderFragmentNodeItem(param.Name, param.Type, archiveName + ":" + param.Name, true, false));
            foreach (var output in fn.Outputs)
                node.AddItem(new ShaderFragmentNodeItem(output.Name, output.Type, archiveName + ":" + output.Name, false, true));
            return node;
        }

        // public static ShaderFragmentArchive.Parameter.SourceType AsSourceType(String input)
        // {
        //     foreach (var e in Enum.GetValues(typeof(ShaderFragmentArchive.Parameter.SourceType)).Cast<ShaderFragmentArchive.Parameter.SourceType>())
        //         if (AsString(e) == input)
        //             return e;
        //     return ShaderFragmentArchive.Parameter.SourceType.Material;
        // }

        private static void ParameterNodeTypeChanged(object sender, HyperGraph.Items.AcceptNodeSelectionChangedEventArgs args)
        {
            if (sender is HyperGraph.Items.NodeDropDownItem)
            {
                var item = (HyperGraph.Items.NodeDropDownItem)sender;
                var node = item.Node;
                var newType = AsEnumValue<ParamSourceType>(item.Items[args.Index], ParamSourceTypeNames);

                    //  We might have to change the input/output settings on this node
                bool isOutput = newType == ParamSourceType.Output;
                var oldItems = new List<HyperGraph.NodeItem>(node.Items);
                foreach (var i in oldItems)
                {
                    if (i is ShaderFragmentNodeItem)
                    {
                                // if this is a node item with exactly 1 input/output
                                // and it is not in the correct direction, then we have to change it.
                                //  we can't change directly. We need to delete and recreate this node item.
                        var fragItem = (ShaderFragmentNodeItem)i;
                        if (    (fragItem.Output.Enabled ^ fragItem.Input.Enabled) == true &&
                                (fragItem.Output.Enabled) != (isOutput == false))
                        {
                            var newItem = new ShaderFragmentNodeItem(
                                fragItem.Name, fragItem.Type, fragItem.ArchiveName,
                                isOutput ? true : false, isOutput ? false : true);
                            node.RemoveItem(fragItem);
                            node.AddItem(newItem);
                        }
                    }
                }
            }
        }

        public Node CreateEmptyParameterNode(ParamSourceType sourceType, String archiveName, String title)
        {
            var node = new Node(title);
            node.Tag = new ShaderParameterNodeTag(archiveName);

            var enumList = AsEnumList(sourceType, ParamSourceTypeNames);
            var typeSelection = new HyperGraph.Items.NodeDropDownItem(enumList.Item1.ToArray(), enumList.Item2, false, false);
            node.AddItem(typeSelection);
            typeSelection.SelectionChanged += ParameterNodeTypeChanged;
            return node;
        }

        public Node CreateParameterNode(ShaderFragmentArchive.ParameterStruct parameter, String archiveName, ParamSourceType type)
        {
            var node = CreateEmptyParameterNode(type, archiveName, parameter.Name);
            foreach (var param in parameter.Parameters)
            {
                bool isOutput = type == ParamSourceType.Output;
                node.AddItem(new ShaderFragmentNodeItem(
                    param.Name, param.Type, archiveName + ":" + param.Name, 
                    isOutput ? true : false, isOutput ? false : true));
            }
            return node;
        }

        [Import]
        CompositionContainer _composer;
    }

    #endregion
    #region Node Util

        //
    /////////////////////////////////////////////////////////////////////////////////////
        //
        //          Utility functions
        //  

    public static class ShaderFragmentNodeUtil
    {
        public static Node GetShaderFragmentNode(HyperGraph.IGraphModel graph, UInt64 id)
        {
            foreach (Node n in graph.Nodes)
            {
                if (n.Tag is ShaderFragmentNodeTag
                    && ((ShaderFragmentNodeTag)n.Tag).Id == (UInt64)id)
                {
                    return n;
                }
            }
            return null;
        }

        //public static Node GetParameterNode(HyperGraph.GraphControl graphControl, UInt64 id)
        //{
        //    foreach (Node n in graphControl.Nodes)
        //    {
        //        if (n.Tag is ShaderParameterNodeTag
        //            && ((ShaderParameterNodeTag)n.Tag).Id == (UInt64)id)
        //        {
        //            return n;
        //        }
        //    }
        //    return null;
        //}

        public static void InvalidateShaderStructure(HyperGraph.IGraphModel graph)
        {
            foreach (Node n in graph.Nodes)
            {
                foreach (NodeItem i in n.Items)
                {
                    if (i is ShaderFragmentPreviewItem)
                    {
                        ((ShaderFragmentPreviewItem)i).InvalidateShaderStructure();
                    }
                }
            }
        }

        public static void InvalidateParameters(HyperGraph.IGraphModel graph)
        {
            foreach (Node n in graph.Nodes)
            {
                foreach (NodeItem i in n.Items)
                {
                    if (i is ShaderFragmentPreviewItem)
                    {
                        ((ShaderFragmentPreviewItem)i).InvalidateParameters();
                    }
                }
            }
        }

        public static void InvalidateAttachedConstants(HyperGraph.IGraphModel graph)
        {
            foreach (Node n in graph.Nodes)
            {
                foreach (NodeItem i in n.Items)
                {
                    if (i is ShaderFragmentPreviewItem)
                    {
                        ((ShaderFragmentPreviewItem)i).InvalidateAttachedConstants();
                    }
                }
            }
        }
    }

    #endregion
    #region Parameter Editing

        //
    /////////////////////////////////////////////////////////////////////////////////////
        //
        //          Utility functions for editing parameters
        //  

    [Export]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ShaderParameterUtil
    {
        public void UpdateGraphConnectionsForParameter(
                                HyperGraph.IGraphModel graph,
                                String oldArchiveName, String newArchiveName)
        {
            //      Look for connections in the graph using the "oldArchiveName" and
            //      update them with parameter state information from "newArchiveName"

            foreach (var n in graph.Nodes)
            {
                foreach (var item in n.Items)
                {
                    if (item is ShaderFragmentNodeItem)
                    {
                        var i = (ShaderFragmentNodeItem)item;
                        if (i.ArchiveName != null && i.ArchiveName.Equals(oldArchiveName))
                        {
                            i.ArchiveName = newArchiveName;

                            //      Name and Type are cached on the connector
                            //      so, we have to update them with the latest info...
                            var param = _shaderFragments.GetParameter(newArchiveName);
                            if (param != null)
                            {
                                i.Name = param.Name;
                                i.Type = param.Type;
                            }
                            else
                            {
                                i.Name = "<<unknown>>";
                                i.Type = "<<unknown>>";
                            }
                        }
                    }
                }
            }
        }

        private static String IdentifierSafeName(String input)
        {
            //      Convert bad characters into underscores
            //      If the identifier doesn't start with a letter, then prepend an underscore
            var regex = new System.Text.RegularExpressions.Regex(
                @"[^\p{Ll}\p{Lu}\p{Lt}\p{Lo}\p{Nd}\p{Nl}\p{Mn}\p{Mc}\p{Cf}\p{Pc}\p{Lm}]");
            string ret = regex.Replace(input, "_");
            if (!Char.IsLetter(ret[0]))
            {
                ret = string.Concat("_", ret);
            }
            return ret;
        }

        public static void EditParameter(HyperGraph.IGraphModel graph, String archiveName)
        {
            //var parameter = ShaderFragmentArchive.Archive.GetParameter(archiveName);
            //if (parameter != null)
            //{
            //    var dialog = new ParameterDialog();
            //    dialog.PullFrom(parameter);
            //    var result = dialog.ShowDialog();
            //    if (result == System.Windows.Forms.DialogResult.OK)
            //    {
            //        var newParam = dialog.Result;
            //
            //        //
            //        //      Have to also update the "Name" and "Type"
            //        //      fields of any ShaderFragmentNodeItems that are
            //        //      using this parameter
            //        //          (also changing the source could change input -> output...)
            //        //
            //
            //        newParam.Name = IdentifierSafeName(newParam.Name);
            //        if (newParam.ArchiveName.Length != 0
            //            && newParam.ArchiveName.Substring(0, 12).Equals("LocalArchive"))
            //        {
            //            newParam.ArchiveName = "LocalArchive[" + newParam.Name + "]";
            //        }
            //
            //        var oldArchiveName = parameter.ArchiveName;
            //        parameter.DeepCopyFrom(newParam);
            //        ShaderFragmentArchive.Archive.RenameParameter(parameter, oldArchiveName);
            //
            //        ShaderFragmentNodeUtil.UpdateGraphConnectionsForParameter(
            //            graphControl, oldArchiveName, parameter.ArchiveName);
            //    }
            //}
        }

        public bool FillInMaterialParameters(ShaderDiagram.Document document, HyperGraph.IGraphModel graph)
        {
                //
                //      Look for new or removed material parameters
                //      and update the material parameters dictionary
                //
            Dictionary<String, String> newMaterialParameters = new Dictionary<String, String>();
            foreach (Node n in graph.Nodes)
            {
                if (n.Tag is ShaderParameterNodeTag && n.Items.Count() > 0)
                {
                        // look for a drop down list element -- this will tell us the type
                    var type = ParamSourceType.System;
                    foreach (var i in n.Items)
                    {
                        if (i is HyperGraph.Items.NodeDropDownItem)
                        {
                            var dropDown = (HyperGraph.Items.NodeDropDownItem)i;
                            var stringForm = dropDown.Items[dropDown.SelectedIndex];
                            type = ShaderFragmentNodeCreator.AsEnumValue<ParamSourceType>(stringForm, ShaderFragmentNodeCreator.ParamSourceTypeNames);
                            break;
                        }
                    }

                    if (type == ParamSourceType.Material)
                    {
                        foreach (var i in n.Items)
                        {
                            if (i is ShaderFragmentNodeItem)
                            {
                                ShaderFragmentNodeItem item = (ShaderFragmentNodeItem)i;
                                if (item.Output != null)
                                {
                                    if (!newMaterialParameters.ContainsKey(item.ArchiveName))
                                    {
                                        var param = _shaderFragments.GetParameter(item.ArchiveName);
                                        if (param != null)
                                        {
                                            newMaterialParameters.Add(item.ArchiveName, param.Type);
                                        }
                                        else
                                        {
                                            newMaterialParameters.Add(item.ArchiveName, "<<unknown>>");
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            bool didSomething = false;
            List<String> entriesToRemove = new List<String>();
            foreach (String s in document.PreviewMaterialState.Keys)
            {
                if (!newMaterialParameters.ContainsKey(s))
                {
                    entriesToRemove.Add(s);
                }
            }

            foreach (String s in entriesToRemove) 
            {
                document.PreviewMaterialState.Remove(s);      // does this invalidate the iteration?
                didSomething = true;
            }

            foreach (KeyValuePair<String,String> s in newMaterialParameters)
            {
                if (!document.PreviewMaterialState.ContainsKey(s.Key))
                {
                    var parameter = _shaderFragments.GetParameter(s.Key);
                    System.Object def = null;
                    if (parameter != null && parameter.Default != null && parameter.Default.Length > 0)
                    {
                        def = ShaderPatcherLayer.TypeRules.CreateFromString(parameter.Default, parameter.Type);
                    }

                    var parameterName = s.Key;
                    if (parameter!=null) parameterName = parameter.Name;

                    if (def != null) 
                    {
                        document.PreviewMaterialState.Add(parameterName, def);
                    }
                    else
                    {
                        document.PreviewMaterialState.Add(parameterName, ShaderPatcherLayer.TypeRules.CreateDefaultObject(s.Value));
                    }

                    didSomething = true;
                }
            }

            return didSomething;
        }

        [Import]
        ShaderFragmentArchive.Archive _shaderFragments;
    }

    #endregion

}
