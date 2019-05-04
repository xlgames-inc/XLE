// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Techniques.h"
#include "../Metal/Forward.h"

namespace RenderCore { class InputElementDesc; class VertexBufferView; class UniformsStream; }

namespace RenderCore { namespace Techniques 
{
	class ParsingContext;
	class ShaderSelectors;
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
		using ShaderFuture = ::Assets::FuturePtr<Metal::ShaderProgram>;
		struct Variation
		{
			uint64_t		_variationHash;
			ShaderFuture	_shaderFuture;
		};
		const Variation& FindVariation(
			const ShaderSelectors& baseSelectors,
			const ParameterBox* globalState[ShaderSelectors::Source::Max],
			IShaderVariationFactory& factory) const;

		UniqueShaderVariationSet();
		~UniqueShaderVariationSet();
	protected:
		mutable std::vector<Variation>							_filteredToResolved;
		mutable std::vector<std::pair<uint64_t, uint64_t>>		_globalToFiltered;

		ShaderFuture MakeShaderVariation(
			const ShaderSelectors& baseSelectors,
			const ParameterBox* globalState[ShaderSelectors::Source::Max],
			IShaderVariationFactory& factory) const;
	};

	/// <summary>Provides convenient access to shader variations generated from a technique file</summary>
    class TechniqueShaderVariationSet
    {
    public:
		::Assets::FuturePtr<Metal::ShaderProgram> FindVariation(
			int techniqueIndex,
			const ParameterBox* shaderSelectors[ShaderSelectors::Source::Max]) const;

		const Technique& GetTechnique() const { return *_technique; }

		TechniqueShaderVariationSet(const std::shared_ptr<Technique>& technique);
		~TechniqueShaderVariationSet();

		///////////////////////////////////////

		const ::Assets::DepValPtr& GetDependencyValidation();
		static void ConstructToFuture(
			::Assets::AssetFuture<TechniqueShaderVariationSet>& future,
			StringSection<::Assets::ResChar> techniqueName);

    protected:
		UniqueShaderVariationSet _variationSet;
		

		std::shared_ptr<Technique> _technique;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		//
        //  <summary>Vertex, constants and resources interface for a technique<summary>
        //
        //  Defines an input interface for a technique. Normally a client may only define
        //  a few "TechniquePrebindingInterface" objects, and reuse them with many techniques.
        //
        //  The interface consists of:
        //      * vertex input layout
        //          - input vertex format (position of elements in the vertex buffer)
        //      * constant buffers
        //          - constant buffer binding locations
        //      * shader resources
        //          - shader resources binding locations
        //
        //  The constants and shader resources bindings define how the "BoundUniforms" object in the ResolvedShader
        //  is created. For example, if you bind a constant buffer to slot 0, then you can set it by passing the constant
        //  buffer in slot 0 to ResolvedShader::_boundUniforms->Apply(...).
        //
        //  When calling BindConstantBuffer or BindShaderResource, the first parameter
        //  is a hash value of the name in the shader. For example, if the shader has:
        //      Texture2D<float> Depths;
        //
        //  Then you can pass:
        //      BindShaderResource(Hash64("Depth"), interfaceSlotIndex);
        //
    class TechniquePrebindingInterface
    {
    public:
		uint64_t	GetHashValue() const;

        void		BindUniformsStream(unsigned streamIndex, const UniformsStreamInterface& interf);
		void		BindGlobalUniforms();

		TechniquePrebindingInterface(IteratorRange<const InputElementDesc*> vertexInputLayout);

        TechniquePrebindingInterface();        
        TechniquePrebindingInterface(TechniquePrebindingInterface&& moveFrom) never_throws;
        TechniquePrebindingInterface&operator=(TechniquePrebindingInterface&& moveFrom) never_throws;
        ~TechniquePrebindingInterface();

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        friend class BoundShaderVariationSet; // makes internal structure easier
    };

	/// <summary>Like TechniqueShaderVariationSet, but also caches objects generated from a TechniquePrebindingInterface</summary>
	class BoundShaderVariationSet : public TechniqueShaderVariationSet
    {
	public:
		class ResolvedShader
		{
		public:
			uint64_t						_variationHash = 0;
			Metal::ShaderProgram*           _shaderProgram = nullptr;
			Metal::BoundUniforms*           _boundUniforms = nullptr;
			Metal::BoundInputLayout*        _boundLayout = nullptr;

			void Apply(
				Metal::DeviceContext& devContext,
				ParsingContext& parserContext,
				const std::initializer_list<VertexBufferView>& vbs) const;

			void ApplyUniforms(
				Metal::DeviceContext& context,
				unsigned streamIdx,
				const UniformsStream& stream) const;
		};

		ResolvedShader FindVariation(
			int techniqueIndex, 
			const ParameterBox* shaderSelectors[ShaderSelectors::Source::Max], 
			const TechniquePrebindingInterface& techniqueInterface) const;

		BoundShaderVariationSet(const std::shared_ptr<Technique>& technique);
		~BoundShaderVariationSet();

		static void ConstructToFuture(
			::Assets::AssetFuture<BoundShaderVariationSet>& future,
			StringSection<::Assets::ResChar> techniqueName);

	protected:
		class Entry
		{
		public:
			class BoundShader
			{
			public:
				std::shared_ptr<Metal::ShaderProgram>		_shaderProgram;
				std::unique_ptr<Metal::BoundUniforms>		_boundUniforms;
				std::unique_ptr<Metal::BoundInputLayout>	_boundLayout;
			};
			mutable std::vector<std::pair<uint64_t, BoundShader>> _boundShaders;
			
			static BoundShader MakeBoundShader(
				const std::shared_ptr<Metal::ShaderProgram>& shader, 
				const TechniqueEntry& techEntry,
				const ParameterBox* shaderSelectors[ShaderSelectors::Source::Max],
				const TechniquePrebindingInterface& techniqueInterface);
			static ResolvedShader AsResolvedShader(uint64_t hash, const BoundShader&);
		};
        Entry	_entries[size_t(TechniqueIndex::Max)];
	};
}}
