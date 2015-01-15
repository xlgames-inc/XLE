// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include <memory>

namespace SceneEngine
{
    class DualContourMesh;
    class LightingParserContext;

    class DualContourRenderer
    {
    public:
        void Render(RenderCore::Metal::DeviceContext* context, 
                    LightingParserContext& lightingParserContext,
                    unsigned techniqueIndex) const;

        void RenderAsCloud( RenderCore::Metal::DeviceContext* context, 
                            LightingParserContext& lightingParserContext);

        DualContourRenderer(const DualContourMesh& mesh);
        ~DualContourRenderer();

        class Pimpl;
    private:
        std::unique_ptr<Pimpl> _pimpl;
    };

    void    DualContourMesh_DebuggingRender(
                RenderCore::Metal::DeviceContext* context, 
                LightingParserContext& lightingParserContext,
                unsigned techniqueIndex, const DualContourMesh& mesh);

}