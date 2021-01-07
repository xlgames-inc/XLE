// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Math/Vector.h"
#include "../Math/MathSerialization.h"
#include "../Utility/Streams/StreamFormatter.h"
#include "../Utility/Streams/StreamDOM.h"
#include "../Utility/Conversion.h"
#include "../Utility/StringFormat.h"
#include "../Utility/ImpliedTyping.h"
#include <string>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using namespace Catch::literals;

namespace UnitTests
{

    const std::basic_string<utf8> testString = (const utf8*)R"~~(~~!Format=1; Tab=4

DiffuseDims={512u, 512u}v; NormalDims={512u, 512u}v; ParamDims={512u, 512u}v

~StrataMaterial
    ~Strata
        ~One
            Texture0=Tex0
            Texture1=Tex1
            Slopes=SlopesTex
            EndHeight = 0.4
            Mapping = {4,3,2}
        ~Two
            Texture0=aaTex0
            Texture1=12Tex1
            Slopes=XXXSlopesTexXXX
            EndHeight = 3
            Mapping = {7,9,10,12,14,16,17}
    MaterialId=2u

~GradFlagMaterial
    MaterialId=0u
    Texture[0]=grassTextureNo9227
    Texture[1]=tr_canyon_rock_700b_800b
    Texture[2]=tr_canyon_rock3d_708a
    Texture[3]=tr_canyon_rock3d_602b
    Texture[4]=grassTextureNo9227; Mapping={1.8f, 1f, 1f, 1f, 1f}

~GradFlagMaterial
    MaterialId=1u
    Texture[0]=ProcTexture
    Texture[1]=stonesTextureNo8648
    Texture[2]=tr_canyon_rock3d_409a
    Texture[3]=tr_canyon_rock3d_409a
    Texture[4]=gravelTextureNo7899; Mapping={1.8f, 1f, 1f, 1f, 1f}

~ProcTextureSetting; Name=ProcTexture; 
    Texture[0]=grassTextureNo7109
    Texture[1]=grassTextureNo6354; HGrid=5f; Gain=0.5f
)~~";

    const std::string testString2 = R"--(~~!Format=1; Tab=4

~EnvSettings
	Name=environment
	
	~ToneMapSettings; BloomDesaturationFactor=0.6f; BloomRampingFactor=0.8f; SceneKey=0.23f
		BloomBlurStdDev=1.32f; Flags=3i; BloomBrightness=20.8f; LuminanceMax=3f; WhitePoint=8f
		BloomThreshold=10f; LuminanceMin=0.06f; BloomScale=-1405359i
	
	~AmbientSettings; AmbientLight=-12080934i; AmbientBrightness=0.1f; SkyReflectionBlurriness=2f
		SkyTexture=xleres/DefaultResources/sky/desertsky.dds; SkyReflectionScale=8f
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

    struct ExampleSerializableObject
    {
        struct StrataMaterial
        {
            struct Strata
            {
                std::string _texture[3];
                float _mappingConstant[3] = { 0.f, 0.f, 0.f };
                float _endHeight = 0.f;
            };
            std::vector<Strata> _strata;
            unsigned _id = 0;
        };

        struct GradFlagMaterial
        {
            std::string _texture[5];
            float _mappingConstant[5] = { 0.f, 0.f, 0.f, 0.f, 0.f };
            unsigned _id = 0;
        };

        struct ProcTextureSetting
        {
            std::string _name;
            std::string _texture[2];
            float _hgrid = 100.f, _gain = .5f;
        };

        std::vector<StrataMaterial>     _strataMaterials;
        std::vector<GradFlagMaterial>   _gradFlagMaterials;
        std::vector<ProcTextureSetting> _procTextures;

        UInt2   _diffuseDims = UInt2(32, 32);
        UInt2   _normalDims = UInt2(32, 32);
        UInt2   _paramDims = UInt2(32, 32);
        float   _specularParameter = 0.05f;
        float   _roughnessMin = 0.7f, _roughnessMax = 1.f;
        float   _shadowSoftness = 15.f;
    };

    static ExampleSerializableObject SerializeViaDom(
        const Document<InputStreamFormatter<utf8>>& doc)
    {
        static const utf8* TextureNames[] = { u("Texture0"), u("Texture1"), u("Slopes") };
        ExampleSerializableObject result;

        for (const auto& matCfg:doc.RootElement().children()) {
            if (XlEqString(matCfg.Name(), u("StrataMaterial"))) {

                ExampleSerializableObject::StrataMaterial mat;
                mat._id = matCfg.Attribute(u("MaterialId"), 0u);

                auto strata = matCfg.Element(u("Strata"));
                unsigned strataCount = 0;
                for (const auto& c:strata.children()) { ++strataCount; }

                unsigned strataIndex = 0;
                for (const auto& c:strata.children()) {
                    ExampleSerializableObject::StrataMaterial::Strata newStrata;
                    for (unsigned t=0; t<dimof(TextureNames); ++t) {
                        auto tName = c.Attribute(TextureNames[t]).Value();
                        if (XlCompareStringI(tName, u("null"))!=0)
                            newStrata._texture[t] = tName.Cast<char>().AsString();
                    }

                    newStrata._endHeight = c.Attribute(u("EndHeight"), 0.f);
                    auto mappingConst = c.Attribute(u("Mapping"), Float4(1.f, 1.f, 1.f, 1.f));
                    newStrata._mappingConstant[0] = mappingConst[0];
                    newStrata._mappingConstant[1] = mappingConst[1];
                    newStrata._mappingConstant[2] = mappingConst[2];

                    mat._strata.push_back(newStrata);
                    ++strataIndex;
                }

                result._strataMaterials.push_back(std::move(mat));

            } else if (XlEqString(matCfg.Name(), u("GradFlagMaterial"))) {

                ExampleSerializableObject::GradFlagMaterial mat;
                mat._id = matCfg.Attribute(u("MaterialId"), 0);
            
                mat._texture[0] = matCfg.Attribute(u("Texture0")).Value().Cast<char>().AsString();
                mat._texture[1] = matCfg.Attribute(u("Texture1")).Value().Cast<char>().AsString();
                mat._texture[2] = matCfg.Attribute(u("Texture2")).Value().Cast<char>().AsString();
                mat._texture[3] = matCfg.Attribute(u("Texture3")).Value().Cast<char>().AsString();
                mat._texture[4] = matCfg.Attribute(u("Texture4")).Value().Cast<char>().AsString();

                char buffer[512];
                auto mappingAttr = matCfg.Attribute(u("Mapping")).Value();
                auto parsedType = ImpliedTyping::Parse(
                    mappingAttr,
                    buffer, sizeof(buffer));
                ImpliedTyping::Cast(
                    MakeIteratorRange(mat._mappingConstant), 
                    ImpliedTyping::TypeDesc{ImpliedTyping::TypeCat::Float, dimof(mat._mappingConstant)},
                    MakeIteratorRange(buffer), parsedType);
                
                result._gradFlagMaterials.push_back(mat);

            } else if (XlEqString(matCfg.Name(), u("ProcTextureSetting"))) {

                ExampleSerializableObject::ProcTextureSetting mat;
                mat._name = matCfg.Attribute(u("Name")).Value().Cast<char>().AsString();
                mat._texture[0] = matCfg.Attribute(u("Texture0")).Value().Cast<char>().AsString();
                mat._texture[1] = matCfg.Attribute(u("Texture1")).Value().Cast<char>().AsString();
                mat._hgrid = matCfg.Attribute(u("HGrid"), mat._hgrid);
                mat._gain = matCfg.Attribute(u("Gain"), mat._gain);
                result._procTextures.push_back(mat);

            }
        }

        return result;
    }

    template<typename CharType>
        void InitStreamDOMForCharacterType()
    {
        auto converted = Conversion::Convert<std::basic_string<CharType>>(testString2);
        InputStreamFormatter<CharType> formatter(MakeStringSection(converted));

        Document<InputStreamFormatter<CharType>> doc(formatter);

        int t = 0;
        (void)t;
    }

    static no_inline void RunPerformanceTest1(
        const std::basic_string<utf8>& testString, unsigned iterationCount)
    {
        /*for (unsigned c=0; c<iterationCount; ++c) {
            InputStreamFormatter<utf8> formatter(MakeStringSection(testString));
            SceneEngine::TerrainMaterialConfig matConfig(formatter, ::Assets::DirectorySearchRules(), std::make_shared<::Assets::DependencyValidation>());
            (void)matConfig;
        }*/
    }

    static no_inline void RunPerformanceTest2(
        const std::basic_string<utf8>& testString, unsigned iterationCount)
    {
        for (unsigned c=0; c<iterationCount; ++c) {
            InputStreamFormatter<utf8> formatter(MakeStringSection(testString));
            Document<InputStreamFormatter<utf8>> doc(formatter);
            auto matConfig = SerializeViaDom(doc);
            (void)matConfig;
        }
    }

    TEST_CASE( "StreamFormatter-BasicDOMInterface", "[utility]" )
    {
        // Load an aribtrary file, and use the basic interface of the Document<> class 
        InputStreamFormatter<utf8> formatter(MakeStringSection(testString));
        Document<InputStreamFormatter<utf8>> doc(formatter);
        auto rootElement = doc.RootElement();

        SECTION("C++11 syntax element iteration") {
            unsigned visitedChildren = 0;
            for (auto i=rootElement.begin_children(); i!=rootElement.end_children(); ++i) {
                std::cout << i->Name().Cast<char>() << std::endl;
                ++visitedChildren;
            }
            REQUIRE(visitedChildren == 4);
        }

        SECTION("for(:) syntax element iteration") {
            unsigned visitedChildren = 0;
            for (const auto& i:rootElement.children()) {
                std::cout << i.Name().Cast<char>() << std::endl;
                ++visitedChildren;
            }
            REQUIRE(visitedChildren == 4);
        }

        SECTION("C++11 syntax attribute iteration") {
            unsigned visitedAttributes = 0;
            for (auto i=rootElement.begin_attributes(); i!=rootElement.end_attributes(); ++i) {
                std::cout << i->Name().Cast<char>() << ":" << i->Value().Cast<char>() << std::endl;
                ++visitedAttributes;
            }
            REQUIRE(visitedAttributes == 3);
        }

        SECTION("for(:) syntax attribute iteration") {
            unsigned visitedAttributes = 0;
            for (const auto& i:rootElement.attributes()) {
                std::cout << i.Name().Cast<char>() << ":" << i.Value().Cast<char>() << std::endl;
                ++visitedAttributes;
            }
            REQUIRE(visitedAttributes == 3);
        }
    }

    TEST_CASE( "StreamFormatter-TestDOMParse", "[utility]" )
    {
        // Parse stream DOM from an example input using various different
        // character types
        InitStreamDOMForCharacterType<utf8>();
        InitStreamDOMForCharacterType<ucs2>();
        InitStreamDOMForCharacterType<ucs4>();
    }

    TEST_CASE( "StreamFormatter-ClassPropertiesPerformance", "[utility]" )
    {
        const unsigned iterationCount = 64 * 128;
        auto start = __rdtsc();
        RunPerformanceTest1(testString, iterationCount);
        auto middle = __rdtsc();
        RunPerformanceTest2(testString, iterationCount);
        auto end = __rdtsc();

        std::cout << "Properties based serialization: " << (middle-start) / iterationCount << " cycles per iteration." << std::endl;
        std::cout << "Old style serialization: " << (end-middle) / iterationCount << " cycles per iteration." << std::endl;
    }
}
