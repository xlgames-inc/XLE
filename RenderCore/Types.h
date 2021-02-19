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

    struct ClearFilter { enum Enum { Depth = 1<<0, Stencil = 1<<1 }; using BitField = unsigned; };
}

