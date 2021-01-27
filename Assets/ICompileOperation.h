// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"
#include "../Utility/IteratorUtils.h"
#include <vector>
#include <memory>

namespace Assets
{
	class DependentFileState;

	class ICompileOperation
	{
	public:
		struct TargetDesc
		{
			uint64_t		_type;
			const char*		_name;
		};
		struct SerializedArtifact
		{
			uint64_t		_type;
			unsigned		_version;
			std::string		_name;
			::Assets::Blob	_data;
		};
		virtual std::vector<TargetDesc>			GetTargets() const = 0;
		virtual std::vector<SerializedArtifact>	SerializeTarget(unsigned idx) = 0;
		virtual std::vector<DependentFileState> GetDependencies() const = 0;

		virtual ~ICompileOperation();
	};

	using IdentifiersList = IteratorRange<const StringSection<char>*>;
	using CreateCompileOperationFn = std::shared_ptr<ICompileOperation>(IdentifiersList identifier);

	// Compiler shared libraries -- 

	class ICompilerDesc
	{
	public:
		virtual std::string			Description() const = 0;

		class FileKind
		{
		public:
			IteratorRange<const uint64_t*>	_assetTypes;
			std::string						_regexFilter = nullptr;
			std::string						_name = nullptr;
			std::string						_extensionsForOpenDlg = nullptr;		// comma separated list of extensions for file-open-dialog scenarios
		};
		virtual unsigned			FileKindCount() const = 0;
		virtual FileKind			GetFileKind(unsigned index) const = 0;

		virtual ~ICompilerDesc();
	};

	using GetCompilerDescFn = std::shared_ptr<ICompilerDesc>();
}

