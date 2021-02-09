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
	std::vector<InputElementDesc> NormalizeInputAssembly(IteratorRange<const InputElementDesc*> layout);
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

	inline InputElementDesc::InputElementDesc()
	{
		_semanticIndex = 0;
		_nativeFormat = (Format)0; _inputSlot = 0;
		_alignedByteOffset = ~0u; _inputSlotClass = InputDataRate::PerVertex;
		_instanceDataStepRate = 0;
	}
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

	bool HasElement(IteratorRange<const MiniInputElementDesc*> elements, uint64 semanticHash);
	unsigned CalculateVertexStride(IteratorRange<const MiniInputElementDesc*> elements, bool enforceAlignment=true);

	class StreamOutputInitializers
	{
	public:
		IteratorRange<const RenderCore::InputElementDesc*> _outputElements;
		IteratorRange<const unsigned*> _outputBufferStrides;
	};

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

    const char* AsString(Topology);

	unsigned CalculatePrimitiveCount(Topology topology, unsigned vertexCount, unsigned drawCallCount = 1);

///////////////////////////////////////////////////////////////////////////////////////////////////

	enum class ShaderStage
    {
        Vertex, Pixel, Geometry, Hull, Domain, Compute,
        Null,
        Max
    };

	const char* AsString(ShaderStage);

	enum class PipelineType { Graphics, Compute };

///////////////////////////////////////////////////////////////////////////////////////////////////

    class Viewport
    {
    public:
        float _x, _y;
        float _width, _height;
        float _minDepth, _maxDepth;
        bool _originIsUpperLeft;

        Viewport(float x=0.f, float y=0.f, float width=0.f, float height=0.f, float minDepth=0.f, float maxDepth=1.f, bool originIsUpperLeft=true)
        : _x(x), _y(y)
        , _width(width), _height(height)
        , _minDepth(minDepth), _maxDepth(maxDepth)
        , _originIsUpperLeft(originIsUpperLeft)
        {
            // To avoid confusion that might stem from flipped viewports, disallow them.  Viewport size must be non-negative.
            assert(_width >= 0.f);
            assert(_height >= 0.f);
        }
    };

    class ScissorRect
    {
    public:
        int _x, _y;
        unsigned _width, _height;
        bool _originIsUpperLeft;

        ScissorRect(int x=0, int y=0, unsigned width=0, unsigned height=0, bool originIsUpperLeft=true)
        : _x(x), _y(y)
        , _width(width), _height(height)
        , _originIsUpperLeft(originIsUpperLeft)
        {}
    };
}

