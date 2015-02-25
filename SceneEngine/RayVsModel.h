// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "LightingParserContext.h"
#include "../RenderCore/Metal/Shader.h"

namespace SceneEngine
{
    class RayVsModelResources;

    class RayVsModelStateContext
    {
    public:
        struct ResultEntry
        {
        public:
            union { unsigned _depthAsInt; float _intersectionDepth; };
            Float4 _pt[3];
        };

        std::vector<ResultEntry> GetResults();
        void SetRay(const std::pair<Float3, Float3> worldSpaceRay);
        LightingParserContext& GetParserContext() { return _parserContext; }

        RayVsModelStateContext(
            RenderCore::Metal::DeviceContext* devContext,
            const RenderCore::Techniques::TechniqueContext& techniqueContext,
            const RenderCore::Techniques::CameraDesc* cameraForLOD = nullptr);
        ~RayVsModelStateContext();

    protected:
        intrusive_ptr<RenderCore::Metal::DeviceContext> _devContext;
        RayVsModelResources* _res;
        RenderCore::Metal::GeometryShader::StreamOutputInitializers _oldSO;

        LightingParserContext _parserContext;

        static const unsigned s_maxResultCount = 256;
    };
}

