// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../Assets/AssetsCore.h"
#include "../../Assets/IntermediateCompilers.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include "../../Utility/Streams/PreprocessorInterpreter.h"
#include "../../Utility/MemoryUtils.h"
#include <memory>

namespace Utility { class OutputStreamFormatter; }
namespace Assets { class ICompileOperation; class DirectorySearchRules; }

namespace RenderCore { namespace Techniques
{
	class ShaderSelectorFilteringRules
	{
	public:
		Utility::Internal::TokenDictionary _tokenDictionary;
		std::map<unsigned, Utility::Internal::ExpressionTokenList> _relevanceTable;
		std::map<unsigned, Utility::Internal::ExpressionTokenList> _defaultSets;

		static const auto CompileProcessType = ConstHash64<'Filt', 'erRu', 'les'>::Value;
		const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }

		friend void SerializationOperator(
			Utility::OutputStreamFormatter& formatter,
			const ShaderSelectorFilteringRules& input);

		ShaderSelectorFilteringRules(
			InputStreamFormatter<utf8>& formatter, 
			const ::Assets::DirectorySearchRules&,
			const ::Assets::DepValPtr& depVal);
		ShaderSelectorFilteringRules();
		~ShaderSelectorFilteringRules();

	private:
		::Assets::DepValPtr _depVal;
	};

	::Assets::IntermediateCompilers::CompilerRegistration RegisterShaderSelectorFilteringCompiler(
		::Assets::IntermediateCompilers& intermediateCompilers);

}}

