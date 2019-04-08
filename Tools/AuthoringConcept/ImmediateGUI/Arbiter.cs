using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Drawing;

using Facebook.Yoga;

namespace AuthoringConcept.ImmediateGUI
{
    using PackedColor = UInt32;

    public class IO
    {
        public UInt64 CurrentMouseOver = 0;
        public bool LButtonDown = false;
        public bool LButtonTransition = false;
        public bool LButtonDblClk = false;
        public PointF MousePosition;

        public System.Windows.Forms.Control ParentControl;
        public System.Drawing.Drawing2D.Matrix LocalToParentControl;
    };

    public class ImbuedNode : YogaNode
    {
        public delegate void DrawDelegate(
            ImbuedGraphics graphics,
            RectangleF frameRect, RectangleF contentRect,
            UInt64 controlId,
            IO io);
        public DrawDelegate Draw;

        public delegate void IODelegate(
            RectangleF frame, RectangleF content,
            UInt64 controlId,
            IO io);
        public IODelegate IO;

        public UInt64 Guid;
        public ImbuedNode(YogaConfig cfg, UInt64 guid) : base(cfg) { this.Guid = guid; }
    }

    public class ImbuedRoot : ImbuedNode
    {
        public delegate void RootFrameDelegate(ImbuedRoot node);
        public RootFrameDelegate RootFrame;
        public YogaValue X
        {
            set
            {
                System.Diagnostics.Debug.Assert(value.Unit == YogaUnit.Point);
                _x = value.Value;
            }
        }
        public YogaValue Y
        {
            set
            {
                System.Diagnostics.Debug.Assert(value.Unit == YogaUnit.Point);
                _y = value.Value;
            }
        }
        internal float? GetAssignedX() { return _x; }
        internal float? GetAssignedY() { return _y; }
        public ImbuedRoot(YogaConfig cfg, UInt64 guid) : base(cfg, guid) { }
        private float? _x;
        private float? _y;
    }

    public class Arbiter
    {
        static Style g_style = new Style
        {
            FrameBorderSize = 1.0f,
            FrameRounding = 4.0f,
            ThumbMinSize = 10.0f,
            ThumbRounding = 0.0f
        };

        public static void RenderCheckMark(ImbuedGraphics graphics, PointF pos, PackedColor col, float sz)
        {
            float thickness = Math.Max(sz / 5.0f, 1.0f);
            sz -= thickness * 0.5f;
            pos = PointF.Add(pos, new SizeF(thickness * 0.25f, thickness * 0.25f));

            float third = sz / 3.0f;
            float bx = pos.X + third;
            float by = pos.Y + sz - third * 0.5f;
            graphics.PathLineTo(new PointF(bx - third, by - third));
            graphics.PathLineTo(new PointF(bx, by));
            graphics.PathLineTo(new PointF(bx + third * 2, by - third * 2));
            graphics.PathStroke(col, false, thickness);
        }

        public enum Direction
        {
            None = -1,
            Left = 0,
            Right = 1,
            Up = 2,
            Down = 3
        };

        public static void RenderFrame(ImbuedGraphics graphics, PointF p_min, PointF p_max, PackedColor fill_col, bool border, float rounding, bool shadow = false)
        {
            graphics.AddRectFilled(p_min, p_max, fill_col, rounding);
            float border_size = g_style.FrameBorderSize;
            if (border && border_size > 0.0f)
            {
                if (shadow)
                    graphics.AddRect(PointF.Add(p_min, new SizeF(1.0f, 1.0f)), PointF.Add(p_max, new SizeF(1.0f, 1.0f)), GetColorU32(SystemColor.ImGuiCol_BorderShadow), rounding, (int)ImbuedGraphics.DrawCornerFlags.All, border_size);
                graphics.AddRect(p_min, p_max, GetColorU32(SystemColor.ImGuiCol_Border), rounding, (int)ImbuedGraphics.DrawCornerFlags.All, border_size);
            }
        }

        public static void RenderFrameBorder(ImbuedGraphics graphics, PointF p_min, PointF p_max, float rounding, bool shadow = false)
        {
            float border_size = g_style.FrameBorderSize;
            if (border_size > 0.0f)
            {
                if (shadow)
                    graphics.AddRect(PointF.Add(p_min, new SizeF(1.0f, 1.0f)), PointF.Add(p_max, new SizeF(1.0f, 1.0f)), GetColorU32(SystemColor.ImGuiCol_BorderShadow), rounding, (int)ImbuedGraphics.DrawCornerFlags.All, border_size);
                graphics.AddRect(p_min, p_max, GetColorU32(SystemColor.ImGuiCol_Border), rounding, (int)ImbuedGraphics.DrawCornerFlags.All, border_size);
            }
        }

        public static void RenderArrow(ImbuedGraphics graphics, RectangleF loc, Direction dir, float scale)
        {
            float h = Math.Min(loc.Width, loc.Height);
            float r = h * 0.40f * scale;
            PointF center = new PointF(loc.X + 0.5f * loc.Width, loc.Y + 0.5f * loc.Height);

            PointF a, b, c;
            switch (dir)
            {
                case Direction.Up:
                case Direction.Down:
                    if (dir == Direction.Up) r = -r;
                    a = new PointF(+0.000f * r, +0.750f * r);
                    b = new PointF(-0.866f * r, -0.750f * r);
                    c = new PointF(+0.866f * r, -0.750f * r);
                    break;
                case Direction.Left:
                case Direction.Right:
                    if (dir == Direction.Left) r = -r;
                    a = new PointF(+0.750f * r, +0.000f * r);
                    b = new PointF(-0.750f * r, +0.866f * r);
                    c = new PointF(-0.750f * r, -0.866f * r);
                    break;
                case Direction.None:
                default:
                    System.Diagnostics.Debug.Assert(false);
                    a = new PointF(0.0f, 0.0f);
                    b = new PointF(0.0f, 0.0f);
                    c = new PointF(0.0f, 0.0f);
                    break;
            }

            graphics.AddTriangleFilled(
                new PointF(center.X + a.X, center.Y + a.Y),
                new PointF(center.X + b.X, center.Y + b.Y),
                new PointF(center.X + c.X, center.Y + c.Y),
                GetColorU32(SystemColor.ImGuiCol_Text));
        }

        public static string HideAfterHash(string label)
        {
            using (var enumerator = label.GetEnumerator())
            {
                if (!enumerator.MoveNext()) return label;
                int idx = 0;
                var first = enumerator.Current;
                for (; ; )
                {
                    if (!enumerator.MoveNext()) return label;
                    var second = enumerator.Current;
                    if (first == '#' && second == '#') return label.Substring(0, idx);
                    first = second;
                    ++idx;
                }
            }
        }

        public static void RenderText(ImbuedGraphics graphics, PointF pos, string text, PackedColor color)
        {
            graphics.Underlying.DrawString(
                text, SystemFonts.DefaultFont,
                new SolidBrush(Color.FromArgb((int)color)),
                pos.X, pos.Y);
        }

        public static PointF GetMin(RectangleF rect) { return new PointF(rect.Left, rect.Top); }
        public static PointF GetMax(RectangleF rect) { return new PointF(rect.Right, rect.Bottom); }

        public static PackedColor GetColorU32(SystemColor colorName)
        {
            return Style.GetColorU32(colorName);
        }

        public static UInt64 MakeGuid(string label) { return (UInt64)label.GetHashCode(); }
        public static UInt64 GuidCombine(UInt64 lhs, UInt64 rhs) { return lhs ^ rhs; } // HashCode.Combine(lhs, rhs); }

        public static bool ButtonBehaviour(IO io, UInt64 controlId, out bool held, out bool hovered)
        {
            hovered = controlId == io.CurrentMouseOver;
            held = hovered && io.LButtonDown;
            return hovered && !io.LButtonDown && io.LButtonTransition;
        }

        public class SimpleControl : IDisposable
        {
            public ImbuedNode Node;

            public void Dispose() { }
        }

        public SimpleControl Checkbox(string label, Utils.GetDelegate<bool> getter, Utils.SetDelegate<bool> setter)
        {
            ImbuedNode node = new ImbuedNode(Config, GuidCombine(_workingStack.Peek().Guid, MakeGuid(label)));
            node.Margin = 2;
            node.SetMeasureFunction((_, width, widthMode, height, heightMode) =>
            {
                SizeF labelSize = Graphics.MeasureString(HideAfterHash(label), SystemFonts.DefaultFont, new SizeF(width, height), StringFormat.GenericDefault);
                float check_sz = Math.Min(labelSize.Width, labelSize.Height);
                return MeasureOutput.Make(labelSize.Width + check_sz + 2.0f, labelSize.Height);
            });
            node.Draw = (ImbuedGraphics graphics, RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                bool pressed = ButtonBehaviour(io, controlId, out bool held, out bool hovered);

                float check_sz = Math.Min(content.Width, content.Height);
                RectangleF check_bb = new RectangleF(GetMin(content), new SizeF(check_sz, check_sz));
                RenderFrame(
                    graphics,
                    GetMin(check_bb), GetMax(check_bb),
                    GetColorU32((held && hovered) ? SystemColor.ImGuiCol_FrameBgActive : hovered ? SystemColor.ImGuiCol_FrameBgHovered : SystemColor.ImGuiCol_FrameBg),
                    false, 0.0f);
                if (getter())
                {
                    float pad = Math.Max(1.0f, (float)(int)(check_sz / 6.0f));
                    RenderCheckMark(graphics, PointF.Add(GetMin(check_bb), new SizeF(pad, pad)), GetColorU32(SystemColor.ImGuiCol_CheckMark), check_bb.Width - pad * 2.0f);
                }
                RenderText(graphics, PointF.Add(GetMin(content), new SizeF(check_sz + 2.0f, 0)), HideAfterHash(label), GetColorU32(SystemColor.ImGuiCol_Text));
            };
            node.IO = (RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                bool pressed = ButtonBehaviour(io, controlId, out bool held, out bool hovered);
                if (pressed)
                {
                    bool oldState = getter();
                    setter(!oldState);
                }
            };
            _workingStack.Peek().AddChild(node);
            return new SimpleControl { Node = node };
        }

        public SimpleControl ArrowCheckbox(Utils.GetDelegate<bool> getter, Utils.SetDelegate<bool> setter)
        {
            ImbuedNode node = new ImbuedNode(Config, GuidCombine(_workingStack.Peek().Guid, MakeGuid("##arrow")));
            node.SetMeasureFunction((_, width, widthMode, height, heightMode) =>
            {
                return MeasureOutput.Make(SystemFonts.DefaultFont.Height, SystemFonts.DefaultFont.Height);
            });
            node.Draw = (ImbuedGraphics graphics, RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                bool pressed = ButtonBehaviour(io, controlId, out bool held, out bool hovered);
                bool is_open = getter();
                RenderArrow(graphics, content, is_open ? Direction.Down : Direction.Right, 1.0f);
            };
            node.IO = (RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                bool pressed = ButtonBehaviour(io, controlId, out bool held, out bool hovered);
                if (pressed)
                {
                    bool oldState = getter();
                    setter(!oldState);
                }
            };
            _workingStack.Peek().AddChild(node);
            return new SimpleControl { Node = node };
        }

        public SimpleControl Label(string label)
        {
            ImbuedNode node = new ImbuedNode(Config, GuidCombine(_workingStack.Peek().Guid, MakeGuid(label)));
            node.SetMeasureFunction((_, width, widthMode, height, heightMode) =>
            {
                SizeF stringSize = Graphics.MeasureString(label, SystemFonts.DefaultFont, new SizeF(width, height), StringFormat.GenericDefault);
                return MeasureOutput.Make(stringSize.Width, stringSize.Height);
            });
            node.Draw = (ImbuedGraphics graphics, RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                RenderText(graphics, GetMin(content), label, GetColorU32(SystemColor.ImGuiCol_Text));
            };
            _workingStack.Peek().AddChild(node);
            return new SimpleControl { Node = node };
        }

        public SimpleControl Label(Utils.GetDelegate<string> label)
        {
            ImbuedNode node = new ImbuedNode(Config, 0);
            node.SetMeasureFunction((_, width, widthMode, height, heightMode) =>
            {
                SizeF stringSize = Graphics.MeasureString(label(), SystemFonts.DefaultFont, new SizeF(width, height), StringFormat.GenericDefault);
                return MeasureOutput.Make(stringSize.Width, stringSize.Height);
            });
            node.Draw = (ImbuedGraphics graphics, RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                RenderText(graphics, GetMin(content), label(), GetColorU32(SystemColor.ImGuiCol_Text));
            };
            _workingStack.Peek().AddChild(node);
            return new SimpleControl { Node = node };
        }

        private static bool CollapsingContainer_IsOpen = false;
        private static bool ComboBox_IsOpen = false;

        public OpenableControl BeginCollapsingContainer(string label)
        {
            ImbuedNode outerNode = new ImbuedNode(Config, GuidCombine(_workingStack.Peek().Guid, MakeGuid("##collapsingcontainer")));
            outerNode.Padding = 0;       // zero padding because the headerContainer and contentContainers have their own padding
            outerNode.Margin = 2;
            _workingStack.Peek().AddChild(outerNode);

            float headerHeight = SystemFonts.DefaultFont.Height * 1.5f;

            {
                ImbuedNode headerContainer = new ImbuedNode(Config, GuidCombine(outerNode.Guid, MakeGuid(label)));
                headerContainer.Margin = 0;
                headerContainer.Width = YogaValue.Percent(100.0f);
                headerContainer.Height = headerHeight;
                headerContainer.AlignItems = YogaAlign.Center;
                headerContainer.FlexDirection = YogaFlexDirection.Row;
                outerNode.AddChild(headerContainer);
                _workingStack.Push(headerContainer);

                var arrowNode = ArrowCheckbox(
                    () => CollapsingContainer_IsOpen,
                    (bool newValue) => CollapsingContainer_IsOpen = newValue);
                arrowNode.Node.Margin = 5;
                Label(label);

                _workingStack.Pop();
            }

            outerNode.Draw = (ImbuedGraphics graphics, RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                RenderFrame(
                    graphics,
                    GetMin(frame), GetMax(frame),
                    GetColorU32(SystemColor.ImGuiCol_FrameBg),
                    true, g_style.FrameRounding, true);

                var headerFrameMax = GetMax(frame);
                headerFrameMax.Y = frame.Y + headerHeight;
                graphics.AddRectFilled(
                    GetMin(frame), headerFrameMax,
                    GetColorU32(SystemColor.ImGuiCol_Header),
                    g_style.FrameRounding,
                    (int)(CollapsingContainer_IsOpen ? ImbuedGraphics.DrawCornerFlags.Top : ImbuedGraphics.DrawCornerFlags.All));
            };

            ImbuedNode contentContainer = new ImbuedNode(Config, GuidCombine(outerNode.Guid, MakeGuid("##contentcontainer")));
            if (CollapsingContainer_IsOpen)
                contentContainer.Margin = 2;
            outerNode.AddChild(contentContainer);

            _workingStack.Push(contentContainer);       // upcoming nodes will go into the content container

            return new OpenableControl { Node = outerNode, IsOpen = CollapsingContainer_IsOpen };
        }

        public void EndCollapsingContainer()
        {
            _workingStack.Pop();
        }

        public SimpleControl BaseComboControl(string label, Utils.GetDelegate<string> selectedContent)
        {
            ImbuedNode node = new ImbuedNode(Config, GuidCombine(_workingStack.Peek().Guid, MakeGuid(label)));
            node.FlexDirection = YogaFlexDirection.Row;
            node.JustifyContent = YogaJustify.FlexEnd;
            node.Draw = (ImbuedGraphics graphics, RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                bool pressed = ButtonBehaviour(io, controlId, out bool held, out bool hovered);
                RenderFrame(
                    graphics,
                    GetMin(frame), GetMax(frame),
                    GetColorU32((held && hovered) ? SystemColor.ImGuiCol_FrameBgActive : hovered ? SystemColor.ImGuiCol_FrameBgHovered : SystemColor.ImGuiCol_FrameBg),
                    true, 0.0f);
            };
            node.IO = (RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                bool pressed = ButtonBehaviour(io, controlId, out bool held, out bool hovered);
                if (pressed)
                {
                    ComboBox_IsOpen = !ComboBox_IsOpen;
                }
            };

            _workingStack.Push(node);
            var labelNode = Label(label + ": " + selectedContent());
            labelNode.Node.Margin = YogaValue.Point(2.0f);
            labelNode.Node.FlexGrow = 1;

            ImbuedNode arrowBox = new ImbuedNode(Config, 0);
            arrowBox.Width = 1.2f * SystemFonts.DefaultFont.Height;
            arrowBox.Height = YogaValue.Auto();
            arrowBox.Margin = 1;

            arrowBox.Draw = (ImbuedGraphics graphics, RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                RenderFrame(
                    graphics,
                    GetMin(frame), GetMax(frame),
                    GetColorU32(SystemColor.ImGuiCol_Button),
                    false, 0.0f);
                RenderArrow(graphics, content, Direction.Down, 1.0f);
            };
            node.AddChild(arrowBox);

            _workingStack.Pop();
            _workingStack.Peek().AddChild(node);
            return new SimpleControl { Node = node };
        }

        public class OpenableControl : SimpleControl
        {
            public bool IsOpen;
        }

        public OpenableControl BeginComboBox(string label, Utils.GetDelegate<string> selectedContent)
        {
            var baseNode = BaseComboControl(label, selectedContent);

            if (ComboBox_IsOpen)
            {
                var hover = BeginHoveringContainer();
                hover.RootFrame = (ImbuedRoot node) =>
                {
                    var topLeft = Utils.GetAbsoluteTopLeft(baseNode.Node);
                    node.X = topLeft.X;
                    node.Y = topLeft.Y + baseNode.Node.LayoutHeight;
                    node.MinWidth = baseNode.Node.LayoutWidth;
                };
            }
            return new OpenableControl { Node = baseNode.Node, IsOpen = ComboBox_IsOpen };
        }

        public void EndComboBox()
        {
            _workingStack.Pop();
        }

        public void MakeComboBoxItem(ImbuedNode node, int itemIndex, Utils.GetDelegate<int> getter, Utils.SetDelegate<int> setter)
        {
            node.Padding = 4;
            var oldDraw = node.Draw;
            node.Draw = (ImbuedGraphics graphics, RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                bool pressed = ButtonBehaviour(io, controlId, out bool held, out bool hovered);
                bool selected = getter() == itemIndex;

                if (held || hovered || selected)
                {
                    RenderFrame(
                        graphics, GetMin(frame), GetMax(frame),
                        GetColorU32((held && hovered) ? SystemColor.ImGuiCol_ButtonActive : hovered ? SystemColor.ImGuiCol_ButtonHovered : SystemColor.ImGuiCol_ButtonActive),
                        false, 0.0f);
                }

                oldDraw(graphics, frame, content, controlId, io);
            };
            node.IO = (RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                bool pressed = ButtonBehaviour(io, controlId, out bool held, out bool hovered);
                if (pressed)
                {
                    ComboBox_IsOpen = false;
                    setter(itemIndex);
                }
            };
        }

        public OpenableControl ComboBox(string label, IEnumerable<string> selectableItems, Utils.GetDelegate<int> getter, Utils.SetDelegate<int> setter)
        {
            var res = BeginComboBox(label, () => { return selectableItems.ElementAt(getter()); });
            if (res.IsOpen)
            {
                for (int c = 0; c < selectableItems.Count(); ++c)
                {
                    var node = Label(selectableItems.ElementAt(c));
                    MakeComboBoxItem(node.Node, c, getter, setter);
                }
                EndComboBox();
            }
            return res;
        }

        private interface InnerSliderHelper
        {
            float GetThumbPosition();
            void SetThumbPosition(float value);
            bool IsDecimal();
            float Range();          // range of values (ie, max-min) in float representation
        }

        private class InnerSliderHelperInt : InnerSliderHelper
        {
            const bool is_decimal = false;
            const float power = 1.0f;

            public virtual float GetThumbPosition()
            {
                return (Getter() - Min) / (float)(Max - Min);
            }
            public virtual void SetThumbPosition(float clicked_t)
            {
                // Derived from imgui_widgets.cpp. However, C# doesn't support template math functions well
                // const bool is_power = (power != 1.0f) && is_decimal;
                int v_min = Min, v_max = Max;

                /*float linear_zero_pos;   // 0.0->1.0f
                if (is_power && v_min * v_max < 0.0f)
                {
                    // Different sign
                    double linear_dist_min_to_0 = Math.Pow(v_min >= 0 ? (double)v_min : -(double)v_min, (double)1.0f / power);
                    double linear_dist_max_to_0 = Math.Pow(v_max >= 0 ? (double)v_max : -(double)v_max, (double)1.0f / power);
                    linear_zero_pos = (float)(linear_dist_min_to_0 / (linear_dist_min_to_0 + linear_dist_max_to_0));
                }
                else
                {
                    // Same sign
                    linear_zero_pos = v_min < 0.0f ? 1.0f : 0.0f;
                }*/

                int v_new;
                // For integer values we want the clicking position to match the grab box so we round above
                // This code is carefully tuned to work with large values (e.g. high ranges of U64) while preserving this property..
                double v_new_off_f = (v_max - v_min) * clicked_t;
                int v_new_off_floor = (int)(v_new_off_f);
                int v_new_off_round = (int)(v_new_off_f + (double)0.5);
                if (!is_decimal && v_new_off_floor < v_new_off_round)
                    v_new = v_min + v_new_off_round;
                else
                    v_new = v_min + v_new_off_floor;

                // Round to user desired precision based on format string
                // v_new = RoundScalarWithFormatT<TYPE, SIGNEDTYPE>(format, data_type, v_new);

                if (v_new != Getter())
                {
                    Setter(v_new);
                }
            }
            public virtual bool IsDecimal() { return false; }
            public virtual float Range() { return Max - Min; }

            public Utils.GetDelegate<int> Getter { get; set; }
            public Utils.SetDelegate<int> Setter { get; set; }
            public int Min { get; set; }
            public int Max { get; set; }
        }

        private static Tuple<float, float> CalculateSliderMinMax(RectangleF content, InnerSliderHelper helper, out float thumbSize)
        {
            thumbSize = g_style.ThumbMinSize;
            if (!helper.IsDecimal())
                thumbSize = Math.Max((float)(content.Width / (helper.Range() + 1)), thumbSize);    // For integer sliders: if possible have the grab size represent 1 unit
            thumbSize = Math.Min(thumbSize, content.Width);

            float thumbMinX = content.Left + thumbSize * 0.5f,
                  thumbMaxX = content.Right - thumbSize * 0.5f;

            return new Tuple<float, float>(thumbMinX, thumbMaxX);
        }

        private SimpleControl InnerSlider(InnerSliderHelper helper)
        {
            ImbuedNode slider = new ImbuedNode(Config, GuidCombine(_workingStack.Peek().Guid, MakeGuid("##innerslider")));
            slider.Draw = (ImbuedGraphics graphics, RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                graphics.AddRectFilled(
                    new PointF(content.Left, content.Y + 0.5f * content.Height - 2.0f),
                    new PointF(content.Right, content.Y + 0.5f * content.Height + 2.0f),
                    GetColorU32(SystemColor.ImGuiCol_Border), 0.0f);

                var thumbMinMax = CalculateSliderMinMax(content, helper, out float thumbSize);

                float thumbPosition = Utils.Clamp(helper.GetThumbPosition(), 0.0f, 1.0f);
                float thumbPos = thumbMinMax.Item1 * (1.0f - thumbPosition) + thumbPosition * thumbMinMax.Item2;
                RectangleF thumb = new RectangleF(
                    thumbPos - thumbSize * 0.5f, content.Top,
                    thumbSize, content.Height);

                graphics.AddRectFilled(
                    GetMin(thumb), GetMax(thumb),
                    GetColorU32(io.CurrentMouseOver == controlId ? SystemColor.ImGuiCol_SliderGrabActive : SystemColor.ImGuiCol_SliderGrab),
                    g_style.ThumbRounding);
            };
            slider.SetMeasureFunction((_, width, widthMode, height, heightMode) =>
            {
                return MeasureOutput.Make(g_style.ThumbMinSize, SystemFonts.DefaultFont.Height);
            });
            slider.IO = (RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                if (io.CurrentMouseOver == controlId && io.LButtonDown)
                {
                    // While mouse down, move the slider to the given point.
                    // This applies regardless of whether it's a mouse click or a hold/drag operation
                    // Also note that the thumb always teleports directly to the pressed location
                    // There's no page up/page down type behaviour
                    var thumbMinMax = CalculateSliderMinMax(content, helper, out float thumbSize);
                    float ratio = (io.MousePosition.X - thumbMinMax.Item1) / (thumbMinMax.Item2 - thumbMinMax.Item1);
                    ratio = Utils.Clamp(ratio, 0.0f, 1.0f);
                    helper.SetThumbPosition(ratio);
                }
            };

            _workingStack.Peek().AddChild(slider);
            return new SimpleControl { Node = slider };
        }

        public SimpleControl ScalarSlider(string label, Utils.GetDelegate<int> getter, Utils.SetDelegate<int> setter, int minValue, int maxValue)
        {
            ImbuedNode outerNode = new ImbuedNode(Config, GuidCombine(_workingStack.Peek().Guid, MakeGuid(label)));
            outerNode.FlexDirection = YogaFlexDirection.Row;
            outerNode.JustifyContent = YogaJustify.FlexEnd;
            _workingStack.Push(outerNode);

            var slider = InnerSlider(
                new InnerSliderHelperInt
                {
                    Getter = getter,
                    Setter = setter,
                    Min = minValue,
                    Max = maxValue
                });
            slider.Node.Margin = 2;
            slider.Node.FlexGrow = 1;

            Label(label);

            _workingStack.Pop();
            _workingStack.Peek().AddChild(outerNode);
            return new SimpleControl { Node = outerNode };
        }

        public SimpleControl Float(string label, Utils.GetDelegate<float> getter, Utils.SetDelegate<float> setter)
        {
            ImbuedNode outerFrame = new ImbuedNode(Config, GuidCombine(_workingStack.Peek().Guid, MakeGuid(label)));
            outerFrame.FlexDirection = YogaFlexDirection.Row;
            outerFrame.JustifyContent = YogaJustify.SpaceBetween;
            outerFrame.Margin = 2;
            _workingStack.Push(outerFrame);

            outerFrame.Draw = (ImbuedGraphics graphics, RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                RenderFrame(
                    graphics,
                    GetMin(frame), GetMax(frame),
                    GetColorU32(SystemColor.ImGuiCol_Header),
                    true, g_style.FrameRounding);
                RenderFrameBorder(graphics, GetMin(frame), GetMax(frame), g_style.FrameRounding);
            };

            {
                ImbuedNode arrowBox = new ImbuedNode(Config, GuidCombine(outerFrame.Guid, MakeGuid("reduce")));
                arrowBox.Width = 1.2f * SystemFonts.DefaultFont.Height;
                arrowBox.Height = YogaValue.Auto();
                arrowBox.Margin = 1;
                arrowBox.Draw = (ImbuedGraphics graphics, RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
                {
                    RenderArrow(graphics, content, Direction.Left, 1.0f);
                };
                arrowBox.IO = (RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
                {
                    ButtonBehaviour(io, controlId, out bool held, out bool hovered);
                    if (held)
                    {
                        float initialValue = getter();
                        if (initialValue >= -0.1f && initialValue <= 0.1f)
                        {
                            setter(initialValue - 0.1f);
                        }
                        else if (initialValue > 0.0f)
                        {
                            setter(1.0f / 1.2f * getter());
                        }
                        else
                        {
                            setter(1.2f * getter());
                        }
                    }
                };
                outerFrame.AddChild(arrowBox);
            }

            Label(label + ": " + getter().ToString("N2")).Node.Margin = 2;

            {
                ImbuedNode arrowBox = new ImbuedNode(Config, GuidCombine(outerFrame.Guid, MakeGuid("increase")));
                arrowBox.Width = 1.2f * SystemFonts.DefaultFont.Height;
                arrowBox.Height = YogaValue.Auto();
                arrowBox.Margin = 1;
                arrowBox.Draw = (ImbuedGraphics graphics, RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
                {
                    RenderArrow(graphics, content, Direction.Right, 1.0f);
                };
                arrowBox.IO = (RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
                {
                    ButtonBehaviour(io, controlId, out bool held, out bool hovered);
                    if (held)
                    {
                        float initialValue = getter();
                        if (initialValue >= -0.1f && initialValue <= 0.1f)
                        {
                            setter(initialValue + 0.1f);
                        }
                        else if (initialValue > 0.0f)
                        {
                            setter(1.2f * getter());
                        }
                        else
                        {
                            setter(1.0f / 1.2f * getter());
                        }
                    }
                };
                outerFrame.AddChild(arrowBox);
            }

            _workingStack.Pop();
            _workingStack.Peek().AddChild(outerFrame);
            return new SimpleControl { Node = outerFrame };
        }

        public SimpleControl InnerBoundedFloat(float min, float max, Utils.GetDelegate<float> getter, Utils.SetDelegate<float> setter)
        {
            ImbuedNode node = new ImbuedNode(Config, GuidCombine(_workingStack.Peek().Guid, MakeGuid("boundedvalue")));
            node.Margin = 0;
            _workingStack.Peek().AddChild(node);

            node.Draw = (ImbuedGraphics graphics, RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                float fraction = (getter() - min) / (max - min);
                fraction = Math.Min(Math.Max(fraction, 0.0f), 1.0f);

                PointF p_min = GetMin(frame);
                PointF p_max = GetMax(frame);

                p_max.X = p_min.X * (1.0f - fraction) + p_max.X * fraction;
                graphics.AddRectFilled(p_min, p_max, Style.SliderFilled);
            };

            node.IO = (RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                ButtonBehaviour(io, controlId, out bool held, out bool hovered);
                if (held)
                {
                    PointF p_min = GetMin(frame);
                    PointF p_max = GetMax(frame);

                    float fraction = (io.MousePosition.X - p_min.X) / (p_max.X - p_min.X);
                    fraction = Math.Min(Math.Max(fraction, 0.0f), 1.0f);

                    setter(min * (1.0f - fraction) + max * fraction);
                }
            };

            return new SimpleControl { Node = node };
        }

        public SimpleControl BoundedFloat(string label, float min, float max, Utils.GetDelegate<float> getter, Utils.SetDelegate<float> setter)
        {
            ImbuedNode outerFrame = new ImbuedNode(Config, GuidCombine(_workingStack.Peek().Guid, MakeGuid(label)));
            outerFrame.FlexDirection = YogaFlexDirection.Row;
            outerFrame.JustifyContent = YogaJustify.SpaceBetween;
            outerFrame.Padding = 0;
            outerFrame.Margin = 2;
            _workingStack.Push(outerFrame);

            outerFrame.Draw = (ImbuedGraphics graphics, RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                RenderFrameBorder(graphics, GetMin(frame), GetMax(frame), 1.5f * g_style.FrameRounding);
            };

            {
                ImbuedNode labelBox = new ImbuedNode(Config, GuidCombine(_workingStack.Peek().Guid, MakeGuid("label")));
                labelBox.Draw = (ImbuedGraphics graphics, RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
                {
                    graphics.AddRectFilled(GetMin(frame), GetMax(frame), Style.ControlDark, 1.5f * g_style.FrameRounding, (int)ImbuedGraphics.DrawCornerFlags.Left);
                    RenderText(graphics, PointF.Add(GetMin(content), new SizeF(2, 2)), label + ": " + getter().ToString("N2"), Style.ControlDarkText);
                };
                labelBox.IO = (RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
                {
                    if (io.CurrentMouseOver == controlId && io.LButtonDblClk)
                    {
                        PointF[] transformables = { frame.Location, new PointF(frame.Right, frame.Bottom) };
                        io.LocalToParentControl.TransformPoints(transformables);

                        var bounds = new Rectangle(
                            (int)transformables[0].X, (int)transformables[0].Y,
                            (int)transformables[1].X - (int)transformables[0].X, (int)transformables[1].Y - (int)transformables[0].Y);

                        Utils.HoveringFloatEditBox(
                            setter, getter,
                            bounds, io.ParentControl);
                    }
                };
                labelBox.SetMeasureFunction((_, width, widthMode, height, heightMode) =>
                {
                    SizeF stringSize = Graphics.MeasureString(label + ": XXXX", SystemFonts.DefaultFont, new SizeF(width, height), StringFormat.GenericDefault);
                    return MeasureOutput.Make(stringSize.Width+4, stringSize.Height+4);
                });
                outerFrame.AddChild(labelBox);

                var innerCtrl = InnerBoundedFloat(min, max, getter, setter);
                innerCtrl.Node.FlexGrow = 1;
                innerCtrl.Node.MarginTop = 2;
                innerCtrl.Node.MarginBottom = 2;
            }

            _workingStack.Pop();
            _workingStack.Peek().AddChild(outerFrame);
            return new SimpleControl { Node = outerFrame };
        }

        public SimpleControl InnerBoundedInt(int min, int max, Utils.GetDelegate<int> getter, Utils.SetDelegate<int> setter)
        {
            ImbuedNode node = new ImbuedNode(Config, GuidCombine(_workingStack.Peek().Guid, MakeGuid("boundedvalue")));
            node.Margin = 0;
            _workingStack.Peek().AddChild(node);

            node.Draw = (ImbuedGraphics graphics, RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                float fraction = (getter() - min) / (float)(max - min);
                fraction = Math.Min(Math.Max(fraction, 0.0f), 1.0f);

                PointF p_min = GetMin(frame);
                PointF p_max = GetMax(frame);

                p_max.X = p_min.X * (1.0f - fraction) + p_max.X * fraction;
                graphics.AddRectFilled(p_min, p_max, GetColorU32(SystemColor.ImGuiCol_Header), 2.0f * g_style.FrameRounding, (int)ImbuedGraphics.DrawCornerFlags.Right);
            };

            node.IO = (RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                ButtonBehaviour(io, controlId, out bool held, out bool hovered);
                if (held)
                {
                    PointF p_min = GetMin(frame);
                    PointF p_max = GetMax(frame);

                    float fraction = (io.MousePosition.X - p_min.X) / (p_max.X - p_min.X);
                    fraction = Math.Min(Math.Max(fraction, 0.0f), 1.0f);

                    setter((int)(min * (1.0f - fraction) + max * fraction));
                }
            };

            return new SimpleControl { Node = node };
        }

        public SimpleControl BoundedInt(string label, int min, int max, Utils.GetDelegate<int> getter, Utils.SetDelegate<int> setter)
        {
            ImbuedNode outerFrame = new ImbuedNode(Config, GuidCombine(_workingStack.Peek().Guid, MakeGuid(label)));
            outerFrame.FlexDirection = YogaFlexDirection.Row;
            outerFrame.JustifyContent = YogaJustify.SpaceBetween;
            outerFrame.Padding = 0;
            outerFrame.Margin = 2;
            _workingStack.Push(outerFrame);

            outerFrame.Draw = (ImbuedGraphics graphics, RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                RenderFrameBorder(graphics, GetMin(frame), GetMax(frame), 2.0f * g_style.FrameRounding);
            };

            {
                ImbuedNode labelBox = new ImbuedNode(Config, 0);
                labelBox.Draw = (ImbuedGraphics graphics, RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
                {
                    graphics.AddRectFilled(GetMin(frame), GetMax(frame), GetColorU32(SystemColor.ImGuiCol_Header), 2.0f * g_style.FrameRounding, (int)ImbuedGraphics.DrawCornerFlags.Left);
                    RenderText(graphics, PointF.Add(GetMin(content), new SizeF(2,2)), label + ": " + getter(), GetColorU32(SystemColor.ImGuiCol_Text));
                };
                labelBox.IO = (RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
                {
                    ButtonBehaviour(io, controlId, out bool held, out bool hovered);
                    if (held)
                        System.Diagnostics.Debug.Print("Increase");
                };
                labelBox.SetMeasureFunction((_, width, widthMode, height, heightMode) =>
                {
                    SizeF stringSize = Graphics.MeasureString(label + ": XXXX", SystemFonts.DefaultFont, new SizeF(width, height), StringFormat.GenericDefault);
                    return MeasureOutput.Make(stringSize.Width + 4, stringSize.Height + 4);
                });
                outerFrame.AddChild(labelBox);

                InnerBoundedInt(min, max, getter, setter).Node.FlexGrow = 1;
            }

            _workingStack.Pop();
            _workingStack.Peek().AddChild(outerFrame);
            return new SimpleControl { Node = outerFrame };
        }

        public SimpleControl BoundedIntLog(string label, int min, int max, Utils.GetDelegate<int> getter, Utils.SetDelegate<int> setter)
        {
            return BoundedInt(label, min, max, getter, setter);
        }

        public SimpleControl BeginGroup(string label)
        {
            ImbuedNode outerFrame = new ImbuedNode(Config, GuidCombine(_workingStack.Peek().Guid, MakeGuid(label)));
            outerFrame.FlexDirection = YogaFlexDirection.Column;
            outerFrame.Margin = 2;
            outerFrame.Padding = 2;
            _workingStack.Peek().AddChild(outerFrame);
            _workingStack.Push(outerFrame);

            outerFrame.Draw = (ImbuedGraphics graphics, RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                RenderFrameBorder(graphics, GetMin(frame), GetMax(frame), g_style.FrameRounding, true);
            };

            var l = Label(label);

            return new SimpleControl { Node = outerFrame };
        }

        public void EndGroup()
        {
            _workingStack.Pop();
        }

        public SimpleControl BeginHiddenHorizontal()
        {
            ImbuedNode outerFrame = new ImbuedNode(Config, 0);
            outerFrame.FlexDirection = YogaFlexDirection.Row;
            outerFrame.Margin = 0;
            outerFrame.Padding = 0;
            _workingStack.Peek().AddChild(outerFrame);
            _workingStack.Push(outerFrame);
            return new SimpleControl { Node = outerFrame };
        }

        public void EndHiddenHorizontal()
        {
            _workingStack.Pop();
        }

        public ImbuedRoot BeginRoot()
        {
            ImbuedRoot windowNode = new ImbuedRoot(Config, MakeGuid("##root"));
            _windowStack.Push(windowNode);
            _workingStack.Push(windowNode);
            return windowNode;
        }

        public void EndRoot()
        {
            _workingStack.Pop();
            _roots.Add(_windowStack.Pop());
        }

        public ImbuedRoot BeginHoveringContainer()
        {
            ImbuedRoot windowNode = BeginRoot();

            windowNode.Draw = (ImbuedGraphics graphics, RectangleF frame, RectangleF content, UInt64 controlId, IO io) =>
            {
                RenderFrame(
                    graphics, GetMin(frame), GetMax(frame),
                    GetColorU32(SystemColor.ImGuiCol_FrameBg),
                    true, g_style.FrameRounding, true);
            };

            return windowNode;
        }

        public void EndHoveringContainer()
        {
            EndRoot();
        }

        public void Reset()
        {
            _roots = new List<ImbuedRoot>();
            _workingStack = new Stack<ImbuedNode>();
            _windowStack = new Stack<ImbuedRoot>();
        }

        public IEnumerable<ImbuedRoot> GetRoots()
        {
            while (_windowStack.Any())
                _roots.Add(_windowStack.Pop());
            return (_roots as IEnumerable<ImbuedRoot>).Reverse();
        }

        public YogaConfig Config = new YogaConfig();
        public Graphics Graphics;       // (used for measuring strings in some of the measure functions)

        private Stack<ImbuedNode> _workingStack;
        private Stack<ImbuedRoot> _windowStack;
        private List<ImbuedRoot> _roots;
    }
}
