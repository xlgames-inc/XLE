// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PreprocessorIncludeHandler.h"
#include "DepVal.h"
#include "IFileSystem.h"
#include <stdexcept>

namespace Assets
{
	auto PreprocessorIncludeHandler::OpenFile(
		StringSection<> requestString,
		StringSection<> fileIncludedFrom) -> Result
	{
		Result result;
		if (!fileIncludedFrom.IsEmpty()) {
			char resolvedFileT[MaxPath];
			::Assets::DefaultDirectorySearchRules(fileIncludedFrom).ResolveFile(resolvedFileT, requestString);
			result._filename = resolvedFileT;
		} else {
			result._filename = requestString.AsString();
		}

		::Assets::DependentFileState mainFileState;
		
		result._fileContents = ::Assets::TryLoadFileAsMemoryBlock_TolerateSharingErrors(result._filename, &result._fileContentsSize, &mainFileState);
		if (!result._fileContentsSize) {
			if (!fileIncludedFrom.IsEmpty())
				Throw(std::runtime_error("Missing or empty file when loading: " + result._filename + " (included from: " + fileIncludedFrom.AsString() + ")"));
			Throw(std::runtime_error("Missing or empty file when loading: " + result._filename));
		}
		assert(!mainFileState._filename.empty());
		_depFileStates.insert(mainFileState);
		return result;
	}

	DepValPtr PreprocessorIncludeHandler::MakeDependencyValidation() const
	{
		auto result = std::make_shared<DependencyValidation>();
		for (const auto& i:_depFileStates)
			::Assets::RegisterFileDependency(result, MakeStringSection(i._filename));
		return result;
	}

	PreprocessorIncludeHandler::PreprocessorIncludeHandler() {}
	PreprocessorIncludeHandler::~PreprocessorIncludeHandler() {}
}
