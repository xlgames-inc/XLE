using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Drawing;

namespace AuthoringConcept.ImmediateGUI
{
    using PackedColor = UInt32;

    public class ImbuedGraphics
    {
        public Graphics Underlying;

        public enum DrawCornerFlags
        {
            TopLeft = 1 << 0, // 0x1
            TopRight = 1 << 1, // 0x2
            BotLeft = 1 << 2, // 0x4
            BotRight = 1 << 3, // 0x8
            Top = TopLeft | TopRight,   // 0x3
            Bot = BotLeft | BotRight,   // 0xC
            Left = TopLeft | BotLeft,    // 0x5
            Right = TopRight | BotRight,  // 0xA
            All = 0xF     // In your function calls you may use ~0 (= all bits sets) instead of ImDrawCornerFlags_All, as a convenience
        };

        public void AddLine(PointF a, PointF b, PackedColor col, float thickness = 1.0f) { }
        public void AddRect(PointF a, PointF b, PackedColor col, float rounding = 0.0f, int rounding_corners_flags = (int)DrawCornerFlags.All, float thickness = 1.0f)
        {
            // a: upper-left, b: lower-right, rounding_corners_flags: 4-bits corresponding to which corner to round

            if ((col & 0xff000000) == 0)
                return;
            // PathRect(Point.Add(a, new SizeF(0.5f, 0.5f)), Point.Add(b, new SizeF(-0.50f, -0.50f)), rounding, rounding_corners_flags);
            PathRect(a, b, rounding, rounding_corners_flags);
            PathStroke(col, true, thickness);
        }
        public void AddRectFilled(PointF a, PointF b, PackedColor col, float rounding = 0.0f, int rounding_corners_flags = (int)DrawCornerFlags.All)
        {
            // a: upper-left, b: lower-right
            if ((col & 0xff000000) == 0)
                return;
            if (rounding > 0.0f)
            {
                PathRect(a, b, rounding, rounding_corners_flags);
                PathFillConvex(col);
            }
            else
            {
                // PrimReserve(6, 4);
                // PrimRect(a, b, col);
                Underlying.FillRectangle(
                    new SolidBrush(Color.FromArgb((int)col)),
                    new RectangleF(a.X, a.Y, b.X - a.X, b.Y - a.Y));
            }
        }

        public void AddRectFilledMultiColor(PointF a, PointF b, PackedColor col_upr_left, PackedColor col_upr_right, PackedColor col_bot_right, PackedColor col_bot_left) { }
        public void AddQuad(PointF a, PointF b, PointF c, PointF d, PackedColor col, float thickness = 1.0f) { }
        public void AddQuadFilled(PointF a, PointF b, PointF c, PointF d, PackedColor col) { }
        public void AddTriangle(PointF a, PointF b, PointF c, PackedColor col, float thickness = 1.0f) { }
        public void AddTriangleFilled(PointF a, PointF b, PointF c, PackedColor col)
        {
            var points = new PointF[] { a, b, c };
            Underlying.FillPolygon(
                new SolidBrush(Color.FromArgb((int)col)),
                points);
        }
        public void AddCircle(PointF centre, float radius, PackedColor col, int num_segments = 12, float thickness = 1.0f) { }
        public void AddCircleFilled(PointF centre, float radius, PackedColor col, int num_segments = 12) { }
        // public void AddText(Point pos, PackedColor col, string text) { }
        // public void AddText(const ImFont* font, float font_size, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end = NULL, float wrap_width = 0.0f, const ImVec4* cpu_fine_clip_rect = NULL);
        // public void AddImage(ImTextureID user_texture_id, const ImVec2& a, const ImVec2& b, const ImVec2& uv_a = ImVec2(0,0), const ImVec2& uv_b = ImVec2(1,1), ImU32 col = 0xFFFFFFFF);
        // public void AddImageQuad(ImTextureID user_texture_id, const ImVec2& a, const ImVec2& b, const ImVec2& c, const ImVec2& d, const ImVec2& uv_a = ImVec2(0,0), const ImVec2& uv_b = ImVec2(1,0), const ImVec2& uv_c = ImVec2(1,1), const ImVec2& uv_d = ImVec2(0,1), ImU32 col = 0xFFFFFFFF);
        // public void AddImageRounded(ImTextureID user_texture_id, const ImVec2& a, const ImVec2& b, const ImVec2& uv_a, const ImVec2& uv_b, ImU32 col, float rounding, int rounding_corners = ImDrawCornerFlags_All);
        public void AddPolyline(IEnumerable<PointF> points, PackedColor col, bool closed, float thickness)
        {
            using (var pen = new Pen(Color.FromArgb((int)col), thickness))
            {
                if (closed)
                {
                    Underlying.DrawPolygon(pen, points.ToArray());
                }
                else
                {
                    Underlying.DrawLines(pen, points.ToArray());
                }
            }
        }
        public void AddConvexPolyFilled(IEnumerable<PointF> points, PackedColor col)
        {
            using (var path = new System.Drawing.Drawing2D.GraphicsPath())
            {
                path.AddPolygon(points.ToArray());
                Underlying.FillPath(
                    new SolidBrush(Color.FromArgb((int)col)),
                    path);
            }
        }
        // public void AddBezierCurve(const ImVec2& pos0, const ImVec2& cp0, const ImVec2& cp1, const ImVec2& pos1, ImU32 col, float thickness, int num_segments = 0);

#if CONSTRUCT_PATH
        public void PathClear() { _Path.Dispose(); _Path = new System.Drawing.Drawing2D.GraphicsPath(); _HasLastPathPoint = false; }
        public void PathLineTo(Point pos)
        {
            if (_HasLastPathPoint)
            {
                _Path.AddLine(_LastPathPoint, pos);
            }
            else
            {
                _FirstPathPoint = pos;
            }
            _LastPathPoint = pos;
            _HasLastPathPoint = true;
        }
#else
        public void PathClear() { _Path.Clear(); }
        public void PathLineTo(PointF pos) { _Path.Add(pos); }
#endif
        public void PathFillConvex(uint col)
        {
#if CONSTRUCT_PATH
            Underlying.FillPath(
                new SolidBrush(Color.FromArgb((int)col)),
                    _Path);
#else
            AddConvexPolyFilled(_Path, col);
#endif
            PathClear();
        }
        public void PathStroke(uint col, bool closed, float thickness = 1.0f)
        {
#if CONSTRUCT_PATH
            if (closed && _HasLastPathPoint)
                PathLineTo(_FirstPathPoint);
            Underlying.DrawPath(
                new Pen(Color.FromArgb((int)col)),
                _Path);
#else
            AddPolyline(_Path, col, closed, thickness);
#endif
            PathClear();
        }
        public void PathArcTo(PointF centre, float radius, float a_min, float a_max, int num_segments = 10)
        { }
        public void PathArcToFast(PointF centre, float radius, int a_min_of_12, int a_max_of_12)
        {
            if (radius == 0.0f || a_min_of_12 > a_max_of_12)
            {
                PathLineTo(centre);
                return;
            }
            for (int a = a_min_of_12; a <= a_max_of_12; a++)
            {
                PointF c = CircleVtx12[a % CircleVtx12.Length];
                PathLineTo(new PointF(centre.X + c.X * radius, centre.Y + c.Y * radius));
            }
        }
        public void PathBezierCurveTo(PointF p1, PointF p2, PointF p3, int num_segments = 0)
        { }
        public void PathRect(PointF a, PointF b, float rounding = 0.0f, int rounding_corners = (int)DrawCornerFlags.All)
        {
            rounding = Math.Min(rounding, Math.Abs(b.X - a.X) * (((rounding_corners & (int)DrawCornerFlags.Top) == (int)DrawCornerFlags.Top) || ((rounding_corners & (int)DrawCornerFlags.Bot) == (int)DrawCornerFlags.Bot) ? 0.5f : 1.0f) - 1.0f);
            rounding = Math.Min(rounding, Math.Abs(b.Y - a.Y) * (((rounding_corners & (int)DrawCornerFlags.Left) == (int)DrawCornerFlags.Left) || ((rounding_corners & (int)DrawCornerFlags.Right) == (int)DrawCornerFlags.Right) ? 0.5f : 1.0f) - 1.0f);

            if (rounding <= 0.0f || rounding_corners == 0)
            {
                PathLineTo(a);
                PathLineTo(new PointF(b.X, a.Y));
                PathLineTo(b);
                PathLineTo(new PointF(a.X, b.Y));
            }
            else
            {
                float rounding_tl = ((rounding_corners & (int)DrawCornerFlags.TopLeft) != 0) ? rounding : 0.0f;
                float rounding_tr = ((rounding_corners & (int)DrawCornerFlags.TopRight) != 0) ? rounding : 0.0f;
                float rounding_br = ((rounding_corners & (int)DrawCornerFlags.BotRight) != 0) ? rounding : 0.0f;
                float rounding_bl = ((rounding_corners & (int)DrawCornerFlags.BotLeft) != 0) ? rounding : 0.0f;
                PathArcToFast(new PointF(a.X + rounding_tl, a.Y + rounding_tl), rounding_tl, 6, 9);
                PathArcToFast(new PointF(b.X - rounding_tr, a.Y + rounding_tr), rounding_tr, 9, 12);
                PathArcToFast(new PointF(b.X - rounding_br, b.Y - rounding_br), rounding_br, 0, 3);
                PathArcToFast(new PointF(a.X + rounding_bl, b.Y - rounding_bl), rounding_bl, 3, 6);
            }
        }

#if CONSTRUCT_PATH
        private System.Drawing.Drawing2D.GraphicsPath _Path = new System.Drawing.Drawing2D.GraphicsPath();
        private Point _LastPathPoint, _FirstPathPoint;
        private bool _HasLastPathPoint = false;
#else
        private List<PointF> _Path = new List<PointF>();
#endif
        private PointF[] CircleVtx12;

        public ImbuedGraphics()
        {
            CircleVtx12 = new PointF[12];
            for (int i = 0; i < CircleVtx12.Length; i++)
            {
                double a = ((double)i * 2.0 * Math.PI) / (double)CircleVtx12.Length;
                CircleVtx12[i] = new PointF((float)Math.Cos(a), (float)Math.Sin(a));
            }
        }
    }
}
