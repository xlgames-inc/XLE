// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Techniques.h"
#include "RenderStateResolver.h"
#include "../../Assets/AssetsCore.h"		// (for ResChar)
#include "../../Utility/ParameterBox.h"

namespace RenderCore { namespace Techniques
{

///////////////////////////////////////////////////////////////////////////////////////////////////

	#pragma pack(push)
    #pragma pack(1)

    /// <summary>Common material settings</summary>
    /// Material parameters and settings are purposefully kept fairly
    /// low-level. These parameters are settings that can be used during
    /// the main render step (rather than some higher level, CPU-side
    /// operation).
    ///
    /// Typically, material parameters effect these shader inputs:
    ///     Resource bindings (ie, textures assigned to shader resource slots)
    ///     Shader constants
    ///     Some state settings (like blend modes, etc)
    ///
    /// Material parameters can also effect the shader selection through the 
    /// _matParams resource box.
    class Material
    {
    public:
        ParameterBox		_bindings;				// shader resource bindings
        ParameterBox		_matParams;				// material parameters used for selecting the appropriate shader variation
        RenderStateSet		_stateSet;				// used by the RenderStateResolver for selecting render state settings (like depth read/write settings, blend modes)
        ParameterBox		_constants;				// values typically passed to shader constants
        ::Assets::ResChar	_techniqueConfig[32];	// root technique config file

        template<typename Serializer>
            void Serialize(Serializer& serializer) const;

        Material();
		Material(Material&& moveFrom) never_throws;
		Material& operator=(Material&& moveFrom) never_throws;
		Material(const Material&) = delete;
		Material& operator=(const Material&) = delete;
    };

    #pragma pack(pop)

///////////////////////////////////////////////////////////////////////////////////////////////////

	class PredefinedCBLayout; 
	
	/// <summary>Utility call for selecting a shader variation matching a given interface</summary>
	class ShaderVariationSet
    {
    public:
        ParameterBox _materialParameters;
        ParameterBox _geometryParameters;
        TechniqueInterface _techniqueInterface;

        class Variation
        {
        public:
            ResolvedShader      _shader;
            const PredefinedCBLayout* _cbLayout;
        };

        Variation FindVariation(
            ParsingContext& parsingContext,
            unsigned techniqueIndex,
            StringSection<> techniqueConfig) const;

        const PredefinedCBLayout& GetCBLayout(StringSection<> techniqueConfig);

        ShaderVariationSet(
            const InputLayout& inputLayout,
            const std::initializer_list<uint64_t>& objectCBs,
            const ParameterBox& materialParameters);
        ShaderVariationSet();
        ShaderVariationSet(ShaderVariationSet&& moveFrom) never_throws = default;
        ShaderVariationSet& operator=(ShaderVariationSet&& moveFrom) never_throws = default;
        ~ShaderVariationSet();
    };

    ParameterBox TechParams_SetGeo(const InputLayout& inputLayout);

///////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename Serializer>
        void Material::Serialize(Serializer& serializer) const
    {
        ::Serialize(serializer, _bindings);
        ::Serialize(serializer, _matParams);
        ::Serialize(serializer, _stateSet.GetHash());
        ::Serialize(serializer, _constants);
        serializer.SerializeRaw(_techniqueConfig);
    }

}}

