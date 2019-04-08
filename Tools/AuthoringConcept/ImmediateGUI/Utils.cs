using System;
using System.Drawing;

namespace AuthoringConcept.ImmediateGUI
{
    public static class Utils
    {
        internal static T Clamp<T>(this T val, T min, T max) where T : IComparable<T>
        {
            if (val.CompareTo(min) < 0) return min;
            else if (val.CompareTo(max) > 0) return max;
            else return val;
        }

        internal static PointF GetAbsoluteTopLeft(Facebook.Yoga.YogaNode node)
        {
            var result = new PointF(node.LayoutX, node.LayoutY);
            while (node.Parent != null)
            {
                node = node.Parent;
                result = PointF.Add(result, new SizeF(node.LayoutX, node.LayoutY));
            }
            var root = node as ImmediateGUI.ImbuedRoot;
            if (root != null)
            {
                if (root.GetAssignedX().HasValue)
                    result.X += root.GetAssignedX().Value;
                if (root.GetAssignedY().HasValue)
                    result.Y += root.GetAssignedY().Value;
            }
            return result;
        }

        public delegate T GetDelegate<T>();
        public delegate void SetDelegate<T>(T newState);

        internal static System.Windows.Forms.Control HoveringFloatEditBox(
            SetDelegate<float> setter, GetDelegate<float> getter,
            Rectangle bounds, System.Windows.Forms.Control parentControl)
        {
            var textBox = new System.Windows.Forms.TextBox();
            textBox.Text = getter().ToString();
            textBox.Bounds = bounds;

            textBox.TextChanged += (object sender, EventArgs e) =>
            {
                var tb = (System.Windows.Forms.TextBox)sender;
                if (Single.TryParse(tb.Text, out float value))
                {
                    setter(value);
                }
                else
                {
                    var r = new System.Text.RegularExpressions.Regex(@"^(([-+]?\d+\.\d*)|([-+]?\d*)|(\.\d*))$"); // This is the main part, can be altered to match any desired form or limitations
                    if (r.IsMatch(tb.Text))
                    {
                        // Matching this regex is a little more leniant than Single.TryParse()
                        // We can match against partial numbers (eg, just "-" or ".", or even an empty string). They evaluate
                        // to zero; but we can't reset the text contents or we'll loose our partial number
                        setter(0.0f);
                    }
                    else
                    {
                        // non-numeric input -- reset to previous known numeric input
                        tb.Text = getter().ToString();
                    }
                }
            };
            textBox.Capture = true;
            textBox.MouseCaptureChanged += (object sender, EventArgs e) =>
            {
                var tb = (System.Windows.Forms.TextBox)sender;
                tb.Parent = null;
                tb.Dispose();
            };
            textBox.Click += (object sender, EventArgs e) =>
            {
                var tb = (System.Windows.Forms.TextBox)sender;
                tb.Parent = null;
                tb.Dispose();
            };
            textBox.LostFocus += (object sender, EventArgs e) =>
            {
                var tb = (System.Windows.Forms.TextBox)sender;
                tb.Parent = null;
                tb.Dispose();
            };
            textBox.KeyPress += (object sender, System.Windows.Forms.KeyPressEventArgs e) =>
            {
                if (e.KeyChar == (char)System.Windows.Forms.Keys.Enter)
                {
                    e.Handled = true;
                    var tb = (System.Windows.Forms.TextBox)sender;
                    tb.Parent = null;
                    tb.Dispose();
                }
            };

            textBox.Parent = parentControl;
            textBox.Visible = true;
            textBox.Focus();

            return textBox;
        }
    }
}
