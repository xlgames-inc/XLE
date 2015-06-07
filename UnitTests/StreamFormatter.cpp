// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CppUnitTest.h"
#include "../Utility/Streams/StreamFormatter.h"
#include "../Utility/Streams/StreamDOM.h"
#include "../Utility/Conversion.h"
#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
    const std::string testString = R"--(~~!Format=1; Tab=4

~EnvSettings
	Name=environment
	
	~ToneMapSettings; BloomDesaturationFactor=0.6f; BloomRampingFactor=0.8f; SceneKey=0.23f
		BloomBlurStdDev=1.32f; Flags=3i; BloomBrightness=20.8f; LuminanceMax=3f; WhitePoint=8f
		BloomThreshold=10f; LuminanceMin=0.06f; BloomScale=-1405359i
	
	~AmbientSettings; AmbientLight=-12080934i; AmbientBrightness=0.1f; SkyReflectionBlurriness=2f
		SkyTexture=Game/xleres/DefaultResources/sky/desertsky.dds; SkyReflectionScale=8f
		SkyBrightness=0.68f
	
	~ShadowFrustumSettings; FrustumCount=5i; ShadowRasterDepthBias=400i; FrustumSizeFactor=4f
		ShadowSlopeScaledBias=1f; Flags=1i; ShadowDepthBiasClamp=0f; MaxBlurSearch=64f
		MinBlurSearch=2f; MaxDistanceFromCamera=500f; WorldSpaceResolveBias=0f; FocusDistance=3f
		BlurAngleDegrees=0.25f; Name=shadows; TextureSize=2048i
	
	~DirectionalLight; DiffuseModel=1i; DiffuseBrightness=3.5f; Diffuse=-1072241i
		Specular=-1647966i; Flags=1i; SpecularBrightness=19f; DiffuseWideningMin=0.2f
		Transform={1f, 0f, 0f, 0f, 0f, 1f, 0f, 0f, 0f, 0f, 1f, 0f, -5.05525f, 32.7171f, 5.73295f, 1f}
		ShadowResolveModel=0i; SpecularNonMetalBrightness=15f; DiffuseWideningMax=0.9f
		Name=DirLight; ShadowFrustumSettings=shadows; Visible=0u
	
	~DirectionalLight; DiffuseModel=0i; DiffuseBrightness=0.75f; Diffuse=-957580i; Specular=-1i
		Flags=0i; SpecularBrightness=5f; DiffuseWideningMin=0.5f
		Transform={1f, 0f, 0f, 0f, 0f, 1f, 0f, 0f, 0f, 0f, 1f, 0f, 3.59605f, 2.4588f, -2.49624f, 1f}
		ShadowResolveModel=0i; SpecularNonMetalBrightness=5f; DiffuseWideningMax=2.5f
		Name=SecondaryLight; ShadowFrustumSettings=shadows; Visible=0u
	
	~DirectionalLight; DiffuseModel=0i; DiffuseBrightness=0.2f; Diffuse=-5247249i; Specular=-1i
		Flags=0i; SpecularBrightness=3.5f; DiffuseWideningMin=0.5f
		Transform={1f, 0f, 0f, 0f, 0f, 1f, 0f, 0f, 0f, 0f, 1f, 0f, -9.9108f, -8.22611f, 2.5275f, 1f}
		ShadowResolveModel=0i; SpecularNonMetalBrightness=3.5f; DiffuseWideningMax=2.5f
		Name=TertiaryLight; ShadowFrustumSettings=shadows; Visible=0u
)--";

    template<typename CharType>
        void RunBasicTest()
    {
        auto converted = Conversion::Convert<std::basic_string<CharType>>(testString);
        MemoryMappedInputStream stream(AsPointer(converted.cbegin()), AsPointer(converted.cend()));
        InputStreamFormatter<CharType> formatter(stream);

        Document<InputStreamFormatter<CharType>> doc(formatter);

        int t = 0;
        (void)t;
    }

	TEST_CLASS(StreamFormatter)
	{
	public:
		
		TEST_METHOD(TestDOMParse)
		{
			RunBasicTest<utf8>();
            RunBasicTest<ucs2>();
            RunBasicTest<ucs4>();
		}

	};
}