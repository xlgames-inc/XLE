// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/IThreadContext_Forward.h"
#include "../Math/Vector.h"
#include "../Math/Matrix.h"
#include "../Core/Types.h"
#include <vector>

namespace RenderCore { namespace Techniques 
{
    class TechniqueContext; class CameraDesc;
	class ParsingContext;
}}

namespace SceneEngine
{
    class ModelIntersectionResources;

    class ModelIntersectionStateContext
    {
    public:
        struct ResultEntry
        {
        public:
            union { unsigned _depthAsInt; float _intersectionDepth; };
            Float4 _pt[3];
            unsigned _drawCallIndex;
            uint64 _materialGuid;

            static bool CompareDepth(const ResultEntry& lhs, const ResultEntry& rhs)
                { return lhs._intersectionDepth < rhs._intersectionDepth; }
        };

        std::vector<ResultEntry> GetResults();
        void SetRay(const std::pair<Float3, Float3> worldSpaceRay);
        void SetFrustum(const Float4x4& frustum);
        RenderCore::Techniques::ParsingContext& GetParserContext();

        enum TestType { RayTest = 0, FrustumTest = 1 };

        ModelIntersectionStateContext(
            TestType testType,
            std::shared_ptr<RenderCore::IThreadContext> threadContext,
            const RenderCore::Techniques::TechniqueContext& techniqueContext,
            const RenderCore::Techniques::CameraDesc* cameraForLOD = nullptr);
        ~ModelIntersectionStateContext();

    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        static const unsigned s_maxResultCount = 256;
    };
}

