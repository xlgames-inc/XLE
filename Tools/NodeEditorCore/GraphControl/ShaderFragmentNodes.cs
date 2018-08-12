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
using System.Drawing;
using HyperGraph;

using ParamSourceType = ShaderFragmentArchive.Parameter.SourceType;

#pragma warning disable 0649        // Field '...' is never assigned to, and will always have its default value null

namespace NodeEditorCore
{
    public enum InterfaceDirection { In, Out };

    public interface IShaderFragmentNodeCreator
    {
        Node CreateNode(ShaderFragmentArchive.Function fn, String archiveName, ShaderPatcherLayer.PreviewSettings previewSettings = null);
        Node CreateEmptyParameterNode(ParamSourceType sourceType, String archiveName, String title);
        Node CreateParameterNode(ShaderFragmentArchive.ParameterStruct parameter, String archiveName, ParamSourceType type);
        Node FindNodeFromId(HyperGraph.IGraphModel graph, UInt64 id);
        HyperGraph.Compatibility.ICompatibilityStrategy CreateCompatibilityStrategy();
        string GetDescription(object item);
    }

    public interface IDiagramDocument
    {
        ShaderPatcherLayer.NodeGraphFile NodeGraphFile { get; }
        ShaderPatcherLayer.NodeGraphContext GraphContext { get; set; }

        void Invalidate();
        void Save(Uri destination);
        void Load(Uri source);
    }

    public interface ISubGraphEditingContext
    {
        IDiagramDocument ContainingDocument { get; set; }
        String SubGraphName { get; set; }
        uint GlobalRevisionIndex { get; }
    }

    #region Node Items

        //
    /////////////////////////////////////////////////////////////////////////////////////
        //
        //          ShaderFragmentNodeConnector
        //
        //      Node Item for the hypergraph that represents an input or output
        //      of a shader graph node.
        //  
    
    internal class ShaderFragmentNodeConnector : NodeConnector
    {
        public ShaderFragmentNodeConnector(string name, string type, string archiveName)
        {
            this.Name = name;
            this.Type = type;
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
                Tag = value;
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
            var mainStringSize = graphics.MeasureString(GetNameText(), SystemFonts.MenuFont);
            var shortTypeSize = graphics.MeasureString(GetTypeText(), SystemFonts.MenuFont);
            const uint separation = 8;
            return new SizeF(
                mainStringSize.Width + separation + shortTypeSize.Width, 
                Math.Max(GraphConstants.MinimumItemHeight, mainStringSize.Height));
        }
        
        public override void Render(Graphics graphics, RectangleF bounds, object context)
        {
            graphics.DrawString(
                this.Name, SystemFonts.MenuFont, Brushes.White,
                bounds, GraphConstants.LeftTextStringFormat);

            var mainStringSize = graphics.MeasureString(GetNameText(), SystemFonts.MenuFont);
            const uint separation = 8;

            RectangleF newRect = new RectangleF(bounds.Location, bounds.Size);
            newRect.X += mainStringSize.Width + separation;
            newRect.Width -= mainStringSize.Width + separation;
            graphics.DrawString(GetTypeText(), SystemFonts.MenuFont, Brushes.DarkGray,
                newRect, GraphConstants.LeftTextStringFormat);
        }

        internal virtual string GetNameText() { return this.Name; }
        internal virtual string GetTypeText() { return "(" + this.ShortType + ")"; }

        internal static Node.Column GetDirectionalColumn(InterfaceDirection direction)
        {
            switch (direction)
            {
                case InterfaceDirection.In: return Node.Column.Input;
                default:
                case InterfaceDirection.Out: return Node.Column.Output;
            }
        }
    }

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
            _cachedBitmap = null;
            Geometry = ShaderPatcherLayer.PreviewGeometry.Plane2D;
            OutputToVisualize = "";
            _shaderStructureHash = 0;
        }

        public override void Render(Graphics graphics, RectangleF boundary, object context)
        {
            if (Node.Tag is ShaderFragmentNodeTag)
            {
                SizeF size = Measure(graphics);
                if (!graphics.IsVisible(boundary))
                    return;

                // We need to get some global context information 
                // (such the the containing graph and material parameter settings)
                // We could use composition to get access to a global. But ideally this
                // should be passed in from the top-level render function.
                // This is a more convenient way to track invalidation, also -- because we
                // can just check if there have been any changes since our last cached bitmap.

                var editingContext = context as ISubGraphEditingContext;
                if (editingContext == null) return;

                uint currentHash = editingContext.GlobalRevisionIndex;
                if (currentHash != _shaderStructureHash)
                    _cachedBitmap = null;

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
                {
                        // note -- much of this work doesn't really need to be repeated for each node.
                    var prevSettings = PreviewSettings;
                    if (prevSettings.OutputToVisualize.StartsWith("SV_Target"))
                        prevSettings.OutputToVisualize = string.Empty;

                    uint target = 0;
                    if (OutputToVisualize.StartsWith("SV_Target"))
                        if (!uint.TryParse(OutputToVisualize.Substring(9), out target))
                            target = 0;

                    _cachedBitmap = _previewManager.BuildPreviewImage(
                        editingContext.ContainingDocument.GraphContext,
                        new ShaderPatcherLayer.NodeGraphPreviewConfiguration
                        {
                            _nodeGraph = editingContext.ContainingDocument.NodeGraphFile,
                            _subGraphName = editingContext.SubGraphName,
                            _previewNodeId = ((ShaderFragmentNodeTag)Node.Tag).Id,
                            _settings = prevSettings,
                            _variableRestrictions = editingContext.ContainingDocument.GraphContext.Variables
                        },
                        idealSize, Geometry, target);
                    _shaderStructureHash = currentHash;
                }

                if (_cachedBitmap != null)
                    graphics.DrawImage(_cachedBitmap, boundary);
            }
        }

        public override SizeF   Measure(Graphics graphics) { return new SizeF(196, 196); }

        public void InvalidateShaderStructure() { _cachedBitmap = null; _shaderStructureHash = 0;  }
        public void InvalidateParameters() { _cachedBitmap = null; }

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
            // InvalidateParameters();
            return true;
        }

        public override bool OnEndDrag() { base.OnEndDrag(); return true; }

        // note -- 
        //  we really want to do an InvokeMiscChange on the IGraphModel when these change...
        //  but we don't have any way to find the graph model from here!
        public ShaderPatcherLayer.PreviewGeometry Geometry  { get { return _previewGeometry; }      set { if (_previewGeometry != value) { _previewGeometry = value; InvalidateShaderStructure(); } } }
        public string OutputToVisualize                     { get { return _outputToVisualize; }    set { if (_outputToVisualize != value) { _outputToVisualize = value; InvalidateShaderStructure(); } } }

        public ShaderPatcherLayer.PreviewSettings PreviewSettings
        {
            get
            {
                return new ShaderPatcherLayer.PreviewSettings
                    {
                        Geometry = this.Geometry,
                        OutputToVisualize = this.OutputToVisualize
                    };
            }

            set
            {
                Geometry = value.Geometry;
                OutputToVisualize = value.OutputToVisualize;
            }
        }

        private PointF _lastDragLocation;
        private ShaderPatcherLayer.PreviewGeometry _previewGeometry;
        private string _outputToVisualize;
        [Import]
        private ShaderPatcherLayer.IPreviewBuilder _previewManager;

        // bitmap cache --
        private uint _shaderStructureHash;
        private System.Drawing.Bitmap _cachedBitmap;
    }

        //
    /////////////////////////////////////////////////////////////////////////////////////
        //
        //          ShaderFragmentNodeCompatibility
        //
        //      Checks node inputs and outputs for type compatibility
        //  

    internal class ShaderFragmentNodeCompatibility : HyperGraph.Compatibility.ICompatibilityStrategy
    {
        public HyperGraph.Compatibility.ConnectionType CanConnect(NodeConnector from, NodeConnector to)
        {
            if (null == from.Tag && null == to.Tag) return HyperGraph.Compatibility.ConnectionType.Compatible;
            if (null == from.Tag || null == to.Tag) return HyperGraph.Compatibility.ConnectionType.Incompatible;
            if (null == from.Node.SubGraphTag  || from.Node.SubGraphTag != to.Node.SubGraphTag) return HyperGraph.Compatibility.ConnectionType.Incompatible;    // can't connect different subgraph

            if (from.Tag is string && to.Tag is string)
            {
                string fromType = (string)from.Tag;
                string toType = (string)to.Tag;

                    // we use "auto" when we want one end to match whatever is on the other end.
                    // so, "auto" can match to anything -- except another auto. Auto to auto is always incompatible.
                bool fromAuto = fromType.Equals("auto", StringComparison.CurrentCultureIgnoreCase);
                bool toAuto = toType.Equals("auto", StringComparison.CurrentCultureIgnoreCase);
                if (fromAuto || toAuto)
                    return (fromAuto && toAuto) ? HyperGraph.Compatibility.ConnectionType.Incompatible : HyperGraph.Compatibility.ConnectionType.Compatible;

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
    internal class ShaderFragmentInterfaceParameterItem : ShaderFragmentNodeConnector
    {
        public ShaderFragmentInterfaceParameterItem(string name, string type, InterfaceDirection direction) :
            base(name, type, string.Empty)
        {
            Direction = direction;
        }

        public string Semantic { get; set; }
        public string Default { get; set; }
        public InterfaceDirection Direction { get; set; }

        // private static Brush BackgroundBrush = new SolidBrush(Color.FromArgb(96, 96, 96));

        public override void Render(Graphics graphics, RectangleF location, object context)
        {
            /*
            var size = Measure(graphics);
            size.Width = Math.Max(minimumSize.Width, size.Width);
            size.Height = Math.Max(minimumSize.Height, size.Height);
            var rect = new RectangleF(location, size);

            bool highlight = (GetState() & RenderState.Hover) == RenderState.Hover;

            // We want the item to work like 2 buttons: "edit" and "delete"
            // Let's split the item into 2 equal parts, and draw button-type shapes there.
            var editRect = new RectangleF(rect.Left, rect.Top, rect.Width / 2.0f, rect.Height);
            var deleteRect = new RectangleF((rect.Left + rect.Right) / 2.0f, rect.Top, rect.Width / 2.0f, rect.Height);

            var editPath = GraphRenderer.CreateRoundedRectangle(editRect.Size, editRect.Location);
            graphics.FillPath(BackgroundBrush, editPath);
            graphics.DrawPath(highlight ? Pens.White : Pens.LightGray, editPath);
            graphics.DrawString("Edit", SystemFonts.MenuFont, Brushes.White, editRect, GraphConstants.CenterTextStringFormat);
            */
        }

        internal override string GetTypeText() { return ": " + Semantic + " (" + ShortType + ")"; }
    }
    
    internal class ShaderFragmentAddParameterItem : NodeItem
    {
        public override bool OnClick(System.Windows.Forms.Control container, System.Windows.Forms.MouseEventArgs evnt, System.Drawing.Drawing2D.Matrix viewTransform)
        {
            using (var fm = new InterfaceParameterForm(false) { Name = "Color", Type = "auto", Semantic = "", Default = "" })
            {
                var result = fm.ShowDialog();
                if (result == System.Windows.Forms.DialogResult.OK)
                {
                    Node.AddItem(
                        new ShaderFragmentInterfaceParameterItem(fm.Name, fm.Type, Direction) { Semantic = fm.Semantic, Default = fm.Default }, 
                        ShaderFragmentNodeConnector.GetDirectionalColumn(Direction));
                }
            }
            return true;
        }

        public override SizeF Measure(Graphics graphics)
        {
            return new Size(GraphConstants.MinimumItemWidth, GraphConstants.MinimumItemHeight);
        }

        public override void Render(Graphics graphics, RectangleF boundary, object context)
        {
            using (var path = GraphRenderer.CreateRoundedRectangle(boundary.Size, boundary.Location))
            {
                using (var brush = new SolidBrush(Color.FromArgb(64, Color.Black)))
                    graphics.FillPath(brush, path);

                graphics.DrawString(
                    "+", SystemFonts.MenuFont, 
                    ((GetState() & RenderState.Hover) != 0) ? Brushes.White : Brushes.Black, boundary, 
                    GraphConstants.CenterTextStringFormat);
            }
        }

        public InterfaceDirection Direction = InterfaceDirection.In;
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

    internal class ShaderInterfaceParameterNodeTag : ShaderFragmentNodeTag
    {
        public ShaderInterfaceParameterNodeTag() : base(String.Empty) {}
        public InterfaceDirection Direction = InterfaceDirection.In;
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
                { ShaderPatcherLayer.PreviewGeometry.Chart, "Chart" },
                { ShaderPatcherLayer.PreviewGeometry.Plane2D, "2D" },
                { ShaderPatcherLayer.PreviewGeometry.Box, "Box" },
                { ShaderPatcherLayer.PreviewGeometry.Sphere, "Sphere" },
                { ShaderPatcherLayer.PreviewGeometry.Model, "Model" }
            };

            ParamSourceTypeNames = new Dictionary<Enum, string>
            {
                // { ParamSourceType.Material, "Material Parameter" },
                // { ParamSourceType.InterpolatorIntoVertex, "Interpolator Into Vertex Shader" },
                // { ParamSourceType.InterpolatorIntoPixel, "Interpolator Into Pixel Shader" },
                // { ParamSourceType.System, "System Parameter" },
                // { ParamSourceType.Output, "Output" },
                // { ParamSourceType.Constant, "Constant" }

                // { ParamSourceType.InterpolatorIntoPixel, "Input" },
                { ParamSourceType.System, "Input" },
                { ParamSourceType.Output, "Output" }
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

        private string VisibleName(string archiveName)
        {
            int i = archiveName.LastIndexOf(':');
            if (i > 0) return archiveName.Substring(i + 1);
            return archiveName;
        }

        public Node CreateNode(ShaderFragmentArchive.Function fn, string archiveName, ShaderPatcherLayer.PreviewSettings previewSettings)
        {
            var node = new Node((fn!= null) ? fn.Name : VisibleName(archiveName));
            node.Tag = new ShaderProcedureNodeTag(archiveName);

            var previewItem = new ShaderFragmentPreviewItem();
            if (previewSettings != null)
                previewItem.PreviewSettings = previewSettings;

            // use composition to access some exported types --
            {
                CompositionBatch compositionBatch = new CompositionBatch();
                compositionBatch.AddPart(previewItem);
                _composer.Compose(compositionBatch);
            }
            node.AddItem(previewItem, Node.Column.Center);

                // Drop-down selection box for "preview mode"
            var enumList = AsEnumList(previewItem.Geometry, PreviewGeoNames);
            var previewModeSelection = new HyperGraph.Items.NodeDropDownItem(enumList.Item1.ToArray(), enumList.Item2);
            node.AddItem(previewModeSelection, Node.Column.Center);
            previewModeSelection.SelectionChanged += 
                (object sender, HyperGraph.Items.AcceptNodeSelectionChangedEventArgs args) => 
                {
                    if (sender is HyperGraph.Items.NodeDropDownItem)
                    {
                        var item = (HyperGraph.Items.NodeDropDownItem)sender;
                        previewItem.Geometry = AsEnumValue<ShaderPatcherLayer.PreviewGeometry>(item.Items[args.Index], PreviewGeoNames);
                    }
                };

                // Text item box for output visualization string
                // note --  should could potentially become a more complex editing control
                //          it might be useful to have some preset defaults and helpers rather than just
                //          requiring the user to construct the raw string
            var outputToVisualize = new HyperGraph.Items.NodeTextBoxItem(previewItem.OutputToVisualize);
            node.AddItem(outputToVisualize, Node.Column.Center);
            outputToVisualize.TextChanged +=
                (object sender, HyperGraph.Items.AcceptNodeTextChangedEventArgs args) => { previewItem.OutputToVisualize = args.Text; };

            if (fn != null)
            {
                foreach (var param in fn.Signature.Parameters)
                {
                    bool isInput = param.Direction == ShaderPatcherLayer.NodeGraphSignature.ParameterDirection.In;
                    node.AddItem(
                        new ShaderFragmentNodeConnector(param.Name, param.Type, archiveName + ":" + param.Name),
                        ShaderFragmentNodeConnector.GetDirectionalColumn(isInput ? InterfaceDirection.In : InterfaceDirection.Out));
                }
            }
            return node;
        }

        private static void ParameterNodeTypeChanged(object sender, HyperGraph.Items.AcceptNodeSelectionChangedEventArgs args)
        {
            if (sender is HyperGraph.Items.NodeDropDownItem)
            {
                var item = (HyperGraph.Items.NodeDropDownItem)sender;
                var node = item.Node;
                var newType = AsEnumValue<ParamSourceType>(item.Items[args.Index], ParamSourceTypeNames);

                    //  We might have to change the input/output settings on this node
                bool isInput = newType != ParamSourceType.Output;
                var oldColumn = ShaderFragmentNodeConnector.GetDirectionalColumn(isInput ? InterfaceDirection.Out : InterfaceDirection.In);
                var newColumn = ShaderFragmentNodeConnector.GetDirectionalColumn(isInput ? InterfaceDirection.In : InterfaceDirection.Out);
                var oldItems = new List<HyperGraph.NodeItem>(node.ItemsForColumn(oldColumn));
                foreach (var i in oldItems)
                {
                    if (i is ShaderFragmentNodeConnector)
                    {
                                // if this is a node item with exactly 1 input/output
                                // and it is not in the correct direction, then we have to change it.
                                //  we can't change directly. We need to delete and recreate this node item.
                        var fragItem = (ShaderFragmentNodeConnector)i;
                        node.RemoveItem(fragItem);
                        node.AddItem(
                            new ShaderFragmentNodeConnector(fragItem.Name, fragItem.Type, fragItem.ArchiveName),
                            newColumn);
                    }
                }
            }
        }

        public Node CreateEmptyParameterNode(ParamSourceType sourceType, String archiveName, String title)
        {
            var node = new Node(title);
            node.Tag = new ShaderParameterNodeTag(archiveName);

            var enumList = AsEnumList(sourceType, ParamSourceTypeNames);
            var typeSelection = new HyperGraph.Items.NodeDropDownItem(enumList.Item1.ToArray(), enumList.Item2);
            node.AddItem(typeSelection, Node.Column.Center);
            typeSelection.SelectionChanged += ParameterNodeTypeChanged;
            return node;
        }

        public Node CreateParameterNode(ShaderFragmentArchive.ParameterStruct parameter, String archiveName, ParamSourceType type)
        {
            var node = CreateEmptyParameterNode(type, archiveName, (parameter != null) ? parameter.Name : VisibleName(archiveName));
            if (parameter != null)
            {
                foreach (var param in parameter.Parameters)
                {
                    bool isInput = type != ParamSourceType.Output;
                    node.AddItem(
                        new ShaderFragmentNodeConnector(param.Name, param.Type, archiveName + ":" + param.Name),
                        ShaderFragmentNodeConnector.GetDirectionalColumn(isInput ? InterfaceDirection.In : InterfaceDirection.Out));
                }
            }
            return node;
        }

        public Node CreateInterfaceNode(String title, InterfaceDirection direction)
        {
            var node = new Node(title);
            node.Tag = new ShaderInterfaceParameterNodeTag { Direction = direction };
            node.AddItem(new ShaderFragmentAddParameterItem { Direction = direction }, Node.Column.Center);
            return node;
        }

        public Node FindNodeFromId(HyperGraph.IGraphModel graph, UInt64 id)
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

        public string GetDescription(object o)
        {
            var node = o as Node;
            if (node != null)
            {
                return node.Title;
            }

            var item = o as ShaderFragmentNodeConnector;
            if (item != null)
            {
                string result = item.GetNameText() + " " + item.GetTypeText();
                foreach (var c in item.Connectors)
                {
                    var t = (c.From != null) ? (c.From as ShaderFragmentNodeConnector) : null;
                    if (t != null)
                    {
                        result += " <-> " + t.GetNameText() + " in " + t.Node.Title;
                    }
                    else
                        result += " <-> " + c.Name;
                }

                return result;
            }

            var preview = o as ShaderFragmentPreviewItem;
            if (preview != null)
                return "preview";

            var dropDownList = o as HyperGraph.Items.NodeDropDownItem;
            if (dropDownList != null)
                return dropDownList.Items[dropDownList.SelectedIndex].ToString();

            var textBox = o as HyperGraph.Items.NodeTextBoxItem;
            if (textBox != null)
                return textBox.Text;

            return string.Empty;
        }

        public HyperGraph.Compatibility.ICompatibilityStrategy CreateCompatibilityStrategy()
        {
            return new ShaderFragmentNodeCompatibility();
        }

        [Import]
        CompositionContainer _composer;
    }

    #endregion

}
