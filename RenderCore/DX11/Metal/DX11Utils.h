// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DX11.h"
#include "IncludeDX11.h"
#include "../../Utility/IntrusivePtr.h"
#include <assert.h>

typedef struct _D3D11_SHADER_TYPE_DESC D3D11_SHADER_TYPE_DESC;

namespace Utility { namespace ImpliedTyping { class TypeDesc; }}

namespace RenderCore { namespace Metal_DX11
{
        ////////////////////////////////////////////////////////////////
            //      Q U E R Y   D E S C
        ////////////////////////////////////////////////////////////////

    template <typename Type> class D3DTypeInfo {};
    template <> class D3DTypeInfo<ID3D::Texture1D> 
    {
    public:
        typedef D3D11_TEXTURE1D_DESC Desc;
    };
    template <> class D3DTypeInfo<ID3D::Texture2D> 
    {
    public:
        typedef D3D11_TEXTURE2D_DESC Desc;
    };
    template <> class D3DTypeInfo<ID3D::Texture3D> 
    {
    public:
        typedef D3D11_TEXTURE3D_DESC Desc;
    };

    template <> class D3DTypeInfo<ID3D::Buffer> 
    {
    public:
        typedef D3D11_BUFFER_DESC Desc;
    };

    template <typename TextureType> class TextureDesc : public D3DTypeInfo<TextureType>::Desc
    {
    public:
        TextureDesc( ID3D::RenderTargetView * rtv );
        TextureDesc( ID3D::ShaderResourceView * srv );
        TextureDesc( ID3D::Resource * srv );
        TextureDesc( TextureType * t2d );
    };

    template <typename ResourceType>
        intrusive_ptr<ResourceType> ExtractResource( ID3D::RenderTargetView * rtv )
    {
        assert(rtv);
        ID3D::Resource * resourceUnknownPtr = NULL;
        rtv->GetResource( &resourceUnknownPtr );
        if (resourceUnknownPtr) {
            intrusive_ptr<ID3D::Resource> resourceUnknown( resourceUnknownPtr, false );
            return QueryInterfaceCast<ResourceType>( resourceUnknown );
        }
        return intrusive_ptr<ResourceType>();
    }

    template <typename ResourceType>
        intrusive_ptr<ResourceType> ExtractResource( ID3D::ShaderResourceView * srv )
    {
        assert(srv);
        ID3D::Resource * resourceUnknownPtr = NULL;
        srv->GetResource( &resourceUnknownPtr );
        if (resourceUnknownPtr) {
            intrusive_ptr<ID3D::Resource> resourceUnknown( resourceUnknownPtr, false );
            return QueryInterfaceCast<ResourceType>( resourceUnknown );
        }
        return intrusive_ptr<ResourceType>();
    }

    template <typename ResourceType>
        intrusive_ptr<ResourceType> ExtractResource( ID3D::DepthStencilView * dsv )
    {
        assert(dsv);
        ID3D::Resource * resourceUnknownPtr = NULL;
        dsv->GetResource( &resourceUnknownPtr );
        if (resourceUnknownPtr) {
            intrusive_ptr<ID3D::Resource> resourceUnknown( resourceUnknownPtr, false );
            return QueryInterfaceCast<ResourceType>( resourceUnknown );
        }
        return intrusive_ptr<ResourceType>();
    }

    template <typename TextureType>
        TextureDesc<TextureType>::TextureDesc( ID3D::RenderTargetView * rtv )
    {
        assert(rtv);
        intrusive_ptr<TextureType> texture = ExtractResource<TextureType>( rtv );
        if (texture) { texture->GetDesc( this ); }
        else         { XlZeroMemory( *this ); }
    }

    template <typename TextureType>
        TextureDesc<TextureType>::TextureDesc( ID3D::ShaderResourceView * srv )
    {
        assert(srv);
        intrusive_ptr<TextureType> texture = ExtractResource<TextureType>( srv );
        if (texture) { texture->GetDesc( this ); }
        else         { XlZeroMemory( *this ); }
    }

    template <typename TextureType>
        TextureDesc<TextureType>::TextureDesc( ID3D::Resource * resource )
    {
        assert(resource);
        intrusive_ptr<TextureType> texture = QueryInterfaceCast<TextureType>( resource );
        if (texture) { texture->GetDesc( this ); }
        else         { XlZeroMemory( *this ); }
    }

    template <typename TextureType>
        TextureDesc<TextureType>::TextureDesc( TextureType * t2d )
    {
        assert(t2d);
        t2d->GetDesc( this );
    }

    typedef TextureDesc<ID3D::Texture1D>    TextureDesc1D;
    typedef TextureDesc<ID3D::Texture2D>    TextureDesc2D;
    typedef TextureDesc<ID3D::Texture3D>    TextureDesc3D;
    typedef TextureDesc<ID3D::Buffer>       D3DBufferDesc;
    
            ////////////////////////////////////////////////////////////////
                //      Q U E R Y   I N T E R F A C E
            ////////////////////////////////////////////////////////////////
            
    template< typename DestinationType, typename SourceType >
        intrusive_ptr<DestinationType> QueryInterfaceCast(SourceType * sourceObject)
    {
        void * result = NULL;
        HRESULT status = sourceObject->QueryInterface(__uuidof(DestinationType), &result);
        if (status != S_OK) {
            if (result) ((DestinationType*)result)->Release();
            return intrusive_ptr<DestinationType>();
        }
        // return moveptr((DestinationType*)result);    (problem with multiple copies of intrusive_ptr)
        return intrusive_ptr<DestinationType>((DestinationType*)result, false);
    }

    template <typename DestinationType, typename SourceType>
        intrusive_ptr<DestinationType> QueryInterfaceCast(intrusive_ptr<SourceType>& sourceObject)
    {
        void * result = NULL;
        HRESULT status = sourceObject->QueryInterface(__uuidof(DestinationType), &result);
        if (status != S_OK) {
            if (result) ((DestinationType*)result)->Release();
            return intrusive_ptr<DestinationType>();
        }
        // return moveptr((DestinationType*)result);    (problem with multiple copies of intrusive_ptr)
        return intrusive_ptr<DestinationType>((DestinationType*)result, false);
    }

            ////////////////////////////////////////////////////////////////
                //      D U P L I C A T E   R E S O U R C E
            ////////////////////////////////////////////////////////////////

    template<typename Type>
        intrusive_ptr<Type> DuplicateResource(ID3D::DeviceContext* context, Type* inputResource);

    template<>
        inline intrusive_ptr<ID3D::Resource> DuplicateResource(ID3D::DeviceContext* context, ID3D::Resource* inputResource)
    {
        if (auto buffer = QueryInterfaceCast<ID3D::Buffer>(inputResource)) {
            D3DBufferDesc bufferDesc(buffer.get());
            ID3D::Buffer* newBuffer = nullptr;
            ID3D::Device* device = nullptr;
            context->GetDevice(&device);
            if (device) {
                auto hresult = device->CreateBuffer(&bufferDesc, nullptr, &newBuffer);
                device->Release();
                if (SUCCEEDED(hresult)) {
                    context->CopyResource(newBuffer, inputResource);
                    ID3D::Resource* result = nullptr;
                    newBuffer->QueryInterface(__uuidof(ID3D::Resource), (void**)&result);
                    newBuffer->Release();
                    return intrusive_ptr<ID3D::Resource>(result, false);
                } else {
                    if (newBuffer) {
                        newBuffer->Release();
                    }
                }
            }
        } else if (auto texture2D = QueryInterfaceCast<ID3D::Texture2D>(inputResource)) {
            TextureDesc2D textureDesc(texture2D.get());
            ID3D::Texture2D* newTexture = nullptr;
            ID3D::Device* device = nullptr;
            context->GetDevice(&device);
            if (device) {
                auto hresult = device->CreateTexture2D(&textureDesc, nullptr, &newTexture);
                device->Release();
                if (SUCCEEDED(hresult)) {
                    context->CopyResource(newTexture, inputResource);
                    ID3D::Resource* result = nullptr;
                    newTexture->QueryInterface(__uuidof(ID3D::Resource), (void**)&result);
                    newTexture->Release();
                    return intrusive_ptr<ID3D::Resource>(result, false);
                } else {
                    if (newTexture) {
                        newTexture->Release();
                    }
                }
            }
        }

        return nullptr;
    }

    template<>
        inline intrusive_ptr<ID3D::Buffer> DuplicateResource(ID3D::DeviceContext* context, ID3D::Buffer* inputResource)
    {
        D3DBufferDesc bufferDesc(inputResource);
        ID3D::Buffer* newBuffer = nullptr;
        ID3D::Device* device = nullptr;
        context->GetDevice(&device);
        if (device) {
                //  we can't make this buffer "immutable" because we want to call
                //  CopyResource() just below. So immutable -> default
            if (bufferDesc.Usage == D3D11_USAGE_IMMUTABLE) {
                bufferDesc.Usage = D3D11_USAGE_DEFAULT;
            }
            auto hresult = device->CreateBuffer(&bufferDesc, nullptr, &newBuffer);
            device->Release();
            if (SUCCEEDED(hresult)) {
                context->CopyResource(newBuffer, inputResource);
                return intrusive_ptr<ID3D::Buffer>(newBuffer, false);
            } else {
                if (newBuffer) {
                    newBuffer->Release();
                }
            }
        }

        return nullptr;
    }

            ////////////////////////////////////////////////////////////////
                //      H L S L   U T I L S
            ////////////////////////////////////////////////////////////////

    Utility::ImpliedTyping::TypeDesc GetType(D3D11_SHADER_TYPE_DESC typeDesc);

}}

