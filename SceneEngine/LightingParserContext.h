// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../Math/Matrix.h"
#include "../Utility/MemoryUtils.h"
#include <functional>

namespace RenderCore { class CameraDesc; }
namespace Assets { namespace Exceptions { class InvalidResource; class PendingResource; } }

namespace SceneEngine
{
    class MetricsBox;
    class ISceneParser;
    class PreparedShadowFrustum;
    class TechniqueContext;
    class TechniqueInterface;
    class ShadowProjectionConstants;
    class ILightingParserPlugin;

    __declspec(align(16)) class ProjectionDesc
    {
    public:
        Float4x4        _worldToProjection;
        Float4x4        _viewToProjection;
        Float4x4        _viewToWorld;
        float           _verticalFov;
        float           _aspectRatio;
        float           _nearClip;
        float           _farClip;
    };

    class LightingParserContext
    {
    public:

            //  ----------------- Global states -----------------
        MetricsBox*     GetMetricsBox()                     { return _metricsBox; }
        void            SetMetricsBox(MetricsBox* box);
        ISceneParser*   GetSceneParser()                    { return _sceneParser; }

            //  ----------------- Active projection context -----------------
        ProjectionDesc&         GetProjectionDesc()         { return *_projectionDesc; }
        const ProjectionDesc&   GetProjectionDesc() const   { return *_projectionDesc; }

            //  ----------------- Working technique context -----------------
        TechniqueContext&                           GetTechniqueContext()               { return *_techniqueContext.get(); }
        const RenderCore::Metal::UniformsStream&    GetGlobalUniformsStream() const     { return *_globalUniformsStream.get(); }
        void                                        SetGlobalCB(unsigned index, RenderCore::Metal::DeviceContext* context, const void* newData, size_t dataSize);
        RenderCore::Metal::ConstantBuffer&          GetGlobalTransformCB()              { return _globalCBs[0]; }
        RenderCore::Metal::ConstantBuffer&          GetGlobalStateCB()                  { return _globalCBs[1]; }

            //  ----------------- Working shadow state ----------------- 
        std::vector<PreparedShadowFrustum>     _preparedShadows;

            //  ----------------- Exception reporting ----------------- 
        std::string                 _errorString;
        std::vector<std::string>    _pendingResources;
        std::vector<std::string>    _invalidResources;
        void                        Process(const Assets::Exceptions::InvalidResource& e);
        void                        Process(const Assets::Exceptions::PendingResource& e);

            //  ----------------- Overlays for late rendering -----------------
        typedef std::function<void(RenderCore::Metal::DeviceContext*, LightingParserContext&)> PendingOverlay;
        std::vector<PendingOverlay> _pendingOverlays;

            //  ----------------- Plugins -----------------
        std::vector<std::shared_ptr<ILightingParserPlugin>> _plugins;

        LightingParserContext(ISceneParser* sceneParser, const TechniqueContext& techniqueContext);
        ~LightingParserContext();

    private:
        RenderCore::Metal::ConstantBuffer   _globalCBs[5];
        MetricsBox*                         _metricsBox;
        ISceneParser*                       _sceneParser;
        std::unique_ptr<TechniqueContext>   _techniqueContext;
        std::unique_ptr<ProjectionDesc, AlignedDeletor>     _projectionDesc;

        std::unique_ptr<RenderCore::Metal::UniformsStream>      _globalUniformsStream;
        std::vector<const RenderCore::Metal::ConstantBuffer*>   _globalUniformsConstantBuffers;
    };
}

