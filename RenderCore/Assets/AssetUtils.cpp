// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AssetUtils.h"
#include "Services.h"
#include "../IDevice.h"
#include "../ResourceDesc.h"
#include "../../Assets/AssetUtils.h"
#include "../../Utility/ParameterBox.h"
#include "../../Utility/StringFormat.h"

namespace RenderCore { namespace Assets
{
	RenderCore::IResourcePtr CreateStaticVertexBuffer(IteratorRange<const void*> data)
	{
		return CreateStaticVertexBuffer(RenderCore::Assets::Services::GetDevice(), data);
	}

	RenderCore::IResourcePtr CreateStaticIndexBuffer(IteratorRange<const void*> data)
	{
		return CreateStaticIndexBuffer(RenderCore::Assets::Services::GetDevice(), data);
	}

	RenderCore::IResourcePtr CreateStaticVertexBuffer(IDevice& device, IteratorRange<const void*> data)
	{
		return device.CreateResource(
			CreateDesc(
				BindFlag::VertexBuffer, 0, GPUAccess::Read,
				LinearBufferDesc::Create(unsigned(data.size())),
				"vb"),
			[data](SubResourceId subres) {
				assert(subres._arrayLayer == 0 && subres._mip == 0);
				return SubResourceInitData{ data };
			});
	}

	RenderCore::IResourcePtr CreateStaticIndexBuffer(IDevice& device, IteratorRange<const void*> data)
	{
		return device.CreateResource(
			CreateDesc(
				BindFlag::IndexBuffer, 0, GPUAccess::Read,
				LinearBufferDesc::Create(unsigned(data.size())),
				"ib"),
			[data](SubResourceId subres) {
				assert(subres._arrayLayer == 0 && subres._mip == 0);
				return SubResourceInitData{ data };
			});
	}
}}
