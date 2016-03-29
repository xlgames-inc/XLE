// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include "../Assets/Assets.h"
#include "../Math/Matrix.h"
#include "../Math/Vector.h"
#include "../Core/Types.h"

namespace Utility { class ParameterBox; }
namespace SceneEngine
{

        //////////////////////////////////////////////////////////////////

    /// <summary>Represents a set of shared projections</summary>
    /// This class is intended to be used with cascaded shadows (and similiar
    /// cascaded effects). Multiple cascades require multiple projections, and
    /// this class represents a small bundle of cascades.
    ///
    /// Sometimes we want to put restrictions on the cascades in order to 
    /// reduce shader calculations. For example, a collection of orthogonal
    /// cascades can be defined by a set of axially aligned volumes in a shared
    /// orthogonal projection space.
    ///
    template<int MaxProjections> class MultiProjection
    {
    public:
        class FullSubProjection
        {
        public:
            Float4x4    _projectionMatrix;
            Float4x4    _viewMatrix;
        };

        class OrthoSubProjection
        {
        public:
            Float3      _projMins;
            Float3      _projMaxs;
        };

        struct Mode { enum Enum { Arbitrary, Ortho }; };

        typename Mode::Enum     _mode;
        unsigned                _normalProjCount;
        bool                    _useNearProj;

        unsigned Count() const { return _normalProjCount + (_useNearProj?1:0); }

            /// @{
        ///////////////////////////////////////////////////////////////////////
            /// When in "Full" mode, each sub projection gets a full view and 
            /// projection matrix. This means that every sub projection can 
            /// have a completely independently defined projection.
        FullSubProjection   _fullProj[MaxProjections];
        ///////////////////////////////////////////////////////////////////////
            /// @}

            /// @{
        ///////////////////////////////////////////////////////////////////////
            /// When a "OrthoSub" mode, the sub projections have some restrictions
            /// There is a single "definition transform" that defines a basic
            /// projection that all sub projects inherit. The sub projects then
            /// define and axially aligned area of XYZ space inside of the 
            /// definition transform. When used with an orthogonal transform, this
            /// allows each sub projection to wrap a volume of space. But all sub
            /// projections must match the rotation and skew of other projections.
        OrthoSubProjection  _orthoSub[MaxProjections];
        Float4x4            _definitionViewMatrix;
        ///////////////////////////////////////////////////////////////////////
            /// @}

            /// @{
            /// In both modes, we often need to store the "minimal projection"
            /// This is the 4 most important elements of the projection matrix.
            /// In typical projection matrices, the remaining parts can be implied
            /// which means that these 4 elements is enough to do reverse projection
            /// work in the shader.
            /// In the case of shadows, mostly we need to convert depth values from
            /// projection space into view space (and since view space typically 
            /// has the same scale as world space, we can assume that view space 
            /// depth values are in natural world space units).
        Float4      _minimalProjection[MaxProjections];
            /// @}

        Float4x4    _specialNearProjection;
        Float4      _specialNearMinimalProjection;

        MultiProjection();
    };

    static const unsigned MaxShadowTexturesPerLight = 6;

    using LightId = unsigned;

    /// <summary>Defines the projected shadows for a single light<summary>
    /// Tied to a specific light via the "_lightId" member.
    class ShadowProjectionDesc
    {
    public:
            /// @{
            /// Shadow texture definition
        uint32      _width, _height;
        typedef RenderCore::Metal::NativeFormat::Enum Format;
        Format      _typelessFormat;
        Format      _writeFormat;
        Format      _readFormat;
            /// @}

        typedef MultiProjection<MaxShadowTexturesPerLight> Projections;

        Projections     _projections;
        Float4x4        _worldToClip;   ///< Intended for use in CPU-side culling. Objects culled by this transform will be culled from all projections

            /// @{
            /// single sided depth bias
        float           _slopeScaledBias;
        float           _depthBiasClamp;
        int             _rasterDepthBias;
            /// @}

            /// @{
            /// Double sided depth bias
            /// This is useful when flipping the culling mode during shadow
            /// gen. In this case single sided geometry doesn't cause acne
            /// (so we can have very small bias values). But double sided 
            /// geometry still gets acne, so needs a larger bias!
        float           _dsSlopeScaledBias;
        float           _dsDepthBiasClamp;
        int             _dsRasterDepthBias;
            /// @}

        float           _worldSpaceResolveBias;
        float           _tanBlurAngle;
        float           _minBlurSearch, _maxBlurSearch;

        enum class ResolveType { DepthTexture, RayTraced };
        ResolveType     _resolveType;

        enum class WindingCull { BackFaces, FrontFaces, None };
        WindingCull     _windingCull;

        LightId         _lightId;

        ShadowProjectionDesc();
    };

    /// <summary>Defines a dynamic light</summary>
    /// Lights defined by this structure 
    class LightDesc
    {
    public:
        enum Shape { Directional, Sphere, Tube, Rectangle, Disc };

        Float3x3    _orientation;
        Float3      _position;
        Float2      _radii;

        float       _cutoffRange;
        Shape       _shape;
        Float3      _diffuseColor;
        Float3      _specularColor;
        float       _diffuseWideningMin;
        float       _diffuseWideningMax;
        unsigned    _diffuseModel;
        unsigned    _shadowResolveModel;

        LightDesc();
        LightDesc(const Utility::ParameterBox& paramBox);
    };

    class GlobalLightingDesc
    {
    public:
        ::Assets::ResChar   _skyTexture[MaxPath];   ///< use "<texturename>_*" when using a half cube style sky texture. The system will fill in "_*" with appropriate characters
        enum class SkyTextureType { HemiCube, Cube, Equirectangular, HemiEquirectangular };
        SkyTextureType      _skyTextureType;

        ::Assets::ResChar   _diffuseIBL[MaxPath];   ///< Diffuse IBL map. Sometimes called irradiance map or ambient map
        ::Assets::ResChar   _specularIBL[MaxPath];  ///< Prefiltered specular IBL map.
        Float3              _ambientLight;

        float   _skyBrightness;
        float   _skyReflectionScale;
        float   _skyReflectionBlurriness;

        bool    _doRangeFog;
        Float3  _rangeFogInscatter;
        float   _rangeFogThickness;     // optical thickness for range based fog

        bool    _doAtmosphereBlur;
        float   _atmosBlurStdDev;
        float   _atmosBlurStart;
        float   _atmosBlurEnd;

        GlobalLightingDesc();
        GlobalLightingDesc(const Utility::ParameterBox& paramBox);
    };

    
    template<int MaxProjections>
        MultiProjection<MaxProjections>::MultiProjection()
    {
        _mode = Mode::Arbitrary;
        _normalProjCount = 0;
        _useNearProj = false;
    }

}

