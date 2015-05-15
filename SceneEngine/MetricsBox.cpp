// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MetricsBox.h"
#include "SceneEngineUtils.h"
#include "../BufferUploads/ResourceLocator.h"

namespace SceneEngine
{
    MetricsBox::MetricsBox(const Desc& desc) 
    {
        using namespace RenderCore;
        using namespace RenderCore::Metal;
        using namespace BufferUploads;

        auto& uploads = GetBufferUploads();
        BufferUploads::BufferDesc metricsBufferDesc;
        metricsBufferDesc._type = BufferUploads::BufferDesc::Type::LinearBuffer;
        metricsBufferDesc._bindFlags = BindFlag::UnorderedAccess|BindFlag::StructuredBuffer|BindFlag::ShaderResource;
        metricsBufferDesc._cpuAccess = 0;
        metricsBufferDesc._gpuAccess = GPUAccess::Read|GPUAccess::Write;
        metricsBufferDesc._allocationRules = 0;
        metricsBufferDesc._linearBufferDesc._structureByteSize = sizeof(unsigned)*16;
        metricsBufferDesc._linearBufferDesc._sizeInBytes = metricsBufferDesc._linearBufferDesc._structureByteSize;
        auto metricsBuffer = uploads.Transaction_Immediate(metricsBufferDesc)->AdoptUnderlying();

        UnorderedAccessView  metricsBufferUAV(metricsBuffer.get());
        ShaderResourceView   metricsBufferSRV(metricsBuffer.get());

        _metricsBufferResource = std::move(metricsBuffer);
        _metricsBufferUAV = std::move(metricsBufferUAV);
        _metricsBufferSRV = std::move(metricsBufferSRV);
    }

    MetricsBox::~MetricsBox() {}
}
