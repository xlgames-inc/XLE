// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Types.h"
#include "Types_Forward.h"
#include "../Utility/IteratorUtils.h"
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

	using InputLayout = IteratorRange<const InputElementDesc*>;

    unsigned CalculateVertexStrideForSlot(IteratorRange<const InputElementDesc*> elements, unsigned slot);
	std::vector<unsigned> CalculateVertexStrides(IteratorRange<const InputElementDesc*> layout);
    unsigned HasElement(IteratorRange<const InputElementDesc*> elements, const char elementSemantic[]);
    unsigned FindElement(IteratorRange<const InputElementDesc*> elements, const char elementSemantic[], unsigned semanticIndex = 0);
   
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
    
///////////////////////////////////////////////////////////////////////////////////////////////////
    
#pragma pack(push)
#pragma pack(1)
    class MiniInputElementDesc
    {
    public:
        uint64  _semanticHash;
        Format	_nativeFormat;
        
        static const bool SerializeRaw = true;
    } attribute_packed;
#pragma pack(pop)

	unsigned CalculateVertexStride(IteratorRange<const MiniInputElementDesc*> elements, bool enforceAlignment=true);

///////////////////////////////////////////////////////////////////////////////////////////////////

	enum class Topology
	{
		PointList = 1,		// D3D11_PRIMITIVE_TOPOLOGY_POINTLIST,
		LineList = 2,		// D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
		LineStrip = 3,		// D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,
		TriangleList = 4,   // D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
		TriangleStrip = 5,  // D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
		LineListAdj = 10,   // D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ


		PatchList1 = 33,    // D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST	= 33,
		PatchList2 = 34,    // D3D11_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST	= 34,
		PatchList3 = 35,    // D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST	= 35,
		PatchList4 = 36,    // D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST	= 36,
		PatchList5 = 37,    // D3D11_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST	= 37,
		PatchList6 = 38,    // D3D11_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST	= 38,
		PatchList7 = 39,    // D3D11_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST	= 39,
		PatchList8 = 40,    // D3D11_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST	= 40,
		PatchList9 = 41,    // D3D11_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST	= 41,
		PatchList10 = 42,   // D3D11_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST	= 42,
		PatchList11 = 43,   // D3D11_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST	= 43,
		PatchList12 = 44,   // D3D11_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST	= 44,
		PatchList13 = 45,   // D3D11_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST	= 45,
		PatchList14 = 46,   // D3D11_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST	= 46,
		PatchList15 = 47,   // D3D11_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST	= 47,
		PatchList16 = 48    // D3D11_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST	= 48
	};

///////////////////////////////////////////////////////////////////////////////////////////////////

	/// <summary>Texture address modes</summary>
	///
	///     These are used to determine how the texture sampler
	///     reads texture data outside of the [0, 1] range.
	///     Normally Wrap and Clamp are used.
	///     <seealso cref="SamplerState"/>
	enum class AddressMode
    {
        Wrap = 1,   // D3D11_TEXTURE_ADDRESS_WRAP
        Mirror = 2, // D3D11_TEXTURE_ADDRESS_MIRROR
        Clamp = 3,  // D3D11_TEXTURE_ADDRESS_CLAMP
        Border = 4  // D3D11_TEXTURE_ADDRESS_BORDER
    };

    enum class FaceWinding
    {
        CCW = 0,    // Front faces are counter clockwise
        CW = 1      // Front faces are clockwise
    };

    /// <summary>Texture filtering modes</summary>
    ///
    ///     These are used when sampling a texture at a floating
    ///     point address. In other words, when sampling at a
    ///     midway point between texels, how do we filter the 
    ///     surrounding texels?
    ///     <seealso cref="SamplerState"/>
    enum class FilterMode
    {
        Point = 0,                  // D3D11_FILTER_MIN_MAG_MIP_POINT
        Trilinear = 0x15,           // D3D11_FILTER_MIN_MAG_MIP_LINEAR
        Anisotropic = 0x55,         // D3D11_FILTER_ANISOTROPIC
        Bilinear = 0x14,            // D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT
        ComparisonBilinear = 0x94   // D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT
    };

    enum class CompareOp
    {
        Never = 1,          // D3D11_COMPARISON_NEVER
        Less = 2,           // D3D11_COMPARISON_LESS
        Equal = 3,          // D3D11_COMPARISON_EQUAL
        LessEqual = 4,      // D3D11_COMPARISON_LESS_EQUAL
        Greater = 5,        // D3D11_COMPARISON_GREATER
        NotEqual = 6,       // D3D11_COMPARISON_NOT_EQUAL
        GreaterEqual = 7,   // D3D11_COMPARISON_GREATER_EQUAL
        Always = 8          // D3D11_COMPARISON_ALWAYS
    };

	/// <summary>Back face culling mode</summary>
	///
	///     Used to determine which side of a triangle to cull.
	///
	///     Note that there is another flag the in rasteriser state
	///     that determines which side of a triangle is the "back"
	///     (ie, clockwise or counterclockwise order). 
	///     Only use the "Front" option if you really want to cull
	///     the front facing triangles (useful for some effects)
	///     <seealso cref="RasterizerState"/>
	enum class CullMode
	{
		None = 1,   // D3D11_CULL_NONE,
		Front = 2,  // D3D11_CULL_FRONT,
		Back = 3    // D3D11_CULL_BACK
	};

	enum class FillMode
	{
		Solid = 3,      // D3D11_FILL_SOLID
		Wireframe = 2   // D3D11_FILL_WIREFRAME
	};

	/// <summary>Settings used for describing a blend state</summary>
	///
	///     The blend operation takes the form:
	///         out colour = Operation(Param1 * (Source colour), Param2 * (Destination colour))
	///         out alpha = Operation(Param1 * (Source alpha), Param2 * (Destination alpha))
	///
	///     Where "Operation" is typically addition.
	///
	///     This enum is used for "Param1" and "Param2"
	///     <seealso cref="BlendOp::Enum"/>
	///     <seealso cref="BlendState"/>
	enum class Blend
	{
		Zero = 1, // D3D11_BLEND_ZERO,
		One = 2, // D3D11_BLEND_ONE,

		SrcColor = 3, // D3D11_BLEND_SRC_COLOR,
		InvSrcColor = 4, // D3D11_BLEND_INV_SRC_COLOR,
		DestColor = 9, // D3D11_BLEND_DEST_COLOR,
		InvDestColor = 10, // D3D11_BLEND_INV_DEST_COLOR,

		SrcAlpha = 5, // D3D11_BLEND_SRC_ALPHA,
		InvSrcAlpha = 6, // D3D11_BLEND_INV_SRC_ALPHA,
		DestAlpha = 7, // D3D11_BLEND_DEST_ALPHA,
		InvDestAlpha = 8 // D3D11_BLEND_INV_DEST_ALPHA
	};

	/// <summary>Settings used for describing a blend state</summary>
	///
	///     The blend operation takes the form:
	///         out colour = Operation(Param1 * (Source colour), Param2 * (Destination colour))
	///         out alpha = Operation(Param1 * (Source alpha), Param2 * (Destination alpha))
	///
	///     This enum is used for "Operation"
	///     <seealso cref="BlendOp::Enum"/>
	///     <seealso cref="BlendState"/>
	enum class BlendOp
	{
		NoBlending,
		Add = 1, // D3D11_BLEND_OP_ADD,
		Subtract = 2, // D3D11_BLEND_OP_SUBTRACT,
		RevSubtract = 3, // D3D11_BLEND_OP_REV_SUBTRACT,
		Min = 4, // D3D11_BLEND_OP_MIN,
		Max = 5 // D3D11_BLEND_OP_MAX
	};

	enum class StencilOp
	{
        Keep = 1,           // D3D11_STENCIL_OP_KEEP
		DontWrite = 1,      // D3D11_STENCIL_OP_KEEP

		Zero = 2,           // D3D11_STENCIL_OP_ZERO
		Replace = 3,        // D3D11_STENCIL_OP_REPLACE
		IncreaseSat = 4,    // D3D11_STENCIL_OP_INCR_SAT
		DecreaseSat = 5,    // D3D11_STENCIL_OP_DECR_SAT
		Invert = 6,         // D3D11_STENCIL_OP_INVERT
		Increase = 7,       // D3D11_STENCIL_OP_INCR
		Decrease = 8        // D3D11_STENCIL_OP_DECR
	};

	enum class ShaderStage
    {
        Vertex, Pixel, Geometry, Hull, Domain, Compute,
        Null,
        Max
    };

}

