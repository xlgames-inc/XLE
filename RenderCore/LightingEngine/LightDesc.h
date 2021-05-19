// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/StateDesc.h"
#include "../RenderCore/Format.h"
#include "../Math/Matrix.h"
#include "../Math/Vector.h"
#include "../Utility/MemoryUtils.h"

namespace Utility { class ParameterBox; }
namespace RenderCore { namespace LightingEngine
{
	enum class LightSourceShape { Directional, Sphere, Tube, Rectangle, Disc };

	/// <summary>Defines a dynamic light</summary>
	/// Lights defined by this structure 
	class LightDesc
	{
	public:
		Float3x3    _orientation;
		Float3      _position;
		Float2      _radii;

		float       _cutoffRange;
		LightSourceShape       _shape;
		Float3      _diffuseColor;
		Float3      _specularColor;
		float       _diffuseWideningMin;
		float       _diffuseWideningMax;
		unsigned    _diffuseModel;
		unsigned    _shadowResolveModel;

		LightDesc();
	};

		//////////////////////////////////////////////////////////////////

	enum class ShadowProjectionMode { Arbitrary, Ortho, ArbitraryCubeMap };
	enum class ShadowResolveType { DepthTexture, RayTraced };

	class ShadowGeneratorDesc
	{
	public:
			/// @{
			/// Shadow texture definition
		uint32_t			_width = 2048u;
		uint32_t			_height = 2048u;
		RenderCore::Format	_format = RenderCore::Format::D16_UNORM;
		unsigned			_arrayCount = 0u;
			/// @}

			/// @{
			/// single sided depth bias
		float				_slopeScaledBias = 0.f;
		float				_depthBiasClamp = 0.f;
		int					_rasterDepthBias = 0;
			/// @}

			/// @{
			/// Double sided depth bias
			/// This is useful when flipping the culling mode during shadow
			/// gen. In this case single sided geometry doesn't cause acne
			/// (so we can have very small bias values). But double sided 
			/// geometry still gets acne, so needs a larger bias!
		float				_dsSlopeScaledBias = 0.f;
		float				_dsDepthBiasClamp = 0.f;
		int					_dsRasterDepthBias = 0;
			/// @}

		ShadowProjectionMode	_projectionMode = ShadowProjectionMode::Arbitrary;
		RenderCore::CullMode	_cullMode = RenderCore::CullMode::Back;
		ShadowResolveType		_resolveType = ShadowResolveType::DepthTexture;
		bool					_enableNearCascade = false;
	};

	uint64_t Hash64(const ShadowGeneratorDesc& shadowGeneratorDesc, uint64_t seed = DefaultSeed64);

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

		ShadowProjectionMode	_mode = ShadowProjectionMode::Arbitrary;
		unsigned                _normalProjCount = 0;
		bool                    _useNearProj = false;

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
	};

	constexpr unsigned MaxShadowTexturesPerLight = 6;

	using LightId = unsigned;

	/// <summary>Defines the projected shadows for a single light<summary>
	/// Tied to a specific light via the "_lightId" member.
	class ShadowProjectionDesc
	{
	public:
		ShadowGeneratorDesc		_shadowGeneratorDesc;
		using Projections = MultiProjection<MaxShadowTexturesPerLight>;

		Projections     _projections;
		Float4x4        _worldToClip = Identity<Float4x4>();   ///< Intended for use in CPU-side culling. Objects culled by this transform will be culled from all projections

		float           _worldSpaceResolveBias = 0.f;
		float           _tanBlurAngle = 0.f;
		float           _minBlurSearch = 0.f, _maxBlurSearch = 0.f;

		LightId         _lightId = ~0u;
	};

	enum class SkyTextureType { HemiCube, Cube, Equirectangular, HemiEquirectangular };
	
	class EnvironmentalLightingDesc
	{
	public:
		std::string   _skyTexture;   ///< use "<texturename>_*" when using a half cube style sky texture. The system will fill in "_*" with appropriate characters	
		SkyTextureType _skyTextureType;

		std::string   _diffuseIBL;   ///< Diffuse IBL map. Sometimes called irradiance map or ambient map
		std::string   _specularIBL;  ///< Prefiltered specular IBL map.

		Float3	_ambientLight = Float3(0.f, 0.f, 0.f);

		float   _skyBrightness = 1.f;
		float   _skyReflectionScale = 1.0f;
		float   _skyReflectionBlurriness = 2.f;

		bool    _doRangeFog = false;
		Float3  _rangeFogInscatter = Float3(0.f, 0.f, 0.f);
		float   _rangeFogThickness = 10000.f;     // optical thickness for range based fog

		bool    _doAtmosphereBlur = false;
		float   _atmosBlurStdDev = 1.3f;
		float   _atmosBlurStart = 1000.f;
		float   _atmosBlurEnd = 1500.f;
	};

	class SceneLightingDesc
	{
	public:
		std::vector<LightDesc> _lights;
		std::vector<ShadowProjectionDesc> _shadowProjections;
		EnvironmentalLightingDesc _env;
	};

	inline LightDesc::LightDesc()
	{
        _shape = LightSourceShape::Directional;
        _position = Normalize(Float3(-.1f, 0.33f, 1.f));
        _orientation = Identity<Float3x3>();
        _cutoffRange = 10000.f;
        _radii = Float2(1.f, 1.f);
        _diffuseColor = Float3(1.f, 1.f, 1.f);
        _specularColor = Float3(1.f, 1.f, 1.f);

        _diffuseWideningMin = 0.5f;
        _diffuseWideningMax = 2.5f;
        _diffuseModel = 1;

        _shadowResolveModel = 0;
	}

}}

