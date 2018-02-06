// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "CommonBindings.h"     // for TechniqueIndex::Max
#include "PredefinedCBLayout.h"
#include "../Metal/Forward.h"
#include "../Types_Forward.h"
#include "../../Assets/AssetsCore.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/ParameterBox.h"
#include "../../Core/Prefix.h"
#include "../../Core/Types.h"
#include <string>
#include <vector>

namespace Utility { template<typename CharType> class InputStreamFormatter; }
using namespace Utility;
namespace Assets { class DependencyValidation; class DirectorySearchRules; }
namespace RenderCore { class VertexBufferView; class UniformsStreamInterface; class UniformsStream; }

namespace RenderCore { namespace Techniques
{
    class IStateSetResolver;

    class ShaderParameters
    {
    public:
        struct Source { enum Enum { Geometry, GlobalEnvironment, Runtime, Material, Max }; };
        ParameterBox    _parameters[Source::Max];

        uint64      CalculateFilteredHash(uint64 inputHash, const ParameterBox* globalState[Source::Max]) const;
        void        BuildStringTable(std::vector<std::pair<const utf8*, std::string>>& defines) const;

    private:
        uint64      CalculateFilteredHash(const ParameterBox* globalState[Source::Max]) const;
        mutable std::vector<std::pair<uint64, uint64>>  _globalToFilteredTable;

        friend class Technique;
    };

        //////////////////////////////////////////////////////////////////
            //      T E C H N I Q U E                               //
        //////////////////////////////////////////////////////////////////

            //      "technique" is a way to select a correct shader
            //      in a data-driven way. The code provides a technique
            //      index and a set of parameters in ParameterBoxes
            //          -- that is transformed into a concrete shader

    class ParsingContext;

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
        void    BindUniformsStream(unsigned streamIndex, const UniformsStreamInterface& interf);
        uint64  GetHashValue() const;

		TechniqueInterface(InputLayout vertexInputLayout);

        TechniqueInterface();        
        TechniqueInterface(TechniqueInterface&& moveFrom) never_throws;
        TechniqueInterface&operator=(TechniqueInterface&& moveFrom) never_throws;
        ~TechniqueInterface();

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        friend class ShaderType; // makes internal structure easier
    };

    // #if defined(_DEBUG)
    //     #define CHECK_TECHNIQUE_HASH_CONFLICTS
    // #endif

    class Technique
    {
    public:
        bool IsValid() const { return !_vertexShaderName.empty(); }
        void MergeIn(const Technique& source);
		void ReplaceSelfReference(StringSection<::Assets::ResChar> filename);

        ShaderParameters    _baseParameters;
        ::Assets::rstring   _vertexShaderName;
        ::Assets::rstring   _pixelShaderName;
        ::Assets::rstring   _geometryShaderName;

        Technique(Technique&&) never_throws = default;
        Technique& operator=(Technique&&) never_throws = default;
		Technique(const Technique&) = default;
        Technique& operator=(const Technique&) = default;
        Technique();
        ~Technique();
    };

	class TechniqueSetFile
	{
	public:
		std::vector<std::pair<uint64_t, Technique>> _settings;
		const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }

		TechniqueSetFile(
            Utility::InputStreamFormatter<utf8>& formatter, 
			const ::Assets::DirectorySearchRules& searchRules, 
			const ::Assets::DepValPtr& depVal);
		~TechniqueSetFile();
	private:
		::Assets::DepValPtr _depVal;

		Technique ParseTechnique(Utility::InputStreamFormatter<utf8>& formatter, const ::Assets::DirectorySearchRules& searchRules, std::vector<::Assets::DepValPtr> inherited);
		void LoadInheritedParameterBoxes(
			Technique& dst,
			Utility::InputStreamFormatter<utf8>& formatter,
			const ::Assets::DirectorySearchRules& searchRules,
			std::vector<::Assets::DepValPtr> inherited);
	};

    class ShaderType
    {
    public:
        ResolvedShader  FindVariation(
            int techniqueIndex, 
            const ParameterBox* globalState[ShaderParameters::Source::Max], 
            const TechniqueInterface& techniqueInterface) const;

        auto GetDependencyValidation() const -> const ::Assets::DepValPtr& { return _validationCallback; }
		const PredefinedCBLayout& TechniqueCBLayout() const { return _cbLayout; }

        ShaderType(StringSection<::Assets::ResChar> resourceName);
        ~ShaderType();
    private:
		class TechniqueObj
		{
		public:
			Technique _technique;
			mutable std::vector<std::pair<uint64, ResolvedShader>>			_filteredToResolved;
			mutable std::vector<std::pair<uint64, ResolvedShader>>			_globalToResolved;
			mutable std::vector<std::unique_ptr<Metal::ShaderProgram>>		_resolvedShaderPrograms;
			mutable std::vector<std::unique_ptr<Metal::BoundUniforms>>		_resolvedBoundUniforms;
			mutable std::vector<std::unique_ptr<Metal::BoundInputLayout>>	_resolvedBoundInputLayouts;

			#if defined(CHECK_TECHNIQUE_HASH_CONFLICTS)
				class HashConflictTest
				{
				public:
					ParameterBox _globalState[ShaderParameters::Source::Max];
					uint64 _rawHash; 
					uint64 _filteredHash; 
					uint64 _interfaceHash;

					HashConflictTest(const ParameterBox* globalState[ShaderParameters::Source::Max], uint64 rawHash, uint64 filteredHash, uint64 interfaceHash);
					HashConflictTest(const ParameterBox globalState[ShaderParameters::Source::Max], uint64 rawHash, uint64 filteredHash, uint64 interfaceHash);
					HashConflictTest();
				};
				mutable std::vector<std::pair<uint64, HashConflictTest>>  _localToResolvedTest;
				mutable std::vector<std::pair<uint64, HashConflictTest>>  _globalToResolvedTest;

				void TestHashConflict(
					const ParameterBox* globalState[ShaderParameters::Source::Max], 
					const HashConflictTest& comparison) const;
			#endif

			void        ResolveAndBind( 
				ResolvedShader& shader, 
				const ParameterBox* globalState[ShaderParameters::Source::Max],
				const TechniqueInterface& techniqueInterface) const;
			ResolvedShader FindVariation(  
				const ParameterBox* globalState[ShaderParameters::Source::Max], 
				const TechniqueInterface& techniqueInterface) const;
		};
        TechniqueObj			_techniques[size_t(TechniqueIndex::Max)];
        ::Assets::DepValPtr		_validationCallback;
		PredefinedCBLayout		_cbLayout;

        void ParseConfigFile(
            InputStreamFormatter<utf8>& formatter, 
			StringSection<::Assets::ResChar> containingFileName,
            const ::Assets::DirectorySearchRules& searchRules,
            std::vector<std::shared_ptr<::Assets::DependencyValidation>>& inheritedAssets);
    };

        //////////////////////////////////////////////////////////////////
            //      C O N T E X T                                   //
        //////////////////////////////////////////////////////////////////
    
    class TechniqueContext
    {
    public:
        ParameterBox   _globalEnvironmentState;
        ParameterBox   _runtimeState;

        std::shared_ptr<ParameterBox> _stateSetEnvironment;
        std::shared_ptr<IStateSetResolver> _defaultStateSetResolver;

        TechniqueContext();
        static void     BindGlobalUniforms(TechniqueInterface&);
		static const UniformsStreamInterface& GetGlobalUniformsStreamInterface();

        static const unsigned CB_GlobalTransform = 0;
        static const unsigned CB_GlobalState = 1;
        static const unsigned CB_ShadowProjection = 2;
        static const unsigned CB_OrthoShadowProjection = 3;
        static const unsigned CB_BasicLightingEnvironment = 4;
    };

}}

