// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "..\RenderCore\Metal\Forward.h"

namespace SceneEngine
{
    class LightingParserContext;
    void VegetationSpawn_Prepare(RenderCore::Metal::DeviceContext* context, LightingParserContext& lightingParserContext);

    bool VegetationSpawn_DrawInstances(
            RenderCore::Metal::DeviceContext* context,
            unsigned instanceId, unsigned indexCount, unsigned startIndexLocation, unsigned baseVertexLocation);
}

