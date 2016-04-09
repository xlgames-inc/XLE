// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DX11.h"
#include "../../IDevice_Forward.h"
#include "../../../Utility/IntrusivePtr.h"
#include "../../../Utility/Threading/Mutex.h"

namespace RenderCore { namespace Metal_DX11
{
	class DeviceContext;

    class ObjectFactory
    {
    public:
        ID3D::Device*   GetUnderlying() { return _device; }

            //  Wrappers for ID3D::Device creation functions. Using a single point
            //  of all for D3D creation methods allows us a lot of useful debugging
            //  and profiling tools (eg, for attaching names and leak detection)

        /// @{
        /// Basic states
        intrusive_ptr<ID3D::BlendState> CreateBlendState(const D3D11_BLEND_DESC*) const;
        intrusive_ptr<ID3D::DepthStencilState> CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC*) const;
        intrusive_ptr<ID3D::RasterizerState> CreateRasterizerState(const D3D11_RASTERIZER_DESC*) const;
        intrusive_ptr<ID3D::SamplerState> CreateSamplerState(const D3D11_SAMPLER_DESC*) const;
        /// @}

        /// @{
        /// Resources
        intrusive_ptr<ID3D::Buffer> CreateBuffer(const D3D11_BUFFER_DESC*, const D3D11_SUBRESOURCE_DATA* = nullptr, const char name[] = nullptr) const;
        intrusive_ptr<ID3D::Texture1D> CreateTexture1D(const D3D11_TEXTURE1D_DESC*, const D3D11_SUBRESOURCE_DATA* = nullptr, const char name[] = nullptr) const;
        intrusive_ptr<ID3D::Texture2D> CreateTexture2D(const D3D11_TEXTURE2D_DESC*, const D3D11_SUBRESOURCE_DATA* = nullptr, const char name[] = nullptr) const;
        intrusive_ptr<ID3D::Texture3D> CreateTexture3D(const D3D11_TEXTURE3D_DESC*, const D3D11_SUBRESOURCE_DATA* = nullptr, const char name[] = nullptr) const;
        /// @}

        /// @{
        /// Resource views
        intrusive_ptr<ID3D::RenderTargetView> CreateRenderTargetView(ID3D::Resource *, const D3D11_RENDER_TARGET_VIEW_DESC* = nullptr) const;
        intrusive_ptr<ID3D::ShaderResourceView> CreateShaderResourceView(ID3D::Resource*, const D3D11_SHADER_RESOURCE_VIEW_DESC* = nullptr) const;
        intrusive_ptr<ID3D::UnorderedAccessView> CreateUnorderedAccessView(ID3D11Resource*, const D3D11_UNORDERED_ACCESS_VIEW_DESC* = nullptr) const;
        intrusive_ptr<ID3D::DepthStencilView> CreateDepthStencilView(ID3D::Resource*, const D3D11_DEPTH_STENCIL_VIEW_DESC* = nullptr) const;
        /// @}

        /// @{
        /// Shaders
        intrusive_ptr<ID3D::VertexShader> CreateVertexShader(const void*, size_t, ID3D11ClassLinkage * = nullptr) const;
        intrusive_ptr<ID3D::PixelShader> CreatePixelShader(const void*, size_t, ID3D11ClassLinkage * = nullptr) const;
        intrusive_ptr<ID3D::ComputeShader> CreateComputeShader(const void*, size_t, ID3D::ClassLinkage* = nullptr) const;
        intrusive_ptr<ID3D::GeometryShader> CreateGeometryShader(const void*, size_t, ID3D::ClassLinkage* = nullptr) const;
        intrusive_ptr<ID3D::GeometryShader> CreateGeometryShaderWithStreamOutput(
            const void*, size_t,
            const D3D11_SO_DECLARATION_ENTRY* declEntries,
            unsigned declEntryCount, const unsigned bufferStrides[], unsigned stridesCount,
            unsigned rasterizedStreamIndex, ID3D::ClassLinkage* = nullptr) const;
        intrusive_ptr<ID3D::DomainShader> CreateDomainShader(const void*, size_t, ID3D::ClassLinkage* = nullptr) const;
        intrusive_ptr<ID3D::HullShader> CreateHullShader(const void*, size_t, ID3D::ClassLinkage* = nullptr) const;
        /// @}

        /// @{
        /// Shader dynamic linking
        intrusive_ptr<ID3D::ClassLinkage> ObjectFactory::CreateClassLinkage() const;
        /// @}

        /// @{
        /// Misc
        intrusive_ptr<ID3D::DeviceContext> CreateDeferredContext() const;
        intrusive_ptr<ID3D::InputLayout> CreateInputLayout(
            const D3D11_INPUT_ELEMENT_DESC inputElements[], unsigned inputElementsCount,
            const void *, size_t) const;
        intrusive_ptr<ID3D::Query> CreateQuery(const D3D11_QUERY_DESC*) const;
        /// @}

        static void PrepareDevice(ID3D::Device&);
        static void ReleaseDevice(ID3D::Device&);

        void AttachCurrentModule();
        void DetachCurrentModule();

		ObjectFactory(ID3D::Device& dev);
        ~ObjectFactory();
    private:
        ID3D::Device* _device;
		mutable Threading::Mutex _creationLock;
    };

	ObjectFactory* GetObjectFactory(IDevice* device);
	ObjectFactory* GetObjectFactory(ID3D::Device& device);
	ObjectFactory* GetObjectFactory(ID3D::Resource& resource);
	ObjectFactory* GetObjectFactory(DeviceContext&);
	ObjectFactory* GetObjectFactory();
}}

