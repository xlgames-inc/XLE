// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AssetUtils.h"
#include "../Utility/Streams/PreprocessorInterpreter.h"
#include <set>

namespace Assets
{
	class PreprocessorIncludeHandler : public Utility::IPreprocessorIncludeHandler
	{
	public:
		virtual Result OpenFile(
			StringSection<> requestString,
			StringSection<> fileIncludedFrom) override;
		DependencyValidation MakeDependencyValidation() const;
		PreprocessorIncludeHandler();
		~PreprocessorIncludeHandler();
		std::set<DependentFileState> _depFileStates;
	};
}
