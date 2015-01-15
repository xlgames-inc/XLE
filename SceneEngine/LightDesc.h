// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../Math/Matrix.h"
#include "../Math/Vector.h"
#include "../Core/Types.h"

namespace SceneEngine
{

        //////////////////////////////////////////////////////////////////

    static const unsigned MaxShadowTexturesPerLight = 6;

    class ShadowFrustumDesc
    {
    public:
        uint32      _width, _height;

        typedef RenderCore::Metal::NativeFormat::Enum Format;
        Format      _typelessFormat;
        Format      _writeFormat;
        Format      _readFormat;

        class ShadowProjection
        {
        public:
            Float4x4    _projectionMatrix;
            Float4x4    _viewMatrix;
            Float2      _projectionDepthRatio;
            Float2      _projectionScale;
        };
        unsigned            _projectionCount;
        ShadowProjection    _projections[MaxShadowTexturesPerLight];
        Float4x4            _worldToClip;

        ShadowFrustumDesc();
        ShadowFrustumDesc(const ShadowFrustumDesc& copyFrom);
        ShadowFrustumDesc& operator=(const ShadowFrustumDesc& copyFrom);
    };

    class LightDesc
    {
    public:
        Float3      _negativeLightDirection;
        float       _radius;
        bool        _isDynamicLight;
        bool        _isPointLight;
        unsigned    _shadowFrustumIndex;
        Float3      _lightColour;
    };

    class GlobalLightingDesc
    {
    public:
        Float3          _ambientLight;
        const char*     _skyTexture;            ///< use "<texturename>_XX" when using a half cube style sky texture

        bool _doAtmosphereBlur;
        bool _doOcean;
        bool _doToneMap;
        bool _doVegetationSpawn;

        GlobalLightingDesc() 
            : _ambientLight(0.f, 0.f, 0.f), _skyTexture(nullptr)
            , _doAtmosphereBlur(false), _doOcean(false), _doToneMap(false), _doVegetationSpawn(false) {}
    };

    class ShadowProjectionConstants
    {
    public:
        uint32      _projectionCount; 
        uint32      _dummy[3];
        Float4      _shadowProjRatio[MaxShadowTexturesPerLight];
        Float4x4    _projection[MaxShadowTexturesPerLight];
    };

    class ProcessedShadowFrustum
    {
    public:
        RenderCore::Metal::ShaderResourceView               _shadowTextureResource;
        std::unique_ptr<RenderCore::Metal::ConstantBuffer>  _projectConstantBuffer;
        int                                                 _frustumCount;
        ShadowProjectionConstants                           _projectConstantBufferSource;

        ProcessedShadowFrustum();
        ProcessedShadowFrustum(ProcessedShadowFrustum&& moveFrom);
        ProcessedShadowFrustum& operator=(ProcessedShadowFrustum&& moveFrom) never_throws;
    };

        //////////////////////////////////////////////////////////////////

    inline ProcessedShadowFrustum::ProcessedShadowFrustum() : _frustumCount(0) {}
    inline ProcessedShadowFrustum::ProcessedShadowFrustum(ProcessedShadowFrustum&& moveFrom)
    : _shadowTextureResource(std::move(moveFrom._shadowTextureResource))
    , _projectConstantBuffer(std::move(moveFrom._projectConstantBuffer))
    , _projectConstantBufferSource(std::move(moveFrom._projectConstantBufferSource))
    , _frustumCount(moveFrom._frustumCount)
    {}

    inline ProcessedShadowFrustum& ProcessedShadowFrustum::operator=(ProcessedShadowFrustum&& moveFrom)
    {
        _shadowTextureResource = std::move(moveFrom._shadowTextureResource);
        _projectConstantBuffer = std::move(moveFrom._projectConstantBuffer);
        _projectConstantBufferSource = std::move(moveFrom._projectConstantBufferSource);
        _frustumCount = moveFrom._frustumCount;
        return *this;
    }

    inline ShadowFrustumDesc::ShadowFrustumDesc()
    {
        _width = _height = 0;
        _typelessFormat = _writeFormat = _readFormat = RenderCore::Metal::NativeFormat::Unknown;
        _projectionCount = 0;
        _worldToClip = Identity<Float4x4>();
    }

    inline ShadowFrustumDesc::ShadowFrustumDesc(const ShadowFrustumDesc& copyFrom)
    {
        _width = copyFrom._width; _height = copyFrom._height;
        _typelessFormat = copyFrom._typelessFormat;
        _writeFormat = copyFrom._writeFormat;
        _readFormat = copyFrom._readFormat;
        std::copy(copyFrom._projections, &copyFrom._projections[dimof(_projections)], _projections);
        _projectionCount = copyFrom._projectionCount;
        _worldToClip = copyFrom._worldToClip;
    }

    inline ShadowFrustumDesc& ShadowFrustumDesc::operator=(const ShadowFrustumDesc& copyFrom)
    {
        _width = copyFrom._width; _height = copyFrom._height;
        _typelessFormat = copyFrom._typelessFormat;
        _writeFormat = copyFrom._writeFormat;
        _readFormat = copyFrom._readFormat;
        std::copy(copyFrom._projections, &copyFrom._projections[dimof(_projections)], _projections);
        _projectionCount = copyFrom._projectionCount;
        _worldToClip = copyFrom._worldToClip;
        return *this;
    }
}

