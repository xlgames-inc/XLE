#pragma once

#include "../../ResourceDesc.h"
#include "IndexedGLType.h"

namespace RenderCore
{
    class SubResourceInitData;
}

namespace RenderCore { namespace Metal_OpenGLES
{
    class ObjectFactory;

    class Resource
    {
    public:
        using Desc = ResourceDesc;

        const Desc& GetDesc() const         { return _desc; }

        const intrusive_ptr<OpenGL::Buffer>& GetBuffer() const  { return _underlyingBuffer; }
        const intrusive_ptr<OpenGL::Texture>& GetTexture() const  { return _underlyingTexture; }

        Resource(
            ObjectFactory& factory, const Desc& desc,
            const SubResourceInitData& initData = SubResourceInitData{});
        Resource();
        ~Resource();
    protected:
        intrusive_ptr<OpenGL::Buffer> _underlyingBuffer;
        intrusive_ptr<OpenGL::Texture> _underlyingTexture;
        Desc _desc;
    };

    GLenum AsBufferTarget(BindFlag::BitField bindFlags);
    GLenum AsUsageMode(CPUAccess::BitField cpuAccess, GPUAccess::BitField gpuAccess);
}}


