// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CppUnitTest.h"
#include "UnitTestHelper.h"
#include "../SceneEngine/TerrainMaterial.h"
#include "../ConsoleRig/Log.h"
#include "../ConsoleRig/GlobalServices.h"
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

    static __declspec(noinline) void RunPerformanceTest1(
        const std::basic_string<utf8>& testString, unsigned iterationCount)
    {
        for (unsigned c=0; c<iterationCount; ++c) {
            MemoryMappedInputStream stream(AsPointer(testString.cbegin()), AsPointer(testString.cend()));
            InputStreamFormatter<utf8> formatter(stream);
            SceneEngine::TerrainMaterialConfig matConfig(formatter, ::Assets::DirectorySearchRules());
            (void)matConfig;
        }
    }

    static __declspec(noinline) void RunPerformanceTest2(
        const std::basic_string<utf8>& testString, unsigned iterationCount)
    {
        for (unsigned c=0; c<iterationCount; ++c) {
            MemoryMappedInputStream stream(AsPointer(testString.cbegin()), AsPointer(testString.cend()));
            InputStreamFormatter<utf8> formatter(stream);
            SceneEngine::TerrainMaterialConfig matConfig(formatter, ::Assets::DirectorySearchRules(), true);
            (void)matConfig;
        }
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

        TEST_METHOD(ClassPropertiesPerformance)
        {
            const std::basic_string<utf8> testString = (const utf8*)R"~~(~~!Format=1; Tab=4
DiffuseDims={512u, 512u}v; NormalDims={512u, 512u}v; ParamDims={512u, 512u}v

~GradFlagMaterial; MaterialId=0u; 
	Texture[0]=Game/plaintextures/grass/grassTextureNo9227
	Texture[1]=Game/aa_terrain/canyon/tr_canyon_rock_700b_800b
	Texture[2]=Game/aa_terrain/canyon/tr_canyon_rock3d_708a
	Texture[3]=Game/aa_terrain/canyon/tr_canyon_rock3d_602b
	Texture[4]=Game/plaintextures/grass/grassTextureNo9227; Mapping={1.8f, 1f, 1f, 1f, 1f}

~GradFlagMaterial; MaterialId=1u; Texture[0]=ProcTexture
	Texture[1]=Game/plaintextures/gravel/stonesTextureNo8648
	Texture[2]=Game/aa_terrain/canyon/tr_canyon_rock3d_409a
	Texture[3]=Game/aa_terrain/canyon/tr_canyon_rock3d_409a
	Texture[4]=Game/plaintextures/gravel/gravelTextureNo7899; Mapping={1.8f, 1f, 1f, 1f, 1f}

~ProcTextureSetting; Name=ProcTexture; Texture[0]=Game/plaintextures/grass/grassTextureNo7109
	Texture[1]=Game/plaintextures/grass/grassTextureNo6354; HGrid=5f; Gain=0.5f)~~";

            UnitTest_SetWorkingDirectory();
            ConsoleRig::GlobalServices services(GetStartupConfig());

            const unsigned iterationCount = 64 * 128;
            auto start = __rdtsc();
            RunPerformanceTest1(testString, iterationCount);
            auto middle = __rdtsc();
            RunPerformanceTest2(testString, iterationCount);
            auto end = __rdtsc();

            LogAlwaysWarning << "Properties based serialization: " << (middle-start) / iterationCount << " cycles per iteration.";
            LogAlwaysWarning << "Old style serialization: " << (end-middle) / iterationCount << " cycles per iteration.";
        }

	};
}