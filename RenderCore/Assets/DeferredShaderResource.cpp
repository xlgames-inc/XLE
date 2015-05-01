// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DeferredShaderResource.h"
#include "../Metal/ShaderResource.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../BufferUploads/DataPacket.h"
#include "../../BufferUploads/ResourceLocator.h"
#include "../../Utility/Streams/PathUtils.h"

namespace RenderCore { namespace Assets 
{
    using ResChar = ::Assets::ResChar;

    const ResChar* DeferredShaderResource::LinearSpace = "linear";
    const ResChar* DeferredShaderResource::SRGBSpace = "srgb";

    static BufferUploads::IManager* s_bufferUploads = nullptr;
    void SetBufferUploads(BufferUploads::IManager* bufferUploads)
    {
        s_bufferUploads = bufferUploads;
    }

    class DeferredShaderResource::Pimpl
    {
    public:
        BufferUploads::TransactionID _transaction;
        intrusive_ptr<BufferUploads::ResourceLocator> _locator;
        Metal::ShaderResourceView _srv;
    };

    DeferredShaderResource::DeferredShaderResource(const ResChar initializer[], const ResChar sourceSpace[])
    {
        DEBUG_ONLY(XlCopyString(_initializer, dimof(_initializer), initializer);)
        _pimpl = std::make_unique<Pimpl>();

        using namespace BufferUploads;
        auto pkt = CreateStreamingTextureSource(initializer);
        _pimpl->_transaction = s_bufferUploads->Transaction_Begin(
            CreateDesc(
                BindFlag::ShaderResource,
                0, GPUAccess::Read,
                TextureDesc::Empty(), initializer),
            pkt.get());

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        RegisterFileDependency(_validationCallback, initializer);
    }

    DeferredShaderResource::~DeferredShaderResource()
    {
        if (_pimpl->_transaction != ~BufferUploads::TransactionID(0))
            s_bufferUploads->Transaction_End(_pimpl->_transaction);
    }

    const Metal::ShaderResourceView&       DeferredShaderResource::GetShaderResource() const
    {
        if (!_pimpl->_srv.GetUnderlying()) {
            if (_pimpl->_transaction == ~BufferUploads::TransactionID(0))
                ThrowException(::Assets::Exceptions::InvalidResource(Initializer(), "Unknown error during loading"));

            if (!s_bufferUploads->IsCompleted(_pimpl->_transaction))
                ThrowException(::Assets::Exceptions::PendingResource(Initializer(), ""));

            _pimpl->_locator = s_bufferUploads->GetResource(_pimpl->_transaction);
            s_bufferUploads->Transaction_End(_pimpl->_transaction);
            _pimpl->_transaction = ~BufferUploads::TransactionID(0);

            if (!_pimpl->_locator || !_pimpl->_locator->GetUnderlying())
                ThrowException(::Assets::Exceptions::InvalidResource(Initializer(), "Unknown error during loading"));

            _pimpl->_srv = Metal::ShaderResourceView(_pimpl->_locator->GetUnderlying());
        }

        return _pimpl->_srv;
    }

    const char*                     DeferredShaderResource::Initializer() const
    {
        #if defined(_DEBUG)
            return _initializer;
        #else
            return "";
        #endif
    }


}}

