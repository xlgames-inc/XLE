// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/IThreadContext_Forward.h"
#include "../Math/Vector.h"
#include <vector>

namespace RenderCore { namespace Techniques 
{
    class TechniqueContext; class CameraDesc;
}}

namespace SceneEngine
{
    class RayVsModelResources;
    class LightingParserContext;

    class RayVsModelStateContext
    {
    public:
        struct ResultEntry
        {
        public:
            union { unsigned _depthAsInt; float _intersectionDepth; };
            Float4 _pt[3];
            unsigned _drawCallIndex;

            static bool CompareDepth(const ResultEntry& lhs, const ResultEntry& rhs)
                { return lhs._intersectionDepth < rhs._intersectionDepth; }
        };

        std::vector<ResultEntry> GetResults();
        void SetRay(const std::pair<Float3, Float3> worldSpaceRay);
        LightingParserContext& GetParserContext();

        RayVsModelStateContext(
            std::shared_ptr<RenderCore::IThreadContext> threadContext,
            const RenderCore::Techniques::TechniqueContext& techniqueContext,
            const RenderCore::Techniques::CameraDesc* cameraForLOD = nullptr);
        ~RayVsModelStateContext();

    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        static const unsigned s_maxResultCount = 256;
    };
}

