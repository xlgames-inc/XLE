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
	class ITechniqueDelegate_Old;
	class SequencerContext;
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
            Float4 _pt[3];
			union { unsigned _depthAsInt; float _intersectionDepth; };
            unsigned _drawCallIndex;
            uint64 _materialGuid;

            static bool CompareDepth(const ResultEntry& lhs, const ResultEntry& rhs)
                { return lhs._intersectionDepth < rhs._intersectionDepth; }
        };

        std::vector<ResultEntry> GetResults();
        void SetRay(const std::pair<Float3, Float3> worldSpaceRay);
        void SetFrustum(const Float4x4& frustum);

		RenderCore::Techniques::SequencerContext MakeRayTestSequencerTechnique();

        enum TestType { RayTest = 0, FrustumTest = 1 };

        ModelIntersectionStateContext(
            TestType testType,
            RenderCore::IThreadContext& threadContext,
            RenderCore::Techniques::ParsingContext& parsingContext,
            const RenderCore::Techniques::CameraDesc* cameraForLOD = nullptr);
        ~ModelIntersectionStateContext();

    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        static const unsigned s_maxResultCount = 256;
    };

	std::shared_ptr<RenderCore::Techniques::ITechniqueDelegate_Old> CreateRayTestTechniqueDelegate();
}

