// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Math/Vector.h"
#include "../Math/MathSerialization.h"
#include "../Utility/Streams/StreamFormatter.h"
#include "../Utility/Streams/StreamDOM.h"
#include "../Utility/Streams/SerializationUtils.h"
#include "../Utility/Conversion.h"
#include "../Utility/StringFormat.h"
#include "../Utility/ImpliedTyping.h"
#include <string>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using namespace Catch::literals;

namespace UnitTests
{
    struct TestClass
    {
        int _c1 = 1;
        int _c2 = 5;
        UInt2 _c3 = {2, 3};
    };

    void DeserializationOperator(const StreamDOMElement<InputStreamFormatter<utf8>>& str, TestClass& cls)
    {
        cls._c1 = str.Attribute("c1").As<int>().value();
        cls._c2 = str.Attribute("c2", 600);
        cls._c3 = str.Attribute("c3").As<UInt2>().value();
    }

    TEST_CASE( "StreamFormatter-BasicDeserialization", "[utility]" )
    {
        const std::basic_string<utf8> testString = (const utf8*)R"(c1 = 10; c2 = 50; c3 = {20, 30})";

        SECTION("Using StreamDOMElement::As() to invoke DeserializationOperator")
        {
            InputStreamFormatter<utf8> formatter(MakeStringSection(testString));
            StreamDOM<InputStreamFormatter<utf8>> doc(formatter);
            auto testClass = doc.RootElement().As<TestClass>();
            REQUIRE( testClass._c1 == 10 );
            REQUIRE( testClass._c2 == 50 );
            REQUIRE( testClass._c3 == UInt2(20, 30) );
        }

        SECTION("Using operator>> to invoke DeserializationOperator")
        {
            InputStreamFormatter<utf8> formatter(MakeStringSection(testString));
            StreamDOM<InputStreamFormatter<utf8>> doc(formatter);
            TestClass testClass;
             doc.RootElement() >> testClass;
            REQUIRE( testClass._c1 == 10 );
            REQUIRE( testClass._c2 == 50 );
            REQUIRE( testClass._c3 == UInt2(20, 30) );
        }
    }

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
    Texture0=grassTextureNo9227
    Texture1=tr_canyon_rock_700b_800b
    Texture2=tr_canyon_rock3d_708a
    Texture3=tr_canyon_rock3d_602b
    Texture4=grassTextureNo9227; Mapping={1.8f, 1f, 1f, 1f, 1f}

~GradFlagMaterial
    MaterialId=1u
    Texture0=ProcTexture
    Texture1=stonesTextureNo8648
    Texture2=tr_canyon_rock3d_409a
    Texture3=tr_canyon_rock3d_409a
    Texture4=gravelTextureNo7899; Mapping={1.8f, 1f, 1f, 1f, 1f}

~ProcTextureSetting; Name=ProcTexture; 
    Texture0=grassTextureNo7109
    Texture1=grassTextureNo6354; HGrid=5f; Gain=0.5f
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
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        //   S T R E A M   D O M   //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    static void DeserializationOperator(
        const Utility::StreamDOMElement<InputStreamFormatter<utf8>>& src,
        ExampleSerializableObject::StrataMaterial& mat)
    {
        static const utf8* TextureNames[] = { "Texture0", "Texture1", "Slopes" };

        mat._id = src.Attribute("MaterialId", 0u);
        for (const auto& c:src.Element("Strata").children()) {
            ExampleSerializableObject::StrataMaterial::Strata newStrata;
            for (unsigned t=0; t<dimof(TextureNames); ++t)
                newStrata._texture[t] = c.Attribute(TextureNames[t]).Value().Cast<char>().AsString();

            newStrata._endHeight = c.Attribute("EndHeight", 0.f);
            auto mappingConst = c.Attribute("Mapping", Float4(1.f, 1.f, 1.f, 1.f));
            newStrata._mappingConstant[0] = mappingConst[0];
            newStrata._mappingConstant[1] = mappingConst[1];
            newStrata._mappingConstant[2] = mappingConst[2];

            mat._strata.push_back(newStrata);
        }
    }

    static void DeserializationOperator(
        const Utility::StreamDOMElement<InputStreamFormatter<utf8>>& src,
        ExampleSerializableObject::GradFlagMaterial& mat)
    {
        mat._id = src.Attribute("MaterialId", 0);
            
        mat._texture[0] = src.Attribute("Texture0").Value().Cast<char>().AsString();
        mat._texture[1] = src.Attribute("Texture1").Value().Cast<char>().AsString();
        mat._texture[2] = src.Attribute("Texture2").Value().Cast<char>().AsString();
        mat._texture[3] = src.Attribute("Texture3").Value().Cast<char>().AsString();
        mat._texture[4] = src.Attribute("Texture4").Value().Cast<char>().AsString();

        // Manually parsing and converting into the array type we want
        // This is sort of a wordy approach to this; normally for vector & matrix types
        // we shouldn't need so many steps
        uint8_t buffer[512];
        auto mappingAttr = src.Attribute("Mapping").Value();
        auto parsedType = ImpliedTyping::Parse(
            mappingAttr,
            buffer, sizeof(buffer));
        ImpliedTyping::Cast(
            MakeIteratorRange(mat._mappingConstant), 
            ImpliedTyping::TypeDesc{ImpliedTyping::TypeCat::Float, dimof(mat._mappingConstant)},
            MakeIteratorRange(buffer), parsedType);
    }

    static void DeserializationOperator(
        const Utility::StreamDOMElement<InputStreamFormatter<utf8>>& src,
        ExampleSerializableObject::ProcTextureSetting& mat)
    {
        mat._name = src.Attribute("Name").Value().Cast<char>().AsString();
        mat._texture[0] = src.Attribute("Texture0").Value().Cast<char>().AsString();
        mat._texture[1] = src.Attribute("Texture1").Value().Cast<char>().AsString();
        mat._hgrid = src.Attribute("HGrid", mat._hgrid);
        mat._gain = src.Attribute("Gain", mat._gain);
    }

    static void DeserializationOperator(
        const Utility::StreamDOMElement<InputStreamFormatter<utf8>>& doc,
        ExampleSerializableObject& result)
    {
        for (const auto& matCfg:doc.children()) {
            if (XlEqString(matCfg.Name(), "StrataMaterial")) {
                result._strataMaterials.push_back(matCfg.As<ExampleSerializableObject::StrataMaterial>());
            } else if (XlEqString(matCfg.Name(), "GradFlagMaterial")) {
                result._gradFlagMaterials.push_back(matCfg.As<ExampleSerializableObject::GradFlagMaterial>());
            } else if (XlEqString(matCfg.Name(), "ProcTextureSetting")) {
                result._procTextures.push_back(matCfg.As<ExampleSerializableObject::ProcTextureSetting>());
            }
        }

        result._diffuseDims = doc.Attribute("DiffuseDims", result._diffuseDims);
        result._normalDims = doc.Attribute("NormalDims", result._normalDims);
        result._paramDims = doc.Attribute("ParamDims", result._paramDims);
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        //   F O R M A T T E R   P A R S I N G   //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    static void DeserializationOperator(
        InputStreamFormatter<utf8>& formatter,
        ExampleSerializableObject::StrataMaterial& result)
    {
        static const utf8* TextureNames[] = { "Texture0", "Texture1", "Slopes" };
        
        for (;;) {
            using Blob = InputStreamFormatter<utf8>::Blob;
            switch (formatter.PeekNext()) {
            case Blob::AttributeName:
                {
                    InputStreamFormatter<utf8>::InteriorSection name, value;
                    if (!formatter.TryAttribute(name, value))
                        Throw(FormatException("Error in attribute declaration", formatter.GetLocation()));

                    if (XlEqString(name, "MaterialId")) {
                        result._id = ImpliedTyping::Parse<decltype(result._id)>(value).value();
                    } else
                        Throw(FormatException("Unknown attribute encountered", formatter.GetLocation()));
                    break;
                }

            case Blob::BeginElement:
                {
                    // We should have one element called "Strata" with an array of elements within it
                    InputStreamFormatter<utf8>::InteriorSection eleName;
                    if (!formatter.TryBeginElement(eleName))
                        Throw(FormatException("Error in element declaration", formatter.GetLocation()));
                    if (!XlEqString(eleName, "Strata"))
                        Throw(FormatException("Unknown element encountered", formatter.GetLocation()));

                    while (formatter.PeekNext() == Blob::BeginElement) {
                        if (!formatter.TryBeginElement(eleName))
                            Throw(FormatException("Error in element declaration", formatter.GetLocation()));

                        ExampleSerializableObject::StrataMaterial::Strata newStrata;
                        while (formatter.PeekNext() == Blob::AttributeName) {
                            InputStreamFormatter<utf8>::InteriorSection name, value;
                            if (!formatter.TryAttribute(name, value))
                                Throw(FormatException("Error in attribute declaration", formatter.GetLocation()));

                            if (XlEqString(name, "EndHeight")) {
                                newStrata._endHeight = ImpliedTyping::Parse<decltype(newStrata._endHeight)>(value).value();
                            } else if (XlEqString(name, "Mapping")) {
                                auto mappingConst = ImpliedTyping::Parse<Float4>(value).value();
                                newStrata._mappingConstant[0] = mappingConst[0];
                                newStrata._mappingConstant[1] = mappingConst[1];
                                newStrata._mappingConstant[2] = mappingConst[2];
                            } else {
                                unsigned t=0;
                                for (; t<dimof(TextureNames); ++t)
                                    if (XlEqString(name, TextureNames[t])) {
                                        newStrata._texture[t] = value.Cast<char>().AsString();
                                        break;
                                    }
                                if (t == dimof(TextureNames))
                                    Throw(FormatException("Unknown attribute encountered", formatter.GetLocation()));
                            }
                        }
                        result._strata.push_back(newStrata);

                        if (!formatter.TryEndElement())
                            Throw(FormatException("Expected end element", formatter.GetLocation()));
                    }

                    if (!formatter.TryEndElement())
                        Throw(FormatException("Expected end element", formatter.GetLocation()));
                    break;
                }

            case Blob::AttributeValue:
            case Blob::CharacterData:
                Throw(FormatException("Unexpected blob", formatter.GetLocation()));

            case Blob::EndElement:
            case Blob::None:
                return;
            default:
                assert(0);
            }
        }
    }
    
    static void DeserializationOperator(
        InputStreamFormatter<utf8>& formatter,
        ExampleSerializableObject::GradFlagMaterial& result)
    {
        for (;;) {
            using Blob = InputStreamFormatter<utf8>::Blob;
            switch (formatter.PeekNext()) {
            case Blob::AttributeName:
                {
                    InputStreamFormatter<utf8>::InteriorSection name, value;
                    if (!formatter.TryAttribute(name, value))
                        Throw(FormatException("Error in attribute declaration", formatter.GetLocation()));

                    if (XlEqString(name, "MaterialId")) {
                        result._id = ImpliedTyping::Parse<decltype(result._id)>(value).value();
                    } else if (XlEqString(name, "Texture0")) {
                        result._texture[0] = value.Cast<char>().AsString();
                    } else if (XlEqString(name, "Texture1")) {
                        result._texture[1] = value.Cast<char>().AsString();
                    } else if (XlEqString(name, "Texture2")) {
                        result._texture[2] = value.Cast<char>().AsString();
                    } else if (XlEqString(name, "Texture3")) {
                        result._texture[3] = value.Cast<char>().AsString();
                    } else if (XlEqString(name, "Texture4")) {
                        result._texture[4] = value.Cast<char>().AsString();
                    } else if (XlEqString(name, "Mapping")) {
                        uint8_t buffer[512];
                        auto parsedType = ImpliedTyping::Parse(
                            value,
                            buffer, sizeof(buffer));
                        ImpliedTyping::Cast(
                            MakeIteratorRange(result._mappingConstant), 
                            ImpliedTyping::TypeDesc{ImpliedTyping::TypeCat::Float, dimof(result._mappingConstant)},
                            MakeIteratorRange(buffer), parsedType);
                    } else
                        Throw(FormatException("Unknown attribute encountered", formatter.GetLocation()));
                    break;
                }

            case Blob::BeginElement:
            case Blob::AttributeValue:
            case Blob::CharacterData:
                Throw(FormatException("Unexpected blob", formatter.GetLocation()));

            case Blob::EndElement:
            case Blob::None:
                return;
            default:
                assert(0);
            }
        }
    }
    
    static void DeserializationOperator(
        InputStreamFormatter<utf8>& formatter,
        ExampleSerializableObject::ProcTextureSetting& result)
    {
        for (;;) {
            using Blob = InputStreamFormatter<utf8>::Blob;
            switch (formatter.PeekNext()) {
            case Blob::AttributeName:
                {
                    InputStreamFormatter<utf8>::InteriorSection name, value;
                    if (!formatter.TryAttribute(name, value))
                        Throw(FormatException("Error in attribute declaration", formatter.GetLocation()));

                    if (XlEqString(name, "Name")) {
                        result._name = value.Cast<char>().AsString();
                    } else if (XlEqString(name, "Texture0")) {
                        result._texture[0] = value.Cast<char>().AsString();
                    } else if (XlEqString(name, "Texture1")) {
                        result._texture[1] = value.Cast<char>().AsString();
                    } else if (XlEqString(name, "HGrid")) {
                        result._hgrid = ImpliedTyping::Parse<decltype(result._hgrid)>(value).value();
                    } else if (XlEqString(name, "Gain")) {
                        result._gain = ImpliedTyping::Parse<decltype(result._gain)>(value).value();
                    } else
                        Throw(FormatException("Unknown attribute encountered", formatter.GetLocation()));
                    break;
                }

            case Blob::BeginElement:
            case Blob::AttributeValue:
            case Blob::CharacterData:
                Throw(FormatException("Unexpected blob", formatter.GetLocation()));

            case Blob::EndElement:
            case Blob::None:
                return;
            default:
                assert(0);
            }
        }
    }
    
    static void DeserializationOperator(
        InputStreamFormatter<utf8>& formatter,
        ExampleSerializableObject& result)
    {
        for (;;) {
            using Blob = InputStreamFormatter<utf8>::Blob;
            switch (formatter.PeekNext()) {
            case Blob::AttributeName:
                {
                    InputStreamFormatter<utf8>::InteriorSection name, value;
                    if (!formatter.TryAttribute(name, value))
                        Throw(FormatException("Error in attribute declaration", formatter.GetLocation()));

                    if (XlEqString(name, "DiffuseDims")) {
                        result._diffuseDims = ImpliedTyping::Parse<UInt2>(value).value();
                    } else if (XlEqString(name, "NormalDims")) {
                        result._normalDims = ImpliedTyping::Parse<UInt2>(value).value();
                    } else if (XlEqString(name, "ParamDims")) {
                        result._paramDims = ImpliedTyping::Parse<UInt2>(value).value();
                    } else
                        Throw(FormatException("Unknown attribute encountered", formatter.GetLocation()));
                    break;
                }

            case Blob::BeginElement:
                {
                    InputStreamFormatter<utf8>::InteriorSection eleName;
                    if (!formatter.TryBeginElement(eleName))
                        Throw(FormatException("Error in element begin", formatter.GetLocation()));

                    if (XlEqString(eleName, "StrataMaterial")) {
                        ExampleSerializableObject::StrataMaterial newMaterial;
                        formatter >> newMaterial;
                        result._strataMaterials.push_back(std::move(newMaterial));
                    } else if (XlEqString(eleName, "GradFlagMaterial")) {
                        ExampleSerializableObject::GradFlagMaterial newMaterial;
                        formatter >> newMaterial;
                        result._gradFlagMaterials.push_back(std::move(newMaterial));
                    } else if (XlEqString(eleName, "ProcTextureSetting")) {
                        ExampleSerializableObject::ProcTextureSetting newMaterial;
                        formatter >> newMaterial;
                        result._procTextures.push_back(std::move(newMaterial));
                    } else
                        Throw(FormatException("Unknown element encountered", formatter.GetLocation()));

                    if (!formatter.TryEndElement())
                        Throw(FormatException("Expected end element", formatter.GetLocation()));
                    break;
                }

            case Blob::AttributeValue:
            case Blob::CharacterData:
                Throw(FormatException("Unexpected blob", formatter.GetLocation()));

            case Blob::EndElement:
            case Blob::None:
                return;
            default:
                assert(0);
            }
        }
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    static no_inline void RunPerformanceTest1(
        const std::basic_string<utf8>& testString, unsigned iterationCount)
    {
        for (unsigned c=0; c<iterationCount; ++c) {
            InputStreamFormatter<utf8> formatter{MakeStringSection(testString)};
            ExampleSerializableObject matConfig;
            formatter >> matConfig;
            (void)matConfig;
        }
    }

    static no_inline void RunPerformanceTest2(
        const std::basic_string<utf8>& testString, unsigned iterationCount)
    {
        for (unsigned c=0; c<iterationCount; ++c) {
            InputStreamFormatter<utf8> formatter(MakeStringSection(testString));
            StreamDOM<InputStreamFormatter<utf8>> doc(formatter);
            ExampleSerializableObject matConfig;
            doc.RootElement() >> matConfig;
            (void)matConfig;
        }
    }

    TEST_CASE( "StreamFormatter-BasicDOMInterface", "[utility]" )
    {
        // Load an aribtrary file, and use the basic interface of the StreamDOM<> class 
        InputStreamFormatter<utf8> formatter(MakeStringSection(testString));
        StreamDOM<InputStreamFormatter<utf8>> doc(formatter);
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

    template<typename CharType>
        void InitStreamDOMForCharacterType()
    {
        auto converted = Conversion::Convert<std::basic_string<CharType>>(testString2);
        InputStreamFormatter<CharType> formatter(MakeStringSection(converted));
        StreamDOM<InputStreamFormatter<CharType>> doc(formatter);
    }

    TEST_CASE( "StreamFormatter-TestDOMParse", "[utility]" )
    {
        // Parse stream DOM from an example input using various different
        // character types
        InitStreamDOMForCharacterType<utf8>();
    }

    TEST_CASE( "StreamFormatter-ClassPropertiesPerformance", "[utility]" )
    {
        const unsigned iterationCount = 64 * 128;
        auto start = __rdtsc();
        RunPerformanceTest1(testString, iterationCount);
        auto middle = __rdtsc();
        RunPerformanceTest2(testString, iterationCount);
        auto end = __rdtsc();

        std::cout << "InputStreamFormatter based deserialization: " << (middle-start) / iterationCount << " cycles per iteration (" << (middle-start) / iterationCount / testString.size() << " cycles per character)." << std::endl;
        std::cout << "StreamDOM based deserialization: " << (end-middle) / iterationCount << " cycles per iteration (" << (end-middle) / iterationCount / testString.size() << " cycles per character)." << std::endl;
    }
}
