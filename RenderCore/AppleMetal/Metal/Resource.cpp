// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Resource.h"
#include "Format.h"
#include "DeviceContext.h"
#include "../Device.h"
#include "../../IThreadContext.h"
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

    std::vector<uint8_t> Resource::ReadBack(IThreadContext& context, SubResourceId subRes) const
    {
        // We must synchronize with the GPU. Since the GPU is working asychronously, we must ensure
        // that all operations that might effect this resource have been completed. Since we don't
        // know exactly what operations effect the resource, we must wait for all!
        auto* metalContext = (ImplAppleMetal::ThreadContext*)context.QueryInterface(typeid(ImplAppleMetal::ThreadContext).hash_code());
        if (!metalContext)
            Throw(std::runtime_error("Incorrect thread context passed to Apple Metal Resource::ReadBack implementation"));

        #if PLATFORMOS_TARGET == PLATFORMOS_OSX
            // With "shared mode" textures, we can go straight to the main texture and
            // get the data directly.
            // With "managed mode", we must call synchronizeResource
            //
            if (_underlyingTexture && _underlyingTexture.get().storageMode == MTLStorageModeManaged) {
                @autoreleasepool {
                    id<MTLBlitCommandEncoder> blitEncoder = [metalContext->GetCurrentCommandBuffer() blitCommandEncoder];
                    [blitEncoder synchronizeResource:_underlyingTexture.get()];
                    [blitEncoder endEncoding];
                }
            } else if (_underlyingBuffer && _underlyingBuffer.get().storageMode == MTLStorageModeManaged) {
                @autoreleasepool {
                    id<MTLBlitCommandEncoder> blitEncoder = [metalContext->GetCurrentCommandBuffer() blitCommandEncoder];
                    [blitEncoder synchronizeResource:_underlyingBuffer.get()];
                    [blitEncoder endEncoding];
                }
            }
        #endif

        // METAL_TODO: Is this really the right place for this? We used to do WaitUntilQueueCompleted, which implicitly assumed that we're at the end of either a headless or an already-drawn but uncommitted frame, committed the current command buffer, and started a new one, so this trio of calls has _almost_ the same functionality, except for keeping the drawable around from one frame to the next (which is probably a bad idea anyway), which will now assert if we try it. But it seems weird. At any rate, even if we do want to keep this, we'll want to switch it to the new method once it's ready.
        metalContext->EndHeadlessFrame();
        metalContext->GetDevice()->Stall();
        metalContext->BeginHeadlessFrame();

        if (_underlyingTexture) {

            #if PLATFORMOS_TARGET == PLATFORMOS_IOS
                if (_underlyingTexture.get().framebufferOnly)
                    Throw(std::runtime_error("Cannot use Resource::ReadBack on a framebuffer only resource on IOS. You must readback through a CPU accessable copy of this textuer."));
            #endif

            auto mipmapDesc = CalculateMipMapDesc(_desc._textureDesc, subRes._mip);
            auto pitches = MakeTexturePitches(mipmapDesc);
            MTLRegion region;
            if (_desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T1D) {
                region = MTLRegionMake1D(0, _desc._textureDesc._width);
            } else if (_desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T2D) {
                region = MTLRegionMake2D(
                    0, 0,
                    _desc._textureDesc._width, _desc._textureDesc._height);
            } else if (_desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T3D) {
                region = MTLRegionMake3D(
                    0, 0, 0,
                    _desc._textureDesc._width, _desc._textureDesc._height, _desc._textureDesc._depth);
            }

            std::vector<uint8_t> result(pitches._slicePitch);
            [_underlyingTexture.get() getBytes:result.data()
                                   bytesPerRow:pitches._rowPitch
                                 bytesPerImage:pitches._slicePitch
                                    fromRegion:region
                                   mipmapLevel:subRes._mip
                                         slice:subRes._arrayLayer];

            return result;

        } else if (_underlyingBuffer) {

            auto* contents = _underlyingBuffer.get().contents;
            if (!contents)
                Throw(std::runtime_error("Could not read back data from buffer object, either because it's empty or not marked for CPU read access"));

            auto length = _underlyingBuffer.get().length;
            std::vector<uint8_t> result(length);
            std::memcpy(result.data(), contents, length);

            return result;

        }

        return {};
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
                if (desc._textureDesc._arrayCount > 1) {
                    textureDesc.textureType = MTLTextureType1DArray;
                } else {
                    textureDesc.textureType = MTLTextureType1D;
                }
            } else if (desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T2D) {
                assert(desc._textureDesc._arrayCount <= 1);
                if (desc._textureDesc._arrayCount > 1) {
                    assert(desc._textureDesc._samples._sampleCount <= 1); // MTLTextureType2DMultisampleArray is not supported in IOS
                    textureDesc.textureType = MTLTextureType2DArray;
                } else {
                    if (desc._textureDesc._samples._sampleCount > 1) {
                        textureDesc.textureType = MTLTextureType2DMultisample;
                    } else
                        textureDesc.textureType = MTLTextureType2D;
                }
            } else if (desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T3D) {
                assert(desc._textureDesc._arrayCount <= 1);
                textureDesc.textureType = MTLTextureType3D;
            } else if (desc._textureDesc._dimensionality == TextureDesc::Dimensionality::CubeMap) {
                assert(desc._textureDesc._arrayCount == 6);
                textureDesc.textureType = MTLTextureTypeCube;
            }

            textureDesc.pixelFormat = AsMTLPixelFormat(desc._textureDesc._format);
            if (textureDesc.pixelFormat == MTLPixelFormatInvalid) {
                // Some formats, like three-byte formats, cannot be handled
                [textureDesc release];
                Throw(::Exceptions::BasicLabel("Cannot create texture resource because format is not supported by Apple Metal: (%s)", AsString(desc._textureDesc._format)));
            }
            assert(textureDesc.pixelFormat != MTLPixelFormatInvalid);

            textureDesc.width = desc._textureDesc._width;
            textureDesc.height = desc._textureDesc._height;
            textureDesc.depth = desc._textureDesc._depth;

            textureDesc.mipmapLevelCount = desc._textureDesc._mipCount;
            textureDesc.sampleCount = desc._textureDesc._samples._sampleCount;
            // In Metal, arrayLength is only set for arrays.  For non-arrays, arrayLength must be 1.
            // That is, the RenderCore arrayCount is not the same as the Metal arrayLength.
            if (textureDesc.textureType != MTLTextureTypeCube) {
                textureDesc.arrayLength = (desc._textureDesc._arrayCount > 1) ? desc._textureDesc._arrayCount : 1;
            } else {
                textureDesc.arrayLength = 1;
            }

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
                        assert(desc._textureDesc._depth <= 1);              // DavidJ -- 3D textures are not supported by this Metal API, so we must ensure that this is a 2d texture
                        auto subRes = initializer({m, f});
                        auto bytesPerRow = subRes._pitches._rowPitch;
                        auto bytesPerImage = subRes._pitches._slicePitch;   // Since 3d textures are not supported, the "slice pitch" is equal to the image pitch
                        if (hasPVRTCPixelFormat) {
                            /* From Apple documentation on replaceRegion...:
                             *    This method is supported if you are copying to an entire texture with a PVRTC pixel format; in
                             *    which case, bytesPerRow and bytesPerImage must both be set to 0. This method is not
                             *    supported for copying to a subregion of a texture that has a PVRTC pixel format.
                             */
                            bytesPerRow = 0;
                            bytesPerImage = 0;
                        } else {
                            // When the input pitches are zero, it means the texture data is densely packed,
                            // and we should just derive the pitches from the dimensions
                            if (bytesPerRow == 0)
                                bytesPerRow = mipWidth * BitsPerPixel(desc._textureDesc._format) / 8u;
                            if (bytesPerImage == 0)
                                bytesPerImage = mipHeight * bytesPerRow;
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
