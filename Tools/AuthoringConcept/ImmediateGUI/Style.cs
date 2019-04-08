using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace AuthoringConcept.ImmediateGUI
{
    using PackedColor = UInt32;

    struct Style
    {
        public float FrameBorderSize;
        public float FrameRounding;
        public float ThumbMinSize;
        public float ThumbRounding;

        public static PackedColor ControlDark = 0xff1f1f1fu;
        public static PackedColor ControlDarkText = 0xffdfdfdfu;
        public static PackedColor ControlLight = 0xffdfdfdfu;
        public static PackedColor ControlLightText = 0xff3f3f3fu;
        public static PackedColor SliderFilled = 0xff9f9f9f;

        public static PackedColor GetColorU32(SystemColor colorName)
        {
            if (PackedColorTable == null)
            {
                SetupColorTables();
            }
            return PackedColorTable[colorName];
        }

        static private Dictionary<SystemColor, Sce.Atf.VectorMath.Vec4F> ColorTable = null;
        static private Dictionary<SystemColor, PackedColor> PackedColorTable = null;

        static private void SetupColorTables()
        {
            ColorTable = new Dictionary<SystemColor, Sce.Atf.VectorMath.Vec4F>();
            ColorTable[SystemColor.ImGuiCol_Text] = new Sce.Atf.VectorMath.Vec4F(1.00f, 1.00f, 1.00f, 1.00f);
            ColorTable[SystemColor.ImGuiCol_TextDisabled] = new Sce.Atf.VectorMath.Vec4F(0.50f, 0.50f, 0.50f, 1.00f);
            ColorTable[SystemColor.ImGuiCol_WindowBg] = new Sce.Atf.VectorMath.Vec4F(0.06f, 0.06f, 0.06f, 0.94f);
            ColorTable[SystemColor.ImGuiCol_ChildBg] = new Sce.Atf.VectorMath.Vec4F(0.00f, 0.00f, 0.00f, 0.00f);
            ColorTable[SystemColor.ImGuiCol_PopupBg] = new Sce.Atf.VectorMath.Vec4F(0.08f, 0.08f, 0.08f, 0.94f);
            ColorTable[SystemColor.ImGuiCol_Border] = new Sce.Atf.VectorMath.Vec4F(0.43f, 0.43f, 0.50f, 0.50f);
            ColorTable[SystemColor.ImGuiCol_BorderShadow] = new Sce.Atf.VectorMath.Vec4F(0.00f, 0.00f, 0.00f, 0.00f);
            ColorTable[SystemColor.ImGuiCol_FrameBg] = new Sce.Atf.VectorMath.Vec4F(0.16f, 0.29f, 0.48f, 0.54f);
            ColorTable[SystemColor.ImGuiCol_FrameBgHovered] = new Sce.Atf.VectorMath.Vec4F(0.26f, 0.59f, 0.98f, 0.40f);
            ColorTable[SystemColor.ImGuiCol_FrameBgActive] = new Sce.Atf.VectorMath.Vec4F(0.26f, 0.59f, 0.98f, 0.67f);
            ColorTable[SystemColor.ImGuiCol_TitleBg] = new Sce.Atf.VectorMath.Vec4F(0.04f, 0.04f, 0.04f, 1.00f);
            ColorTable[SystemColor.ImGuiCol_TitleBgActive] = new Sce.Atf.VectorMath.Vec4F(0.16f, 0.29f, 0.48f, 1.00f);
            ColorTable[SystemColor.ImGuiCol_TitleBgCollapsed] = new Sce.Atf.VectorMath.Vec4F(0.00f, 0.00f, 0.00f, 0.51f);
            ColorTable[SystemColor.ImGuiCol_MenuBarBg] = new Sce.Atf.VectorMath.Vec4F(0.14f, 0.14f, 0.14f, 1.00f);
            ColorTable[SystemColor.ImGuiCol_ScrollbarBg] = new Sce.Atf.VectorMath.Vec4F(0.02f, 0.02f, 0.02f, 0.53f);
            ColorTable[SystemColor.ImGuiCol_ScrollbarGrab] = new Sce.Atf.VectorMath.Vec4F(0.31f, 0.31f, 0.31f, 1.00f);
            ColorTable[SystemColor.ImGuiCol_ScrollbarGrabHovered] = new Sce.Atf.VectorMath.Vec4F(0.41f, 0.41f, 0.41f, 1.00f);
            ColorTable[SystemColor.ImGuiCol_ScrollbarGrabActive] = new Sce.Atf.VectorMath.Vec4F(0.51f, 0.51f, 0.51f, 1.00f);
            ColorTable[SystemColor.ImGuiCol_CheckMark] = new Sce.Atf.VectorMath.Vec4F(0.26f, 0.59f, 0.98f, 1.00f);
            ColorTable[SystemColor.ImGuiCol_SliderGrab] = new Sce.Atf.VectorMath.Vec4F(0.24f, 0.52f, 0.88f, 1.00f);
            ColorTable[SystemColor.ImGuiCol_SliderGrabActive] = new Sce.Atf.VectorMath.Vec4F(0.26f, 0.59f, 0.98f, 1.00f);
            ColorTable[SystemColor.ImGuiCol_Button] = new Sce.Atf.VectorMath.Vec4F(0.26f, 0.59f, 0.98f, 0.40f);
            ColorTable[SystemColor.ImGuiCol_ButtonHovered] = new Sce.Atf.VectorMath.Vec4F(0.26f, 0.59f, 0.98f, 1.00f);
            ColorTable[SystemColor.ImGuiCol_ButtonActive] = new Sce.Atf.VectorMath.Vec4F(0.06f, 0.53f, 0.98f, 1.00f);
            ColorTable[SystemColor.ImGuiCol_Header] = new Sce.Atf.VectorMath.Vec4F(0.26f, 0.59f, 0.98f, 0.31f);
            ColorTable[SystemColor.ImGuiCol_HeaderHovered] = new Sce.Atf.VectorMath.Vec4F(0.26f, 0.59f, 0.98f, 0.80f);
            ColorTable[SystemColor.ImGuiCol_HeaderActive] = new Sce.Atf.VectorMath.Vec4F(0.26f, 0.59f, 0.98f, 1.00f);
            ColorTable[SystemColor.ImGuiCol_Separator] = ColorTable[SystemColor.ImGuiCol_Border];
            ColorTable[SystemColor.ImGuiCol_SeparatorHovered] = new Sce.Atf.VectorMath.Vec4F(0.10f, 0.40f, 0.75f, 0.78f);
            ColorTable[SystemColor.ImGuiCol_SeparatorActive] = new Sce.Atf.VectorMath.Vec4F(0.10f, 0.40f, 0.75f, 1.00f);
            ColorTable[SystemColor.ImGuiCol_ResizeGrip] = new Sce.Atf.VectorMath.Vec4F(0.26f, 0.59f, 0.98f, 0.25f);
            ColorTable[SystemColor.ImGuiCol_ResizeGripHovered] = new Sce.Atf.VectorMath.Vec4F(0.26f, 0.59f, 0.98f, 0.67f);
            ColorTable[SystemColor.ImGuiCol_ResizeGripActive] = new Sce.Atf.VectorMath.Vec4F(0.26f, 0.59f, 0.98f, 0.95f);
            // ColorTable[ImGuiCol_.ImGuiCol_Tab] = ImLerp(ColorTable[ImGuiCol_.ImGuiCol_Header], ColorTable[ImGuiCol_.ImGuiCol_TitleBgActive], 0.80f);
            // ColorTable[ImGuiCol_.ImGuiCol_TabHovered] = ColorTable[ImGuiCol_.ImGuiCol_HeaderHovered];
            // ColorTable[ImGuiCol_.ImGuiCol_TabActive] = ImLerp(ColorTable[ImGuiCol_.ImGuiCol_HeaderActive], ColorTable[ImGuiCol_.ImGuiCol_TitleBgActive], 0.60f);
            // ColorTable[ImGuiCol_.ImGuiCol_TabUnfocused] = ImLerp(ColorTable[ImGuiCol_.ImGuiCol_Tab], ColorTable[ImGuiCol_.ImGuiCol_TitleBg], 0.80f);
            // ColorTable[ImGuiCol_.ImGuiCol_TabUnfocusedActive] = ImLerp(ColorTable[ImGuiCol_.ImGuiCol_TabActive], ColorTable[ImGuiCol_.ImGuiCol_TitleBg], 0.40f);
            ColorTable[SystemColor.ImGuiCol_PlotLines] = new Sce.Atf.VectorMath.Vec4F(0.61f, 0.61f, 0.61f, 1.00f);
            ColorTable[SystemColor.ImGuiCol_PlotLinesHovered] = new Sce.Atf.VectorMath.Vec4F(1.00f, 0.43f, 0.35f, 1.00f);
            ColorTable[SystemColor.ImGuiCol_PlotHistogram] = new Sce.Atf.VectorMath.Vec4F(0.90f, 0.70f, 0.00f, 1.00f);
            ColorTable[SystemColor.ImGuiCol_PlotHistogramHovered] = new Sce.Atf.VectorMath.Vec4F(1.00f, 0.60f, 0.00f, 1.00f);
            ColorTable[SystemColor.ImGuiCol_TextSelectedBg] = new Sce.Atf.VectorMath.Vec4F(0.26f, 0.59f, 0.98f, 0.35f);
            ColorTable[SystemColor.ImGuiCol_DragDropTarget] = new Sce.Atf.VectorMath.Vec4F(1.00f, 1.00f, 0.00f, 0.90f);
            ColorTable[SystemColor.ImGuiCol_NavHighlight] = new Sce.Atf.VectorMath.Vec4F(0.26f, 0.59f, 0.98f, 1.00f);
            ColorTable[SystemColor.ImGuiCol_NavWindowingHighlight] = new Sce.Atf.VectorMath.Vec4F(1.00f, 1.00f, 1.00f, 0.70f);
            ColorTable[SystemColor.ImGuiCol_NavWindowingDimBg] = new Sce.Atf.VectorMath.Vec4F(0.80f, 0.80f, 0.80f, 0.20f);
            ColorTable[SystemColor.ImGuiCol_ModalWindowDimBg] = new Sce.Atf.VectorMath.Vec4F(0.80f, 0.80f, 0.80f, 0.35f);

            PackedColorTable = new Dictionary<SystemColor, PackedColor>();
            foreach (var col in ColorTable)
            {
                PackedColor packedCol =
                      //(((uint)(255.0f * col.Value.W) & 0xff) << 24)
                      0xff000000
                    | (((uint)(255.0f * col.Value.X) & 0xff) << 16)
                    | (((uint)(255.0f * col.Value.Y) & 0xff) << 8)
                    | (((uint)(255.0f * col.Value.Z) & 0xff) << 0);
                PackedColorTable.Add(col.Key, packedCol);
            }

            PackedColorTable[SystemColor.ImGuiCol_BorderShadow] = 0x3f000000;
        }
    };

    public enum SystemColor
    {
        ImGuiCol_Text,
        ImGuiCol_TextDisabled,
        ImGuiCol_WindowBg,              // Background of normal windows
        ImGuiCol_ChildBg,               // Background of child windows
        ImGuiCol_PopupBg,               // Background of popups, menus, tooltips windows
        ImGuiCol_Border,
        ImGuiCol_BorderShadow,
        ImGuiCol_FrameBg,               // Background of checkbox, radio button, plot, slider, text input
        ImGuiCol_FrameBgHovered,
        ImGuiCol_FrameBgActive,
        ImGuiCol_TitleBg,
        ImGuiCol_TitleBgActive,
        ImGuiCol_TitleBgCollapsed,
        ImGuiCol_MenuBarBg,
        ImGuiCol_ScrollbarBg,
        ImGuiCol_ScrollbarGrab,
        ImGuiCol_ScrollbarGrabHovered,
        ImGuiCol_ScrollbarGrabActive,
        ImGuiCol_CheckMark,
        ImGuiCol_SliderGrab,
        ImGuiCol_SliderGrabActive,
        ImGuiCol_Button,
        ImGuiCol_ButtonHovered,
        ImGuiCol_ButtonActive,
        ImGuiCol_Header,
        ImGuiCol_HeaderHovered,
        ImGuiCol_HeaderActive,
        ImGuiCol_Separator,
        ImGuiCol_SeparatorHovered,
        ImGuiCol_SeparatorActive,
        ImGuiCol_ResizeGrip,
        ImGuiCol_ResizeGripHovered,
        ImGuiCol_ResizeGripActive,
        ImGuiCol_Tab,
        ImGuiCol_TabHovered,
        ImGuiCol_TabActive,
        ImGuiCol_TabUnfocused,
        ImGuiCol_TabUnfocusedActive,
        ImGuiCol_PlotLines,
        ImGuiCol_PlotLinesHovered,
        ImGuiCol_PlotHistogram,
        ImGuiCol_PlotHistogramHovered,
        ImGuiCol_TextSelectedBg,
        ImGuiCol_DragDropTarget,
        ImGuiCol_NavHighlight,          // Gamepad/keyboard: current highlighted item
        ImGuiCol_NavWindowingHighlight, // Highlight window when using CTRL+TAB
        ImGuiCol_NavWindowingDimBg,     // Darken/colorize entire screen behind the CTRL+TAB window list, when active
        ImGuiCol_ModalWindowDimBg,      // Darken/colorize entire screen behind a modal window, when one is active
        ImGuiCol_COUNT
    };
}
