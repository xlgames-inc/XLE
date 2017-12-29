
#include "Resource.h"
#include "IncludeGLES.h"

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
    {
        if (desc._type == ResourceDesc::Type::LinearBuffer) {
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
                glBufferData(bindTarget, std::max((GLsizeiptr)initData._data.size(), (GLsizeiptr)desc._linearBufferDesc._sizeInBytes), initData._data.data(), usageMode);
            }
        } else {
            Throw(Exceptions::BasicLabel("Unsupported resource type in Resource constructor"));
        }
    }

    Resource::Resource() {}
    Resource::~Resource() {}

}}


