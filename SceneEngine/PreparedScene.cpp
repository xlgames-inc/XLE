// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "PreparedScene.h"

namespace SceneEngine
{
    PreparedScene::PreparedScene() {}
    PreparedScene::~PreparedScene() 
    {
        for (auto& b:_blocks)
            (*b.second._destructor)(b.second._allocation._allocation);
    }

    PreparedScene::PreparedScene(PreparedScene&& moveFrom)
    : _heap(std::move(moveFrom._heap)), _blocks(std::move(moveFrom._blocks))
    {}

    PreparedScene& PreparedScene::operator=(PreparedScene&& moveFrom)
    {
        _heap = std::move(moveFrom._heap);
        _blocks = std::move(moveFrom._blocks);
        return *this;
    }
}

