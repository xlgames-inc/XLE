// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Techniques.h"
#include "../Metal/Forward.h"
#include <functional>

namespace RenderCore { class InputElementDesc; class VertexBufferView; class UniformsStream; }

namespace RenderCore { namespace Techniques 
{
	class ParsingContext;

        //
        //  <summary>Vertex, constants and resources interface for a technique<summary>
        //
        //  Defines an input interface for a technique. Normally a client may only define
        //  a few "TechniqueInterface" objects, and reuse them with many techniques.
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
    class TechniqueInterface
    {
    public:
		uint64_t	GetHashValue() const;

        void		BindUniformsStream(unsigned streamIndex, const UniformsStreamInterface& interf);
		void		BindGlobalUniforms();

		TechniqueInterface(IteratorRange<const InputElementDesc*> vertexInputLayout);

        TechniqueInterface();        
        TechniqueInterface(TechniqueInterface&& moveFrom) never_throws;
        TechniqueInterface&operator=(TechniqueInterface&& moveFrom) never_throws;
        ~TechniqueInterface();

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        friend class ResolvedTechniqueShaders; // makes internal structure easier
    };

	class ResolvedShaderVariationSet
	{
	public:
		using ShaderFuture = ::Assets::FuturePtr<Metal::ShaderProgram>;
		ShaderFuture FindVariation(
			const TechniqueEntry& techEntry,
			const ParameterBox* globalState[ShaderSelectors::Source::Max]) const;

		using CreationFn = std::function<ShaderFuture(
			StringSection<> vsName,
			StringSection<> gsName,
			StringSection<> psName,
			StringSection<> defines)>;
		CreationFn _creationFn;

		ResolvedShaderVariationSet();
		~ResolvedShaderVariationSet();
	protected:
		mutable std::vector<std::pair<uint64_t, ::Assets::FuturePtr<Metal::ShaderProgram>>>		_filteredToResolved;
		mutable std::vector<std::pair<uint64_t, uint64_t>>										_globalToFiltered;

		ShaderFuture MakeShaderVariation(
			const TechniqueEntry& techEntry,
			const ParameterBox* globalState[ShaderSelectors::Source::Max]) const;
	};

    class ResolvedTechniqueShaders
    {
    public:
		::Assets::FuturePtr<Metal::ShaderProgram> FindVariation(
			int techniqueIndex,
			const ParameterBox* shaderSelectors[ShaderSelectors::Source::Max]) const;

		const Technique& GetTechnique() const { return *_technique; }

		ResolvedTechniqueShaders(const std::shared_ptr<Technique>& technique);
		~ResolvedTechniqueShaders();

		///////////////////////////////////////
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

        ResolvedShader  FindVariation(
            int techniqueIndex, 
            const ParameterBox* shaderSelectors[ShaderSelectors::Source::Max], 
            const TechniqueInterface& techniqueInterface) const;
		///////////////////////////////////////

		const ::Assets::DepValPtr& GetDependencyValidation();
		static void ConstructToFuture(
			::Assets::AssetFuture<ResolvedTechniqueShaders>& future,
			StringSection<::Assets::ResChar> techniqueName);

    private:
		class Entry : public ResolvedShaderVariationSet
		{
		public:
			class BoundShader
			{
			public:
				std::shared_ptr<Metal::ShaderProgram>		_shaderProgram;
				std::unique_ptr<Metal::BoundUniforms>		_boundUniforms;
				std::unique_ptr<Metal::BoundInputLayout>	_boundLayout;
			};
			mutable std::vector<std::pair<uint64_t, BoundShader>>									_filteredToBoundShader;
			
			ResolvedShader FindResolvedShaderVariation(
				const TechniqueEntry& techEntry,
				const ParameterBox* shaderSelectors[ShaderSelectors::Source::Max], 
				const TechniqueInterface& techniqueInterface) const;

			static BoundShader MakeBoundShader(
				const std::shared_ptr<Metal::ShaderProgram>& shader, 
				const TechniqueEntry& techEntry,
				const ParameterBox* shaderSelectors[ShaderSelectors::Source::Max],
				const TechniqueInterface& techniqueInterface);
			static ResolvedTechniqueShaders::ResolvedShader AsResolvedShader(uint64_t hash, const BoundShader&);
		};
        Entry	_entries[size_t(TechniqueIndex::Max)];

		std::shared_ptr<Technique> _technique;
    };
}}
