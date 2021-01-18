// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ColladaConversion.h"
#include "../Assets/ICompileOperation.h"
#include "../ConsoleRig/AttachableLibrary.h"
#include "../ConsoleRig/AttachablePtr.h"
#include "../ConsoleRig/GlobalServices.h"
#include "../OSServices/Log.h"
#include "../Utility/IteratorUtils.h"

namespace ColladaConversion
{

	static uint64_t s_knownAssetTypes[] = { Type_Model, Type_RawMat, Type_Skeleton, Type_AnimationSet };
	static uint64_t s_animSetAssetTypes[] = { Type_AnimationSet };

	class CompilerDesc : public ::Assets::ICompilerDesc
	{
	public:
		const char*			Description() const { return "Compiler and converter for Collada asset files"; }

		virtual unsigned	FileKindCount() const { return 3; }
		virtual FileKind	GetFileKind(unsigned index) const
		{
			assert(index == 0 || index == 1 || index == 2);
			if (index == 0)
				return FileKind { MakeIteratorRange(s_knownAssetTypes), R"(.*\.dae)", "Collada XML asset", "dae" };

			if (index == 1)
				return FileKind { MakeIteratorRange(s_animSetAssetTypes), R"(.*\.daelst)", "Animation List", "daelst" };

			return FileKind { MakeIteratorRange(s_animSetAssetTypes), R"(.*[\\/]alldae)", "All collada animations in a directory", "folder" };
		}

		CompilerDesc() {}
		~CompilerDesc() {}
	};

	std::shared_ptr<::Assets::ICompilerDesc> GetCompilerDesc() 
	{
#pragma comment(linker, "/EXPORT:GetCompilerDesc=" __FUNCDNAME__)
		return std::make_shared<CompilerDesc>();
	}

}

extern "C" 
{

	dll_export ConsoleRig::LibVersionDesc GetVersionInformation()
	{
		return ConsoleRig::GetLibVersionDesc();
	}

	static ConsoleRig::AttachablePtr<ConsoleRig::GlobalServices> s_attachRef;

	dll_export void AttachLibrary(ConsoleRig::CrossModule& crossModule)
	{
		ConsoleRig::CrossModule::SetInstance(crossModule);
		s_attachRef = ConsoleRig::GetAttachablePtr<ConsoleRig::GlobalServices>();
		auto versionDesc = ConsoleRig::GetLibVersionDesc();
		Log(Verbose) << "Attached Collada Compiler DLL: {" << versionDesc._versionString << "} -- {" << versionDesc._buildDateString << "}" << std::endl;
	}

	dll_export void DetachLibrary()
	{
		s_attachRef.reset();
		ConsoleRig::CrossModule::ReleaseInstance();
	}

}
