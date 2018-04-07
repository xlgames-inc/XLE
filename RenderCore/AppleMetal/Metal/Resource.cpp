// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Resource.h"
#include "Format.h"
#include "../../ResourceUtils.h"
#include "../../../ConsoleRig/Log.h"

#include "IncludeAppleMetal.h"

namespace RenderCore { namespace Metal_AppleMetal
{
    static std::shared_ptr<Resource> AsResource(const IResourcePtr& rp)
    {
        auto* res = (Resource*)rp->QueryInterface(typeid(Resource).hash_code());
        if (!res || res != rp.get())
            Throw(::Exceptions::BasicLabel("Unexpected resource type passed to texture view"));
        return std::static_pointer_cast<Resource>(rp);
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    void* Resource::QueryInterface(size_t guid)
    {
        if (guid == typeid(Resource).hash_code())
            return this;
        return nullptr;
    }

    uint64_t Resource::GetGUID() const
    {
        return _guid;
    }

    static uint64_t s_nextResourceGUID = 0;

    Resource::Resource(
        ObjectFactory& factory, const Desc& desc,
        const SubResourceInitData& initData)
    : Resource(factory, desc, [&initData](SubResourceId subRes) {
            assert(subRes._mip == 0 && subRes._arrayLayer == 0);
            return initData;
        })
    {}

    Resource::Resource(
        ObjectFactory& factory, const Desc& desc,
        const IDevice::ResourceInitializer& initializer)
    : _desc(desc)
    , _guid(s_nextResourceGUID++)
    {
        /* Overview: This is the base constructor for the Resource.
         * The ObjectFactory uses the MTLDevice to create the actual MTLTexture or MTLBuffer.
         */

        if (desc._type == ResourceDesc::Type::Texture) {
            MTLTextureDescriptor* textureDesc = [[MTLTextureDescriptor alloc] init];

            /* Not supporting arrays of 1D or 2D textures at this point */
            if (desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T1D) {
                assert(desc._textureDesc._height == 1);
                assert(desc._textureDesc._arrayCount <= 1);
                textureDesc.textureType = MTLTextureType1D;
            } else if (desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T2D) {
                assert(desc._textureDesc._arrayCount <= 1);
                textureDesc.textureType = MTLTextureType2D;
            } else if (desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T3D) {
                assert(desc._textureDesc._arrayCount <= 1);
                textureDesc.textureType = MTLTextureType3D;
            } else if (desc._textureDesc._dimensionality == TextureDesc::Dimensionality::CubeMap) {
                assert(desc._textureDesc._arrayCount == 6);
                textureDesc.textureType = MTLTextureTypeCube;
            }

            textureDesc.pixelFormat = AsMTLPixelFormat(desc._textureDesc._format);
            assert(textureDesc.pixelFormat != MTLPixelFormatInvalid);

            textureDesc.width = desc._textureDesc._width;
            textureDesc.height = desc._textureDesc._height;
            textureDesc.depth = desc._textureDesc._depth;

            textureDesc.mipmapLevelCount = desc._textureDesc._mipCount;
            textureDesc.sampleCount = desc._textureDesc._samples._sampleCount;
            // In Metal, arrayLength is only set for arrays.  For non-arrays, arrayLength must be 1.
            // That is, the RenderCore arrayCount is not the same as the Metal arrayLength.
            textureDesc.arrayLength = 1;

            // KenD -- leaving unset for now
            // textureDesc.resourceOptions / compare to allocationRules in ResourceDesc
            // textureDesc.cpuCacheMode / Metal documentation suggests this is only worth considering changing if there are known performance issues

            /* KenD -- Metal TODO -- when populating a texture with data by using ReplaceRegion,
             * we cannot have a private storage mode.  Instead of using ReplaceRegion to populate
             * a texture that does not need CPU, prefer to populate it using a blit command encoder
             * and making the storage private.
             * This is also suggested by frame capture.
             * Currently, if CPU access is required, leaving storage mode as default.
             */
            if (desc._cpuAccess == 0 && !initializer) {
                textureDesc.storageMode = MTLStorageModePrivate;
            }

            textureDesc.usage = MTLTextureUsageUnknown;
            if (desc._bindFlags & BindFlag::ShaderResource) {
                textureDesc.usage |= MTLTextureUsageShaderRead;
                if (desc._gpuAccess & GPUAccess::Write) {
                    textureDesc.usage |= MTLTextureUsageShaderWrite;
                }
            }
            if (desc._bindFlags & BindFlag::RenderTarget ||
                desc._bindFlags & BindFlag::DepthStencil) {
                textureDesc.usage |= MTLTextureUsageRenderTarget;
            }

            assert(textureDesc.width != 0);
            _underlyingTexture = factory.CreateTexture(textureDesc);
#if DEBUG
            if (desc._name[0]) {
                [_underlyingTexture.get() setLabel:[NSString stringWithCString:desc._name encoding:NSUTF8StringEncoding]];
            }
#endif
            [textureDesc release];

            unsigned faceCount = 6; // cube map
            if (desc._textureDesc._dimensionality != TextureDesc::Dimensionality::CubeMap) {
                faceCount = 1;
            }

            unsigned bytesPerTexel = BitsPerPixel(desc._textureDesc._format) / 8u;
            /* Metal does not support three-byte formats, so the texture content loader should have
             * expanded a three-byte format into a four-byte format.
             * If not, we skip out early without trying to populate the texture.
             */
            if (bytesPerTexel == 3) {
                assert(0);
                return;
            }

            /* KenD -- note that in Metal, for a cubemap, there are six slices, but arrayCount is still 1.
             * The order of the faces is pretty typical.
             Slice Index    Slice Orientation
             0              +X
             1              -X
             2              +Y
             3              -Y
             4              +Z
             5              -Z
             */

            /* The only BlockCompression type expected to be used with Metal would be PVRTC */
            auto hasPVRTCPixelFormat = GetCompressionType(desc._textureDesc._format) == FormatCompressionType::BlockCompression;

            for (unsigned f=0; f < faceCount; ++f) {
                if (initializer) {
                    for (unsigned m=0; m < desc._textureDesc._mipCount; ++m) {
                        auto mipWidth  = std::max(desc._textureDesc._width >> m, 1u);
                        auto mipHeight = std::max(desc._textureDesc._height >> m, 1u);
                        auto subRes = initializer({m, f});
                        auto bytesPerRow = subRes._pitches._rowPitch;
                        auto bytesPerImage = subRes._pitches._slicePitch;
                        if (hasPVRTCPixelFormat) {
                            /* From Apple documentation on replaceRegion...:
                             *    This method is supported if you are copying to an entire texture with a PVRTC pixel format; in
                             *    which case, bytesPerRow and bytesPerImage must both be set to 0. This method is not
                             *    supported for copying to a subregion of a texture that has a PVRTC pixel format.
                             */
                            bytesPerRow = 0;
                            bytesPerImage = 0;
                        }
                        [_underlyingTexture replaceRegion:MTLRegionMake2D(0, 0, mipWidth, mipHeight)
                                              mipmapLevel:m
                                                    slice:f
                                                withBytes:subRes._data.begin()
                                              bytesPerRow:bytesPerRow
                                            bytesPerImage:bytesPerImage];
                    }
                } else {
                    // KenD -- in the case where we don't have initialization data, leave the texture as is
                }
            }
            //Log(Verbose) << "Created texture resource and might have populated it" << std::endl;
        } else if (desc._type == ResourceDesc::Type::LinearBuffer) {
            if (desc._cpuAccess == 0 && desc._gpuAccess == GPUAccess::Read) {
                // KenD -- the case of creating read-only GPU buffers is supported (used for constant/vertex/index buffers)
                assert(initializer);
                assert(desc._bindFlags & BindFlag::ConstantBuffer || desc._bindFlags & BindFlag::VertexBuffer || desc._bindFlags & BindFlag::IndexBuffer);
                _underlyingBuffer = factory.CreateBuffer(initializer({0,0})._data.begin(), desc._linearBufferDesc._sizeInBytes);
            } else {
                // KenD -- Metal TODO -- support creating linear buffers with different access modes; also consider different binding types
                // Dynamic geo buffer has cpu access write | write dynamic; gpu access read.
                const void* bytes = nullptr;
                if (initializer) {
                    bytes = initializer({0,0})._data.begin();
                }
                _underlyingBuffer = factory.CreateBuffer(bytes, desc._linearBufferDesc._sizeInBytes);
            }
        } else {
            assert(0);
        }
    }

    Resource::Resource(const id<MTLTexture>& texture, const ResourceDesc& desc)
    : _underlyingTexture(texture)
    , _desc(desc)
    , _guid(s_nextResourceGUID++)
    {
        // KenD -- this wraps a MTL resource in an IResource, such as with the drawable for the current framebuffer

        if (![texture conformsToProtocol:@protocol(MTLTexture)]) {
            Throw(::Exceptions::BasicLabel("Creating non-texture as texture resource"));
        }
        //Log(Verbose) << "Created resource from a texture (wrapping a MTLTexture in a Resource; this is done for the current framebuffer)" << std::endl;
    }

    Resource::Resource(const IResourcePtr& res, const ResourceDesc& desc)
    : _desc(desc)
    , _guid(s_nextResourceGUID++)
    {
        std::shared_ptr<Resource> resource = AsResource(res);
        if (resource->GetBuffer()) {
            _underlyingBuffer = resource->GetBuffer();
        } else if (resource->GetTexture()) {
            _underlyingTexture = resource->GetTexture();
        } else {
            assert(0);
        }
    }

    Resource::Resource() : _guid(s_nextResourceGUID++) {}
    Resource::~Resource() {}

    ResourceDesc ExtractDesc(const IResource& input)
    {
        auto* res = (Resource*)const_cast<IResource&>(input).QueryInterface(typeid(Resource).hash_code());
        if (res)
            return res->GetDesc();
        return ResourceDesc{};
    }

    IResourcePtr CreateResource(
                                ObjectFactory& factory, const ResourceDesc& desc,
                                const IDevice::ResourceInitializer& initData)
    {
        return std::make_shared<Resource>(factory, desc, initData);
    }

    ResourceDesc ExtractRenderBufferDesc(const id<MTLTexture>& texture)
    {
        return CreateDesc(BindFlag::RenderTarget, 0, GPUAccess::Write, TextureDesc::Plain2D((uint32)texture.width, (uint32)texture.height, AsRenderCoreFormat(texture.pixelFormat)), "");
    }
}}
