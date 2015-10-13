// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Vector.h"
#include <vector>

namespace XLEMath
{
    /// <summary>Sequentially pack rectangles within a fixed area</summary>
    /// When building a texture atlas, we often want to find efficient algorithms
    /// for packing many differently sized textures within one larger texture.
    /// 
    /// We want to find the arrangement of rectangles that leaves the least
    /// unused space left over. We can also (perhaps) rotate the rectangle 90
    /// degrees (if it produces a better packed result).
    ///
    /// There are two variations to this:
    ///     1) when we know all of the rectangles to pack before hand
    ///     2) when we will be sequentially adding rectangles in an 
    ///         undeterminate manner
    ///
    /// If we know all of the rectangles before hand, there are some very accurate
    /// approximations
    ///         see: https://www.jair.org/media/3735/live-3735-6794-jair.pdf
    ///             http://clb.demon.fi/files/RectangleBinPack.pdf
    /// However, many of these methods aren't perfectly suitable for the texture
    /// altas problem we'd like to solve.
    ///
    /// However, we will use a simple & practical method. This method uses a binary
    /// tree so that when every rectangle is placed, we partition the remaining space
    /// into "right" and "down" space.
    ///
    /// See http://codeincomplete.com/posts/2011/5/7/bin_packing/ for a good description
    /// of this method.
    ///
    /// The results are reasonably good, but not perfect. When we know all rectangles
    /// before-hand, we want can get good packing by sorting them by max(width, height)
    ///
    /// There are many other references available; eg:
    ///     https://code.google.com/p/texture-atlas/source/browse/trunk/TexturePacker.cpp
    ///     http://pollinimini.net/blog/rectangle-packing-2d-packing/
    ///     http://www.blackpawn.com/texts/lightmaps/default.html
    ///     http://codesuppository.blogspot.kr/2009/04/texture-packing-code-snippet-to-compute.html
    ///     http://www.eng.biu.ac.il/~rawitzd/Papers/srp.pdf (scheduling WiMax packets)
    class RectanglePacker
    {
    public:
        using Rectangle = std::pair<UInt2, UInt2>;

        Rectangle Add(UInt2 dims);

        RectanglePacker(const UInt2 dimensions);
        RectanglePacker(RectanglePacker&& moveFrom);
        RectanglePacker& operator=(RectanglePacker&& moveFrom);
        ~RectanglePacker();

        RectanglePacker(const RectanglePacker&) = delete;
        RectanglePacker& operator=(const RectanglePacker&) = delete;

    private:
        static const size_t s_invalidNode = ~size_t(0x0);
        
        class Node
        {
        public:
            Rectangle   _space;
            size_t      _children;

            bool IsAllocated() const { return _children != s_invalidNode; }
        };

        std::vector<Node> _nodes;
        

        size_t SearchNodes(size_t startingNode, UInt2 dims) const;
    };
}

