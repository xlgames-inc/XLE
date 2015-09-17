using Sce.Atf;

namespace LevelEditorXLE
{
    public static class Resources
    {
        [ImageResource("TerrainManip64.png", "TerrainManip32.png", "TerrainManip16.png")]
        public static readonly string TerrainManip;

        [ImageResource("ScatterPlace64.png", "ScatterPlace32.png", "ScatterPlace16.png")]
        public static readonly string ScatterPlace;

        private const string ResourcePath = "LevelEditorXLE.Resources.";

        static Resources()
        {
            ResourceUtil.Register(typeof(Resources), ResourcePath);
        }
    }
}
