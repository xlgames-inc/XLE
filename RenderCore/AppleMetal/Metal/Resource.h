// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ObjectFactory.h"
#include "../../ResourceDesc.h"
#include "../../IDevice.h"
#include "../../../Utility/IteratorUtils.h"

namespace RenderCore
{
    class SubResourceInitData;
}

namespace RenderCore { namespace Metal_AppleMetal
{
    class ObjectFactory;

    class Resource : public IResource
    {
    public:
        using Desc = ResourceDesc;

        Desc GetDesc() const override        { return _desc; }

        const TBC::OCPtr<AplMtlTexture>& GetTexture() const { return _underlyingTexture; }; // <MTLTexture>
        const TBC::OCPtr<AplMtlBuffer>& GetBuffer() const { return _underlyingBuffer; }; // <MTLBuffer>

        /* KenD -- GetRenderBuffer() is not necessary for Metal implementation at this point */
        //TBC::OCPtr<id> GetRenderBuffer() const { return _underlyingRenderBuffer; }; // <MTLTexture>

        const bool IsBackBuffer() { return false; }

        virtual void*       QueryInterface(size_t guid) override;
        virtual uint64_t    GetGUID() const override;
        virtual std::vector<uint8_t>    ReadBack(IThreadContext& context, SubResourceId subRes) const override;

        Resource(
            ObjectFactory& factory, const Desc& desc,
            const SubResourceInitData& initData = SubResourceInitData{});
        Resource(
            ObjectFactory& factory, const Desc& desc,
            const IDevice::ResourceInitializer& initData);

        Resource(const id<MTLTexture>&, const ResourceDesc& = {});
        Resource(const id<MTLTexture>&, const ResourceDesc&, uint64_t guidOverride);
        Resource(const IResourcePtr&, const ResourceDesc& = {});

        Resource();
        ~Resource();

        static uint64_t ReserveGUID();

    protected:
        TBC::OCPtr<AplMtlBuffer> _underlyingBuffer; // id<MTLBuffer>
        TBC::OCPtr<AplMtlTexture> _underlyingTexture; // id<MTLTexture>
        //TBC::OCPtr<id> _underlyingRenderBuffer; // id<MTLTexture>
        Desc _desc;
        uint64_t _guid;
    };

    inline RawMTLHandle GetBufferRawMTLHandle(const IResource& resource)
    {
        return (RawMTLHandle)static_cast<const Resource&>(resource).GetBuffer().get();
    }

    ResourceDesc ExtractDesc(const IResource& input);
    IResourcePtr CreateResource(
                                ObjectFactory& factory, const ResourceDesc& desc,
                                const IDevice::ResourceInitializer& initData = nullptr);

    ResourceDesc ExtractRenderBufferDesc(const id<MTLTexture>& texture);
    //ResourceDesc ExtractTextureDesc(const id<MTLTexture>& texture);
    //ResourceDesc ExtractBufferDesc(const id<MTLBuffer>& buffer);


    class BlitPass
    {
    public:
        class CopyPartial_Dest
        {
        public:
            IResource*          _resource;
            SubResourceId       _subResource;
            VectorPattern<unsigned, 3>      _leftTopFront;
        };

        class CopyPartial_Src
        {
        public:
            IResource*          _resource;
            SubResourceId       _subResource;
            VectorPattern<unsigned, 3>      _leftTopFront;
            VectorPattern<unsigned, 3>      _rightBottomBack;
        };

        void    Write(
            const CopyPartial_Dest& dst,
            const SubResourceInitData& srcData,
            Format srcDataFormat,
            VectorPattern<unsigned, 3> srcDataDimensions);

        void    Copy(
            const CopyPartial_Dest& dst,
            const CopyPartial_Src& src);

        BlitPass(IThreadContext& threadContext);
        ~BlitPass();

    private:
        DeviceContext* _devContext;
        bool _openedEncoder;
    };
}}
