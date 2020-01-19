// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "CommonBindings.h"     // for TechniqueIndex::Max
#include "../Assets/PredefinedCBLayout.h"
#include "../Init.h"
#include "../../Utility/ParameterBox.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace RenderCore { class UniformsStreamInterface; class IThreadContext; }

namespace RenderCore { namespace Techniques
{
    class IRenderStateDelegate;

    class ShaderSelectorFiltering
    {
    public:
        struct Source { enum Enum { Geometry, GlobalEnvironment, Runtime, Material, Max }; };

		ParameterBox    _setValues;
		std::unordered_map<std::string, std::string> _relevanceMap;

		uint64_t GetHash() const { return _hash; }

		void GenerateHash();
		uint64_t _hash = 0ull;
    };

        //////////////////////////////////////////////////////////////////
            //      T E C H N I Q U E                               //
        //////////////////////////////////////////////////////////////////

            //      "technique" is a way to select a correct shader
            //      in a data-driven way. The code provides a technique
            //      index and a set of parameters in ParameterBoxes
            //          -- that is transformed into a concrete shader

	class TechniqueEntry
    {
    public:
        bool IsValid() const { return !_vertexShaderName.empty(); }
        void MergeIn(const TechniqueEntry& source);

        ShaderSelectorFiltering		_selectorFiltering;
        ::Assets::rstring   _vertexShaderName;
        ::Assets::rstring   _pixelShaderName;
        ::Assets::rstring   _geometryShaderName;
		uint64_t			_shaderNamesHash = 0;		// hash of the shader names, but not _baseSelectors

		void GenerateHash();
    };

	class TechniqueSetFile
	{
	public:
		std::vector<std::pair<uint64_t, TechniqueEntry>> _settings;
		const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }

		const TechniqueEntry* FindEntry(uint64_t hashName) const;

		TechniqueSetFile(
            Utility::InputStreamFormatter<utf8>& formatter, 
			const ::Assets::DirectorySearchRules& searchRules, 
			const ::Assets::DepValPtr& depVal);
		~TechniqueSetFile();
	private:
		::Assets::DepValPtr _depVal;
	};

	DEPRECATED_ATTRIBUTE class Technique
	{
	public:
		auto GetDependencyValidation() const -> const ::Assets::DepValPtr& { return _validationCallback; }
		const RenderCore::Assets::PredefinedCBLayout& TechniqueCBLayout() const { return _cbLayout; }
		TechniqueEntry& GetEntry(unsigned idx);
		const TechniqueEntry& GetEntry(unsigned idx) const;

        Technique(StringSection<::Assets::ResChar> resourceName);
        ~Technique();
	private:
		TechniqueEntry			_entries[size_t(TechniqueIndex::Max)];

		::Assets::DepValPtr		_validationCallback;
		RenderCore::Assets::PredefinedCBLayout		_cbLayout;

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
        ParameterBox							_globalEnvironmentState;
        std::shared_ptr<IRenderStateDelegate>	_defaultRenderStateDelegate;

        TechniqueContext();
		static const UniformsStreamInterface& GetGlobalUniformsStreamInterface();

        static const unsigned CB_GlobalTransform = 0;
        static const unsigned CB_GlobalState = 1;
        static const unsigned CB_ShadowProjection = 2;
        static const unsigned CB_OrthoShadowProjection = 3;
        static const unsigned CB_BasicLightingEnvironment = 4;
    };

	UnderlyingAPI GetTargetAPI();

	std::shared_ptr<IThreadContext> GetThreadContext();
	void SetThreadContext(const std::shared_ptr<IThreadContext>&);

}}

