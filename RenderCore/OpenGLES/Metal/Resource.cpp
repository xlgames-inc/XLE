
#include "Resource.h"
#include "Format.h"
#include "../../ResourceUtils.h"
#include "IncludeGLES.h"

#include "../../../../CoreServices/GLWrappers.h"

namespace RenderCore { namespace Metal_OpenGLES
{

////////////////////////////////////////////////////////////////////////////////////////////////////

    GLenum AsBufferTarget(RenderCore::BindFlag::BitField bindFlags)
    {
        // Valid "target" values for glBindBuffer:
        // GL_ARRAY_BUFFER, GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, GL_ELEMENT_ARRAY_BUFFER, GL_PIXEL_PACK_BUFFER, GL_PIXEL_UNPACK_BUFFER, GL_TRANSFORM_FEEDBACK_BUFFER, or GL_UNIFORM_BUFFER
        // These are not accessible from here:
        // GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, GL_PIXEL_PACK_BUFFER & GL_PIXEL_UNPACK_BUFFER
        // Also note that this function will always 0 when multiple flags are set in 'bindFlags'

        using namespace RenderCore;
        switch (bindFlags) {
        case BindFlag::VertexBuffer: return GL_ARRAY_BUFFER;
        case BindFlag::IndexBuffer: return GL_ELEMENT_ARRAY_BUFFER;
        case BindFlag::ConstantBuffer: return GL_UNIFORM_BUFFER;
        case BindFlag::StreamOutput: return GL_TRANSFORM_FEEDBACK_BUFFER;
        }
        return 0;
    }

    GLenum AsUsageMode( RenderCore::CPUAccess::BitField cpuAccess,
                        RenderCore::GPUAccess::BitField gpuAccess)
    {
        // GL_STREAM_DRAW, GL_STREAM_READ, GL_STREAM_COPY, GL_STATIC_DRAW, GL_STATIC_READ, GL_STATIC_COPY, GL_DYNAMIC_DRAW, GL_DYNAMIC_READ, or GL_DYNAMIC_COPY
        // These values are not accessible here:
        //  GL_STREAM_COPY, GL_STATIC_COPY, GL_DYNAMIC_COPY

        using namespace RenderCore;
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
    : Resource(factory, desc, [&initData](SubResourceId subRes) {
            assert(subRes._mip == 0 && subRes._arrayLayer == 0);
            return initData;
        })
    {}

    static void checkError()
    {
        auto error = glGetError();
        assert(error == GL_NO_ERROR);
    }

    static GLenum AsBindingQuery(GLenum binding)
    {
        switch (binding) {
        case GL_TEXTURE_2D:         return GL_TEXTURE_BINDING_2D;
        case GL_TEXTURE_3D:         return GL_TEXTURE_BINDING_3D;
        case GL_TEXTURE_CUBE_MAP:   return GL_TEXTURE_BINDING_CUBE_MAP;
        default:                    return 0;
        }
    }


    Resource::Resource(
        ObjectFactory& factory, const Desc& desc,
        const IDevice::ResourceInitializer& initializer)
    : _desc(desc)
    {
        if (desc._type == ResourceDesc::Type::LinearBuffer) {
            auto initData = initializer({0,0});
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

            checkError();

            auto fmt = AsTexelFormatType(desc._textureDesc._format);

            _underlyingTexture = factory.CreateTexture();

            if (    desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T1D
                ||  desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T2D
                ||  desc._textureDesc._dimensionality == TextureDesc::Dimensionality::CubeMap) {

                auto bindTarget = GL_TEXTURE_2D;
                bool useTexStorage = true;
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

                GLint prevTexture;
                glGetIntegerv(AsBindingQuery(bindTarget), &prevTexture);

                checkError();

                glBindTexture(bindTarget, _underlyingTexture->AsRawGLHandle());
                checkError();

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
                                checkError();
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
                                checkError();
                            }
                        }
                    }

                }

                glTexParameteri(bindTarget, GL_TEXTURE_MIN_FILTER, (desc._textureDesc._mipCount > 1) ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
                glTexParameteri(bindTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(bindTarget, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(bindTarget, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(bindTarget, GL_TEXTURE_WRAP_R, GL_REPEAT);
                glBindTexture(bindTarget, prevTexture);

                checkError();

            } else if (desc._textureDesc._dimensionality == TextureDesc::Dimensionality::T3D) {
                assert(0);  // not supported yet
            }

        }
    }

    Resource::Resource() {}
    Resource::~Resource() {}

}}


