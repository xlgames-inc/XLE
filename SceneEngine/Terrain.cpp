// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Terrain.h"
#include "TerrainInternal.h"
#include "TerrainUberSurface.h"

#include "../Utility/Streams/FileUtils.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/MemoryUtils.h"


namespace SceneEngine
{

    TerrainCell::TerrainCell() {}
    TerrainCell::~TerrainCell() {}

    std::vector<uint8> TerrainCell::BuildHeightMapData(unsigned nodeIndex, BasicFile& sourceFile, BasicFile& secondaryCache)
    {
            // Build the height map data for a given node, from higher resolution nodes
            // We need to figure out the proper coordinates for this node, and then
            // find the nodes in the next node field down that have data
        auto containingField = std::find_if(_nodeFields.begin(), _nodeFields.end(),
            [=](const NodeField& f) { return nodeIndex >= f._nodeBegin && nodeIndex < f._nodeEnd; });
        if (containingField == _nodeFields.end())
            return std::vector<uint8>();
        
        unsigned fieldIndex = nodeIndex - containingField->_nodeBegin;
        unsigned fieldx = fieldIndex % containingField->_widthInNodes;
        unsigned fieldy = fieldIndex / containingField->_widthInNodes;
        if (fieldy > containingField->_heightInNodes)
            return std::vector<uint8>();    // invalid coordinates (?)

        auto sourceField = containingField+1;
        if (sourceField == _nodeFields.end())
            return std::vector<uint8>();    // if there are no higher resolution nodes, we can't build the data

        // we can now find 4 source nodes, from which we'll get the higher resolution data
        unsigned sourceNodes[] = 
        {
                // todo -- check order here after adjusting flipping method
            sourceField->_nodeBegin + (fieldx*2+0) + (fieldy*2+0) * sourceField->_widthInNodes,
            sourceField->_nodeBegin + (fieldx*2+1) + (fieldy*2+0) * sourceField->_widthInNodes,
            sourceField->_nodeBegin + (fieldx*2+0) + (fieldy*2+1) * sourceField->_widthInNodes,
            sourceField->_nodeBegin + (fieldx*2+1) + (fieldy*2+1) * sourceField->_widthInNodes
        };
        for (unsigned c=0; c<dimof(sourceNodes); ++c)
            if (sourceNodes[c] >= _nodes.size()
                || (!_nodes[sourceNodes[c]]->_heightMapFileSize && !_nodes[sourceNodes[c]]->_secondaryCacheSize))
                return std::vector<uint8>();    // no data in the source nodes

        auto secondaryCacheStart = secondaryCache.TellP();

            // assume that all nodes have the same number of elements in that node
        const unsigned nodeDim = _nodes[0]->_widthInElements;

        // each node has a different coordinate space for the z element. so we have to do from our 16 bit coordinates
        // into float point
        auto sourceHeights = std::make_unique<float[]>((nodeDim*2) * (nodeDim*2));
        
        const unsigned expectingSize = nodeDim*nodeDim*2;
        auto sourceData = std::make_unique<uint8[]>(expectingSize);

        for (unsigned n=0; n<dimof(sourceNodes); ++n) {
            const Node& node = *_nodes[sourceNodes[n]];
            if (node._heightMapFileSize > 0) {
                if (node._heightMapFileSize == expectingSize) {
                    sourceFile.Seek(node._heightMapFileOffset, SEEK_SET);
                    sourceFile.Read(sourceData.get(), 2, nodeDim*nodeDim);
                } else {
                        //  some nodes have holes... These aren't fully supported.
                        //  just use the lowest valid height
                    XlSetMemory(sourceData.get(), 0, expectingSize);
                }
            } else {
                assert(node._secondaryCacheSize == expectingSize);
                secondaryCache.Seek(node._secondaryCacheOffset, SEEK_SET);
                secondaryCache.Read(sourceData.get(), 2, nodeDim*nodeDim);
            }

            for (unsigned y=0; y<nodeDim; ++y)
                for (unsigned x=0; x<nodeDim; ++x) {
                    unsigned dx = (n%2) * nodeDim + x;
                    unsigned dy = (n/2) * nodeDim + y;

                    uint16 compr = ((uint16*)sourceData.get())[y*nodeDim+x];
                    sourceHeights[dy * (nodeDim*2) + dx] =
                        node._localToCell(2,2) * float(compr) + node._localToCell(2,3);
                }
        }

        // reset "secondaryCache" file pointer
        secondaryCache.Seek(long(secondaryCacheStart), SEEK_SET);

        float finalMin = FLT_MAX, finalMax = -FLT_MAX;
        auto finalHeights = std::make_unique<float[]>(nodeDim * nodeDim);
        for (unsigned y=0; y<nodeDim; ++y)
            for (unsigned x=0; x<nodeDim; ++x) {
                float h0 = sourceHeights[(y*2+0) * (nodeDim*2) + (x*2+0)];
                float h1 = sourceHeights[(y*2+0) * (nodeDim*2) + (x*2+1)];
                float h2 = sourceHeights[(y*2+1) * (nodeDim*2) + (x*2+0)];
                float h3 = sourceHeights[(y*2+1) * (nodeDim*2) + (x*2+1)];
                    // note --  this method creates seems at the edges of nodes.
                    //          to get the right result, we have to read into the neighbours
                    //          (even if neighbours are in different nodes, or different cells...)
                finalHeights[y*nodeDim+x] = (h0 + h1 + h2 + h3) / 4.f;
                finalMin = std::min(finalMin, finalHeights[y*nodeDim+x]);
                finalMax = std::max(finalMax, finalHeights[y*nodeDim+x]);
            }

        std::vector<uint8> result;
        result.resize(sizeof(uint16) * nodeDim * nodeDim, 0);
        for (unsigned y=0; y<nodeDim; ++y)
            for (unsigned x=0; x<nodeDim; ++x) {
                float a = (finalHeights[y*nodeDim + x] - finalMin) / (finalMax - finalMin);
                ((uint16*)AsPointer(result.begin()))[y*nodeDim + x] = uint16(a * float(0xffff));
            }

        _nodes[nodeIndex]->_localToCell(2,2) = (finalMax - finalMin) / float(0xffff);
        _nodes[nodeIndex]->_localToCell(2,3) = finalMin;
        return std::move(result);
    }

    TerrainCell::NodeField::NodeField(unsigned widthInNodes, unsigned heightInNodes, unsigned nodeBegin, unsigned nodeEnd)
    : _widthInNodes(widthInNodes), _heightInNodes(heightInNodes)
    , _nodeBegin(nodeBegin), _nodeEnd(nodeEnd)
    {
    }

    TerrainCell::Node::Node(const Float4x4& localToCell, size_t heightMapFileOffset, size_t heightMapFileSize, unsigned widthInElements)
    : _localToCell(localToCell), _heightMapFileOffset(heightMapFileOffset), _heightMapFileSize(heightMapFileSize)
    , _secondaryCacheOffset(0x0), _secondaryCacheSize(0x0)
    , _widthInElements(widthInElements)
    {
    }

    //////////////////////////////////////////////////////////////////////////////////////////

    TerrainCellTexture::TerrainCellTexture() 
    {
        _nodeTextureByteCount = 0;
        _fieldCount = 0;
    }

    TerrainCellTexture::~TerrainCellTexture() {}

}

