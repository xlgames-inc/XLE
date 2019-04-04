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
    }
}
