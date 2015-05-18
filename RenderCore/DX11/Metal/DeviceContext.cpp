// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DeviceContext.h"
#include "InputLayout.h"
#include "Shader.h"
#include "State.h"
#include "Buffer.h"
#include "Resource.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/Threading/Mutex.h"
#include "../../Utility/MemoryUtils.h"

#include "../IDeviceDX11.h"
#include "IncludeDX11.h"
#include "DX11Utils.h"

namespace RenderCore { namespace Metal_DX11
{
    static_assert(      Topology::PointList      == D3D11_PRIMITIVE_TOPOLOGY_POINTLIST
                &&  Topology::LineList       == D3D11_PRIMITIVE_TOPOLOGY_LINELIST
                &&  Topology::LineStrip      == D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP
                &&  Topology::TriangleList   == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
                &&  Topology::TriangleStrip  == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
                "Toplogy flags are out-of-sync");

    void DeviceContext::Bind(unsigned startSlot, unsigned bufferCount, const VertexBuffer* VBs[], const unsigned strides[], const unsigned offsets[])
    {
        ID3D::Buffer* buffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
        for (unsigned c=0; c<bufferCount; ++c) buffers[c] = VBs[c]->GetUnderlying();
        _underlying->IASetVertexBuffers(startSlot, bufferCount, buffers, strides, offsets);
    }

    void DeviceContext::Bind(const BoundInputLayout& inputLayout)
    {
        _underlying->IASetInputLayout(inputLayout.GetUnderlying());
    }

    void DeviceContext::Bind(Topology::Enum topology)
    {
        _underlying->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY(topology));
    }

    void DeviceContext::Bind(const VertexShader& vertexShader)
    {
        _underlying->VSSetShader(vertexShader.GetUnderlying(), nullptr, 0);
    }

    void DeviceContext::Bind(const GeometryShader& geometryShader)
    {
        _underlying->GSSetShader(geometryShader.GetUnderlying(), nullptr, 0);
    }

    void DeviceContext::Bind(const PixelShader& pixelShader)
    {
        _underlying->PSSetShader(pixelShader.GetUnderlying(), nullptr, 0);
    }

    void DeviceContext::Bind(const ComputeShader& computeShader)
    {
        _underlying->CSSetShader(computeShader.GetUnderlying(), nullptr, 0);
    }

    void DeviceContext::Bind(const DomainShader& domainShader)
    {
        _underlying->DSSetShader(domainShader.GetUnderlying(), nullptr, 0);
    }

    void DeviceContext::Bind(const HullShader& hullShader)
    {
        _underlying->HSSetShader(hullShader.GetUnderlying(), nullptr, 0);
    }

    void DeviceContext::Bind(const ShaderProgram& shaderProgram)
    {
        _underlying->VSSetShader(shaderProgram.GetVertexShader().GetUnderlying(), nullptr, 0);
        _underlying->GSSetShader(shaderProgram.GetGeometryShader().GetUnderlying(), nullptr, 0);
        _underlying->PSSetShader(shaderProgram.GetPixelShader().GetUnderlying(), nullptr, 0);
    }

    void DeviceContext::Bind(const DeepShaderProgram& shaderProgram)
    {
        _underlying->VSSetShader(shaderProgram.GetVertexShader().GetUnderlying(), nullptr, 0);
        _underlying->GSSetShader(shaderProgram.GetGeometryShader().GetUnderlying(), nullptr, 0);
        _underlying->PSSetShader(shaderProgram.GetPixelShader().GetUnderlying(), nullptr, 0);
        _underlying->HSSetShader(shaderProgram.GetHullShader().GetUnderlying(), nullptr, 0);
        _underlying->DSSetShader(shaderProgram.GetDomainShader().GetUnderlying(), nullptr, 0);
    }

    void DeviceContext::Bind(const RasterizerState& rasterizer)
    {
        _underlying->RSSetState(rasterizer.GetUnderlying());
    }

    void DeviceContext::Bind(const BlendState& blender)
    {
        const FLOAT blendFactors[] = {1.f, 1.f, 1.f, 1.f};
        _underlying->OMSetBlendState(blender.GetUnderlying(), blendFactors, 0xffffffff);
    }

    void DeviceContext::Bind(const DepthStencilState& depthStencil, unsigned stencilRef)
    {
        _underlying->OMSetDepthStencilState(depthStencil.GetUnderlying(), stencilRef);
    }

    void DeviceContext::Bind(const IndexBuffer& ib, NativeFormat::Enum indexFormat, unsigned offset)
    {
        _underlying->IASetIndexBuffer(ib.GetUnderlying(), AsDXGIFormat(indexFormat), offset);
    }

    void DeviceContext::Bind(const ViewportDesc& viewport)
    {
            // ("ViewportDesc" is equivalent to D3D11_VIEWPORT)
            //      --  we could do static_asserts to check the offsets of the members
            //          to make sure.
        _underlying->RSSetViewports(1, (D3D11_VIEWPORT*)&viewport);
    }

    void DeviceContext::Draw(unsigned vertexCount, unsigned startVertexLocation)
    {
        _underlying->Draw(vertexCount, startVertexLocation);
    }

    void DeviceContext::DrawIndexed(unsigned indexCount, unsigned startIndexLocation, unsigned baseVertexLocation)
    {
        _underlying->DrawIndexed(indexCount, startIndexLocation, baseVertexLocation);
    }

    void DeviceContext::Clear(RenderTargetView& renderTargets, const Float4& clearColour)
    {
        _underlying->ClearRenderTargetView(renderTargets.GetUnderlying(), &clearColour[0]);
    }

    void DeviceContext::Clear(DepthStencilView& depthStencil, float depth, unsigned stencil)
    {
        _underlying->ClearDepthStencilView(depthStencil.GetUnderlying(), D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, depth, (UINT8)stencil);
    }

    void DeviceContext::Clear(UnorderedAccessView& unorderedAccess, unsigned values[4])
    {
        _underlying->ClearUnorderedAccessViewUint(unorderedAccess.GetUnderlying(), values);
    }

    void DeviceContext::Clear(UnorderedAccessView& unorderedAccess, float values[4])
    {
        _underlying->ClearUnorderedAccessViewFloat(unorderedAccess.GetUnderlying(), values);
    }

    void DeviceContext::ClearStencil(DepthStencilView& depthStencil, unsigned stencil)
    {
        _underlying->ClearDepthStencilView(depthStencil.GetUnderlying(), D3D11_CLEAR_STENCIL, 1.f, (UINT8)stencil);
    }

    template<>
        void  DeviceContext::UnbindVS<ShaderResourceView>(unsigned startSlot, unsigned count)
    {
            // note --  D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT is 128 for D3D11
            //          It's a little too big to declared on the stack. So let's limit
            //          the maximum size
        ID3D::ShaderResourceView* srv[16];
        assert(count <= dimof(srv));
        count = std::min(count, (unsigned)dimof(srv));
        std::fill(srv, &srv[count], nullptr);
        _underlying->VSSetShaderResources(startSlot, count, srv);
    }

    template<>
        void  DeviceContext::UnbindGS<ShaderResourceView>(unsigned startSlot, unsigned count)
    {
        ID3D::ShaderResourceView* srv[16];
        assert(count <= dimof(srv));
        count = std::min(count, (unsigned)dimof(srv));
        std::fill(srv, &srv[count], nullptr);
        _underlying->GSSetShaderResources(startSlot, count, srv);
    }

    template<>
        void  DeviceContext::UnbindPS<ShaderResourceView>(unsigned startSlot, unsigned count)
    {
        ID3D::ShaderResourceView* srv[16];
        assert(count <= dimof(srv));
        count = std::min(count, (unsigned)dimof(srv));
        std::fill(srv, &srv[count], nullptr);
        _underlying->PSSetShaderResources(startSlot, count, srv);
    }

    template<>
        void  DeviceContext::UnbindCS<ShaderResourceView>(unsigned startSlot, unsigned count)
    {
        ID3D::ShaderResourceView* srv[16];
        assert(count <= dimof(srv));
        count = std::min(count, (unsigned)dimof(srv));
        std::fill(srv, &srv[count], nullptr);
        _underlying->CSSetShaderResources(startSlot, count, srv);
    }

    template<>
        void  DeviceContext::UnbindCS<UnorderedAccessView>(unsigned startSlot, unsigned count)
    {
        ID3D::UnorderedAccessView* uoavs[16];
        unsigned initialCounts[16] = {
            unsigned(-1), unsigned(-1), unsigned(-1), unsigned(-1), unsigned(-1), unsigned(-1), unsigned(-1), unsigned(-1),
            unsigned(-1), unsigned(-1), unsigned(-1), unsigned(-1), unsigned(-1), unsigned(-1), unsigned(-1), unsigned(-1)
        };
        assert(count <= dimof(uoavs));
        count = std::min(count, (unsigned)dimof(uoavs));
        std::fill(uoavs, &uoavs[count], nullptr);
        _underlying->CSSetUnorderedAccessViews(startSlot, count, uoavs, initialCounts);
    }

    template<> void DeviceContext::Unbind<BoundInputLayout>()
    {
        _underlying->IASetInputLayout(nullptr);
    }

    template<> void DeviceContext::Unbind<VertexBuffer>()
    {
        ID3D::Buffer* vb = nullptr;
        UINT strides = 0, offsets = 0;
        _underlying->IASetVertexBuffers(0, 1, &vb, &strides, &offsets);
    }

    void DeviceContext::Dispatch(unsigned countX, unsigned countY, unsigned countZ)
    {
        _underlying->Dispatch(countX, countY, countZ);
    }

    void        DeviceContext::InvalidateCachedState()
    {
        XlZeroMemory(_currentCBs);
        XlZeroMemory(_currentSRVs);
    }

    DeviceContext::DeviceContext(ID3D::DeviceContext* context)
    : _underlying(context)
    {
        XlZeroMemory(_currentCBs);
        XlZeroMemory(_currentSRVs);
        _annotations = QueryInterfaceCast<ID3D::UserDefinedAnnotation>(_underlying);
    }

    DeviceContext::DeviceContext(intrusive_ptr<ID3D::DeviceContext>&& context)
    : _underlying(std::forward<intrusive_ptr<ID3D::DeviceContext>>(context))
    {
        XlZeroMemory(_currentCBs);
        XlZeroMemory(_currentSRVs);
        _annotations = QueryInterfaceCast<ID3D::UserDefinedAnnotation>(_underlying);
    }

    DeviceContext::~DeviceContext()
    {}


    void                        DeviceContext::BeginCommandList()
    {
            // (nothing required in D3D11)
    }

    intrusive_ptr<CommandList>     DeviceContext::ResolveCommandList()
    {
        ID3D::CommandList* commandListTemp = nullptr;
        HRESULT hresult = _underlying->FinishCommandList(FALSE, &commandListTemp);
        if (SUCCEEDED(hresult) && commandListTemp) {
            intrusive_ptr<ID3D::CommandList> underlyingCommandList = moveptr(commandListTemp);
            return make_intrusive<CommandList>(underlyingCommandList.get());
        }
        return intrusive_ptr<CommandList>();
    }

    bool    DeviceContext::IsImmediate() const
    {
        auto type = _underlying->GetType();
        return type == D3D11_DEVICE_CONTEXT_IMMEDIATE;
    }

    void    DeviceContext::CommitCommandList(CommandList& commandList)
    {
        _underlying->ExecuteCommandList(commandList.GetUnderlying(), FALSE);
    }

    std::shared_ptr<DeviceContext>  DeviceContext::Get(IThreadContext& threadContext)
    {
        auto tc = (IThreadContextDX11*)threadContext.QueryInterface(__uuidof(IThreadContextDX11));
        if (tc) {
            return tc->GetUnderlying();
        }
        return nullptr;
    }

    void DeviceContext::PrepareForDestruction(IDevice* device, IPresentationChain* presentationChain)
    {
        auto metalContext = Get(*device->GetImmediateContext());
        if (metalContext) {
            metalContext->GetUnderlying()->ClearState();
            for (unsigned c=0; c<6; ++c) {
                device->BeginFrame(presentationChain);
                metalContext->GetUnderlying()->Flush();
            }
        }
    }


    static intrusive_ptr<ID3D::Device> ExtractUnderlyingDevice(RenderCore::IDevice* device)
    {
        RenderCore::IDeviceDX11* dx11Device =
            (RenderCore::IDeviceDX11*)device->QueryInterface(__uuidof(RenderCore::IDeviceDX11));
        if (dx11Device) {
            return dx11Device->GetUnderlyingDevice();
        }
        return nullptr;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    class ObjectFactory::AttachedData : public IUnknown, RefCountedObject
    {
    public:
        static const GUID Guid;
        Threading::Mutex _creationLock;

        virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
        {
            if (riid == IID_IUnknown) {
                *reinterpret_cast<IUnknown**>(ppvObject) = this;
                return S_OK;
            }

            *ppvObject = NULL;
            return E_NOINTERFACE;
        }

        virtual ULONG STDMETHODCALLTYPE AddRef() { return RefCountedObject::AddRef(); }
        virtual ULONG STDMETHODCALLTYPE Release() { return RefCountedObject::Release(); }
        AttachedData() {}
        virtual ~AttachedData();
    };

    ObjectFactory::AttachedData::~AttachedData() {}

        // {D2192F4A-E4D8-4A33-891A-FDBCEBB83E5D}
    const GUID ObjectFactory::AttachedData::Guid = { 0xd2192f4a, 0xe4d8, 0x4a33, { 0x89, 0x1a, 0xfd, 0xbc, 0xeb, 0xb8, 0x3e, 0x5d } };

    template<typename ResultType>
        static intrusive_ptr<ResultType> D3DDevice_FinalizeCreate(ResultType* tempPtr, HRESULT hresult, const char name[])
    {
        if (!SUCCEEDED(hresult) || !tempPtr) {
            throw Exceptions::GenericFailure("Failure during construction of D3D object");
        }

        intrusive_ptr<ResultType> result(tempPtr, false);
        // if (name && name[0]) { Resource_Register(*result.get(), name); }
        return result;
    }

    template <typename ResultType, typename IntermediateType, typename Desc>
        static intrusive_ptr<ResultType> D3DDeviceCreate(
            ID3D::Device* device,
            HRESULT (__stdcall ID3D11Device::*CreationFunction)(const Desc*, const D3D11_SUBRESOURCE_DATA*, IntermediateType**),
            const Desc* desc, const D3D11_SUBRESOURCE_DATA* subResourceData = NULL, const char name[] = nullptr)
    {
        IntermediateType* tempPtr = 0;
        auto hresult = (device->*CreationFunction)(desc, subResourceData, &tempPtr);
        return D3DDevice_FinalizeCreate(tempPtr, hresult, name);
    }

    template <typename ResultType, typename IntermediateType, typename Desc>
        static intrusive_ptr<ResultType> D3DDeviceCreate(
            ID3D::Device* device,
            HRESULT (__stdcall ID3D11Device::*CreationFunction)(const Desc*, IntermediateType**),
            const Desc* desc, const char name[] = nullptr)
    {
        IntermediateType* tempPtr = 0;
        auto hresult = (device->*CreationFunction)(desc, &tempPtr);
        return D3DDevice_FinalizeCreate(tempPtr, hresult, name);
    }

    template <typename ResultType, typename IntermediateType, typename Desc>
        static intrusive_ptr<ResultType> D3DDeviceCreate(
            ID3D::Device* device,
            HRESULT (__stdcall ID3D11Device::*CreationFunction)(ID3D::Resource*, const Desc*, IntermediateType**),
            ID3D::Resource* resource, const Desc* desc, const char name[] = nullptr)
    {
        IntermediateType* tempPtr = 0;
        auto hresult = (device->*CreationFunction)(resource, desc, &tempPtr);
        return D3DDevice_FinalizeCreate(tempPtr, hresult, name);
    }

    template <typename ResultType, typename IntermediateType>
        static intrusive_ptr<ResultType> D3DDeviceCreate(
            ID3D::Device* device,
            HRESULT (__stdcall ID3D11Device::*CreationFunction)(const void*, SIZE_T, ID3D::ClassLinkage*, IntermediateType**),
            const void* byteCode, SIZE_T size, ID3D::ClassLinkage* linkage, const char name[] = nullptr)
    {
        IntermediateType* tempPtr = 0;
        auto hresult = (device->*CreationFunction)(byteCode, size, linkage, &tempPtr);
        return D3DDevice_FinalizeCreate(tempPtr, hresult, name);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        /// @{
        /// Basic states
    intrusive_ptr<ID3D::BlendState> ObjectFactory::CreateBlendState(const D3D11_BLEND_DESC* desc) const
    {
        ScopedLock(_attachedData->_creationLock);
        return D3DDeviceCreate<ID3D::BlendState>(_device.get(), &ID3D::Device::CreateBlendState, desc);
    }

    intrusive_ptr<ID3D::DepthStencilState> ObjectFactory::CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC* desc) const
    {
        ScopedLock(_attachedData->_creationLock);
        return D3DDeviceCreate<ID3D::DepthStencilState>(_device.get(), &ID3D::Device::CreateDepthStencilState, desc);
    }

    intrusive_ptr<ID3D::RasterizerState> ObjectFactory::CreateRasterizerState(const D3D11_RASTERIZER_DESC* desc) const
    {
        ScopedLock(_attachedData->_creationLock);
        return D3DDeviceCreate<ID3D::RasterizerState>(_device.get(), &ID3D::Device::CreateRasterizerState, desc);
    }

    intrusive_ptr<ID3D::SamplerState> ObjectFactory::CreateSamplerState(const D3D11_SAMPLER_DESC* desc) const
    {
        ScopedLock(_attachedData->_creationLock);
        return D3DDeviceCreate<ID3D::SamplerState>(_device.get(), &ID3D::Device::CreateSamplerState, desc);
    }
        /// @}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        /// @{
        /// Resources
    intrusive_ptr<ID3D::Buffer> ObjectFactory::CreateBuffer(const D3D11_BUFFER_DESC* desc, const D3D11_SUBRESOURCE_DATA* subResData, const char[]) const
    {
        ScopedLock(_attachedData->_creationLock);
        return D3DDeviceCreate<ID3D::Buffer>(_device.get(), &ID3D::Device::CreateBuffer, desc, subResData);
    }

    intrusive_ptr<ID3D::Texture1D> ObjectFactory::CreateTexture1D(const D3D11_TEXTURE1D_DESC* desc, const D3D11_SUBRESOURCE_DATA* subResData, const char[]) const
    {
        ScopedLock(_attachedData->_creationLock);
        return D3DDeviceCreate<ID3D::Texture1D>(_device.get(), &ID3D::Device::CreateTexture1D, desc, subResData);
    }

    intrusive_ptr<ID3D::Texture2D> ObjectFactory::CreateTexture2D(const D3D11_TEXTURE2D_DESC* desc, const D3D11_SUBRESOURCE_DATA* subResData, const char[]) const
    {
        ScopedLock(_attachedData->_creationLock);
        return D3DDeviceCreate<ID3D::Texture2D>(_device.get(), &ID3D::Device::CreateTexture2D, desc, subResData);
    }

    intrusive_ptr<ID3D::Texture3D> ObjectFactory::CreateTexture3D(const D3D11_TEXTURE3D_DESC* desc, const D3D11_SUBRESOURCE_DATA* subResData, const char[]) const
    {
        ScopedLock(_attachedData->_creationLock);
        return D3DDeviceCreate<ID3D::Texture3D>(_device.get(), &ID3D::Device::CreateTexture3D, desc, subResData);
    }
        /// @}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        /// @{
        /// Resource views
    intrusive_ptr<ID3D::RenderTargetView> ObjectFactory::CreateRenderTargetView(ID3D::Resource* resource, const D3D11_RENDER_TARGET_VIEW_DESC* desc) const
    {
        ScopedLock(_attachedData->_creationLock);
        return D3DDeviceCreate<ID3D::RenderTargetView>(_device.get(), &ID3D::Device::CreateRenderTargetView, resource, desc);
    }

    intrusive_ptr<ID3D::ShaderResourceView> ObjectFactory::CreateShaderResourceView(ID3D::Resource* resource, const D3D11_SHADER_RESOURCE_VIEW_DESC* desc) const
    {
        ScopedLock(_attachedData->_creationLock);
        return D3DDeviceCreate<ID3D::ShaderResourceView>(_device.get(), &ID3D::Device::CreateShaderResourceView, resource, desc);
    }

    intrusive_ptr<ID3D::UnorderedAccessView> ObjectFactory::CreateUnorderedAccessView(ID3D11Resource* resource, const D3D11_UNORDERED_ACCESS_VIEW_DESC* desc) const
    {
        ScopedLock(_attachedData->_creationLock);
        return D3DDeviceCreate<ID3D::UnorderedAccessView>(_device.get(), &ID3D::Device::CreateUnorderedAccessView, resource, desc);
    }

    intrusive_ptr<ID3D::DepthStencilView> ObjectFactory::CreateDepthStencilView(ID3D::Resource* resource, const D3D11_DEPTH_STENCIL_VIEW_DESC* desc) const
    {
        ScopedLock(_attachedData->_creationLock);
        return D3DDeviceCreate<ID3D::DepthStencilView>(_device.get(), &ID3D::Device::CreateDepthStencilView, resource, desc);
    }
        /// @}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        /// @{
        /// Shaders
    intrusive_ptr<ID3D::VertexShader> ObjectFactory::CreateVertexShader(const void* data, size_t size, ID3D11ClassLinkage* linkage) const
    {
        ScopedLock(_attachedData->_creationLock);
        return D3DDeviceCreate<ID3D::VertexShader>(_device.get(), &ID3D::Device::CreateVertexShader, data, size, linkage);
    }

    intrusive_ptr<ID3D::PixelShader> ObjectFactory::CreatePixelShader(const void* data, size_t size, ID3D11ClassLinkage* linkage) const
    {
        ScopedLock(_attachedData->_creationLock);
        return D3DDeviceCreate<ID3D::PixelShader>(_device.get(), &ID3D::Device::CreatePixelShader, data, size, linkage);
    }

    intrusive_ptr<ID3D::ComputeShader> ObjectFactory::CreateComputeShader(const void* data, size_t size, ID3D11ClassLinkage* linkage) const
    {
        ScopedLock(_attachedData->_creationLock);
        return D3DDeviceCreate<ID3D::ComputeShader>(_device.get(), &ID3D::Device::CreateComputeShader, data, size, linkage);
    }

    intrusive_ptr<ID3D::GeometryShader> ObjectFactory::CreateGeometryShader(const void* data, size_t size, ID3D11ClassLinkage* linkage) const
    {
        ScopedLock(_attachedData->_creationLock);
        return D3DDeviceCreate<ID3D::GeometryShader>(_device.get(), &ID3D::Device::CreateGeometryShader, data, size, linkage);
    }

    intrusive_ptr<ID3D::GeometryShader> ObjectFactory::CreateGeometryShaderWithStreamOutput(
        const void* data, size_t size,
        const D3D11_SO_DECLARATION_ENTRY* declEntries,
        unsigned declEntryCount, const unsigned bufferStrides[], unsigned stridesCount,
        unsigned rasterizedStreamIndex, ID3D::ClassLinkage* linkage) const
    {
        ScopedLock(_attachedData->_creationLock);
        ID3D::GeometryShader* tempPtr = nullptr;
        auto hresult = _device->CreateGeometryShaderWithStreamOutput(
            data, size, declEntries, declEntryCount, bufferStrides, stridesCount,
            rasterizedStreamIndex, linkage, &tempPtr);
        return D3DDevice_FinalizeCreate(tempPtr, hresult, nullptr);
    }

    intrusive_ptr<ID3D::DomainShader> ObjectFactory::CreateDomainShader(const void* data, size_t size, ID3D11ClassLinkage* linkage) const
    {
        ScopedLock(_attachedData->_creationLock);
        return D3DDeviceCreate<ID3D::DomainShader>(_device.get(), &ID3D::Device::CreateDomainShader, data, size, linkage);
    }

    intrusive_ptr<ID3D::HullShader> ObjectFactory::CreateHullShader(const void* data, size_t size, ID3D11ClassLinkage* linkage) const
    {
        ScopedLock(_attachedData->_creationLock);
        return D3DDeviceCreate<ID3D::HullShader>(_device.get(), &ID3D::Device::CreateHullShader, data, size, linkage);
    }
        /// @}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        /// @{
        /// Misc
    intrusive_ptr<ID3D::DeviceContext> ObjectFactory::CreateDeferredContext() const
    {
        ScopedLock(_attachedData->_creationLock);
        ID3D::DeviceContext* tempPtr = nullptr;
        auto hresult = _device->CreateDeferredContext(0, &tempPtr);
        return D3DDevice_FinalizeCreate(tempPtr, hresult, nullptr);
    }

    intrusive_ptr<ID3D::InputLayout> ObjectFactory::CreateInputLayout(
        const D3D11_INPUT_ELEMENT_DESC inputElements[], unsigned inputElementsCount,
        const void * byteCode, size_t byteCodeSize) const
    {
        ScopedLock(_attachedData->_creationLock);
        ID3D::InputLayout* tempPtr = nullptr;
        auto hresult = _device->CreateInputLayout(inputElements, inputElementsCount, byteCode, byteCodeSize, &tempPtr);
        return D3DDevice_FinalizeCreate(tempPtr, hresult, nullptr);
    }

    intrusive_ptr<ID3D::Query> ObjectFactory::CreateQuery(const D3D11_QUERY_DESC* desc) const
    {
        ScopedLock(_attachedData->_creationLock);
        return D3DDeviceCreate<ID3D::Query>(_device.get(), &ID3D::Device::CreateQuery, desc);
    }
        /// @}

    ObjectFactory::ObjectFactory(IDevice* device)
    : _device(ExtractUnderlyingDevice(device))
    {
        _attachedData = InitAttachedData(_device.get());
    }

    ObjectFactory::ObjectFactory(ID3D::Device& device)
    : _device(&device)
    {
        _attachedData = InitAttachedData(_device.get());
    }

    ObjectFactory::ObjectFactory(ID3D::Resource& resource)
    {
        ID3D::Device* deviceTemp = nullptr;
        resource.GetDevice(&deviceTemp);
        _device = moveptr(deviceTemp);
        _attachedData = InitAttachedData(_device.get());
    }

    ObjectFactory::ObjectFactory()
    : _device(GetDefaultUnderlyingDevice())
    {
        _attachedData = InitAttachedData(_device.get());
    }

    ObjectFactory::~ObjectFactory() {}
    ObjectFactory::ObjectFactory(const ObjectFactory& cloneFrom) : _device(cloneFrom._device), _attachedData(cloneFrom._attachedData) {}
    ObjectFactory& ObjectFactory::operator=(const ObjectFactory& cloneFrom)
    {
        _device = cloneFrom._device;
        _attachedData = cloneFrom._attachedData;
        return *this;
    }

    ObjectFactory::ObjectFactory(ObjectFactory&& moveFrom) never_throws : _device(std::move(moveFrom._device)), _attachedData(std::move(moveFrom._attachedData)) {}
    ObjectFactory& ObjectFactory::operator=(ObjectFactory&& moveFrom) never_throws
    {
        _device = std::move(moveFrom._device);
        _attachedData = std::move(moveFrom._attachedData);
        return *this;
    }

    void ObjectFactory::PrepareDevice(ID3D::Device& device)
    {
        InitAttachedData(&device);
    }

    void ObjectFactory::ReleaseDevice(ID3D::Device& device)
    {
        device.SetPrivateDataInterface(ObjectFactory::AttachedData::Guid, nullptr);
    }

    auto ObjectFactory::InitAttachedData(ID3D::Device* device) -> intrusive_ptr<AttachedData>
    {
        intrusive_ptr<AttachedData> result;
        if (device) {
            ObjectFactory::AttachedData* tempPtr = nullptr;
            unsigned size = sizeof(tempPtr);
            auto hresult = device->GetPrivateData(ObjectFactory::AttachedData::Guid, &size, (void*)&tempPtr);
            if (SUCCEEDED(hresult) && tempPtr && size == sizeof(tempPtr)) {
                result = tempPtr;    // we take our own reference here
            }

            if (!result) {
                result = make_intrusive<AttachedData>();
                device->SetPrivateDataInterface(ObjectFactory::AttachedData::Guid, result.get());
            }
        }
        return std::move(result);
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    CommandList::CommandList(ID3D::CommandList* underlying)
    :   _underlying(underlying)
    {
    }

    CommandList::CommandList(intrusive_ptr<ID3D::CommandList>&& underlying)
    :   _underlying(std::forward<intrusive_ptr<ID3D::CommandList>>(underlying))
    {}

}}

intrusive_ptr<RenderCore::Metal_DX11::Underlying::Resource>;
