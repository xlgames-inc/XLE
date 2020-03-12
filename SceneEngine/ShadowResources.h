// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once 

#include "SceneEngineUtils.h"
#include "../RenderCore/Techniques/RenderStateResolver.h"
#include "../RenderCore/Metal/TextureView.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Types.h"
#include <vector>

namespace BufferUploads { class ResourceLocator; }
namespace RenderCore { namespace Techniques { class IRenderStateDelegate; }}

namespace SceneEngine
{
    class ShadowResourcesBox
    {
    public:
        class Desc {};

        RenderCore::Metal::ConstantBuffer       _sampleKernel32;

        ShadowResourcesBox(const Desc& desc);
        ~ShadowResourcesBox();
    };
}
