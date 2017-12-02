// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/TextureView.h"
#include "../RenderCore/Metal/Forward.h"
#include "../RenderCore/Format.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/IntrusivePtr.h"

class GFSDK_SSAO_Context_D3D11;

namespace BufferUploads { class ResourceLocator; }

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
            RenderCore::Format _destinationFormat;
            bool _useNormals;
            RenderCore::Format _normalsResolveFormat;
            Desc(   unsigned width, unsigned height, 
                    RenderCore::Format destinationFormat,
                    bool useNormals,
                    RenderCore::Format normalsResolveFormat)
            {
                std::fill((char*)this, PtrAdd((char*)this, sizeof(*this)), '\0');
                _width = width;
                _height = height;
                _destinationFormat = destinationFormat;
                _useNormals = useNormals;
                _normalsResolveFormat = normalsResolveFormat;
            }
        };

        intrusive_ptr<BufferUploads::ResourceLocator>          _aoTexture;
        RenderCore::Metal::RenderTargetView     _aoTarget;
        RenderCore::Metal::ShaderResourceView   _aoSRV;

        bool                                    _useNormals;
        RenderCore::Format                      _normalsResolveFormat;
        intrusive_ptr<BufferUploads::ResourceLocator>          _resolvedNormals;
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
                                    const RenderCore::Metal::ShaderResourceView& depthBuffer,
                                    const RenderCore::Metal::ShaderResourceView* normalsBuffer,
                                    const RenderCore::Metal::ViewportDesc& mainViewport);
    /// @}
}

