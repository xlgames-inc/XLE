// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <utility>      // for std::pair

namespace RenderCore { class SharedPkt; class Viewport; }

namespace RenderCore { namespace Metal_DX11
{
    class ShaderProgram;
	class GraphicsPipeline;

    class Buffer;

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

    class FrameBuffer;
    class FrameBufferPool;

    using ConstantBufferPacket = SharedPkt;
	using ConstantBuffer = Buffer;
	using ViewportDesc = RenderCore::Viewport;
}}

