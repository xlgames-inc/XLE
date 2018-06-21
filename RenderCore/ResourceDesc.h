// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Types_Forward.h"
#include "../Core/Types.h"
#include "../Core/Prefix.h"
#include "../Utility/StringUtils.h"
#include "../Utility/IntrusivePtr.h"
#include "../Utility/IteratorUtils.h"   // for VectorPattern

namespace RenderCore
{
    /// Container for CPUAccess::Enum
    namespace CPUAccess
    {
        /// <summary>Determines CPU access privileges</summary>
        /// Determines what access (if any) the CPU will have to the
        /// buffer. This can control how the object is stored in memory
        /// in into what memory it is stored.
        ///
        /// It is always a good idea to use as few flags as possible. Many
        /// buffers should be fine with a CPUAccess flag of "0".
        ///
        /// Note that this flag can change how the system performs upload
        /// operations. For example, in D3D11, when the CPUAccess::WriteDynamic flag
        /// is set, UpdateSubresource() can't be used to some types of buffers.
        /// In these cases, Map() is used instead.
        ///
        /// Try to avoid "WriteDynamic" unless the buffer will be Locked
        /// multiple times per frame. If a buffer only needs to be updated
        /// once per frame (or less), use CPUAccess::Write
        enum Enum
        {
            Read                = 1<<0,             ///< CPU can read from this resource (using IManager::Resource_Readback)
            Write               = 1<<1,             ///< CPU can write to this resource, but will only do so once (or less) per frame
            WriteDynamic        = (1<<2)|Write      ///< CPU can write to this resource, and will lock it multiple times during a single per frame
        };
        typedef unsigned BitField;
    }

    /// Container for GPUAccess::Enum
    namespace GPUAccess
    {
        /// <summary>Determines GPU access privileges</summary>
        /// Determines whether the GPU will read from or write to a resource (or both).
        /// As usual, try to limit the privileges were possible.
        enum Enum
        {
            Read                = 1<<0,     ///< GPU can read from a resource (eg, shader resource, texture, input structured buffer)
            Write               = 1<<1      ///< GPU can write to the resource (eg, render target, RWTexture, RWStructuredBuffer)
        };
        typedef unsigned BitField;
    }

    /// Container for BindFlag::Enum
    namespace BindFlag
    {
        /// <summary>Determines how the buffer will be bound to the pipeline</summary>
        /// Most buffers are just blocks of data on the GPU. They can be bound to the
        /// pipeline in multiple ways, for different purposes. 
        /// 
        /// This flag controls how the buffer can be used. Most buffer only have a single
        /// bind flag. But sometimes we need to combine input and output binding modes
        /// eg: 
        ///     <list>
        ///         <item> BindFlag::RenderTarget | BindFlag::ShaderResource
        ///         <item> BindFlag::DepthStencil | BindFlag::ShaderResource
        ///         <item> BindFlag::StructuredBuffer | BindFlag::VertexBuffer
        ///     </list>
        enum Enum
        {
            VertexBuffer        = 1<<0,     ///< Used as an vertex buffer (ie, IASetVertexBuffers)
            IndexBuffer         = 1<<1,     ///< Used as an index buffer (ie, IASetIndexBuffer)
            ShaderResource      = 1<<2,     ///< Used as a shader resource (ie, PSSetShaderResources)
            RenderTarget        = 1<<3,     ///< Used as a render target (ie, OMSetRenderTargets)
            DepthStencil        = 1<<4,     ///< Used as a depth buffer (ie, OMSetRenderTargets)
            UnorderedAccess     = 1<<5,     ///< Used as a unordered access buffer (ie, CSSetUnorderedAccessViews)
            StructuredBuffer    = 1<<6,     ///< Used as a structured buffer (ie, CSSetShaderResources)
            ConstantBuffer      = 1<<7,     ///< Used as a constant buffer (ie, VSSetConstantBuffers)
            StreamOutput        = 1<<8,     ///< Used as a stream-output buffer from the geomtry shader (ie, SOSetTargets)
            DrawIndirectArgs    = 1<<9,     ///< Used with DrawInstancedIndirect or DrawIndexedInstancedIndirect
            RawViews            = 1<<10,    ///< Enables use of raw shader resource views
            TransferSrc         = 1<<11,    ///< Primarily used as the source resource in a copy operation (typically for staging texture)
            TransferDst         = 1<<12     ///< Primarily used as the destination resource in a copy operation (typically for readback textures)
        };
        typedef unsigned BitField;
    }

    /// Container for AllocationRules::Enum
    namespace AllocationRules
    {
        /// <summary>Determines how BufferUploads will allocate a resource</summary>
        /// Special flags that determine how the system will allocate a resource.
        enum Enum
        {
            Pooled              = 1<<0,     ///< If a compatible resource has been recently released, reuse it
            Batched             = 1<<1,     ///< Batch together similar uploads, so they become a single low level operation per frame
            Staging             = 1<<2,     ///< Staging memory only (ie, don't send to GPU)
            NonVolatile         = 1<<3      ///< Allow the underlying API to manage memory so that it will survive device resets (ie, create a managed resource in D3D9)
        };
        typedef unsigned BitField;
    }
        
        /////////////////////////////////////////////////

    class LinearBufferDesc
    {
	public:
        unsigned _sizeInBytes;
        unsigned _structureByteSize;

        static LinearBufferDesc Create(unsigned sizeInBytes, unsigned structureByteSize=0);
    };

    class TextureSamples
    {
	public:
        uint8 _sampleCount;
        uint8 _samplingQuality;
        static TextureSamples Create(uint8 sampleCount=1, uint8 samplingQuality=0)
        {
            TextureSamples result;
            result._sampleCount = sampleCount;
            result._samplingQuality = samplingQuality;
            return result;
        }
    };

    class TextureDesc
    {
	public:
        uint32 _width, _height, _depth;
        Format _format;
        enum class Dimensionality { Undefined, T1D, T2D, T3D, CubeMap };
        Dimensionality _dimensionality;
        uint8 _mipCount;
        uint16 _arrayCount;
        TextureSamples _samples;

        static TextureDesc Plain1D(
            uint32 width, Format format, 
            uint8 mipCount=1, uint16 arrayCount=0);
        static TextureDesc Plain2D(
            uint32 width, uint32 height, Format format, 
            uint8 mipCount=1, uint16 arrayCount=0, const TextureSamples& samples = TextureSamples::Create());
        static TextureDesc Plain3D(
            uint32 width, uint32 height, uint32 depth, Format format, uint8 mipCount=1);
        static TextureDesc Empty();
    };

    /// <summary>Description of a buffer</summary>
    /// Description of a buffer, used during creation operations.
    /// Usually, BufferDesc is filled with a description of a new buffer to create,
    /// and passed to IManager::Transaction_Begin.
    class ResourceDesc
    {
    public:
            // following the D3D11 style; let's use a "type" member, with a union
        struct Type { enum Enum { LinearBuffer, Texture, Unknown, Max }; };
        Type::Enum _type;
        BindFlag::BitField _bindFlags;
        CPUAccess::BitField _cpuAccess; 
        GPUAccess::BitField _gpuAccess;
        AllocationRules::BitField _allocationRules;
        union {
            LinearBufferDesc _linearBufferDesc;
            TextureDesc _textureDesc;
        };
        char _name[48];

		ResourceDesc();
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    class TextureViewDesc
    {
    public:
        struct SubResourceRange { unsigned _min; unsigned _count; };
        static const unsigned Unlimited = ~0x0u;
        static const SubResourceRange All;

		struct Flags
        {
			enum Bits 
            { 
                AttachedCounter = 1<<0, AppendBuffer = 1<<1, 
                ForceArray = 1<<2, ForceSingleSample = 1<<3, 
                JustDepth = 1<<4, JustStencil = 1<<5 
            };
			using BitField = unsigned;
		};

        enum Aspect { UndefinedAspect, ColorLinear, ColorSRGB, DepthStencil, Depth, Stencil };
        
        struct FormatFilter
        {
            Aspect      _aspect;
            Format      _explicitFormat;

            FormatFilter(Aspect aspect = UndefinedAspect)
                : _aspect(aspect), _explicitFormat(Format(0)) {}
            FormatFilter(Format explicitFormat) : _aspect(UndefinedAspect), _explicitFormat(explicitFormat) {}
        };

        FormatFilter                _format = FormatFilter {};
        SubResourceRange            _mipRange = All;
        SubResourceRange            _arrayLayerRange = All;
        TextureDesc::Dimensionality _dimensionality = TextureDesc::Dimensionality::Undefined;
		Flags::BitField				_flags = 0;
    };

    enum class FormatUsage { SRV, RTV, DSV, UAV };
    Format ResolveFormat(Format baseFormat, TextureViewDesc::FormatFilter filter, FormatUsage usage);

///////////////////////////////////////////////////////////////////////////////////////////////////

    class SubResourceId 
    { 
    public:
        unsigned _mip;
        unsigned _arrayLayer;
    };

    class PresentationChainDesc
    {
    public:
        unsigned _width, _height;
        Format _format;
        TextureSamples _samples;
        bool _mainColorIsReadable;

        PresentationChainDesc() : _width(0u), _height(0u), _format(Format(0)), _samples(TextureSamples::Create()) {}
        PresentationChainDesc(unsigned width, unsigned height, Format format = Format(0), TextureSamples samples = TextureSamples::Create(), bool colorReadable = false)
        : _width(width), _height(height), _format(format), _samples(samples), _mainColorIsReadable(colorReadable) {}
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    inline ResourceDesc CreateDesc(
        BindFlag::BitField bindFlags,
        CPUAccess::BitField cpuAccess, 
        GPUAccess::BitField gpuAccess,
        const TextureDesc& textureDesc,
        StringSection<char> name)
    {
		ResourceDesc desc;
        desc._type = ResourceDesc::Type::Texture;
        desc._bindFlags = bindFlags;
        desc._cpuAccess = cpuAccess;
        desc._gpuAccess = gpuAccess;
        desc._allocationRules = 0;
        desc._textureDesc = textureDesc;
        XlCopyString(desc._name, dimof(desc._name), name);
        return desc;
    }

    inline ResourceDesc CreateDesc(
        BindFlag::BitField bindFlags,
        CPUAccess::BitField cpuAccess, 
        GPUAccess::BitField gpuAccess,
        const LinearBufferDesc& linearBufferDesc,
        StringSection<char> name)
    {
		ResourceDesc desc;
        desc._type = ResourceDesc::Type::LinearBuffer;
        desc._bindFlags = bindFlags;
        desc._cpuAccess = cpuAccess;
        desc._gpuAccess = gpuAccess;
        desc._allocationRules = 0;
        desc._linearBufferDesc = linearBufferDesc;
        XlCopyString(desc._name, dimof(desc._name), name);
        return desc;
    }

	inline TextureDesc TextureDesc::Plain1D(
		uint32 width, Format format,
		uint8 mipCount, uint16 arrayCount)
	{
		TextureDesc result;
		result._width = width;
		result._height = 1;
		result._depth = 1;
		result._format = format;
		result._dimensionality = Dimensionality::T1D;
		result._mipCount = mipCount;
		result._arrayCount = arrayCount;
		result._samples = TextureSamples::Create();
		return result;
	}

	inline TextureDesc TextureDesc::Plain2D(
		uint32 width, uint32 height, Format format,
		uint8 mipCount, uint16 arrayCount,
		const TextureSamples& samples)
	{
		TextureDesc result;
		result._width = width;
		result._height = height;
		result._depth = 1;
		result._format = format;
		result._dimensionality = Dimensionality::T2D;
		result._mipCount = mipCount;
		result._arrayCount = arrayCount;
		result._samples = samples;
		return result;
	}

	inline TextureDesc TextureDesc::Plain3D(
		uint32 width, uint32 height, uint32 depth,
		Format format, uint8 mipCount)
	{
		TextureDesc result;
		result._width = width;
		result._height = height;
		result._depth = depth;
		result._format = format;
		result._dimensionality = Dimensionality::T3D;
		result._mipCount = mipCount;
		result._arrayCount = 0;
		result._samples = TextureSamples::Create();
		return result;
	}

	inline TextureDesc TextureDesc::Empty()
	{
		TextureDesc result;
		result._width = 0;
		result._height = 0;
		result._depth = 0;
		result._format = (Format)0;
		result._dimensionality = Dimensionality::T1D;
		result._mipCount = 0;
		result._arrayCount = 0;
		result._samples = TextureSamples::Create();
		return result;
	}

	inline LinearBufferDesc LinearBufferDesc::Create(unsigned sizeInBytes, unsigned structureByteSize)
	{
		return LinearBufferDesc{ sizeInBytes, structureByteSize };
	}

    /// <summary>Distance (in bytes) between adjacent rows, depth slices or array layers in a texture</summary>
    /// Note that for compressed textures, the "row pitch" is always the distance between adjacent rows of
    /// compressed blocks. Most compression formats use blocks of 4x4 pixels. So the row pitch is actually
    /// the distance between one row of 4x4 blocks and the next row of 4x4 blocks.
    /// Another way to think of this is to imagine that each 4x4 block is 1 pixel in a texture that is 1/16th
    /// of the size. This may make the pitch values more clear.
    class TexturePitches
    {
    public:
        unsigned _rowPitch = 0u;
        unsigned _slicePitch = 0u;
        unsigned _arrayPitch = 0u;
    };

	class SubResourceInitData
	{
	public:
		IteratorRange<const void*>  _data;
		TexturePitches              _pitches = {};
	};

    class Box2D
    {
    public:
        signed _left, _top, _right, _bottom;

        Box2D() : _left(0), _top(0), _right(0), _bottom(0) {}
        Box2D(signed left, signed top, signed right, signed bottom) 
            : _left(left), _top(top), _right(right), _bottom(bottom) {}
    };

    bool operator==(const Box2D& lhs, const Box2D& rhs);
}
