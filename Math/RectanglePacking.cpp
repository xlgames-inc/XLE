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

    auto RectanglePacker::Add(UInt2 dims) -> Rectangle
    {
        auto i = SearchNodes(0, dims);
        if (i != s_invalidNode) {
            auto& n = _nodes[i];
            assert(!n.IsAllocated());
            n._children = _nodes.size();

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

            return std::make_pair(n._space.first, n._space.first + dims);
        }

        return std::make_pair(UInt2(0,0), UInt2(0,0));
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

    RectanglePacker::RectanglePacker(const UInt2 dimensions)
    {
        _nodes.reserve(128);
        _nodes.push_back(
            Node { std::make_pair(UInt2(0,0), dimensions), s_invalidNode });
    }

    RectanglePacker::~RectanglePacker() {}

    RectanglePacker::RectanglePacker(RectanglePacker&& moveFrom)
    : _nodes(std::move(moveFrom._nodes))
    {}
     
    RectanglePacker& RectanglePacker::operator=(RectanglePacker&& moveFrom)
    {
        _nodes = std::move(moveFrom._nodes);
        return *this;
    }

    RectanglePacker::RectanglePacker(const RectanglePacker& cloneFrom)
    : _nodes(cloneFrom._nodes)
    {}

    RectanglePacker& RectanglePacker::operator=(const RectanglePacker& cloneFrom)
    {
        _nodes = cloneFrom._nodes;
        return *this;
    }

    RectanglePacker::RectanglePacker() {}

}

