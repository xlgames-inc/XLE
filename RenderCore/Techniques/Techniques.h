// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "CommonBindings.h"     // for TechniqueIndex::Max
#include "PredefinedCBLayout.h"
#include "../Init.h"
#include "../../Utility/ParameterBox.h"
#include <string>
#include <vector>

namespace RenderCore { class UniformsStreamInterface; }

namespace RenderCore { namespace Techniques
{
    class IRenderStateDelegate;

    class ShaderSelectors
    {
    public:
        struct Source { enum Enum { Geometry, GlobalEnvironment, Runtime, Material, Max }; };
        ParameterBox    _selectors[Source::Max];

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

	class TechniqueEntry
    {
    public:
        bool IsValid() const { return !_vertexShaderName.empty(); }
        void MergeIn(const TechniqueEntry& source);

        ShaderSelectors		_baseSelectors;
        ::Assets::rstring   _vertexShaderName;
        ::Assets::rstring   _pixelShaderName;
        ::Assets::rstring   _geometryShaderName;

        TechniqueEntry();
        ~TechniqueEntry();
    };

	class Technique
	{
	public:
		auto GetDependencyValidation() const -> const ::Assets::DepValPtr& { return _validationCallback; }
		const PredefinedCBLayout& TechniqueCBLayout() const { return _cbLayout; }
		TechniqueEntry& GetEntry(unsigned idx);
		const TechniqueEntry& GetEntry(unsigned idx) const;

        Technique(StringSection<::Assets::ResChar> resourceName);
        ~Technique();
	private:
		TechniqueEntry			_entries[size_t(TechniqueIndex::Max)];

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
        ParameterBox							_globalEnvironmentState;
        std::shared_ptr<ParameterBox>			_renderStateDelegateParameters;
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

}}

