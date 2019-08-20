//
//  ObjectFactory.mm
//  RenderCore_AppleMetal
//
//  Created by Ken Domke on 2/13/18.
//

#include "ObjectFactory.h"
#include <assert.h>

#include "IncludeAppleMetal.h"

namespace RenderCore { namespace Metal_AppleMetal
{
    TBC::OCPtr<AplMtlTexture> ObjectFactory::CreateTexture(MTLTextureDescriptor* textureDesc)
    {
        /* KenD -- this is actually creating Metal textures.
         * Other methods create Resources from MTLTextures (keeping a reference to a MTLTexture),
         * or create Resources from IResourcePtrs (keeping a reference to a MTLTexture).
         */
        assert([_mtlDevice conformsToProtocol:@protocol(MTLDevice)]);
        id<MTLDevice> device = (id<MTLDevice>)_mtlDevice;

        assert(textureDesc.width != 0);
        auto obj = TBC::OCPtr<AplMtlTexture>(TBC::moveptr([device newTextureWithDescriptor:textureDesc]));
        return obj;
    }

    TBC::OCPtr<AplMtlBuffer> ObjectFactory::CreateBuffer(const void* bytes, unsigned length)
    {
        /* KenD -- ideally we could use a private storage mode if the buffer is only meant to be read by the GPU...
         * Documentation for `newBufferWithBytes`:
         * "MTLBuffer objects created with this method are CPU-accessible and can be specified with a
         * MTLResourceStorageModeShared or MTLResourceStorageModeManaged storage mode, but not a
         * MTLResourceStorageModePrivate storage mode."
         */
        assert([_mtlDevice conformsToProtocol:@protocol(MTLDevice)]);
        id<MTLDevice> device = (id<MTLDevice>)_mtlDevice;

        if (bytes) {
            return TBC::OCPtr<AplMtlBuffer>(TBC::moveptr([device newBufferWithBytes:bytes
                length:length
                options:MTLResourceStorageModeShared]));
        } else {
            return TBC::OCPtr<AplMtlBuffer>(TBC::moveptr([device newBufferWithLength:length
                options:MTLResourceStorageModeShared]));
        }
    }

    TBC::OCPtr<AplMtlSamplerState> ObjectFactory::CreateSamplerState(MTLSamplerDescriptor* samplerDesc)
    {
        assert([_mtlDevice conformsToProtocol:@protocol(MTLDevice)]);
        id<MTLDevice> device = (id<MTLDevice>)_mtlDevice;

        return TBC::OCPtr<AplMtlSamplerState>(TBC::moveptr([device newSamplerStateWithDescriptor:samplerDesc]));
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////

    static TBC::OCPtr<AplMtlTexture> CreateStandIn2DTexture(ObjectFactory& factory)
    {
        TBC::OCPtr<MTLTextureDescriptor> textureDesc = TBC::moveptr([[MTLTextureDescriptor alloc] init]);
        textureDesc.get().textureType = MTLTextureType2D;
        textureDesc.get().pixelFormat = MTLPixelFormatRGBA8Unorm;
        textureDesc.get().width = 4;
        textureDesc.get().height = 4;
        textureDesc.get().usage = MTLTextureUsageShaderRead;

        unsigned data[4*4];
        memset(data, 0xff, sizeof(data));

        auto tex = factory.CreateTexture(textureDesc);
        [tex.get() replaceRegion:MTLRegionMake2D(0, 0, 4, 4)
                     mipmapLevel:0
                           slice:0
                       withBytes:data
                     bytesPerRow:4*4
                   bytesPerImage:4*4*4];
        return tex;
    }

    static TBC::OCPtr<AplMtlTexture> CreateStandInCubeTexture(ObjectFactory& factory)
    {
        TBC::OCPtr<MTLTextureDescriptor> textureDesc = TBC::moveptr([[MTLTextureDescriptor alloc] init]);
        textureDesc.get().textureType = MTLTextureTypeCube;
        textureDesc.get().pixelFormat = MTLPixelFormatRGBA8Unorm;
        textureDesc.get().width = 4;
        textureDesc.get().height = 4;
        textureDesc.get().usage = MTLTextureUsageShaderRead;

        unsigned data[4*4];
        memset(data, 0xff, sizeof(data));

        auto tex = factory.CreateTexture(textureDesc);
        for (unsigned f=0; f<6; ++f) {
            [tex.get() replaceRegion:MTLRegionMake2D(0, 0, 4, 4)
                         mipmapLevel:0
                               slice:f
                           withBytes:data
                         bytesPerRow:4*4
                       bytesPerImage:4*4*4];
        }

        return tex;
    }

    static TBC::OCPtr<AplMtlSamplerState> CreateStandInSamplerState(ObjectFactory& factory)
    {
        TBC::OCPtr<MTLSamplerDescriptor> desc = TBC::moveptr([[MTLSamplerDescriptor alloc] init]);
        desc.get().rAddressMode = MTLSamplerAddressModeRepeat;
        desc.get().sAddressMode = MTLSamplerAddressModeRepeat;
        desc.get().tAddressMode = MTLSamplerAddressModeRepeat;
        desc.get().minFilter = MTLSamplerMinMagFilterLinear;
        desc.get().magFilter = MTLSamplerMinMagFilterLinear;
        desc.get().mipFilter = MTLSamplerMipFilterLinear;
        desc.get().compareFunction = MTLCompareFunctionNever;

        auto samplerState = factory.CreateSamplerState(desc);
        return samplerState;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////

    static ObjectFactory* s_objectFactory_instance = nullptr;

    ObjectFactory::ObjectFactory(id<MTLDevice> mtlDevice)
    : _mtlDevice(mtlDevice)
    {
        assert(s_objectFactory_instance == nullptr);
        s_objectFactory_instance = this;

        _standIn2DTexture = CreateStandIn2DTexture(*this);
        _standInCubeTexture = CreateStandInCubeTexture(*this);
        _standInSamplerState = CreateStandInSamplerState(*this);
    }
    ObjectFactory::~ObjectFactory()
    {
        _standIn2DTexture = TBC::OCPtr<AplMtlTexture>();
        _standInCubeTexture = TBC::OCPtr<AplMtlTexture>();
        _standInSamplerState = TBC::OCPtr<AplMtlSamplerState>();

        assert(s_objectFactory_instance == this);
        s_objectFactory_instance = nullptr;
    }

    ObjectFactory& GetObjectFactory(IDevice& device) { assert(s_objectFactory_instance); return *s_objectFactory_instance; }
    ObjectFactory& GetObjectFactory(DeviceContext&) { assert(s_objectFactory_instance); return *s_objectFactory_instance; }
    ObjectFactory& GetObjectFactory() { assert(s_objectFactory_instance); return *s_objectFactory_instance; }

    void CheckGLError(const char context[]) {}
}}
