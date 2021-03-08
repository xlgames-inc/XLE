// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Techniques.h"
#include "../Metal/Forward.h"
#include <unordered_map>
#include <string>

namespace ShaderSourceParser
{
	class ManualSelectorFiltering;
	class SelectorFilteringRules;
}

namespace RenderCore { namespace Techniques 
{
	class ParsingContext;
	class Technique;
	class TechniqueEntry;

	/// <summary>Used with UniqueShaderVariationSet to provide customizability for shader construction</summary>
	class IShaderVariationFactory
	{
	public:
		uint64_t _factoryGuid = 0;

		virtual ::Assets::FuturePtr<Metal::ShaderProgram> MakeShaderVariation(
			StringSection<> defines) = 0;
		virtual ~IShaderVariationFactory();
	};

	/// <summary>Filters shader variation construction parameters to avoid construction of duplicate shaders</summary>
	///
	/// Sometimes 2 different sets of construction parameters for a shader can result in equivalent final byte code.
	/// Ideally we want to minimize the number of different shaders; so this object will filter construction parameters
	/// to attempt to identify though which will result in duplicates.
	///
	/// UniqueShaderVariationSet maintains a list of previously generated shaders, which can be reused as appropriate.
	class UniqueShaderVariationSet
	{
	public:
		struct FilteredSelectorSet
		{
			uint64_t _hashValue;
			std::string _selectors;
		};

		FilteredSelectorSet FilterSelectors(
			IteratorRange<const ParameterBox**> selectors,
			const ShaderSourceParser::ManualSelectorFiltering& techniqueFiltering,
			const ShaderSourceParser::SelectorFilteringRules& automaticFiltering);

		UniqueShaderVariationSet();
		~UniqueShaderVariationSet();
	protected:
		// std::vector<Variation>							_filteredToResolved;
		std::vector<std::pair<uint64_t, FilteredSelectorSet>>		_globalToFiltered;
	};

	/// <summary>Provides convenient management of shader variations generated from a technique file</summary>
    class TechniqueShaderVariationSet
    {
    public:
		::Assets::FuturePtr<Metal::ShaderProgram> FindVariation(
			int techniqueIndex,
			const ParameterBox* shaderSelectors[SelectorStages::Max]) const;

		const Technique& GetTechnique() const { return *_technique; }

		TechniqueShaderVariationSet(const std::shared_ptr<Technique>& technique);
		~TechniqueShaderVariationSet();

		///////////////////////////////////////

		const ::Assets::DepValPtr& GetDependencyValidation() const;
		static void ConstructToFuture(
			::Assets::AssetFuture<TechniqueShaderVariationSet>& future,
			StringSection<::Assets::ResChar> techniqueName);

    protected:
		UniqueShaderVariationSet _variationSet;
		std::shared_ptr<Technique> _technique;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class ShaderVariationFactory_Basic : public IShaderVariationFactory
	{
	public:
		::Assets::FuturePtr<Metal::ShaderProgram> MakeShaderVariation(StringSection<> defines);
		ShaderVariationFactory_Basic(const TechniqueEntry& entry, const std::shared_ptr<ICompiledPipelineLayout>& pipelineLayout);
		~ShaderVariationFactory_Basic();
	private:
		const TechniqueEntry* _entry;
		std::shared_ptr<ICompiledPipelineLayout> _pipelineLayout;
	};

}}
