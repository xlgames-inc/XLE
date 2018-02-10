
#include "Resource.h"
#include "Format.h"
#include "../../ResourceUtils.h"
#include "IncludeGLES.h"

#include "../../../../CoreServices/GLWrappers.h"

namespace RenderCore { namespace Metal_OpenGLES
{

////////////////////////////////////////////////////////////////////////////////////////////////////

    GLenum AsBufferTarget(BindFlag::BitField bindFlags)
    {
        // Valid "target" values for glBindBuffer:
        // GL_ARRAY_BUFFER, GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, GL_ELEMENT_ARRAY_BUFFER, GL_PIXEL_PACK_BUFFER, GL_PIXEL_UNPACK_BUFFER, GL_TRANSFORM_FEEDBACK_BUFFER, or GL_UNIFORM_BUFFER
        // These are not accessible from here:
        // GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, GL_PIXEL_PACK_BUFFER & GL_PIXEL_UNPACK_BUFFER
        // Also note that this function will always 0 when multiple flags are set in 'bindFlags'

        switch (bindFlags) {
        case BindFlag::VertexBuffer: return GL_ARRAY_BUFFER;
        case BindFlag::IndexBuffer: return GL_ELEMENT_ARRAY_BUFFER;
        case BindFlag::ConstantBuffer: return GL_UNIFORM_BUFFER;
        case BindFlag::StreamOutput: return GL_TRANSFORM_FEEDBACK_BUFFER;
        }
        return 0;
    }

    GLenum AsUsageMode( CPUAccess::BitField cpuAccess,
                        GPUAccess::BitField gpuAccess)
    {
        // GL_STREAM_DRAW, GL_STREAM_READ, GL_STREAM_COPY, GL_STATIC_DRAW, GL_STATIC_READ, GL_STATIC_COPY, GL_DYNAMIC_DRAW, GL_DYNAMIC_READ, or GL_DYNAMIC_COPY
        // These values are not accessible here:
        //  GL_STREAM_COPY, GL_STATIC_COPY, GL_DYNAMIC_COPY

        if (cpuAccess == 0) {
            return GL_STATIC_DRAW;
        } else if (cpuAccess & CPUAccess::WriteDynamic) {
            if (cpuAccess & CPUAccess::Read) return GL_DYNAMIC_READ;
            else return GL_DYNAMIC_DRAW;
        } else if (cpuAccess & CPUAccess::Write) {
            if (cpuAccess & CPUAccess::Read) return GL_STREAM_READ;
            else return GL_STREAM_DRAW;
        }

        return 0;
    }

    static std::pair<CPUAccess::BitField, GPUAccess::BitField> AsAccessBitFields(GLenum glAccess)
    {
        switch (glAccess) {
        case GL_STATIC_DRAW: return {0, GPUAccess::Read};
        case GL_STATIC_READ: return {CPUAccess::Read, GPUAccess::Read};
        case GL_DYNAMIC_READ: return {CPUAccess::WriteDynamic | CPUAccess::Read, GPUAccess::Read};
        case GL_DYNAMIC_DRAW: return {CPUAccess::WriteDynamic, GPUAccess::Read};
        case GL_STREAM_READ: return {CPUAccess::Write | CPUAccess::Read, GPUAccess::Read};
        case GL_STREAM_DRAW: return {CPUAccess::Write, GPUAccess::Read};
        default: return {0,0};
        }
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    void* Resource::QueryInterface(size_t guid)
    {
        if (guid == typeid(Resource).hash_code())
            return this;
        return nullptr;
    }


    Resource::Resource(
        ObjectFactory& factory, const Desc& desc,
        const SubResourceInitData& initData)
    : Resource(factory, desc,
        initData._data.size()
        ? [&initData](SubResourceId subRes) {
            assert(subRes._mip == 0 && subRes._arrayLayer == 0);
            return initData;
        }
        : IDevice::ResourceInitializer{})
    {}

    static void checkError()
    {
        /*auto error = glGetError();
        assert(error == GL_NO_ERROR);*/
    }

    /*
    static GLenum AsBindingQuery(GLenum binding)
    {
        switch (binding) {
        case GL_TEXTURE_2D:         return GL_TEXTURE_BINDING_2D;
        case GL_TEXTURE_3D:         return GL_TEXTURE_BINDING_3D;
        case GL_TEXTURE_CUBE_MAP:   return GL_TEXTURE_BINDING_CUBE_MAP;
        default:                    return 0;
        }
    }
    */

    Resource::Resource(
        ObjectFactory& factory, const Desc& desc,
        const IDevice::ResourceInitializer& initializer)
    : _desc(desc)
    , _isBackBuffer(false)
    {
        checkError();

        if (desc._type == ResourceDesc::Type::LinearBuffer) {

            SubResourceInitData initData;
            if (initializer)
                initData = initializer({0,0});
            if (desc._bindFlags & BindFlag::ConstantBuffer) {
                _constantBuffer.insert(_constantBuffer.end(), (const uint8_t*)initData._data.begin(), (const uint8_t*)initData._data.end());
            } else {
                _underlyingBuffer = factory.CreateBuffer();
                assert(_underlyingBuffer->AsRawGLHandle() != 0); // "Failed to allocate buffer name in Magnesium::Buffer::Buffer");

                auto bindTarget = AsBufferTarget(desc._bindFlags);
                assert(bindTarget != 0); // "Could not resolve buffer binding target for bind flags (0x%x)", bindFlags);

                auto usageMode = AsUsageMode(desc._cpuAccess, desc._gpuAccess);
                assert(bindTarget != 0); // "Could not resolve buffer usable mode for cpu access flags (0x%x) and gpu access flags (0x%x)", cpuAccess, gpuAccess);

                // upload data to opengl buffer...
                glBindBuffer(bindTarget, _underlyingBuffer->AsRawGLHandle());
                (*GetGLWrappers()->BufferData)(bindTarget, std::max((GLsizeiptr)initData._data.size(), (GLsizeiptr)desc._linearBufferDesc._sizeInBytes), initData._data.data(), usageMode);
            }

        } else {

            // write only textures can become render buffers
            auto fmt = AsTexelFormatType(desc._textureDesc._format);
            bool canBeRenderBuffer = desc._cpuAccess == 0 && desc._gpuAccess == GPUAccess::Write && (!(desc._bindFlags & BindFlag::ShaderResource)) && !initializer;
            if (!canBeRenderBuffer) {

                _underlyingTexture = factory.CreateTexture();
                if (    desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T1D
                    ||  desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T2D
                    ||  desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T3D
                    ||  desc._textureDesc._dimensionality == TextureDesc::Dimensionality::CubeMap) {

                    auto bindTarget = GL_TEXTURE_2D;
                    #if PLATFORMOS_ACTIVE == PLATFORMOS_OSX     // DavidJ -- temporary hack -- glTexStorage path not working for me on OSX (pixel format compatibility issue?) -- falling back to old path
                        bool useTexStorage = false;
                    #else
                        bool useTexStorage = true;
                    #endif
                    if (desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T1D) {
                        assert(desc._textureDesc._height == 1);
                        assert(desc._textureDesc._arrayCount <= 1);
                    } else if (desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T2D) {
                        assert(desc._textureDesc._arrayCount <= 1);
                    } else if (desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T3D) {
                        assert(desc._textureDesc._arrayCount <= 1);
                        bindTarget = GL_TEXTURE_3D;
                    } else if (desc._textureDesc._dimensionality == TextureDesc::Dimensionality::CubeMap) {
                        assert(desc._textureDesc._arrayCount == 6);
                        bindTarget = GL_TEXTURE_CUBE_MAP;
                    }

                    // GLint prevTexture;
                    // glGetIntegerv(AsBindingQuery(bindTarget), &prevTexture);
                    glBindTexture(bindTarget, _underlyingTexture->AsRawGLHandle());

                    if (!initializer && useTexStorage) {
                        (*GetGLWrappers()->TexStorage2D)(bindTarget, desc._textureDesc._mipCount, fmt._internalFormat, desc._textureDesc._width, desc._textureDesc._height);
                    } else {

                        // If we're uploading a cubemap, we must iterate through all of the faces
                        // and use a binding target for each face.
                        // Otherwise we only have a single face, which is going to be the main
                        // binding target.
                        GLenum faceBinding[] = {
                            GL_TEXTURE_CUBE_MAP_POSITIVE_X,
                            GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                            GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
                            GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                            GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
                            GL_TEXTURE_CUBE_MAP_NEGATIVE_Z };
                        unsigned faceCount = dimof(faceBinding);

                        if (desc._textureDesc._dimensionality != TextureDesc::Dimensionality::CubeMap) {
                            faceBinding[0] = bindTarget;
                            faceCount = 1;
                        }

                        for (unsigned f=0; f<faceCount; ++f) {
                            if (initializer) {
                                for (unsigned m=0; m<desc._textureDesc._mipCount; ++m) {
                                    auto mipWidth  = std::max(desc._textureDesc._width >> m, 1u);
                                    auto mipHeight = std::max(desc._textureDesc._height >> m, 1u);
                                    auto subRes = initializer({m, f});
                                    if (fmt._type != 0) {
                                        (*GetGLWrappers()->TexImage2D)(
                                            faceBinding[f],
                                            m, fmt._internalFormat,
                                            mipWidth, mipHeight, 0,
                                            fmt._format, fmt._type,
                                            subRes._data.begin());
                                    } else {
                                        (*GetGLWrappers()->CompressedTexImage2D)(
                                            faceBinding[f],
                                            m, fmt._internalFormat,
                                            mipWidth, mipHeight, 0,
                                            (GLsizei)subRes._data.size(),
                                            subRes._data.begin());
                                    }
                                }
                            } else  {
                                assert(fmt._type != 0);
                                for (unsigned m=0; m<desc._textureDesc._mipCount; ++m) {
                                    auto mipWidth  = std::max(desc._textureDesc._width >> m, 1u);
                                    auto mipHeight = std::max(desc._textureDesc._height >> m, 1u);
                                    (*GetGLWrappers()->TexImage2D)(
                                        faceBinding[f],
                                        m, fmt._internalFormat,
                                        mipWidth, mipHeight, 0,
                                        fmt._format, fmt._type,
                                        nullptr);
                                }
                            }
                        }

                    }

                    // glBindTexture(bindTarget, prevTexture);
                }

            } else {

                _underlyingRenderBuffer = factory.CreateRenderBuffer();
                glBindRenderbuffer(GL_RENDERBUFFER, _underlyingRenderBuffer->AsRawGLHandle());
                glRenderbufferStorage(GL_RENDERBUFFER, fmt._internalFormat, desc._textureDesc._width, desc._textureDesc._height);

            }

        }

        checkError();
    }

    Resource::Resource(const intrusive_ptr<OpenGL::Texture>& texture, const ResourceDesc& desc)
    : _underlyingTexture(texture)
    , _desc(desc)
    {
        if (!glIsTexture(texture->AsRawGLHandle()))
            Throw(::Exceptions::BasicLabel("Binding non-texture as texture resource"));
        _isBackBuffer = false;
    }

    Resource::Resource(const intrusive_ptr<OpenGL::RenderBuffer>& renderbuffer)
    : _underlyingRenderBuffer(renderbuffer)
    {
        if (!glIsRenderbuffer(renderbuffer->AsRawGLHandle()))
            Throw(::Exceptions::BasicLabel("Binding non-render buffer as render buffer resource"));

        _desc = ExtractDesc(renderbuffer.get());
        _isBackBuffer = false;
    }

    Resource::Resource(const intrusive_ptr<OpenGL::Buffer>& buffer)
    : _underlyingBuffer(buffer)
    {
        if (!glIsBuffer(buffer->AsRawGLHandle()))
            Throw(::Exceptions::BasicLabel("Binding non-render buffer as render buffer resource"));

        _desc = ExtractDesc(buffer.get());
        _isBackBuffer = false;
    }

    Resource::Resource() : _isBackBuffer(false) {}
    Resource::~Resource() {}

    Resource Resource::CreateBackBuffer(const Desc& desc)
    {
        Resource result;
        result._isBackBuffer = true;
        result._desc = desc;
        return result;
    }

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

    ResourceDesc ExtractDesc(OpenGL::RenderBuffer* renderbuffer)
    {
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer->AsRawGLHandle());
        GLint width = 0, height = 0, internalFormat = 0, samples = 0;
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &internalFormat);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &samples);

        auto fmt = SizedInternalFormatAsRenderCoreFormat(internalFormat);
        auto components = GetComponents(fmt);
        bool isDepthStencilBuffer =
                components == FormatComponents::Depth
            ||  components == FormatComponents::DepthStencil
            ||  components == FormatComponents::Stencil;

        return CreateDesc(
            isDepthStencilBuffer ? BindFlag::DepthStencil : BindFlag::RenderTarget,
            0, GPUAccess::Write,
            TextureDesc::Plain2D(width, height, fmt, 1, 0, TextureSamples::Create(samples)),
            "");
    }

    ResourceDesc ExtractDesc(OpenGL::Texture* texture)
    {
        assert(0);  // can't access information about the texture using opengles (until 3.1)
        return ResourceDesc{};
    }

    ResourceDesc ExtractDesc(OpenGL::Buffer* buffer)
    {
        GLenum bindingTarget = GL_ARRAY_BUFFER;
        glBindBuffer(bindingTarget, buffer->AsRawGLHandle());
        GLint size = 0, usage = 0;
        glGetBufferParameteriv(bindingTarget, GL_BUFFER_SIZE, &size);
        glGetBufferParameteriv(bindingTarget, GL_BUFFER_USAGE, &usage);

        auto accessFlags = AsAccessBitFields(usage);
        return CreateDesc(
            BindFlag::VertexBuffer | BindFlag::IndexBuffer | BindFlag::ConstantBuffer,
            accessFlags.first, accessFlags.second,
            LinearBufferDesc::Create(size),
            "");
    }

}}


