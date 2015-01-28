// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Device.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/WinAPI/WinAPIWrapper.h"
#include "../../Core/Exceptions.h"
#include <type_traits>
#include <assert.h>

#include "Metal/DX11Utils.h"
#include "dxgidebug.h"

namespace RenderCore
{
    static ID3D::Device* gDefaultDevice = nullptr;
    static void DumpAllDXGIObjects();

    //////////////////////////////////////////////////////////////////////////////////////////////////

    intrusive_ptr<IDXGI::Adapter> SelectAdapter() { return intrusive_ptr<IDXGI::Adapter>(); };

    static HRESULT D3D11CreateDevice_Wrapper(
        IDXGI::Adapter* pAdapter,
        D3D_DRIVER_TYPE DriverType,
        HMODULE Software,
        UINT Flags,
        const D3D_FEATURE_LEVEL* pFeatureLevels,
        UINT FeatureLevels,
        UINT SDKVersion,
        ID3D::Device** ppDevice,
        D3D_FEATURE_LEVEL* pFeatureLevel,
        ID3D::DeviceContext** ppImmediateContext )
    {
        // This is a wrapped for the D3DCreateDevice() call from the D3D DLL. It allows is to
        // manually load the DLL. If we use the D3D library version directly, the OS will attempt
        // to load the D3D dll as the executable is loading. We don't have any way to catch
        // errors in the case. So if there are any problems (eg, DirectX isn't installed, or it's
        // the wrong version), the user will just we the OS error dialog box. 
        //
        // But, if we bind the dll in this way, we have some control over what happens. We can
        // pop up a more informative error box for the user.
        //
        // We could also do some verification here to check for cases where someone has injected
        // a custom dll in place of the real dll here.
        // Note that if the module is opened successfully, then we never close it.

        static HMODULE module = (HMODULE)INVALID_HANDLE_VALUE;
        if (module == INVALID_HANDLE_VALUE) {
            module = (*Windows::Fn_LoadLibrary)("d3d11.dll");
        }
        if (!module || module == INVALID_HANDLE_VALUE) {
            ThrowException(::Exceptions::BasicLabel("Could not load D3D11 library"));
        }

        auto fn = (PFN_D3D11_CREATE_DEVICE)(*Windows::Fn_GetProcAddress)(module, "D3D11CreateDevice");
        if (!fn) {
            (*Windows::FreeLibrary)(module);
            module = (HMODULE)INVALID_HANDLE_VALUE;
            ThrowException(::Exceptions::BasicLabel("D3D11 library appears corrupt"));
        }

        return (*fn)(
            pAdapter, DriverType, Software, Flags, 
            pFeatureLevels, FeatureLevels, 
            SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);
    }

    Device::Device() : _featureLevel(D3D_FEATURE_LEVEL(~unsigned(0x0)))
    {
            //
            //      Create an underlying D3D device with all of the sensible (typical) defaults
            //
        auto adapter = SelectAdapter();

        #if defined(_DEBUG)
            unsigned deviceCreationFlags = D3D11_CREATE_DEVICE_DEBUG;
        #else
            unsigned deviceCreationFlags = 0;
        #endif

        // deviceCreationFlags |= D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS;

        const D3D_FEATURE_LEVEL featureLevelsToTarget[] = {
            #if DX_VERSION >= DX_VERSION_11_1             // Note -- this is causing problems when running on a machine that doesn't have the Win8 SDK redist installed. We're getting back an "INVALID_ARG" hresult from DirectX
                D3D_FEATURE_LEVEL_11_1,
            #endif
            D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1 
        };

        intrusive_ptr<ID3D::Device> underlying;
        intrusive_ptr<ID3D::DeviceContext> immediateContext;
        
        auto hresult = S_OK;
        {
            ID3D::Device* deviceTemp = 0;
            ID3D::DeviceContext* contextTemp = 0;

            hresult = D3D11CreateDevice_Wrapper(
                adapter.get(), D3D_DRIVER_TYPE_HARDWARE, NULL,
                deviceCreationFlags, featureLevelsToTarget, dimof(featureLevelsToTarget),
                D3D11_SDK_VERSION, &deviceTemp, &_featureLevel, &contextTemp);

                //
                //   Note --  std::move here avoids adding extra references.
                //            intrusive_ptr::operator= should never throw 
                //              (it invokes release, but destructors should suppress exceptions)
                //            So we should be safe to assign these 2 pointers.
                //
            underlying = moveptr(deviceTemp);
            immediateContext = moveptr(contextTemp);
        }

        if (!SUCCEEDED(hresult)) {
            ThrowException(Exceptions::BasicLabel("Failure in D3D11 device construction. Aborting."));
        }

            //  Once we know there can be no more exceptions thrown, we can commit
            //  locals to the members.
        std::swap(_underlying, underlying);
        std::swap(_immediateContext, immediateContext);
    }

    Device::~Device()
    {
        _underlying.reset();
        _immediateContext.reset();

            //  After we've released our references, dump a list of all 
            //  active objects. Ideally we want to have no active objects
            //  after the device is released (if there are any active
            //  child objects, it will prevent the device from being destroyed).
        DumpAllDXGIObjects();
    }

    static void DumpAllDXGIObjects()
    {
            //  We can use the "IDXGIDebug" interface to dump a list of all live DXGI objects.
            //  the libraries for this don't seem to be always available in every winsdk; so let's
            //  call this in a way that doesn't require any extra .lib files.
        #if defined(_DEBUG)
            auto debugModule = LoadLibrary(TEXT("dxgidebug.dll"));
            auto rawProc = GetProcAddress(debugModule, "DXGIGetDebugInterface");
            typedef HRESULT WINAPI getDebugInterfaceType(REFIID, void **);
            auto proc = (getDebugInterfaceType*)rawProc;
            if (proc) {
                IDXGIDebug* debugInterface = nullptr;
                auto hresult = (*proc)(__uuidof(IDXGIDebug), (void**)&debugInterface);
                if (SUCCEEDED(hresult) && debugInterface) {
                    const GUID debugAll = { 0xe48ae283, 0xda80, 0x490b, 0x87, 0xe6, 0x43, 0xe9, 0xa9, 0xcf, 0xda, 0x8 };
                    debugInterface->ReportLiveObjects(debugAll, DXGI_DEBUG_RLO_ALL);
                    debugInterface->Release();
                }
            }

            FreeLibrary(debugModule);
        #endif
    }

    template<typename DestinationType, typename SourceType>
        intrusive_ptr<DestinationType> GetParent(intrusive_ptr<SourceType> child)
        {
            DestinationType* tempPtr = 0;
            auto hresult = child->GetParent(__uuidof(DestinationType), (void**)&tempPtr);
            if (!SUCCEEDED(hresult)) {
                if (tempPtr) { tempPtr->Release(); }
                return intrusive_ptr<DestinationType>();
            }
            return moveptr(tempPtr);
        }

    intrusive_ptr<IDXGI::Factory>      Device::GetDXGIFactory()
    {
        if (auto dxgiDevice = Metal_DX11::QueryInterfaceCast<IDXGI::Device>(_underlying)) {
            if (auto dxgiAdapter = GetParent<IDXGI::Adapter>(dxgiDevice)) {
                return GetParent<IDXGI::Factory>(dxgiAdapter);
            }
        }
        return intrusive_ptr<IDXGI::Factory>();
    }

    std::unique_ptr<IPresentationChain>   Device::CreatePresentationChain(const void* platformValue, unsigned width, unsigned height)
    {
        intrusive_ptr<IDXGI::Factory> factory = GetDXGIFactory();
        if (!factory) {
            return std::unique_ptr<IPresentationChain>();
        }

        DXGI_MODE_DESC modeDesc = {
            width, height, {0,0}, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 
            DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED, DXGI_MODE_SCALING_UNSPECIFIED };
        DXGI_SAMPLE_DESC sampleDesc  = { 1, 0 };     // (no multi-sampling)
        const auto backBufferCount   = 2u;
        const auto windowed          = true;
        DXGI_SWAP_CHAIN_DESC swapChainDesc = {
            modeDesc, sampleDesc,
            DXGI_USAGE_BACK_BUFFER | DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT,       // (select shader input, etc, in order to provide greater access to the back buffer)
            backBufferCount, HWND(platformValue),
            windowed, DXGI_SWAP_EFFECT_DISCARD, 
            DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
        };

        intrusive_ptr<IDXGI::SwapChain> result;
        auto hresult = S_OK;
        {
            IDXGI::SwapChain* swapChainTemp = 0;
            hresult = factory->CreateSwapChain(_underlying.get(), &swapChainDesc, &swapChainTemp);
            result = moveptr(swapChainTemp);
        }
        if (!SUCCEEDED(hresult)) {
            ThrowException(Exceptions::BasicLabel("Failure while constructing a swap chain. Aborting."));
        }

            //
            //      Build a window association here.
            //
        factory->MakeWindowAssociation(HWND(platformValue), 0);

        return std::make_unique<PresentationChain>(std::move(result), platformValue);
    }

    void    Device::BeginFrame(IPresentationChain* presentationChain)
    {
        Assets::CompileAndAsyncManager::GetInstance().Update(); // todo -- move this update somewhere else!
        PresentationChain* swapChain = checked_cast<PresentationChain*>(presentationChain);
        swapChain->AttachToContext(_immediateContext.get(), _underlying.get());
    }

    extern char VersionString[];
    extern char BuildDateString[];
        
    std::pair<const char*, const char*> Device::GetVersionInformation()
    {
        return std::make_pair(VersionString, BuildDateString);
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////

    #if !FLEX_USE_VTABLE_Device && !DOXYGEN
        namespace Detail
        {
            void* Ignore_Device::QueryInterface(const GUID& guid)
            {
                return nullptr;
            }
        }
    #endif

    void*                   DeviceDX11::QueryInterface(const GUID& guid)
    {
        if (guid == __uuidof(Base_DeviceDX11)) {
            return (IDeviceDX11*)this;
        }
        return nullptr;
    }

    ID3D::Device*           DeviceDX11::GetUnderlyingDevice()
    {
        return _underlying.get();
    }

    ID3D::DeviceContext*    DeviceDX11::GetImmediateContext()
    {
        return _immediateContext.get();
    }

    DeviceDX11::DeviceDX11()
    {
        gDefaultDevice = _underlying.get();
    }

    DeviceDX11::~DeviceDX11()
    {
        if (gDefaultDevice == _underlying.get()) {
            gDefaultDevice = nullptr;
        }
        _immediateContext->Flush();
    }

    ID3D::Device*        GetDefaultUnderlyingDevice()
    {
        return gDefaultDevice;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////

    PresentationChain::PresentationChain(intrusive_ptr<IDXGI::SwapChain> underlying, const void* attachedWindow)
    : _underlying(std::move(underlying))
    , _attachedWindow(attachedWindow)
    {
    }

    PresentationChain::~PresentationChain()
    {
    }

    void            PresentationChain::Present()
    {
        _underlying->Present(0, 0);
    }

    void            PresentationChain::Resize(unsigned newWidth, unsigned newHeight)
    {
        if (newWidth == 0 || newHeight == 0) {
            if (_attachedWindow != INVALID_HANDLE_VALUE) {
                RECT clientRect; 
                GetClientRect(HWND(_attachedWindow), &clientRect);
                newWidth = clientRect.right - clientRect.left;
                newHeight = clientRect.bottom - clientRect.top;
            } else {
                ThrowException(Exceptions::BasicLabel("Cannot resize because this presentation chain isn't attached to a window."));
            }
        }

        const auto backBufferCount   = 2u;
        _underlying->ResizeBuffers(
            backBufferCount, 
            newWidth, newHeight, 
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
    
        _defaultDepthTarget.reset(0);
    }

    PresentationChainDesc   PresentationChain::GetDesc() const
    {
        PresentationChainDesc result;
        DXGI_SWAP_CHAIN_DESC dxgiDesc;
        _underlying->GetDesc(&dxgiDesc);
        result._width = dxgiDesc.BufferDesc.Width;
        result._height = dxgiDesc.BufferDesc.Height;
        return result;
    }

    void PresentationChain::AttachToContext(ID3D::DeviceContext* context, ID3D::Device* device)
    {
            //
            //      DavidJ -- temporary implementation -- replace with better management of 
            //                  render target types in the future
            //
            
        IDXGI::SwapChain* swapChain = _underlying.get();
        ID3D::Texture2D* backBuffer0Temp = nullptr;
        HRESULT hresult = swapChain->GetBuffer(0, __uuidof(ID3D::Texture2D), (void**)&backBuffer0Temp);
        intrusive_ptr<ID3D::Texture2D> backBuffer0 = moveptr(backBuffer0Temp);
        if (SUCCEEDED(hresult)) {
            Metal_DX11::TextureDesc2D textureDesc(backBuffer0.get());
            D3D11_VIEWPORT viewport;
            viewport.TopLeftX = 0.f;
            viewport.TopLeftY = 0.f;
            viewport.Width = float(textureDesc.Width);
            viewport.Height = float(textureDesc.Height);
            viewport.MinDepth = 0.f;
            viewport.MaxDepth = 1.f;
            context->RSSetViewports(1, &viewport);

            if (!_defaultDepthTarget) {
                D3D11_TEXTURE2D_DESC depthDesc = textureDesc;
                depthDesc.Format             = DXGI_FORMAT_R24G8_TYPELESS;
                depthDesc.Usage              = D3D11_USAGE_DEFAULT;
                depthDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
                depthDesc.CPUAccessFlags     = 0;

                ID3D::Texture2D* tempDepthTarget;
                hresult = device->CreateTexture2D(&depthDesc, nullptr, &tempDepthTarget);
                if (tempDepthTarget) {
                    if (SUCCEEDED(hresult)) {
                        _defaultDepthTarget = moveptr(tempDepthTarget);
                    } else tempDepthTarget->Release();
                }
            }

            intrusive_ptr<ID3D::DepthStencilView> depthStencilView;
            if (_defaultDepthTarget) {
                ID3D::DepthStencilView* tempDepthView = nullptr;
                D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc;
                viewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
                viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                viewDesc.Flags = 0;
                viewDesc.Texture2D.MipSlice = 0;
                hresult = device->CreateDepthStencilView(_defaultDepthTarget.get(), &viewDesc, &tempDepthView);
                if (tempDepthView) {
                    if (SUCCEEDED(hresult)) {
                        depthStencilView = moveptr(tempDepthView);
                    } else tempDepthView->Release();
                }
            }

                //  By default, when writing to this texture, we should
                //  write in "SRGB" mode. This will automatically convert
                //  from linear colour outputs into SRGB space (and do the
                //  blending correctly in linear space).
                //      ... however, there are some cases were we want to
                //      write to this buffer in non-SRGB mode (ie, we will
                //      manually apply SRGB convertion)
            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
            rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice = 0;

            ID3D::RenderTargetView* rtvTemp = nullptr;
            hresult = device->CreateRenderTargetView(backBuffer0.get(), &rtvDesc, &rtvTemp);
            intrusive_ptr<ID3D::RenderTargetView> rtv = moveptr(rtvTemp);
            if (SUCCEEDED(hresult)) {
                ID3D::RenderTargetView* t = rtv.get();

                // FLOAT clearColor[] = {1.f, 0.25f, 0.25f, 0.25f};
                // context->ClearRenderTargetView(t, clearColor);

                if (depthStencilView.get()) {
                    context->ClearDepthStencilView(depthStencilView.get(), D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.f, 0);
                }

                context->OMSetRenderTargets(1, &t, depthStencilView.get());
            }
        }
    }


    //////////////////////////////////////////////////////////////////////////////////////////////////

    render_dll_export std::unique_ptr<IDevice>    CreateDevice()
    {
        return std::make_unique<DeviceDX11>();
    }
}

