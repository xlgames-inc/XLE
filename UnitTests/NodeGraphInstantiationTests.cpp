// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "ReusableDataFiles.h"
#include "../Assets/IFileSystem.h"
#include "../ShaderParser/ShaderInstantiation.h"
#include "../Assets/AssetServices.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/OSFileSystem.h"
#include "../Assets/MountingTree.h"
#include "../Assets/MemoryFile.h"
#include "../Assets/AssetTraits.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/Log.h"
#include "../ConsoleRig/AttachablePtr.h"
#include "../Utility/Streams/FileUtils.h"
#include <CppUnitTest.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

static const char* s_complicatedGraphFile = R"--(
	import simple_example = "example.graph";
	import simple_example_dupe = "ut-data/example.graph";
	import example_perpixel = "example-perpixel.psh";
	import templates = "xleres/nodes/templates.sh";
	import conditions = "xleres/nodes/conditions.sh";

	GBufferValues Internal_PerPixel(VSOutput geo)
	{
		return example_perpixel::PerPixel(geo:geo).result;
	}

	GBufferValues Bind_PerPixel(VSOutput geo) implements templates::PerPixel
	{
		captures MaterialUniforms = ( float3 DiffuseColor );
		captures AnotherCaptures = ( float SecondaryCaptures );
		if "defined(SIMPLE_BIND)" return simple_example::Bind_PerPixel(geo:geo).result;
		if "!defined(SIMPLE_BIND)" return Internal_PerPixel(geo:geo).result;
	}

	bool Bind_EarlyRejectionTest(VSOutput geo) implements templates::EarlyRejectionTest
	{
		captures MaterialUniforms = ( float AlphaWeight = "0.5" );
		if "defined(ALPHA_TEST)" return conditions::LessThan(lhs:MaterialUniforms.AlphaWeight, rhs:"0.5").result;
		return "false";
	}
)--";

namespace UnitTests
{

	static std::unordered_map<std::string, ::Assets::Blob> s_utData {
		std::make_pair("example-perpixel.psh", ::Assets::AsBlob(s_examplePerPixelShaderFile)),
		std::make_pair("example.graph", ::Assets::AsBlob(s_exampleGraphFile)),
		std::make_pair("complicated.graph", ::Assets::AsBlob(s_complicatedGraphFile))
	};

	TEST_CLASS(NodeGraphInstantiationTests)
	{
	public:
		TEST_METHOD(InstantiateFromNodeGraphFile)
		{
			// Test with a simple example graph file
			{
				ShaderSourceParser::InstantiationRequest_ArchiveName instRequests[] {
					{ "ut-data/example.graph" }
				};

				auto inst = ShaderSourceParser::InstantiateShader(
					MakeIteratorRange(instRequests), 
					RenderCore::ShaderLanguage::GLSL);

				::Assert::AreNotEqual(inst._sourceFragments.size(), (size_t)0);		// ensure that we at least got some output
				::Assert::AreEqual(inst._entryPoints.size(), (size_t)1);
				::Assert::AreEqual(inst._entryPoints[0]._name, std::string{"Bind_PerPixel"});
				::Assert::AreEqual(inst._entryPoints[0]._implementsName, std::string{"PerPixel"});
			}

			// Test with a slightly more complicated graph file
			try {
				ShaderSourceParser::InstantiationRequest_ArchiveName instRequests[] {
					{ "ut-data/complicated.graph" }
				};

				instRequests[0]._selectors.SetParameter(u("SIMPLE_BIND"), 1);

				auto inst = ShaderSourceParser::InstantiateShader(
					MakeIteratorRange(instRequests), 
					RenderCore::ShaderLanguage::GLSL);

				::Assert::AreNotEqual(inst._sourceFragments.size(), (size_t)0);		// ensure that we at least got some output
				::Assert::AreEqual(inst._entryPoints.size(), (size_t)3);

				// We must have all of the following entry points, and "implements" behaviour
				const char* expectedEntryPoints[] = { "Internal_PerPixel", "Bind_PerPixel", "Bind_EarlyRejectionTest" };
				const char* expectedImplements[] = { "PerPixel", "EarlyRejectionTest" };

				for (const char* entryPoint:expectedEntryPoints) {
					auto i = std::find_if(
						inst._entryPoints.begin(), inst._entryPoints.end(),
						[entryPoint](const ShaderSourceParser::InstantiatedShader::EntryPoint& e) {
							return e._name == entryPoint;
						});
					::Assert::IsTrue(i != inst._entryPoints.end());
				}

				for (const char* entryPoint:expectedImplements) {
					auto i = std::find_if(
						inst._entryPoints.begin(), inst._entryPoints.end(),
						[entryPoint](const ShaderSourceParser::InstantiatedShader::EntryPoint& e) {
							return e._implementsName == entryPoint;
						});
					::Assert::IsTrue(i != inst._entryPoints.end());
				}

			} catch (const std::exception& e) {
				std::stringstream str;
				str << "Failed in complicated graph test, with exception message: " << e.what() << std::endl;
				::Assert::Fail(ToString(str.str()).c_str());
			}
		}

		static ConsoleRig::AttachablePtr<ConsoleRig::GlobalServices> _globalServices;
		static ConsoleRig::AttachablePtr<::Assets::Services> _assetServices;

		TEST_CLASS_INITIALIZE(Startup)
		{
			UnitTest_SetWorkingDirectory();
			_globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());
			::Assets::MainFileSystem::GetMountingTree()->Mount(u("xleres"), ::Assets::CreateFileSystem_OS(u("Game/xleres")));
			::Assets::MainFileSystem::GetMountingTree()->Mount(u("ut-data"), ::Assets::CreateFileSystem_Memory(s_utData));
			_assetServices = ConsoleRig::MakeAttachablePtr<::Assets::Services>(0);
		}

		TEST_CLASS_CLEANUP(Shutdown)
		{
			_assetServices.reset();
			_globalServices.reset();
		}
    };

	ConsoleRig::AttachablePtr<ConsoleRig::GlobalServices> NodeGraphInstantiationTests::_globalServices;
	ConsoleRig::AttachablePtr<::Assets::Services> NodeGraphInstantiationTests::_assetServices;
}


