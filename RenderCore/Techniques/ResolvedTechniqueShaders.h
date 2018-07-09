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
		uint64  GetHashValue() const;

        void	BindUniformsStream(unsigned streamIndex, const UniformsStreamInterface& interf);
		void	BindGlobalUniforms();

		TechniqueInterface(IteratorRange<const InputElementDesc*> vertexInputLayout);

        TechniqueInterface();        
        TechniqueInterface(TechniqueInterface&& moveFrom) never_throws;
        TechniqueInterface&operator=(TechniqueInterface&& moveFrom) never_throws;
        ~TechniqueInterface();

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        friend class ResolvedTechniqueInterfaceShaders; // makes internal structure easier
    };

	#if defined(_DEBUG)
		// #define CHECK_TECHNIQUE_HASH_CONFLICTS
    #endif

    class ResolvedTechniqueInterfaceShaders
    {
    public:
		class ResolvedShader
		{
		public:
			uint64                          _variationHash;
			Metal::ShaderProgram*           _shaderProgram;
			Metal::BoundUniforms*           _boundUniforms;
			Metal::BoundInputLayout*        _boundLayout;

			void Apply(
				Metal::DeviceContext& devContext,
				ParsingContext& parserContext,
				const std::initializer_list<VertexBufferView>& vbs) const;

			void ApplyUniforms(
				Metal::DeviceContext& context,
				unsigned streamIdx,
				const UniformsStream& stream) const;

			ResolvedShader();
		};

        ResolvedShader  FindVariation(
            int techniqueIndex, 
            const ParameterBox* globalState[ShaderSelectors::Source::Max], 
            const TechniqueInterface& techniqueInterface) const;

		const Technique& GetTechnique() const { return *_technique; }

		ResolvedTechniqueInterfaceShaders(const std::shared_ptr<Technique>& technique);
		~ResolvedTechniqueInterfaceShaders();

		const ::Assets::DepValPtr& GetDependencyValidation();
		static void ConstructToFuture(
			::Assets::AssetFuture<ResolvedTechniqueInterfaceShaders>& future,
			StringSection<::Assets::ResChar> techniqueName);

    private:
		class Entry
		{
		public:
			mutable std::vector<std::pair<uint64, ResolvedShader>>			_filteredToResolved;
			mutable std::vector<std::pair<uint64, ResolvedShader>>			_globalToResolved;
			mutable std::vector<std::shared_ptr<Metal::ShaderProgram>>		_resolvedShaderPrograms;
			mutable std::vector<std::unique_ptr<Metal::BoundUniforms>>		_resolvedBoundUniforms;
			mutable std::vector<std::unique_ptr<Metal::BoundInputLayout>>	_resolvedBoundInputLayouts;

			#if defined(CHECK_TECHNIQUE_HASH_CONFLICTS)
				class HashConflictTest
				{
				public:
					ParameterBox _globalState[ShaderSelectors::Source::Max];
					uint64 _rawHash; 
					uint64 _filteredHash; 
					uint64 _interfaceHash;

					HashConflictTest(const ParameterBox* globalState[ShaderSelectors::Source::Max], uint64 rawHash, uint64 filteredHash, uint64 interfaceHash);
					HashConflictTest(const ParameterBox globalState[ShaderSelectors::Source::Max], uint64 rawHash, uint64 filteredHash, uint64 interfaceHash);
					HashConflictTest();
				};
				mutable std::vector<std::pair<uint64, HashConflictTest>>  _localToResolvedTest;
				mutable std::vector<std::pair<uint64, HashConflictTest>>  _globalToResolvedTest;

				void TestHashConflict(
					const ParameterBox* globalState[ShaderSelectors::Source::Max], 
					const HashConflictTest& comparison) const;
			#endif

			void        ResolveAndBind( 
				ResolvedShader& shader, 
				const TechniqueEntry& techEntry,
				const ParameterBox* globalState[ShaderSelectors::Source::Max],
				const TechniqueInterface& techniqueInterface) const;
			ResolvedShader FindVariation(
				const TechniqueEntry& techEntry,
				const ParameterBox* globalState[ShaderSelectors::Source::Max], 
				const TechniqueInterface& techniqueInterface) const;
		};
        Entry	_entries[size_t(TechniqueIndex::Max)];

		std::shared_ptr<Technique> _technique;
    };
}}
