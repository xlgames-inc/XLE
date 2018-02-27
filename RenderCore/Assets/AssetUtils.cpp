// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DeferredShaderResource.h"
#include "Services.h"
#include "../IDevice.h"
#include "../ResourceDesc.h"
#include "../../Assets/AssetUtils.h"
#include "../../Utility/ParameterBox.h"
#include "../../Utility/StringFormat.h"

namespace RenderCore { namespace Assets
{
    ParameterBox TechParams_SetResHas(
        const ParameterBox& inputMatParameters, const ParameterBox& resBindings,
        const ::Assets::DirectorySearchRules& searchRules)
    {
        static const auto DefaultNormalsTextureBindingHash = ParameterBox::MakeParameterNameHash("NormalsTexture");
            // The "material parameters" ParameterBox should contain some "RES_HAS_..."
            // settings. These tell the shader what resource bindings are available
            // (and what are missing). We need to set these parameters according to our
            // binding list
        ParameterBox result = inputMatParameters;
        for (auto param=resBindings.Begin(); !param.IsEnd(); ++param) {
            result.SetParameter(StringMeld<64, utf8>() << "RES_HAS_" << param.Name(), 1);
            if (param.HashName() == DefaultNormalsTextureBindingHash) {
                auto resourceName = resBindings.GetString<::Assets::ResChar>(DefaultNormalsTextureBindingHash);
                ::Assets::ResChar resolvedName[MaxPath];
                searchRules.ResolveFile(resolvedName, dimof(resolvedName), resourceName.c_str());
                result.SetParameter(
                    (const utf8*)"RES_HAS_NormalsTexture_DXT", 
                    DeferredShaderResource::IsDXTNormalMap(resolvedName));
            }
        }
        return std::move(result);
    }

	static RenderCore::IResourcePtr CreateStaticVertexBuffer(IteratorRange<const void*> data)
	{
		auto& device = RenderCore::Assets::Services::GetDevice();
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

	static RenderCore::IResourcePtr CreateStaticIndexBuffer(IteratorRange<const void*> data)
	{
		auto& device = RenderCore::Assets::Services::GetDevice();
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
