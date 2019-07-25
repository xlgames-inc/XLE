
#include "Resource.h"
#include "Format.h"
#include "../../ResourceUtils.h"
#include "../../../ConsoleRig/Log.h"
#include "IncludeGLES.h"
#include <sstream>

#include "GLWrappers.h"

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

    uint64_t Resource::GetGUID() const
    {
        return _guid;
    }

    std::vector<uint8_t> Resource::ReadBack(SubResourceId subRes) const
    {
        if (_underlyingBuffer) {
            auto bindTarget = GetDesc()._type != ResourceDesc::Type::Unknown ? AsBufferTarget(GetDesc()._bindFlags) : GL_ARRAY_BUFFER;
            glBindBuffer(bindTarget, GetBuffer()->AsRawGLHandle());

            GLint bufferSize = 0;
            glGetBufferParameteriv(bindTarget, GL_BUFFER_SIZE, &bufferSize);

            void* mappedData = glMapBufferRange(bindTarget, 0, bufferSize, GL_MAP_READ_BIT);
            std::vector<uint8_t> result(bufferSize);
            std::memcpy(result.data(), mappedData, bufferSize);
            glUnmapBuffer(bindTarget);

            return result;
        } else if (_underlyingRenderBuffer || _underlyingTexture) {

            if (subRes._arrayLayer != 0 || subRes._mip != 0)
                Throw(std::runtime_error("Can only get the first subresource from a renderbuffer in OpenGLES"));

            if (    _desc._textureDesc._dimensionality != TextureDesc::Dimensionality::T2D
                &&  _desc._textureDesc._dimensionality != TextureDesc::Dimensionality::T1D)
                Throw(std::runtime_error("Only 1D and 2D textures supported in readback operations in OpenGLES"));

            //
            // In OpenGLES, glReadPixels transforms the format of the texture into some simpler
            // format. However the Resource::ReadBack() interface is designed to return the same
            // pixel format as the format that was used when the resource was created. For simplicity,
            // let's only allow some fixed set of formats, where we know the conversion behaviour
            // in glReadPixels isn't going to actually change the pixels.
            //
            auto pixFmt = AsTexelFormatType(_desc._textureDesc._format);
            if (pixFmt._format != GL_RGBA && pixFmt._format != GL_RGBA_INTEGER)
                Throw(std::runtime_error("Can only read back from textures with simple pixel formats in OpenGLES"));

            if (pixFmt._type != GL_UNSIGNED_BYTE && pixFmt._type != GL_UNSIGNED_INT && pixFmt._type != GL_INT && pixFmt._type != GL_FLOAT)
                Throw(std::runtime_error("Can only read back from textures with simple pixel formats in OpenGLES"));

            std::vector<uint8_t> result(_desc._textureDesc._width * _desc._textureDesc._height * BitsPerPixel(_desc._textureDesc._format) / 8);

            GLint prevFrameBuffer;
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFrameBuffer);

            auto& factory = GetObjectFactory();
            auto fb = factory.CreateFrameBuffer();
            glBindFramebuffer(GL_FRAMEBUFFER, fb->AsRawGLHandle());
            if (_underlyingRenderBuffer) {
                glFramebufferRenderbuffer(
                    GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + 0,
                    GL_RENDERBUFFER, _underlyingRenderBuffer->AsRawGLHandle());
            } else {
                if (_desc._textureDesc._arrayCount > 1u) {
                    glFramebufferTextureLayer(
                        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + 0,
                        _underlyingTexture->AsRawGLHandle(),
                        0,
                        0);
                } else {
                    glFramebufferTexture2D(
                        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + 0, GL_TEXTURE_2D,
                        _underlyingTexture->AsRawGLHandle(),
                        0);
                }
            }

            GLenum b[] = { GL_NONE, GL_NONE, GL_NONE, GL_NONE };
            glDrawBuffers(dimof(b), b);
            glReadBuffer(GL_COLOR_ATTACHMENT0);

            #if defined(_DEBUG)
                auto fbComplete = glCheckFramebufferStatus(GL_FRAMEBUFFER);
                if (fbComplete != GL_FRAMEBUFFER_COMPLETE) {
                    Log(Warning) << "glCheckFramebufferStatus check failed in Resource ReadBack operation: " << CheckFramebufferStatusToString(fbComplete) << std::endl;
                    assert(fbComplete == GL_FRAMEBUFFER_COMPLETE);
                }
            #endif

            GLint packAlignment, packRowLength;
            glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
            glGetIntegerv(GL_PACK_ROW_LENGTH, &packRowLength);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glPixelStorei(GL_PACK_ROW_LENGTH, 0);

            glReadPixels(
                0, 0, _desc._textureDesc._width, _desc._textureDesc._height,
                pixFmt._format, pixFmt._type,
                result.data());

            glPixelStorei(GL_PACK_ROW_LENGTH, packRowLength);
            glPixelStorei(GL_PACK_ALIGNMENT, packAlignment);
            glBindFramebuffer(GL_FRAMEBUFFER, prevFrameBuffer);

            CheckGLError("After resource readback");
            return result;

        }

        return _constantBuffer;
    }

    static uint64_t s_nextResourceGUID = 1;

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
    , _guid(s_nextResourceGUID++)
    {
        if (desc._type == ResourceDesc::Type::LinearBuffer) {

            SubResourceInitData initData;
            if (initializer)
                initData = initializer({0,0});

            bool supportsUniformBuffers = (factory.GetFeatureSet() & FeatureSet::GLES300);
            if ((desc._bindFlags & BindFlag::ConstantBuffer) && !supportsUniformBuffers) {
                // If we request a constant buffer on a device that doesn't support uniform
                // buffers; we just use a basic data array.
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
                GL_WRAP(BufferData)(bindTarget, std::max((GLsizeiptr)initData._data.size(), (GLsizeiptr)desc._linearBufferDesc._sizeInBytes), initData._data.data(), usageMode);

                if (ObjectFactory::WriteObjectLabels() && (factory.GetFeatureSet() & FeatureSet::Flags::LabelObject) && desc._name[0])
                    glLabelObjectEXT(GL_BUFFER_OBJECT_EXT, _underlyingBuffer->AsRawGLHandle(), 0, desc._name);
            }

            CheckGLError("Creating linear buffer resource");

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

                    #if APPORTABLE // glTexStorage2D is not yet supported in Apportable
                        bool useTexStorage = false;
                    #else
                        bool useTexStorage = factory.GetFeatureSet() & FeatureSet::GLES300;
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
                        GL_WRAP(TexStorage2D)(bindTarget, desc._textureDesc._mipCount, fmt._internalFormat, desc._textureDesc._width, desc._textureDesc._height);
                    } else {

                        // OpenGLES2.0 doesn't understand the "internal formats" when using glTexImage2D
                        // It requires that the internalFormat parameter matches the format parameter
                        // However, OpenGLES2.0 does use the internal formats in the glRenderbufferStorage
                        // call.
                        if (!(factory.GetFeatureSet() & FeatureSet::GLES300) && fmt._type != 0) {
                            fmt._internalFormat = fmt._format;
                        }

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

                        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

                        for (unsigned f=0; f<faceCount; ++f) {
                            if (initializer) {
                                for (unsigned m=0; m<desc._textureDesc._mipCount; ++m) {
                                    auto mipWidth  = std::max(desc._textureDesc._width >> m, 1u);
                                    auto mipHeight = std::max(desc._textureDesc._height >> m, 1u);
                                    auto subRes = initializer({m, f});
                                    if (fmt._type != 0) {
                                        GL_WRAP(TexImage2D)(
                                            faceBinding[f],
                                            m, fmt._internalFormat,
                                            mipWidth, mipHeight, 0,
                                            fmt._format, fmt._type,
                                            subRes._data.begin());
                                    } else {
                                        GL_WRAP(CompressedTexImage2D)(
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
                                    GL_WRAP(TexImage2D)(
                                        faceBinding[f],
                                        m, fmt._internalFormat,
                                        mipWidth, mipHeight, 0,
                                        fmt._format, fmt._type,
                                        nullptr);
                                }
                            }
                        }
                        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
                    }
                    // glBindTexture(bindTarget, prevTexture);
                }

                if (ObjectFactory::WriteObjectLabels() && (factory.GetFeatureSet() & FeatureSet::Flags::LabelObject) && desc._name[0])
                    glLabelObjectEXT(GL_TEXTURE, _underlyingTexture->AsRawGLHandle(), 0, desc._name);

                CheckGLError("Creating texture resource");
            } else {
                _underlyingRenderBuffer = factory.CreateRenderBuffer();
                glBindRenderbuffer(GL_RENDERBUFFER, _underlyingRenderBuffer->AsRawGLHandle());

                // apportable doesn't properly wrap glRenderbufferStorageMultisample()
                #if defined(PGDROID)
                    bool supportsMultisampleRenderbuffer = false;
                // in "fake GLES" mode (ex. in OSX), the feature is backed by OpenGL
                #elif !defined(GL_ES_VERSION_2_0) && !defined(GL_ES_VERSION_3_0)
                    bool supportsMultisampleRenderbuffer = true;
                #else
                    bool supportsMultisampleRenderbuffer = factory.GetFeatureSet() & FeatureSet::GLES300;
                #endif

                if (supportsMultisampleRenderbuffer && desc._textureDesc._samples._sampleCount > 1) {
                     glRenderbufferStorageMultisample(GL_RENDERBUFFER, desc._textureDesc._samples._sampleCount, fmt._internalFormat, desc._textureDesc._width, desc._textureDesc._height);
                } else {
                    GL_WRAP(RenderbufferStorage)(GL_RENDERBUFFER, fmt._internalFormat, desc._textureDesc._width, desc._textureDesc._height);
                }

                if (ObjectFactory::WriteObjectLabels() && (factory.GetFeatureSet() & FeatureSet::Flags::LabelObject) && desc._name[0])
                    glLabelObjectEXT(GL_RENDERBUFFER, _underlyingRenderBuffer->AsRawGLHandle(), 0, desc._name);

                CheckGLError("Creating render buffer resource");
            }
        }
    }

    Resource::Resource(const intrusive_ptr<OpenGL::Texture>& texture, const ResourceDesc& desc)
    : _underlyingTexture(texture)
    , _desc(desc)
    , _guid(s_nextResourceGUID++)
    {
        if (!glIsTexture(texture->AsRawGLHandle()))
            Throw(::Exceptions::BasicLabel("Binding non-texture as texture resource"));
        _isBackBuffer = false;
    }

    Resource::Resource(const intrusive_ptr<OpenGL::RenderBuffer>& renderbuffer)
    : _underlyingRenderBuffer(renderbuffer)
    , _guid(s_nextResourceGUID++)
    {
        if (!glIsRenderbuffer(renderbuffer->AsRawGLHandle()))
            Throw(::Exceptions::BasicLabel("Binding non-render buffer as render buffer resource"));

        _desc = ExtractDesc(renderbuffer.get());
        _isBackBuffer = false;
    }

    Resource::Resource(const intrusive_ptr<OpenGL::Buffer>& buffer)
    : _underlyingBuffer(buffer)
    , _guid(s_nextResourceGUID++)
    {
        if (!glIsBuffer(buffer->AsRawGLHandle()))
            Throw(::Exceptions::BasicLabel("Binding non-render buffer as render buffer resource"));

        _desc = ExtractDesc(buffer.get());
        _isBackBuffer = false;
    }

    Resource::Resource() : _isBackBuffer(false), _guid(s_nextResourceGUID++) {}
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

    std::string DescribeUnknownObject(unsigned glName)
    {
        std::stringstream str;
        bool started = false;
        if (glIsBuffer(glName)) {
            auto desc = ExtractDesc((OpenGL::Buffer*)(size_t)glName);
            str << "[Buffer] Bytes: " << desc._linearBufferDesc._sizeInBytes << " BindFlags: 0x" << std::hex << desc._bindFlags << std::dec;
            started = true;
        }
        if (glIsFramebuffer(glName)) {
            if (started) str << std::endl;
            str << "[FrameBuffer]";
            started = true;
        }
        if (glIsProgram(glName)) {
            if (started) str << std::endl;
            str << "[Program]";
            started = true;
        }
        if (glIsQuery(glName)) {
            if (started) str << std::endl;
            str << "[Query]";
            started = true;
        }
        if (glIsRenderbuffer(glName)) {
            if (started) str << std::endl;
            auto desc = ExtractDesc((OpenGL::RenderBuffer*)(size_t)glName);
            str << "[Renderbuffer] " << desc._textureDesc._width << "x" << desc._textureDesc._height;
            started = true;
        }
        if (glIsSampler(glName)) {
            if (started) str << std::endl;
            str << "[Sampler]";
            started = true;
        }
        if (glIsShader(glName)) {
            if (started) str << std::endl;
            str << "[Shader]";
            started = true;
        }
        if (glIsTexture(glName)) {
            if (started) str << std::endl;
            str << "[Texture] ";
            started = true;
        }
        if (glIsTransformFeedback(glName)) {
            if (started) str << std::endl;
            str << "[TransformFeedback]";
            started = true;
        }
        if (glIsVertexArray(glName)) {
            if (started) str << std::endl;
            str << "[VertexArray]";
            started = true;
        }
        return str.str();
    }

}}


