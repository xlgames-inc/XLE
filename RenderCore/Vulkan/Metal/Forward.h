// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <utility>      // for std::pair

namespace RenderCore { class SharedPkt; }

namespace RenderCore { namespace Metal_Vulkan
{
    class ShaderProgram;

    class VertexBuffer;
    class IndexBuffer;
    class ConstantBuffer;

    class BoundUniforms;
    class BoundInputLayout;

    class RenderTargetView;
    class DepthStencilView;
    class UnorderedAccessView;

    class ShaderResourceView;

    class RasterizerState;
    class SamplerState;
    class BlendState;
    class DepthStencilState;

    class DeviceContext;
    class ObjectFactory;

	class ViewportDesc;

    typedef SharedPkt ConstantBufferPacket;

	class VertexShader;
	class GeometryShader;
	class PixelShader;
	class ComputeShader;
	class DomainShader;
	class HullShader;
	class DeepShaderProgram;
	class BoundClassInterfaces;
    
    class FrameBuffer;
    class FrameBufferPool;
}}

