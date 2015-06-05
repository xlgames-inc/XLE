// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../../Core/Prefix.h"

    //
    //      Forward declarations for many DX11 types.
    //
    //      Most the Metal interface for DX11 can be included without DX11.h
    //      by just forward declarations for the DX11 types (as so...)
    //
struct ID3D11Texture2D;
struct ID3D11RenderTargetView;
struct ID3D11ShaderResourceView;
struct ID3D11DepthStencilView;
struct ID3D11Resource;
struct ID3D11DeviceContext;
struct ID3D11Device;
struct ID3D11CommandList;
struct ID3D11Buffer;
struct ID3D11Texture1D;
struct ID3D11Texture3D;
struct ID3D11DeviceChild;
struct ID3D11Query;
struct ID3D11View;
struct ID3D10Blob;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11GeometryShader;
struct ID3D11ComputeShader;
struct ID3D11HullShader;
struct ID3D11DomainShader;
struct ID3D11InputLayout;
struct ID3D11SamplerState;
struct ID3D11RasterizerState;
struct ID3D11BlendState;
struct ID3D11DepthStencilState;
struct ID3D11ShaderReflection;
struct ID3D11ShaderReflectionConstantBuffer;
struct ID3D11ShaderReflectionVariable;
struct ID3D11UnorderedAccessView;
struct ID3D11ClassLinkage;
struct ID3D11ClassInstance;
struct IUnknown;

struct ID3D11Resource1;
struct ID3D11DeviceChild1;
struct ID3D11DeviceContext1;
struct ID3D11Device1;

struct ID3DUserDefinedAnnotation;

struct _GUID;
typedef struct _GUID GUID;
typedef long HRESULT;
enum D3D_FEATURE_LEVEL;

struct IDXGIDevice1;
struct IDXGISwapChain;
struct IDXGIAdapter1;
struct IDXGIFactory1;
typedef enum DXGI_FORMAT DXGI_FORMAT;

typedef struct D3D11_BLEND_DESC D3D11_BLEND_DESC;
typedef struct D3D11_DEPTH_STENCIL_DESC D3D11_DEPTH_STENCIL_DESC;
typedef struct D3D11_RASTERIZER_DESC D3D11_RASTERIZER_DESC;
typedef struct D3D11_SAMPLER_DESC D3D11_SAMPLER_DESC;
typedef struct D3D11_BUFFER_DESC D3D11_BUFFER_DESC;
typedef struct D3D11_TEXTURE1D_DESC D3D11_TEXTURE1D_DESC;
typedef struct D3D11_TEXTURE2D_DESC D3D11_TEXTURE2D_DESC;
typedef struct D3D11_TEXTURE3D_DESC D3D11_TEXTURE3D_DESC;
typedef struct D3D11_SUBRESOURCE_DATA D3D11_SUBRESOURCE_DATA;
typedef struct D3D11_RENDER_TARGET_VIEW_DESC D3D11_RENDER_TARGET_VIEW_DESC;
typedef struct D3D11_SHADER_RESOURCE_VIEW_DESC D3D11_SHADER_RESOURCE_VIEW_DESC;
typedef struct D3D11_UNORDERED_ACCESS_VIEW_DESC D3D11_UNORDERED_ACCESS_VIEW_DESC;
typedef struct D3D11_DEPTH_STENCIL_VIEW_DESC D3D11_DEPTH_STENCIL_VIEW_DESC;
typedef struct D3D11_SO_DECLARATION_ENTRY D3D11_SO_DECLARATION_ENTRY;
typedef struct D3D11_INPUT_ELEMENT_DESC D3D11_INPUT_ELEMENT_DESC;
typedef struct D3D11_QUERY_DESC D3D11_QUERY_DESC;

#define DX_VERSION_11           1
#define DX_VERSION_11_1         2
#define DX_VERSION              DX_VERSION_11_1

namespace ID3D
{
    typedef ID3D11Texture2D                         Texture2D;
    typedef ID3D11Texture2D                         TextureCube;
    typedef ID3D11RenderTargetView                  RenderTargetView;
    typedef ID3D11ShaderResourceView                ShaderResourceView;
    typedef ID3D11DepthStencilView                  DepthStencilView;
    typedef ID3D11CommandList                       CommandList;
    typedef ID3D11Buffer                            Buffer;
    typedef ID3D11Texture1D                         Texture1D;
    typedef ID3D11Texture3D                         Texture3D;
    typedef ID3D11Query                             Query;
    typedef ID3D11View                              View;
    typedef ID3D10Blob                              Blob;
    typedef ID3D11VertexShader                      VertexShader;
    typedef ID3D11PixelShader                       PixelShader;
    typedef ID3D11GeometryShader                    GeometryShader;
    typedef ID3D11ComputeShader                     ComputeShader;
    typedef ID3D11HullShader                        HullShader;
    typedef ID3D11DomainShader                      DomainShader;
    typedef ID3D11InputLayout                       InputLayout;
    typedef ID3D11SamplerState                      SamplerState;
    typedef ID3D11RasterizerState                   RasterizerState;
    typedef ID3D11BlendState                        BlendState;
    typedef ID3D11DepthStencilState                 DepthStencilState;
    typedef ID3D11ShaderReflection                  ShaderReflection;
    typedef ID3D11ShaderReflectionConstantBuffer    ShaderReflectionConstantBuffer;
    typedef ID3D11ShaderReflectionVariable          ShaderReflectionVariable;
    typedef ID3D11UnorderedAccessView               UnorderedAccessView;
    typedef ID3D11ClassLinkage                      ClassLinkage;
    typedef ID3D11ClassInstance                     ClassInstance;

    #if DX_VERSION == DX_VERSION_11_1
        typedef ID3D11DeviceContext1                DeviceContext1;
        typedef ID3D11Device1                       Device1;
    #endif

    typedef ID3D11DeviceContext                 DeviceContext;
    typedef ID3D11Device                        Device;
    typedef ID3D11DeviceChild                   DeviceChild;
    typedef ID3D11Resource                      Resource;
    typedef ID3DUserDefinedAnnotation           UserDefinedAnnotation;
}

namespace IDXGI
{
    typedef IDXGIDevice1                Device;
    typedef IDXGISwapChain              SwapChain;
    typedef IDXGIAdapter1               Adapter;
    typedef IDXGIFactory1               Factory;
}
