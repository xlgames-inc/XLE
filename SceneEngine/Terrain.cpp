// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Terrain.h"
#include "TerrainScaffold.h"
#include "TerrainUberSurface.h"

#include "../Utility/Streams/FileUtils.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/MemoryUtils.h"

namespace SceneEngine
{
    TerrainCell::TerrainCell() : _encodedGradientFlags(false) {}
    TerrainCell::~TerrainCell() {}

    unsigned CompressedHeightMask(bool encodedGradientFlags)
    {
        return encodedGradientFlags ? 0x3fff : 0xffff;
    }

    TerrainCell::NodeField::NodeField(unsigned widthInNodes, unsigned heightInNodes, unsigned nodeBegin, unsigned nodeEnd)
    : _widthInNodes(widthInNodes), _heightInNodes(heightInNodes)
    , _nodeBegin(nodeBegin), _nodeEnd(nodeEnd)
    {
    }

    TerrainCell::Node::Node(const Float4x4& localToCell, size_t heightMapFileOffset, size_t heightMapFileSize, unsigned widthInElements)
    : _localToCell(localToCell), _heightMapFileOffset(heightMapFileOffset), _heightMapFileSize(heightMapFileSize)
    , _widthInElements(widthInElements)
    {}

    //////////////////////////////////////////////////////////////////////////////////////////

    TerrainCellTexture::TerrainCellTexture() 
    {
        _nodeTextureByteCount = 0;
        _fieldCount = 0;
    }

    TerrainCellTexture::~TerrainCellTexture() {}


    ITerrainFormat::~ITerrainFormat() {}
}

