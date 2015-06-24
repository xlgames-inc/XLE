// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "VegetationSpawn.h"
#include "SceneEngineUtils.h"
#include "LightingParserContext.h"
#include "LightingParser.h"
#include "SceneParser.h"
#include "Noise.h"

#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/DeviceContextImpl.h"
#include "../RenderCore/Metal/GPUProfiler.h"
#include "../RenderCore/RenderUtils.h"

#include "../RenderCore/Assets/ModelCache.h"

#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/DataPacket.h"
#include "../BufferUploads/ResourceLocator.h"

#include "../ConsoleRig/Console.h"
#include "../Utility/StringFormat.h"

#include "../RenderCore/DX11/Metal/DX11Utils.h"

#include "../RenderCore/Assets/ModelRunTime.h"
#include "../RenderCore/Assets/DelayedDrawCall.h"
#include "../RenderCore/Assets/SharedStateSet.h"
#include "../RenderCore/Assets/ModelCache.h"
#include "../RenderCore/Techniques/Techniques.h"

#pragma warning(disable:4127)       // warning C4127: conditional expression is constant

namespace SceneEngine
{
    using namespace RenderCore;

    class VegetationSpawnResources
    {
    public:
        class Desc 
        {
        public:
            unsigned _bufferCount;
            Desc(unsigned bufferCount) : _bufferCount(bufferCount) {}
        };

        intrusive_ptr<ID3D::Buffer> _streamOutputBuffers[2];
        RenderCore::Metal::ShaderResourceView _streamOutputSRV[2];
        intrusive_ptr<ID3D::Buffer> _indirectArgsBuffer;
        intrusive_ptr<ID3D::Query> _streamOutputCountsQuery;

        intrusive_ptr<ID3D::Resource> _clearedTypesResource;

        std::vector<intrusive_ptr<ID3D::Buffer>> _instanceBuffers;
        std::vector<RenderCore::Metal::UnorderedAccessView> _instanceBufferUAVs;
        std::vector<RenderCore::Metal::ShaderResourceView> _instanceBufferSRVs;

        bool _isPrepared;
        unsigned _objectTypeCount;

        VegetationSpawnResources(const Desc&);
    };

    const unsigned StreamOutputMaxCount = 16*1024;
    const unsigned InstanceBufferMaxCount = 16*1024;

    VegetationSpawnResources::VegetationSpawnResources(const Desc& desc)
    {
        _isPrepared = false;

        using namespace BufferUploads;
        using namespace RenderCore::Metal;
        auto& uploads = GetBufferUploads();

        BufferDesc bufferDesc;
        bufferDesc._type = BufferDesc::Type::LinearBuffer;
        bufferDesc._bindFlags = BindFlag::StreamOutput | BindFlag::ShaderResource | BindFlag::RawViews;
        bufferDesc._cpuAccess = 0;
        bufferDesc._gpuAccess = GPUAccess::Read | GPUAccess::Write;
        bufferDesc._allocationRules = 0;
        bufferDesc._linearBufferDesc._sizeInBytes = 4*4*StreamOutputMaxCount;
        bufferDesc._linearBufferDesc._structureByteSize = 4*4;
        XlCopyString(bufferDesc._name, dimof(bufferDesc._name), "SpawningInstancesBuffer");

        intrusive_ptr<ID3D::Resource> so0r = uploads.Transaction_Immediate(bufferDesc)->AdoptUnderlying();
        intrusive_ptr<ID3D::Buffer> so0b = QueryInterfaceCast<ID3D::Buffer>(so0r);
        bufferDesc._linearBufferDesc._sizeInBytes = 4*StreamOutputMaxCount;
        bufferDesc._linearBufferDesc._structureByteSize = 4;
        intrusive_ptr<ID3D::Resource> so1r = uploads.Transaction_Immediate(bufferDesc)->AdoptUnderlying();
        intrusive_ptr<ID3D::Buffer> so1b = QueryInterfaceCast<ID3D::Buffer>(so1r);

        auto clearedBufferData = BufferUploads::CreateEmptyPacket(bufferDesc);
        XlSetMemory(clearedBufferData->GetData(), 0, clearedBufferData->GetDataSize());
        intrusive_ptr<ID3D::Resource> clearedTypesResource = uploads.Transaction_Immediate(bufferDesc, clearedBufferData.get())->AdoptUnderlying();

        ShaderResourceView so0srv(so0r.get(), NativeFormat::R32_TYPELESS); // NativeFormat::R32G32B32A32_FLOAT);
        ShaderResourceView so1srv(so1r.get(), NativeFormat::R32_TYPELESS); // NativeFormat::R32_UINT);

            // create the true instancing buffers
            //      Note that it might be ideal if these were vertex buffers! But we can't make a buffer that is both a vertex buffer and structured buffer
            //      We want to write to an append structure buffer. So let's make it a shader resource, and read from it using Load int the vertex shader
        bufferDesc._bindFlags = BindFlag::UnorderedAccess | BindFlag::StructuredBuffer | BindFlag::ShaderResource;
        bufferDesc._linearBufferDesc._sizeInBytes = 4*4*InstanceBufferMaxCount;
        bufferDesc._linearBufferDesc._structureByteSize = 4*4;

        std::vector<intrusive_ptr<ID3D::Buffer>> instanceBuffers; instanceBuffers.reserve(desc._bufferCount);
        std::vector<RenderCore::Metal::UnorderedAccessView> instanceBufferUAVs; instanceBufferUAVs.reserve(desc._bufferCount);
        std::vector<RenderCore::Metal::ShaderResourceView> instanceBufferSRVs; instanceBufferSRVs.reserve(desc._bufferCount);
        for (unsigned c=0; c<desc._bufferCount; ++c) {
            intrusive_ptr<ID3D::Resource> res = uploads.Transaction_Immediate(bufferDesc, nullptr)->AdoptUnderlying();
            instanceBuffers.push_back(QueryInterfaceCast<ID3D::Buffer>(res));
            instanceBufferUAVs.push_back(RenderCore::Metal::UnorderedAccessView(res.get(), RenderCore::Metal::NativeFormat::Unknown, 0, true));
            instanceBufferSRVs.push_back(RenderCore::Metal::ShaderResourceView(res.get()));
        }

        auto indirectArgsBufferDesc = bufferDesc;
        indirectArgsBufferDesc._cpuAccess = CPUAccess::WriteDynamic;
        indirectArgsBufferDesc._bindFlags = BindFlag::DrawIndirectArgs | BindFlag::VertexBuffer;
        indirectArgsBufferDesc._linearBufferDesc._sizeInBytes = 4*5;
        indirectArgsBufferDesc._linearBufferDesc._structureByteSize = 4*5;
        XlCopyString(indirectArgsBufferDesc._name, dimof(bufferDesc._name), "IndirectArgsBuffer");
        auto indirectArgsRes = uploads.Transaction_Immediate(indirectArgsBufferDesc, nullptr)->AdoptUnderlying();
        auto indirectArgsBuffer = QueryInterfaceCast<ID3D::Buffer>(indirectArgsRes);

        D3D11_QUERY_DESC queryDesc;
        queryDesc.Query = D3D11_QUERY_SO_STATISTICS;
        queryDesc.MiscFlags = 0;
        auto streamOutputCountsQuery = ObjectFactory().CreateQuery(&queryDesc);

        _streamOutputBuffers[0] = std::move(so0b);
        _streamOutputBuffers[1] = std::move(so1b);
        _streamOutputSRV[0] = std::move(so0srv);
        _streamOutputSRV[1] = std::move(so1srv);
        _clearedTypesResource = std::move(clearedTypesResource);
        _instanceBuffers = std::move(instanceBuffers);
        _indirectArgsBuffer = std::move(indirectArgsBuffer);
        _streamOutputCountsQuery = std::move(streamOutputCountsQuery);
        _instanceBufferUAVs = std::move(instanceBufferUAVs);
        _instanceBufferSRVs = std::move(instanceBufferSRVs);
        _objectTypeCount = desc._bufferCount;
    }

    void VegetationSpawn_Prepare(
        RenderCore::Metal::DeviceContext* context,
        LightingParserContext& parserContext,
        const VegetationSpawnConfig& cfg,
        VegetationSpawnResources& res)
    {
            //  Prepare the scene for vegetation spawn
            //  This means binding our output buffers to the stream output slots,
            //  and then rendering the terrain with a special technique.
            //  We can use flags to force the scene parser to render only the terrain
            //
            //  If we use "GeometryShader::SetDefaultStreamOutputInitializers", then future
            //  geometry shaders will be created as stream-output shaders.
            //

        using namespace RenderCore::Metal;
        auto oldSO = GeometryShader::GetDefaultStreamOutputInitializers();
        // SavedTargets oldTargets(context);
        ID3D::Query* begunQuery = nullptr;

        auto oldCamera = parserContext.GetSceneParser()->GetCameraDesc();
        ViewportDesc viewport(*context);

        TRY 
        {
            auto& perlinNoiseRes = Techniques::FindCachedBox<SceneEngine::PerlinNoiseResources>(SceneEngine::PerlinNoiseResources::Desc());
            context->BindGS(RenderCore::MakeResourceList(12, perlinNoiseRes._gradShaderResource, perlinNoiseRes._permShaderResource));
            context->BindGS(RenderCore::MakeResourceList(RenderCore::Metal::SamplerState()));

                //  we have to clear vertex input "3", because this is the instancing input slot -- and 
                //  we're going to be writing to buffers that will be used for instancing.
            // ID3D::Buffer* nullBuffer = nullptr; unsigned zero = 0;
            // context->GetUnderlying()->IASetVertexBuffers(3, 1, &nullBuffer, &zero, &zero);
            context->UnbindVS<ShaderResourceView>(15, 1);

            unsigned objectTypeCount = (unsigned)cfg._buckets.size();
            class InstanceSpawnConstants
            {
            public:
                Float4x4    _worldToCullFrustum;
                float       _gridSpacing, _objectTypeCount;
                unsigned    _dummy[2];
                Float4       _jitterAmount[8];
                Float4       _maxDrawDistanceSq[8];
            } instanceSpawnConstants = {
                parserContext.GetProjectionDesc()._worldToProjection,
                cfg._baseGridSpacing, float(objectTypeCount), 0, 0,
                { Zero<Float4>(), Zero<Float4>(), Zero<Float4>(), Zero<Float4>(), Zero<Float4>(), Zero<Float4>(), Zero<Float4>(), Zero<Float4>() },
                { Zero<Float4>(), Zero<Float4>(), Zero<Float4>(), Zero<Float4>(), Zero<Float4>(), Zero<Float4>(), Zero<Float4>(), Zero<Float4>() }
            };

            for (unsigned c=0; c<(unsigned)std::min(dimof(instanceSpawnConstants._jitterAmount), cfg._buckets.size()); ++c) {
                instanceSpawnConstants._jitterAmount[c][0] = cfg._buckets[c]._jitterAmount;
                instanceSpawnConstants._maxDrawDistanceSq[c][0] = cfg._buckets[c]._maxDrawDistance * cfg._buckets[c]._maxDrawDistance;
            }

            context->BindGS(RenderCore::MakeResourceList(5, ConstantBuffer(&instanceSpawnConstants, sizeof(InstanceSpawnConstants))));

            const bool needQuery = false;
            if (needQuery) {
                begunQuery = res._streamOutputCountsQuery.get();
                context->GetUnderlying()->Begin(begunQuery);
            }

            static const InputElementDesc eles[] = {

                    //  Our instance format is very simple. It's just a position and 
                    //  rotation value (in 32 bit floats)
                    //  I'm not sure if the hardware will support 16 bit floats in a
                    //  stream output buffer (though maybe we could use fixed point
                    //  16 bit integers?)
                    //  We write a "type" value to a second buffer. Let's keep that 
                    //  buffer as small as possible, because we have to clear it 
                    //  before hand

                InputElementDesc("INSTANCEPOS", 0, NativeFormat::R32G32B32A32_FLOAT),
                    // vertex in slot 1 must have a vertex stride that is a multiple of 4
                InputElementDesc("INSTANCEPARAM", 0, NativeFormat::R32_UINT, 1)
            };

                //  How do we clear an SO buffer? We can't make it an unorderedaccess view or render target.
                //  The only obvious way is to use CopyResource, and copy from a prepared "cleared" buffer
            context->GetUnderlying()->CopyResource(res._streamOutputBuffers[1].get(), res._clearedTypesResource.get());

            unsigned strides[2] = { 4*4, 4 };
            unsigned offsets[2] = { 0, 0 };
            GeometryShader::SetDefaultStreamOutputInitializers(
                GeometryShader::StreamOutputInitializers(eles, dimof(eles), strides, 2));

            SceneParseSettings parseSettings(
                SceneParseSettings::BatchFilter::General,
                SceneParseSettings::Toggles::Terrain);

                // Adjust the far clip so that it's very close...
                // We might want to widen the field of view slightly
                // by moving the camera back a bit. This could help make
                // sure that objects near the camera and on the edge of the screen
                // get included
            auto cameraDesc = oldCamera;
            cameraDesc._farClip = 100.f;

                //  We have to call "SetGlobalTransform" to force the camera changes to have effect.
                //  Ideally there would be a cleaner way to automatically update the constants
                //  when the bound camera changes...
            LightingParser_SetGlobalTransform(
                context, parserContext, cameraDesc, 
                unsigned(viewport.Width), unsigned(viewport.Height));

            ID3D::Buffer* targets[2] = { res._streamOutputBuffers[0].get(), res._streamOutputBuffers[1].get() };
            context->GetUnderlying()->SOSetTargets(2, targets, offsets);
            parserContext.GetSceneParser()->ExecuteScene(context, parserContext, parseSettings, 5);

            context->GetUnderlying()->SOSetTargets(0, nullptr, nullptr);

                //  After the scene execute, we need to use a compute shader to separate the 
                //  stream output data into it's bins.
            ID3D::UnorderedAccessView* outputBins[8];
            UINT initialCounts[8];
            std::fill(outputBins, &outputBins[dimof(outputBins)], nullptr);
            std::fill(initialCounts, &initialCounts[dimof(initialCounts)], 0);
            for (unsigned c=0; c<std::min(objectTypeCount, (unsigned)dimof(outputBins)); ++c) {
                outputBins[c] = res._instanceBufferUAVs[c].GetUnderlying();

                unsigned clearValues[] = { 0, 0, 0, 0 };
                context->Clear(res._instanceBufferUAVs[c], clearValues);
            }
            context->BindCS(RenderCore::MakeResourceList(res._streamOutputSRV[0], res._streamOutputSRV[1]));
            context->GetUnderlying()->CSSetUnorderedAccessViews(0, std::min(objectTypeCount, (unsigned)dimof(outputBins)), outputBins, initialCounts);

            context->Bind(::Assets::GetAssetDep<ComputeShader>(
                "game/xleres/Vegetation/InstanceSpawnSeparate.csh:main:cs_*", 
                (StringMeld<64>() << "INSTANCE_BIN_COUNT=" << objectTypeCount).get()));
            context->Dispatch(StreamOutputMaxCount / 256);

                // clear all of the UAVs again
            context->UnbindCS<UnorderedAccessView>(0, std::min(objectTypeCount, (unsigned)dimof(outputBins)));
            context->UnbindCS<ShaderResourceView>(0, 2);

            res._isPrepared = true;

        } 
        CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH_END

        if (begunQuery) {
            context->GetUnderlying()->End(begunQuery);
        }

            // (reset the camera transform if it's changed)
        LightingParser_SetGlobalTransform(
            context, parserContext, oldCamera, 
            unsigned(viewport.Width), unsigned(viewport.Height));

        context->GetUnderlying()->SOSetTargets(0, nullptr, nullptr);
        GeometryShader::SetDefaultStreamOutputInitializers(oldSO);
        // oldTargets.ResetToOldTargets(context);
    }

    static unsigned GetSOPrimitives(RenderCore::Metal::DeviceContext* context, ID3D::Query* query)
    {
        auto querySize = query->GetDataSize();
        uint8 soStatsBuffer[256];
        assert(querySize <= sizeof(soStatsBuffer));
        context->GetUnderlying()->GetData(query, soStatsBuffer, std::min(querySize, (unsigned)sizeof(soStatsBuffer)), 0);
        return (unsigned)((D3D11_QUERY_DATA_SO_STATISTICS*)soStatsBuffer)->NumPrimitivesWritten;
    }

    bool VegetationSpawn_DrawInstances(
        RenderCore::Metal::DeviceContext* context,
        VegetationSpawnResources& res,
        unsigned instanceId, unsigned indexCount, unsigned startIndexLocation, unsigned baseVertexLocation)
    {
            //  We must draw the currently queued geometry using instance information 
            //  we calculated in the prepare phase.
            //  We have to write the following structure into the indirect args buffer:
            //      struct DrawIndexedInstancedIndirectArgs {
            //          UINT IndexCountPerInstance; 
            //          UINT InstanceCount;
            //          UINT StartIndexLocation;
            //          INT BaseVertexLocation;
            //          UINT StartInstanceLocation;
            //      }
        if (!res._isPrepared || instanceId > res._instanceBuffers.size())
            return false;

        D3D11_MAPPED_SUBRESOURCE mappedSub;
        auto hresult = context->GetUnderlying()->Map(res._indirectArgsBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSub);
        if (!SUCCEEDED(hresult))
            return false;

            //
            //      We don't have a good way to copy the stream output counts data from the SO target into the 
            //      args buffer. We can do that easily with an AppendStructuredBuffer, but not with an 
            //      SO target.
            //
            //      we have two options:
            //          * Use D3D11.1, and write to an UnorderedAccessView from the geometry shader
            //          * Use a compute shader for the prepare step
            //
            //      Using a compute shader would be more powerful. But we'd have to adjust the mesh rendering
            //      to support Dispatch()ing a compute shader instead of Draw() a shader program
            //
            //      Write now, we can just use the "D3D11_QUERY_SO_STATISTICS" query. But this causes a CPU/GPU
            //      sync! So it needs to be replaced.
            //
            //      Even with D3D11.1, there is a problem. We can't use interlocked instructions from the GS,
            //      so we can't maintain a instance count (or even use AppendStructuredBuffer). Also, the compute
            //      shader only approach won't work for tessellated terrain, because the LOD work for terrain would 
            //      be too complex to re-implement in the compute shader.
            //
            //      To get XLE terrain working; we could use a terrain geometry shader write out the raw triangles
            //      from the tessellation processing -- (which could then be used for both drawing an calculating
            //      instances)... But they we run into the same problem. How do we know the number of primitives
            //      written out by the geometry shader?
            //
            //      Even worse, it might be that the geometry shader won't always write to the start of the output
            //      buffer... With the hit detection code, it seems like stream output will sometimes start at
            //      a random offset in the stream output targets.
            //
            //      So how do we get the instance count? We could use another compute shader to count the number
            //      of valid instances that ended up in the buffer. That would mean clearing the buffer first,
            //      and then looking for instances that are still clear. We could use the same process to 
            //      separate the instances into different bins (for different geometry objects).
            //
            //          -- so we write many instances from the geometry shader first, of the form:
            //              x, y, z, rotation, type (....)
            //
            //      Then we use a compute shader to separate those instances into AppendStructuredBuffer 
            //      so that when we finally drop the objects, we have separate input vertex buffers for each
            //      type of geometry input.
            //
            //      In this way, we get the final result we need (with the number of instances correct). But
            //      it requires an extra pass with the compute shader -- that might add a little overhead.
            //      It seems to be the best way, however, because we keep the flexibility of using a geometry
            //      shader -- and this can be used as part of a shader pipeline with many different vertex
            //      shaders and input geometry types.
            //

        enum PrimitiveCountMethod { FromQuery, FromUAV } primitiveCountMethod = FromUAV;

        struct DrawIndexedInstancedIndirectArgs 
        {
            UINT IndexCountPerInstance; 
            UINT InstanceCount;
            UINT StartIndexLocation;
            INT BaseVertexLocation;
            UINT StartInstanceLocation;
        };
        auto& args = *(DrawIndexedInstancedIndirectArgs*)mappedSub.pData;
        args.IndexCountPerInstance = indexCount;
        args.StartIndexLocation = startIndexLocation;
        args.BaseVertexLocation = baseVertexLocation;
        args.StartInstanceLocation = 0;
        args.InstanceCount = 0;

        if (primitiveCountMethod == FromQuery) {
            auto primitiveCount = GetSOPrimitives(context, res._streamOutputCountsQuery.get());
            args.InstanceCount = (UINT)primitiveCount;
        }

            // note --  we may be able to use the query to skip the compute shader step
            //          in cases where there is zero vegetation generated. Possibly a GPU
            //          predicate can help avoid a CPU sync when doing that.

        context->GetUnderlying()->Unmap(res._indirectArgsBuffer.get(), 0);

        if (primitiveCountMethod == FromUAV) {
                // copy the "structure count" from the UAV into the indirect args buffer
            context->GetUnderlying()->CopyStructureCount(
                res._indirectArgsBuffer.get(), unsigned(&((DrawIndexedInstancedIndirectArgs*)nullptr)->InstanceCount), res._instanceBufferUAVs[instanceId].GetUnderlying());
        }

            // bind the instancing buffer as an input vertex buffer
            //  This "instancing buffer" is the output from our separation compute shader
        // unsigned stride = 4*4, offset = 0;
        // auto* buffer = res._instanceBuffers[instanceId].get();
        // const unsigned slotForVertexInput = 3;
        // context->GetUnderlying()->IASetVertexBuffers(slotForVertexInput, 1, &buffer, &stride, &offset);
        context->BindVS(RenderCore::MakeResourceList(15, res._instanceBufferSRVs[instanceId]));

            // finally -- draw
        context->GetUnderlying()->DrawIndexedInstancedIndirect(res._indirectArgsBuffer.get(), 0);

        return true;
    }

    class VegetationSpawnManager::Pimpl
    {
    public:
        std::shared_ptr<RenderCore::Assets::ModelCache> _modelCache;
        std::shared_ptr<VegetationSpawnPlugin> _parserPlugin;
        std::unique_ptr<VegetationSpawnResources> _resources;
        VegetationSpawnConfig _cfg;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    class VegetationSpawnPlugin : public ILightingParserPlugin
    {
    public:
        virtual void OnPreScenePrepare(
            RenderCore::Metal::DeviceContext* context, LightingParserContext&) const;
        virtual void OnLightingResolvePrepare(
            RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext,
            LightingResolveContext& resolveContext) const;
        virtual void OnPostSceneRender(
            RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext, 
            const SceneParseSettings& parseSettings, unsigned techniqueIndex) const;

        VegetationSpawnPlugin(VegetationSpawnManager::Pimpl& pimpl);
        ~VegetationSpawnPlugin();

    protected:
        VegetationSpawnManager::Pimpl* _pimpl; // (must be unprotected to avoid cyclic dependency)
    };

    void VegetationSpawnPlugin::OnPreScenePrepare(
        RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext) const
    {
        if (_pimpl->_cfg._buckets.empty()) return;

        Metal::GPUProfiler::DebugAnnotation anno(*context, L"VegetationSpawn");
        VegetationSpawn_Prepare(context, parserContext, _pimpl->_cfg, *_pimpl->_resources.get()); 
    }

    void VegetationSpawnPlugin::OnLightingResolvePrepare(
        RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext,
        LightingResolveContext& resolveContext) const {}

    void VegetationSpawnPlugin::OnPostSceneRender(
        RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext, 
        const SceneParseSettings& parseSettings, unsigned techniqueIndex) const {}

    VegetationSpawnPlugin::VegetationSpawnPlugin(VegetationSpawnManager::Pimpl& pimpl)
    : _pimpl(&pimpl) {}
    VegetationSpawnPlugin::~VegetationSpawnPlugin() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    void VegetationSpawnManager::Render(
        RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext,
        unsigned techniqueIndex)
    {
        if (_pimpl->_cfg._buckets.empty()) return;

        using namespace RenderCore;
        using namespace RenderCore::Assets;
        auto& cache = *_pimpl->_modelCache;
        
        auto& sharedStates = cache.GetSharedStateSet();
        sharedStates.CaptureState(context);
        parserContext.GetTechniqueContext()._runtimeState.SetParameter((const utf8*)"SPAWNED_INSTANCE", 1);
        
        auto* resources = _pimpl->_resources.get();

        DelayedDrawCallSet preparedDrawCalls(typeid(ModelRenderer).hash_code());
        for (unsigned b=0; b<unsigned(_pimpl->_cfg._buckets.size()); ++b) {
            TRY {
                const auto& bkt = _pimpl->_cfg._buckets[b];
                auto model = cache.GetModel(bkt._modelName.c_str(), bkt._materialName.c_str());
                preparedDrawCalls.Reset();
                model._renderer->Prepare(preparedDrawCalls, sharedStates, Identity<Float4x4>());
                ModelRenderer::RenderPrepared(
                    ModelRendererContext(context, parserContext, techniqueIndex),
                    sharedStates, preparedDrawCalls, DelayStep::OpaqueRender,
                    [context, b, resources](uint32 indexCount, uint32 startIndexLoc, uint32 baseVertexLoc)
                    {
                        VegetationSpawn_DrawInstances(
                            context, *resources, b, 
                            indexCount, startIndexLoc, baseVertexLoc);
                    });
            } CATCH(...) {
            } CATCH_END
        }

        parserContext.GetTechniqueContext()._runtimeState.SetParameter((const utf8*)"SPAWNED_INSTANCE", 0);
        sharedStates.ReleaseState(context);
    }

    void VegetationSpawnManager::Load(const VegetationSpawnConfig& cfg)
    {
        _pimpl->_cfg = cfg;
        _pimpl->_resources = std::make_unique<VegetationSpawnResources>(
            VegetationSpawnResources::Desc((unsigned)cfg._buckets.size()));
    }

    void VegetationSpawnManager::Reset()
    {
        _pimpl->_cfg = VegetationSpawnConfig();
        _pimpl->_resources.reset();
    }

    std::shared_ptr<ILightingParserPlugin> VegetationSpawnManager::GetParserPlugin()
    {
        return _pimpl->_parserPlugin;
    }

    VegetationSpawnManager::VegetationSpawnManager(std::shared_ptr<RenderCore::Assets::ModelCache> modelCache)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_modelCache = std::move(modelCache);
        _pimpl->_parserPlugin = std::make_shared<VegetationSpawnPlugin>(*_pimpl);
    }

    VegetationSpawnManager::~VegetationSpawnManager() {}


    VegetationSpawnConfig::VegetationSpawnConfig(const ::Assets::ResChar src[])
    {
    }
    VegetationSpawnConfig::VegetationSpawnConfig() {}
    VegetationSpawnConfig::~VegetationSpawnConfig() {}

}

