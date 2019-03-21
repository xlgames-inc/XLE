// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "TextureView.h"
#include "../../FrameBufferDesc.h"
#include "../../Types_Forward.h"
#include <memory>

namespace RenderCore { namespace Metal_DX11
{
    class ObjectFactory;
    class RenderTargetView;
    class ShaderResourceView;
    class DepthStencilView;
    class DeviceContext;

    class FrameBuffer
	{
	public:
        void BindSubpass(DeviceContext& context, unsigned subpassIndex, IteratorRange<const ClearValue*> clearValues) const;
		unsigned GetSubpassCount() const { return (unsigned)_subpasses.size(); }

		FrameBuffer(
			ObjectFactory& factory,
            const FrameBufferDesc& desc,
            const INamedAttachments& namedResources);
		FrameBuffer();
		~FrameBuffer();
	private:
        static const unsigned s_maxMRTs = 4u;
        static const unsigned s_maxSubpasses = 4u;
                
        class Subpass
        {
        public:
			RenderTargetView _rtvs[s_maxMRTs];
			DepthStencilView _dsv;
            unsigned _rtvCount;

			LoadStore _rtvLoad[s_maxMRTs];
			unsigned _rtvClearValue[s_maxMRTs];
			LoadStore _dsvLoad;
			unsigned _dsvClearValue;
        };
        std::vector<Subpass> _subpasses;
	};

    void BeginRenderPass(
        DeviceContext& context,
        FrameBuffer& frameBuffer,
        const FrameBufferDesc& layout,
        const FrameBufferProperties& props,
        IteratorRange<const ClearValue*> clearValues);

    void BeginNextSubpass(DeviceContext& context, FrameBuffer& frameBuffer);
	void EndSubpass(DeviceContext& context);
    void EndRenderPass(DeviceContext& context);
	unsigned GetCurrentSubpassIndex(DeviceContext& context);

}}