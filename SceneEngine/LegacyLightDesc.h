// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

namespace SceneEngine
{
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
}