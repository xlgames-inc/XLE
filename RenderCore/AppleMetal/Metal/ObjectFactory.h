//
//  ObjectFactory.h
//  RenderCore_AppleMetal
//
//  Created by Ken Domke on 2/13/18.
//

#pragma once

#include "../../../Externals/Misc/OCPtr.h"

@protocol MTLDevice;
@protocol MTLTexture;
@protocol MTLBuffer;
@protocol MTLSamplerState;
@class MTLTextureDescriptor;
@class MTLSamplerDescriptor;

/* KenD -- could switch all of these typedefs from NSObject with a protocol to simply `id`
 * However, cannot have `OCPtr<id<MTLTexture>>` because OCPtr will not work; ObjType will be `id<MTLTexture> *`, which is
 * off because `id` is already a pointer.  `OCPtr<NSObject<MTLTexture>>` is fine because ObjType will be `NSObject<MTLTexture> *`, which is fine.
 * `OCPtr<id>` is also fine.
 */
typedef NSObject<MTLTexture> AplMtlTexture;
typedef NSObject<MTLBuffer> AplMtlBuffer;
typedef NSObject<MTLSamplerState> AplMtlSamplerState;
typedef NSObject<MTLDevice> AplMtlDevice;

namespace RenderCore { class IDevice; }

namespace RenderCore { namespace Metal_AppleMetal
{
    /* KenD -- void* instead for RawMTLHandle? */
    using RawMTLHandle = uint64_t;
    static const RawMTLHandle RawMTLHandle_Invalid = 0;

    class ObjectFactory
    {
    public:
        TBC::OCPtr<AplMtlTexture> CreateTexture(MTLTextureDescriptor* textureDesc); // <MTLTexture>
        TBC::OCPtr<AplMtlBuffer> CreateBuffer(const void* bytes, unsigned length); // <MTLBuffer>
        TBC::OCPtr<AplMtlSamplerState> CreateSamplerState(MTLSamplerDescriptor* samplerDesc); // <MTLSamplerState>

        const TBC::OCPtr<AplMtlTexture>& StandIn2DTexture()     { return _standIn2DTexture; }
        const TBC::OCPtr<AplMtlTexture>& StandInCubeTexture()   { return _standInCubeTexture; }
        const TBC::OCPtr<AplMtlSamplerState>& StandInSamplerState() { return _standInSamplerState; }

        ObjectFactory(id<MTLDevice> mtlDevice);
        ObjectFactory() = delete;
        ~ObjectFactory();

        ObjectFactory& operator=(const ObjectFactory&) = delete;
        ObjectFactory(const ObjectFactory&) = delete;
    private:
        TBC::OCPtr<AplMtlDevice> _mtlDevice; // <MTLDevice>

        TBC::OCPtr<AplMtlTexture> _standIn2DTexture;
        TBC::OCPtr<AplMtlTexture> _standInCubeTexture;
        TBC::OCPtr<AplMtlSamplerState> _standInSamplerState;
    };

    class DeviceContext;

    ObjectFactory& GetObjectFactory(IDevice& device);
    ObjectFactory& GetObjectFactory(DeviceContext&);
    ObjectFactory& GetObjectFactory();

    void CheckGLError(const char context[]);
}}
