// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Types_Forward.h"
#include <string>

namespace RenderCore
{
	#define VS_DefShaderModel   "vs_*"
	#define GS_DefShaderModel   "gs_*"
	#define PS_DefShaderModel   "ps_*"
	#define CS_DefShaderModel   "cs_*"
	#define HS_DefShaderModel   "hs_*"
	#define DS_DefShaderModel   "ds_*"

///////////////////////////////////////////////////////////////////////////////////////////////////

    enum class InputDataRate
    {
        PerVertex, PerInstance
    };

	/// <summary>Vertex input element description</summary>
	/// This is used to bind vertex buffer data to the vertex shader inputs. The structure helps
	/// specify the position and offsets of certain vertex elements. Those elements are matched
	/// against the vertex shader inputs.
    class InputElementDesc
    {
    public:
        std::string     _semanticName;
        unsigned        _semanticIndex;
        Format          _nativeFormat;
        unsigned        _inputSlot;
        unsigned        _alignedByteOffset;
        InputDataRate	_inputSlotClass;
        unsigned        _instanceDataStepRate;

        InputElementDesc();
        InputElementDesc(   const std::string& name, unsigned semanticIndex, 
							Format nativeFormat, unsigned inputSlot = 0,
                            unsigned alignedByteOffset = ~unsigned(0x0), 
                            InputDataRate inputSlotClass = InputDataRate::PerVertex,
                            unsigned instanceDataStepRate = 0);
    };

	using InputLayout = std::pair<const InputElementDesc*, size_t>;

    unsigned CalculateVertexStride(
        const InputElementDesc* start, const InputElementDesc* end,
        unsigned slot);

    unsigned HasElement(const InputElementDesc* begin, const InputElementDesc* end, const char elementSemantic[]);
    unsigned FindElement(const InputElementDesc* begin, const InputElementDesc* end, const char elementSemantic[], unsigned semanticIndex = 0);

    /// Contains some common reusable vertex input layouts
    namespace GlobalInputLayouts
    {
        extern InputLayout P;
        extern InputLayout PC;
        extern InputLayout P2C;
        extern InputLayout P2CT;
        extern InputLayout PCT;
        extern InputLayout PT;
        extern InputLayout PN;
        extern InputLayout PNT;
        extern InputLayout PNTT;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	inline InputElementDesc::InputElementDesc() {}
	inline InputElementDesc::InputElementDesc(const std::string& name, unsigned semanticIndex,
		Format nativeFormat, unsigned inputSlot,
		unsigned alignedByteOffset,
		InputDataRate inputSlotClass,
		unsigned instanceDataStepRate)
	{
		_semanticName = name; _semanticIndex = semanticIndex;
		_nativeFormat = nativeFormat; _inputSlot = inputSlot;
		_alignedByteOffset = alignedByteOffset; _inputSlotClass = inputSlotClass;
		_instanceDataStepRate = instanceDataStepRate;
	}


}

