// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/State.h"
#include "../Utility/PtrUtils.h"

class GFSDK_SSAO_Context_D3D11;

namespace SceneEngine
{
    /// \defgroup AO Ambient Occlusion
    /// @{
    class AmbientOcclusionResources
    {
    public:
        class Desc
        {
        public:
            unsigned _width, _height;
            RenderCore::Metal::NativeFormat::Enum _destinationFormat;
            bool _useNormals;
            RenderCore::Metal::NativeFormat::Enum _normalsResolveFormat;
            Desc(   unsigned width, unsigned height, 
                    RenderCore::Metal::NativeFormat::Enum destinationFormat,
                    bool useNormals,
                    RenderCore::Metal::NativeFormat::Enum normalsResolveFormat)
            {
                std::fill((char*)this, PtrAdd((char*)this, sizeof(*this)), 0);
                _width = width;
                _height = height;
                _destinationFormat = destinationFormat;
                _useNormals = useNormals;
                _normalsResolveFormat = normalsResolveFormat;
            }
        };

        intrusive_ptr<ID3D::Resource>           _aoTexture;
        RenderCore::Metal::RenderTargetView     _aoTarget;
        RenderCore::Metal::ShaderResourceView   _aoSRV;

        bool                                    _useNormals;
        RenderCore::Metal::NativeFormat::Enum   _normalsResolveFormat;
        intrusive_ptr<ID3D::Resource>           _resolvedNormals;
        RenderCore::Metal::ShaderResourceView   _resolvedNormalsSRV;

        struct ContextDeletor { void operator()(void* ptr); };
        std::unique_ptr<GFSDK_SSAO_Context_D3D11, ContextDeletor> _aoContext;

        AmbientOcclusionResources(const Desc& desc);
        ~AmbientOcclusionResources();
    };

    class LightingParserContext;
    void AmbientOcclusion_Render(   RenderCore::Metal::DeviceContext* context,
                                    LightingParserContext& parserContext,
                                    AmbientOcclusionResources& resources,
                                    RenderCore::Metal::ShaderResourceView& depthBuffer,
                                    RenderCore::Metal::ShaderResourceView* normalsBuffer,
                                    const RenderCore::Metal::ViewportDesc& mainViewport);
    /// @}
}

