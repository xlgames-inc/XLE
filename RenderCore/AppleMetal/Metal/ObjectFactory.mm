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

        return TBC::OCPtr<AplMtlBuffer>(TBC::moveptr([device newBufferWithBytes:bytes
            length:length
            options:MTLResourceStorageModeShared]));
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////

    static ObjectFactory* s_objectFactory_instance = nullptr;

    ObjectFactory::ObjectFactory(id<MTLDevice> mtlDevice)
    : _mtlDevice(mtlDevice)
    {
        assert(s_objectFactory_instance == nullptr);
        s_objectFactory_instance = this;
    }
    ObjectFactory::~ObjectFactory()
    {
        assert(s_objectFactory_instance == this);
        s_objectFactory_instance = nullptr;
    }

    ObjectFactory& GetObjectFactory(IDevice& device) { assert(s_objectFactory_instance); return *s_objectFactory_instance; }
    ObjectFactory& GetObjectFactory(DeviceContext&) { assert(s_objectFactory_instance); return *s_objectFactory_instance; }
    ObjectFactory& GetObjectFactory() { assert(s_objectFactory_instance); return *s_objectFactory_instance; }
}}
