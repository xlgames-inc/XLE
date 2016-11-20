// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/AssetsCore.h"
#include "../Utility/UTFUtils.h"
#include "../Utility/StringUtils.h"
#include <vector>
#include <string>
#include <memory>

namespace Assets { class DependencyValidation; }

namespace Utility
{
	template<typename Type> class DocElementHelper;
	template<typename Type> class InputStreamFormatter;
}

namespace RenderCore { namespace ColladaConversion
{
	class BindingConfig
	{
	public:
		using String = std::basic_string<utf8>;

		String AsNative(StringSection<utf8> input) const;
		bool IsSuppressed(StringSection<utf8> input) const;

		BindingConfig(const DocElementHelper<InputStreamFormatter<utf8>>& source);
		BindingConfig();
		~BindingConfig();
	private:
		std::vector<std::pair<String, String>> _exportNameToBinding;
		std::vector<String> _bindingSuppressed;
	};

	class ImportConfiguration
	{
	public:
		const BindingConfig& GetResourceBindings() const { return _resourceBindings; }
		const BindingConfig& GetConstantBindings() const { return _constantsBindings; }
		const BindingConfig& GetVertexSemanticBindings() const { return _vertexSemanticBindings; }

		const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _depVal; }

		ImportConfiguration(const ::Assets::ResChar filename[]);
		ImportConfiguration();
		~ImportConfiguration();
	private:
		BindingConfig _resourceBindings;
		BindingConfig _constantsBindings;
		BindingConfig _vertexSemanticBindings;

		std::shared_ptr<::Assets::DependencyValidation> _depVal;
	};

}}

