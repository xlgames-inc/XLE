// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"
#include "../Core/Types.h"
#include "../Utility/StringUtils.h"
#include "../Utility/IteratorUtils.h"
#include <vector>
#include <memory>

namespace Assets
{
	class DependentFileState;

	class ICompilerDesc
	{
	public:
		virtual const char*			Description() const = 0;

		class FileKind
		{
		public:
			IteratorRange<const uint64_t*>	_assetTypes;
			const ::Assets::ResChar*		_regexFilter = nullptr;
			const char*						_name = nullptr;
			const char*						_extensionsForOpenDlg = nullptr;		// comma separated list of extensions for file-open-dialog scenarios
		};
		virtual unsigned			FileKindCount() const = 0;
		virtual FileKind			GetFileKind(unsigned index) const = 0;

		virtual ~ICompilerDesc();
	};

	class ICompileOperation
	{
	public:
		struct TargetDesc
		{
			uint64_t		_type;
			const char*		_name;
		};
		struct OperationResult
		{
			uint64_t		_type;
			unsigned		_version;
			std::string		_name;
			::Assets::Blob	_data;
		};
		virtual std::vector<TargetDesc>			GetTargets() const = 0;
		virtual std::vector<OperationResult>	SerializeTarget(unsigned idx) = 0;
		virtual std::vector<DependentFileState> GetDependencies() const = 0;

		virtual ~ICompileOperation();
	};

	typedef std::shared_ptr<ICompilerDesc> GetCompilerDescFn();
	typedef std::shared_ptr<ICompileOperation> CreateCompileOperationFn(StringSection<::Assets::ResChar> identifier);
}

