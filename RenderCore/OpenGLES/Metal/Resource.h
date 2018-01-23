#pragma once

#include "../../ResourceDesc.h"
#include "../../IDevice.h"
#include "IndexedGLType.h"
#include "../../../Utility/IteratorUtils.h"

namespace RenderCore
{
    class SubResourceInitData;
}

namespace RenderCore { namespace Metal_OpenGLES
{
    class ObjectFactory;

    class Resource : public IResource
    {
    public:
        using Desc = ResourceDesc;

        const Desc& GetDesc() const         { return _desc; }

        const intrusive_ptr<OpenGL::Buffer>& GetBuffer() const  { return _underlyingBuffer; }
        const intrusive_ptr<OpenGL::Texture>& GetTexture() const  { return _underlyingTexture; }
        IteratorRange<const void*> GetConstantBuffer() const { return MakeIteratorRange(_constantBuffer); }

        virtual void*       QueryInterface(size_t guid);

        Resource(
            ObjectFactory& factory, const Desc& desc,
            const SubResourceInitData& initData = SubResourceInitData{});
        Resource(
            ObjectFactory& factory, const Desc& desc,
            const IDevice::ResourceInitializer& initData);
        Resource();
        ~Resource();
    protected:
        intrusive_ptr<OpenGL::Buffer> _underlyingBuffer;
        intrusive_ptr<OpenGL::Texture> _underlyingTexture;
        std::vector<uint8_t> _constantBuffer;
        Desc _desc;
    };

    GLenum AsBufferTarget(BindFlag::BitField bindFlags);
    GLenum AsUsageMode(CPUAccess::BitField cpuAccess, GPUAccess::BitField gpuAccess);

    inline RawGLHandle GetBufferRawGLHandle(const IResource& resource)
    {
        return static_cast<const Resource&>(resource).GetBuffer()->AsRawGLHandle();
    }
}}


