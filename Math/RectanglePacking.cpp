// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RectanglePacking.h"

namespace XLEMath
{
    static unsigned Width(std::pair<UInt2, UInt2> rect)   { return rect.second[0] - rect.first[0]; }
    static unsigned Height(std::pair<UInt2, UInt2> rect)  { return rect.second[1] - rect.first[1]; }

    const auto s_emptyRect = std::make_pair(UInt2(0,0), UInt2(0,0));

    auto RectanglePacker::Add(UInt2 dims) -> Rectangle
    {
        auto i = SearchNodes(0, dims);
        if (i != s_invalidNode) {
            auto& n = _nodes[i];
            assert(!n.IsAllocated());
            n._children = _nodes.size();

            auto result = std::make_pair(n._space.first, UInt2(n._space.first + dims));

                // note that the order we push the children in here will determine
                // how they are searched. We will push in the "right" space first,
                // and then the "down" space.
            auto rightSpace = std::make_pair(
                UInt2(n._space.first[0] + dims[0], n._space.first[1]),
                UInt2(n._space.second[0], n._space.first[1] + dims[1]));
            auto downSpace = std::make_pair(
                UInt2(n._space.first[0], n._space.first[1] + dims[1]),
                UInt2(n._space.second[0], n._space.second[1]));
            _nodes.push_back(Node { rightSpace, s_invalidNode });
            _nodes.push_back(Node { downSpace, s_invalidNode });

            return result;
        }

        return s_emptyRect;
    }

    size_t RectanglePacker::SearchNodes(size_t startingNode, UInt2 dims) const
    {
        assert(startingNode < _nodes.size());
        const auto& n = _nodes[startingNode];
        if (n.IsAllocated()) {
            auto result = SearchNodes(n._children, dims);
            if (result == s_invalidNode)
                result = SearchNodes(n._children+1, dims);
            return result;
        } else if (dims[0] <= Width(n._space) && dims[1] <= Height(n._space)) {
            return startingNode;
        } else 
            return s_invalidNode;
    }

    static std::pair<UInt2, UInt2> LargestArea(
        std::pair<UInt2, UInt2>& lhs, std::pair<UInt2, UInt2>& rhs)
    {
        auto a0 = (lhs.second[0] - lhs.first[0]) * (lhs.second[1] - lhs.first[1]);
        auto a1 = (rhs.second[0] - rhs.first[0]) * (rhs.second[1] - rhs.first[1]);
        if (a0 >= a1) return lhs;
        return rhs;
    }

    static std::pair<UInt2, UInt2> LargestSide(
        std::pair<UInt2, UInt2>& lhs, std::pair<UInt2, UInt2>& rhs)
    {
        auto a0 = std::max(lhs.second[0] - lhs.first[0], lhs.second[1] - lhs.first[1]);
        auto a1 = std::max(rhs.second[0] - rhs.first[0], rhs.second[1] - rhs.first[1]);
        if (a0 >= a1) return lhs;
        return rhs;
    }

    auto RectanglePacker::SearchLargestFree(size_t startingNode) const 
        -> std::pair<Rectangle, Rectangle>
    {
        assert(startingNode < _nodes.size());
        const auto& n = _nodes[startingNode];
        if (!n.IsAllocated()) {
            auto area = (n._space.second[0] - n._space.first[0]) * (n._space.second[1] - n._space.first[1]);
            if (area > 0)   return std::make_pair(n._space, n._space);
            else            return std::make_pair(s_emptyRect, s_emptyRect);
        }

        auto c0 = SearchLargestFree(n._children);
        auto c1 = SearchLargestFree(n._children+1);
        return std::make_pair(
            LargestArea(c0.first, c1.first),
            LargestSide(c0.second, c1.second));
    }

    std::pair<UInt2, UInt2> RectanglePacker::LargestFreeBlock() const
    {
        if (_nodes.empty()) { return std::make_pair(UInt2(0,0), UInt2(0,0)); }

        auto search = SearchLargestFree(0);
        return std::make_pair(
            search.first.second - search.first.first,
            search.second.second - search.second.first);
    }

    RectanglePacker::RectanglePacker(const UInt2 dimensions)
    {
        _nodes.reserve(128);
        _nodes.push_back(
            Node { std::make_pair(UInt2(0,0), dimensions), s_invalidNode });
        _totalSize = dimensions;
    }

    RectanglePacker::~RectanglePacker() {}

    RectanglePacker::RectanglePacker(RectanglePacker&& moveFrom)
    : _nodes(std::move(moveFrom._nodes))
    , _totalSize(moveFrom._totalSize)
    {}
     
    RectanglePacker& RectanglePacker::operator=(RectanglePacker&& moveFrom)
    {
        _nodes = std::move(moveFrom._nodes);
        _totalSize = moveFrom._totalSize;
        return *this;
    }

    RectanglePacker::RectanglePacker(const RectanglePacker& cloneFrom)
    : _nodes(cloneFrom._nodes)
    , _totalSize(cloneFrom._totalSize)
    {}

    RectanglePacker& RectanglePacker::operator=(const RectanglePacker& cloneFrom)
    {
        _nodes = cloneFrom._nodes;
        _totalSize = cloneFrom._totalSize;
        return *this;
    }

    RectanglePacker::RectanglePacker() {}

}

