using Sce.Atf;

namespace RenderingInterop
{
    public static class Resources
    {
        [ImageResource("MoveAcrossTerrain64.png", "MoveAcrossTerrain32.png", "MoveAcrossTerrain16.png")]
        public static readonly string MoveAcrossTerrain;

        private const string ResourcePath = "RenderingInterop.Resources.";

        static Resources()
        {
            ResourceUtil.Register(typeof(Resources), ResourcePath);
        }
    }
}
