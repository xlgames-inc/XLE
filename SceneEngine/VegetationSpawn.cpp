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
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/GPUProfiler.h"
#include "../RenderCore/Metal/ObjectFactory.h"
#include "../RenderCore/RenderUtils.h"

#include "../RenderCore/Assets/ModelCache.h"

#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/DataPacket.h"
#include "../BufferUploads/ResourceLocator.h"

#include "../ConsoleRig/Console.h"
#include "../Utility/StringFormat.h"
#include "../Utility/FunctionUtils.h"
#include "../Math/Transformations.h"

#include "../RenderCore/Assets/ModelRunTime.h"
#include "../RenderCore/Assets/DelayedDrawCall.h"
#include "../RenderCore/Assets/SharedStateSet.h"
#include "../RenderCore/Techniques/Techniques.h"

#include "../RenderCore/Metal/DeviceContextImpl.h"
#include "../RenderCore/DX11/Metal/DX11Utils.h"


namespace SceneEngine
{
    using namespace RenderCore;

    extern bool g_TerrainVegetationSpawn_AlignToTerrainUp;

    class IndirectDrawBuffer
    {
    public:
        struct DrawIndexedInstancedIndirectArgs 
        {
            unsigned IndexCountPerInstance; 
            unsigned InstanceCount;
            unsigned StartIndexLocation;
            int BaseVertexLocation;
            unsigned StartInstanceLocation;
        };

        void Draw(Metal::DeviceContext& metalContext);
        bool WriteParams(
            Metal::DeviceContext& metalContext, 
            unsigned indexCountPerinstance, unsigned startIndexLocation, 
            unsigned baseVertexLocation, unsigned instanceCount);
        void CopyInstanceCount(
            Metal::DeviceContext& metalContext, Metal::UnorderedAccessView& src);

        IndirectDrawBuffer();
        ~IndirectDrawBuffer();
    private:
        Metal::VertexBuffer     _indirectArgsBuffer;
    };

    class VegetationSpawnResources
    {
    public:
        class Desc 
        {
        public:
            unsigned _bufferCount;
            unsigned _alignToTerrainUp;
            Desc(unsigned bufferCount, bool alignToTerrainUp) 
            : _bufferCount(bufferCount), _alignToTerrainUp(alignToTerrainUp) {}
        };

        Metal::VertexBuffer         _streamOutputBuffers[2];
        Metal::ShaderResourceView   _streamOutputSRV[2];
        intrusive_ptr<ID3D::Query>  _streamOutputCountsQuery;

        using ResLocator = intrusive_ptr<BufferUploads::ResourceLocator>;
        ResLocator          _clearedTypesResource;
        ResLocator          _streamOutputResources[2];

        using UAV = Metal::UnorderedAccessView;
        using SRV = Metal::ShaderResourceView;
        std::vector<UAV>    _instanceBufferUAVs;
        std::vector<SRV>    _instanceBufferSRVs;
        IndirectDrawBuffer  _indirectDrawBuffer;
        bool                _isPrepared;
        unsigned            _objectTypeCount;
        bool                _alignToTerrainUp;

        VegetationSpawnResources(const Desc&);
    };

    static const auto StreamOutputMaxCount = 16u*1024u;
    static const auto InstanceBufferMaxCount = 16u*1024u;
    static const auto Stream0VertexSize = 4*4;
    static const auto Stream1VertexSize_NoAlign = 4;
    static const auto Stream1VertexSize_TerrainAlign = 3*4;

    VegetationSpawnResources::VegetationSpawnResources(const Desc& desc)
    {
        _isPrepared = false;

        using namespace BufferUploads;
        auto& uploads = GetBufferUploads();

        auto bufferDesc = CreateDesc(
            BindFlag::StreamOutput | BindFlag::ShaderResource | BindFlag::RawViews,
            0, GPUAccess::Read | GPUAccess::Write,
            LinearBufferDesc::Create(Stream0VertexSize*StreamOutputMaxCount, Stream0VertexSize),
            "SpawningInstancesBuffer");
        auto so0r = uploads.Transaction_Immediate(bufferDesc);

        auto stream1VertexSize = desc._alignToTerrainUp ? Stream1VertexSize_TerrainAlign : Stream1VertexSize_NoAlign;
        bufferDesc._linearBufferDesc = LinearBufferDesc::Create(stream1VertexSize*StreamOutputMaxCount, stream1VertexSize);
        auto so1r = uploads.Transaction_Immediate(bufferDesc);

        auto clearedBufferData = BufferUploads::CreateEmptyPacket(bufferDesc);
        XlSetMemory(clearedBufferData->GetData(), 0, clearedBufferData->GetDataSize());
        auto clearedTypesResource = uploads.Transaction_Immediate(bufferDesc, clearedBufferData.get());

        Metal::ShaderResourceView so0srv(so0r->GetUnderlying(), Metal::NativeFormat::R32_TYPELESS); // NativeFormat::R32G32B32A32_FLOAT);
        Metal::ShaderResourceView so1srv(so1r->GetUnderlying(), Metal::NativeFormat::R32_TYPELESS); // NativeFormat::R32_UINT);

            // create the true instancing buffers
            //      Note that it might be ideal if these were vertex buffers! But we can't make a buffer that is both a vertex buffer and structured buffer
            //      We want to write to an append structure buffer. So let's make it a shader resource, and read from it using Load int the vertex shader
        bufferDesc._bindFlags = BindFlag::UnorderedAccess | BindFlag::StructuredBuffer | BindFlag::ShaderResource;
        bufferDesc._linearBufferDesc._structureByteSize = 4*4 + (desc._alignToTerrainUp ? 3*3*4 : 2*4);
        bufferDesc._linearBufferDesc._sizeInBytes = bufferDesc._linearBufferDesc._structureByteSize*InstanceBufferMaxCount;
        

        std::vector<intrusive_ptr<ID3D::Buffer>> instanceBuffers; instanceBuffers.reserve(desc._bufferCount);
        std::vector<Metal::UnorderedAccessView> instanceBufferUAVs; instanceBufferUAVs.reserve(desc._bufferCount);
        std::vector<Metal::ShaderResourceView> instanceBufferSRVs; instanceBufferSRVs.reserve(desc._bufferCount);
        for (unsigned c=0; c<desc._bufferCount; ++c) {
            auto res = uploads.Transaction_Immediate(bufferDesc);
            instanceBufferUAVs.push_back(Metal::UnorderedAccessView(res->GetUnderlying(), Metal::NativeFormat::Unknown, 0, true));
            instanceBufferSRVs.push_back(Metal::ShaderResourceView(res->GetUnderlying()));
        }

        D3D11_QUERY_DESC queryDesc;
        queryDesc.Query = D3D11_QUERY_SO_STATISTICS;
        queryDesc.MiscFlags = 0;
        auto streamOutputCountsQuery = Metal::GetObjectFactory()->CreateQuery(&queryDesc);

        _streamOutputBuffers[0] = Metal::VertexBuffer(so0r->GetUnderlying());
        _streamOutputBuffers[1] = Metal::VertexBuffer(so1r->GetUnderlying());
        _streamOutputResources[0] = std::move(so0r);
        _streamOutputResources[1] = std::move(so1r);
        _streamOutputSRV[0] = std::move(so0srv);
        _streamOutputSRV[1] = std::move(so1srv);
        _clearedTypesResource = clearedTypesResource;
        _streamOutputCountsQuery = std::move(streamOutputCountsQuery);
        _instanceBufferUAVs = std::move(instanceBufferUAVs);
        _instanceBufferSRVs = std::move(instanceBufferSRVs);
        _objectTypeCount = desc._bufferCount;
        _alignToTerrainUp = !!desc._alignToTerrainUp;
    }

    static RenderCore::Techniques::ProjectionDesc AdjustProjDesc(
        const RenderCore::Techniques::ProjectionDesc& input,
        float farClip)
    {
        auto result = input;
        auto clipSpaceType = RenderCore::Techniques::GetDefaultClipSpaceType();

        auto& proj = result._cameraToProjection;
        auto n = CalculateNearAndFarPlane(
            ExtractMinimalProjection(proj), clipSpaceType).first;

            // this should work for perspective & orthogonal
        auto n32 = -proj(3,2);
        if (clipSpaceType == ClipSpaceType::Positive) {
            proj(2,2) =     -LinearInterpolate(1.f, farClip, n32) / (farClip-n);
            proj(2,3) = -n * LinearInterpolate(1.f, farClip, n32) / (farClip-n);
        } else {
            assert(0);  // not implemented
        }

        result._farClip = farClip;
        result._cameraToProjection = Combine(result._cameraToWorld, proj);
        return result;
    }

    void VegetationSpawn_Prepare(
        RenderCore::IThreadContext& context,
        RenderCore::Metal::DeviceContext& metalContext, LightingParserContext& parserContext,
        PreparedScene& preparedScene,
        const VegetationSpawnConfig& cfg, VegetationSpawnResources& res)
    {
            //  Prepare the scene for vegetation spawn
            //  This means binding our output buffers to the stream output slots,
            //  and then rendering the terrain with a special technique.
            //  We can use flags to force the scene parser to render only the terrain
            //
            //  If we use "GeometryShader::SetDefaultStreamOutputInitializers", then future
            //  geometry shaders will be created as stream-output shaders.

        using namespace RenderCore;
        auto oldSO = Metal::GeometryShader::GetDefaultStreamOutputInitializers();
        ID3D::Query* begunQuery = nullptr;

        auto oldCamera = parserContext.GetProjectionDesc();

        CATCH_ASSETS_BEGIN
            auto& perlinNoiseRes = Techniques::FindCachedBox2<SceneEngine::PerlinNoiseResources>();
            metalContext.BindGS(MakeResourceList(12, perlinNoiseRes._gradShaderResource, perlinNoiseRes._permShaderResource));
            metalContext.BindGS(MakeResourceList(Metal::SamplerState()));

                //  we have to clear vertex input "3", because this is the instancing input slot -- and 
                //  we're going to be writing to buffers that will be used for instancing.
            // ID3D::Buffer* nullBuffer = nullptr; unsigned zero = 0;
            // context->GetUnderlying()->IASetVertexBuffers(3, 1, &nullBuffer, &zero, &zero);
            metalContext.Unbind<Metal::VertexBuffer>();
            metalContext.UnbindVS<Metal::ShaderResourceView>(15, 1);

            float maxDrawDistance = 0.f;
            for (const auto& m:cfg._materials)
                for (const auto& b:m._buckets)
                    maxDrawDistance = std::max(b._maxDrawDistance, maxDrawDistance);

            class InstanceSpawnConstants
            {
            public:
                Float4x4    _worldToCullFrustum;
                float       _gridSpacing, _baseDrawDistanceSq, _jitterAmount;
                unsigned    _dummy;
                Float4      _materialParams[8];
                Float4      _suppressionNoiseParams[8];
            } instanceSpawnConstants = {
                parserContext.GetProjectionDesc()._worldToProjection,
                cfg._baseGridSpacing, maxDrawDistance*maxDrawDistance, cfg._jitterAmount, 0,
                {   Zero<Float4>(), Zero<Float4>(), Zero<Float4>(), Zero<Float4>(), 
                    Zero<Float4>(), Zero<Float4>(), Zero<Float4>(), Zero<Float4>() },
                {   Zero<Float4>(), Zero<Float4>(), Zero<Float4>(), Zero<Float4>(), 
                    Zero<Float4>(), Zero<Float4>(), Zero<Float4>(), Zero<Float4>() }
            };

            for (unsigned mi=0; mi<std::min(cfg._materials.size(), dimof(instanceSpawnConstants._materialParams)); ++mi) {
                instanceSpawnConstants._materialParams[mi][0] = cfg._materials[mi]._suppressionThreshold;

                instanceSpawnConstants._suppressionNoiseParams[mi][0] = cfg._materials[mi]._suppressionNoise;
                instanceSpawnConstants._suppressionNoiseParams[mi][1] = cfg._materials[mi]._suppressionGain;
                instanceSpawnConstants._suppressionNoiseParams[mi][2] = cfg._materials[mi]._suppressionLacunarity;
            }

            metalContext.BindGS(MakeResourceList(5, Metal::ConstantBuffer(&instanceSpawnConstants, sizeof(InstanceSpawnConstants))));

            const bool needQuery = false;
            if (constant_expression<needQuery>::result()) {
                begunQuery = res._streamOutputCountsQuery.get();
                metalContext.GetUnderlying()->Begin(begunQuery);
            }

            static const Metal::InputElementDesc eles[] = {

                    //  Our instance format is very simple. It's just a position and 
                    //  rotation value (in 32 bit floats)
                    //  I'm not sure if the hardware will support 16 bit floats in a
                    //  stream output buffer (though maybe we could use fixed point
                    //  16 bit integers?)
                    //  We write a "type" value to a second buffer. Let's keep that 
                    //  buffer as small as possible, because we have to clear it 
                    //  before hand

                Metal::InputElementDesc("INSTANCEPOS", 0, Metal::NativeFormat::R32G32B32A32_FLOAT),
                    // vertex in slot 1 must have a vertex stride that is a multiple of 4
                Metal::InputElementDesc("INSTANCEPARAM", 0, Metal::NativeFormat::R32_UINT, 1)
            };

            static const Metal::InputElementDesc elesTerrainNormal[] = {
                Metal::InputElementDesc("INSTANCEPOS", 0, Metal::NativeFormat::R32G32B32A32_FLOAT),
                Metal::InputElementDesc("INSTANCEPARAM", 0, Metal::NativeFormat::R32G32B32_FLOAT, 1)
            };

                //  How do we clear an SO buffer? We can't make it an unorderedaccess view or render target.
                //  The only obvious way is to use CopyResource, and copy from a prepared "cleared" buffer
            Metal::Copy(metalContext, res._streamOutputResources[1]->GetUnderlying(), res._clearedTypesResource->GetUnderlying());

            const bool alignToTerrainUp = res._alignToTerrainUp;
            if (alignToTerrainUp) {
                unsigned strides[2] = { Stream0VertexSize, Stream1VertexSize_TerrainAlign };
                Metal::GeometryShader::SetDefaultStreamOutputInitializers(
                    Metal::GeometryShader::StreamOutputInitializers(elesTerrainNormal, dimof(elesTerrainNormal), strides, 2));
            } else {
                unsigned strides[2] = { Stream0VertexSize, Stream1VertexSize_NoAlign };
                Metal::GeometryShader::SetDefaultStreamOutputInitializers(
                    Metal::GeometryShader::StreamOutputInitializers(eles, dimof(eles), strides, 2));
            }
            g_TerrainVegetationSpawn_AlignToTerrainUp = alignToTerrainUp;

            SceneParseSettings parseSettings(
                SceneParseSettings::BatchFilter::General,
                SceneParseSettings::Toggles::Terrain);

                // Adjust the far clip so that it's very close...
                // We might want to widen the field of view slightly
                // by moving the camera back a bit. This could help make
                // sure that objects near the camera and on the edge of the screen
                // get included
            auto newProjDesc = AdjustProjDesc(oldCamera, maxDrawDistance);

                //  We have to call "SetGlobalTransform" to force the camera changes to have effect.
                //  Ideally there would be a cleaner way to automatically update the constants
                //  when the bound camera changes...
            LightingParser_SetGlobalTransform(metalContext, parserContext, newProjDesc);

            metalContext.BindSO(MakeResourceList(res._streamOutputBuffers[0], res._streamOutputBuffers[1]));
            parserContext.GetSceneParser()->ExecuteScene(context, parserContext, parseSettings, preparedScene, 5);
            metalContext.UnbindSO();

                //  After the scene execute, we need to use a compute shader to separate the 
                //  stream output data into it's bins.
            static const unsigned MaxOutputBinCount = 8;
            ID3D::UnorderedAccessView* outputBins[MaxOutputBinCount];
            unsigned initialCounts[MaxOutputBinCount];
            std::fill(outputBins, &outputBins[dimof(outputBins)], nullptr);
            std::fill(initialCounts, &initialCounts[dimof(initialCounts)], 0);

            auto outputBinCount = std::min((unsigned)cfg._objectTypes.size(), (unsigned)res._instanceBufferUAVs.size());
            for (unsigned c=0; c<outputBinCount; ++c) {
                unsigned clearValues[] = { 0, 0, 0, 0 };
                metalContext.Clear(res._instanceBufferUAVs[c], clearValues);
                outputBins[c] = res._instanceBufferUAVs[c].GetUnderlying();
            }

            metalContext.BindCS(MakeResourceList(res._streamOutputSRV[0], res._streamOutputSRV[1]));
            metalContext.GetUnderlying()->CSSetUnorderedAccessViews(0, outputBinCount, outputBins, initialCounts);

            class InstanceSeparateConstants
            {
            public:
                UInt4 _binThresholds[16];
                Float4 _drawDistanceSq[16];
            } instanceSeparateConstants;
            XlZeroMemory(instanceSeparateConstants);

            StringMeld<1024> shaderParams;

            unsigned premapBinCount = 0;
            for (unsigned mi=0; mi<cfg._materials.size(); ++mi) {
                const auto& m = cfg._materials[mi];
                float combinedWeight = m._noSpawnWeight;
                for (const auto& b:m._buckets) combinedWeight += b._frequencyWeight;

                unsigned weightIterator = 0;
                for (unsigned c=0; c<std::min(dimof(instanceSeparateConstants._binThresholds), m._buckets.size()); ++c) {
                    weightIterator += unsigned(4095.f * m._buckets[c]._frequencyWeight / combinedWeight);

                    instanceSeparateConstants._binThresholds[premapBinCount][0] = (mi<<12) | weightIterator;
                    instanceSeparateConstants._drawDistanceSq[premapBinCount][0] 
                        = m._buckets[c]._maxDrawDistance * m._buckets[c]._maxDrawDistance;

                    shaderParams << "OUTPUT_BUFFER_MAP" << premapBinCount << "=" << m._buckets[c]._objectType << ";";
                    ++premapBinCount;
                }
            }

            for (unsigned c=premapBinCount; c<dimof(instanceSeparateConstants._binThresholds); ++c)
                shaderParams << "OUTPUT_BUFFER_MAP" << c << "=0;";

            shaderParams << "INSTANCE_BIN_COUNT=" << premapBinCount;

            if (alignToTerrainUp)
                shaderParams << ";TERRAIN_NORMAL=1";

            metalContext.BindCS(MakeResourceList(
                parserContext.GetGlobalTransformCB(),
                Metal::ConstantBuffer(&instanceSeparateConstants, sizeof(instanceSeparateConstants))));

            metalContext.Bind(::Assets::GetAssetDep<Metal::ComputeShader>(
                "game/xleres/Vegetation/InstanceSpawnSeparate.csh:main:cs_*", 
                shaderParams.get()));
            metalContext.Dispatch(StreamOutputMaxCount / 256);

                // unbind all of the UAVs again
            metalContext.UnbindCS<Metal::UnorderedAccessView>(0, outputBinCount);
            metalContext.UnbindCS<Metal::ShaderResourceView>(0, 2);

            res._isPrepared = true;

        CATCH_ASSETS_END(parserContext)

        if (begunQuery)
            metalContext.GetUnderlying()->End(begunQuery);

            // (reset the camera transform if it's changed)
        LightingParser_SetGlobalTransform(metalContext, parserContext, oldCamera);

        metalContext.UnbindSO();
        Metal::GeometryShader::SetDefaultStreamOutputInitializers(oldSO);
        // oldTargets.ResetToOldTargets(context);
    }

    static unsigned GetSOPrimitives(Metal::DeviceContext* context, ID3D::Query* query)
    {
        auto querySize = query->GetDataSize();
        uint8 soStatsBuffer[256];
        assert(querySize <= sizeof(soStatsBuffer));
        context->GetUnderlying()->GetData(query, soStatsBuffer, std::min(querySize, (unsigned)sizeof(soStatsBuffer)), 0);
        return (unsigned)((D3D11_QUERY_DATA_SO_STATISTICS*)soStatsBuffer)->NumPrimitivesWritten;
    }

    bool VegetationSpawn_DrawInstances(
        Metal::DeviceContext& context,
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
        if (!res._isPrepared || instanceId > res._instanceBufferSRVs.size())
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
            //      Right now, we can just use the "D3D11_QUERY_SO_STATISTICS" query. But this causes a CPU/GPU
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

        unsigned instanceCount = 0;
        if (primitiveCountMethod == FromQuery)
            instanceCount = GetSOPrimitives(&context, res._streamOutputCountsQuery.get());
        res._indirectDrawBuffer.WriteParams(context, indexCount, startIndexLocation, baseVertexLocation, instanceCount);

            // note --  we may be able to use the query to skip the compute shader step
            //          in cases where there is zero vegetation generated. Possibly a GPU
            //          predicate can help avoid a CPU sync when doing that.

        if (primitiveCountMethod == FromUAV)
            res._indirectDrawBuffer.CopyInstanceCount(context, res._instanceBufferUAVs[instanceId]);

            // bind the instancing buffer as an input vertex buffer
            //  This "instancing buffer" is the output from our separation compute shader
        // unsigned stride = 4*4, offset = 0;
        // auto* buffer = res._instanceBuffers[instanceId].get();
        // const unsigned slotForVertexInput = 3;
        // context->GetUnderlying()->IASetVertexBuffers(slotForVertexInput, 1, &buffer, &stride, &offset);
        context.BindVS(MakeResourceList(15, res._instanceBufferSRVs[instanceId]));

            // finally -- draw
        res._indirectDrawBuffer.Draw(context);

        return true;
    }

    class VegetationSpawnManager::Pimpl
    {
    public:
        std::shared_ptr<RenderCore::Assets::ModelCache>     _modelCache;
        std::shared_ptr<VegetationSpawnPlugin>              _parserPlugin;
        std::unique_ptr<VegetationSpawnResources>           _resources;
        VegetationSpawnConfig _cfg;

        using DepVal = std::shared_ptr<::Assets::DependencyValidation>;
        std::vector<RenderCore::Assets::DelayedDrawCallSet> _drawCallSets;
        std::vector<DepVal> _drawCallSetDepVals;

        uint32 _modelCacheReloadId;

        void FillInDrawCallSets();
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    class VegetationSpawnPlugin : public ILightingParserPlugin
    {
    public:
        virtual void OnPreScenePrepare(
            RenderCore::IThreadContext&, LightingParserContext&, PreparedScene&) const;
        virtual void OnLightingResolvePrepare(
            Metal::DeviceContext& context, LightingParserContext& parserContext,
            LightingResolveContext& resolveContext) const;
        virtual void OnPostSceneRender(
            Metal::DeviceContext& context, LightingParserContext& parserContext, 
            const SceneParseSettings& parseSettings, unsigned techniqueIndex) const;
        virtual void InitBasicLightEnvironment(
            Metal::DeviceContext&, LightingParserContext&, ShaderLightDesc::BasicEnvironment& env) const;

        VegetationSpawnPlugin(VegetationSpawnManager::Pimpl& pimpl);
        ~VegetationSpawnPlugin();

    protected:
        VegetationSpawnManager::Pimpl* _pimpl; // (must be unprotected to avoid cyclic dependency)
    };

    void VegetationSpawnPlugin::OnPreScenePrepare(
        RenderCore::IThreadContext& context, LightingParserContext& parserContext, 
        PreparedScene& preparedScene) const
    {
        if (_pimpl->_cfg._objectTypes.empty()) return;

        auto metalContext = RenderCore::Metal::DeviceContext::Get(context);
        Metal::GPUProfiler::DebugAnnotation anno(*metalContext, L"VegetationSpawn");
        VegetationSpawn_Prepare(context, *metalContext, parserContext, preparedScene, _pimpl->_cfg, *_pimpl->_resources.get()); 
    }

    void VegetationSpawnPlugin::OnLightingResolvePrepare(
        Metal::DeviceContext& context, LightingParserContext& parserContext,
        LightingResolveContext& resolveContext) const {}

    void VegetationSpawnPlugin::OnPostSceneRender(
        Metal::DeviceContext& context, LightingParserContext& parserContext, 
        const SceneParseSettings& parseSettings, unsigned techniqueIndex) const {}

    void VegetationSpawnPlugin::InitBasicLightEnvironment(
        Metal::DeviceContext&, LightingParserContext&, ShaderLightDesc::BasicEnvironment& env) const {}

    VegetationSpawnPlugin::VegetationSpawnPlugin(VegetationSpawnManager::Pimpl& pimpl)
    : _pimpl(&pimpl) {}
    VegetationSpawnPlugin::~VegetationSpawnPlugin() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    using DelayedDrawCallSet = RenderCore::Assets::DelayedDrawCallSet;
    using ModelRenderer = RenderCore::Assets::ModelRenderer;

    void VegetationSpawnManager::Pimpl::FillInDrawCallSets()
    {
        auto& cache = *_modelCache;
        auto& sharedStates = cache.GetSharedStateSet();

        // if we got a reload event in the model cache, we need to reset and start again
        if (cache.GetReloadId() != _modelCacheReloadId) {
            _drawCallSets.clear();
            _drawCallSetDepVals.clear();
            _modelCacheReloadId = cache.GetReloadId();
        }

        if (_drawCallSets.size() != _cfg._objectTypes.size()) {
            _drawCallSets.resize(
                _cfg._objectTypes.size(),
                DelayedDrawCallSet(typeid(ModelRenderer).hash_code()));
            _drawCallSetDepVals.resize(_cfg._objectTypes.size());
        }

            // We need to create a separate DelayedDrawCallSet for each
            // model. They must be separate because of the way we assign transforms
            // from the spawning shader.
        for (unsigned b=0; b<unsigned(_cfg._objectTypes.size()); ++b) {
            if (_drawCallSetDepVals[b] && _drawCallSetDepVals[b]->GetValidationIndex()==0) continue;

            TRY {
                const auto& bkt = _cfg._objectTypes[b];
                auto model = cache.GetModel(bkt._modelName.c_str(), bkt._materialName.c_str());

                model._renderer->Prepare(
                    _drawCallSets[b], sharedStates, Identity<Float4x4>(), 
                    RenderCore::Assets::MeshToModel(*model._model));
                ModelRenderer::Sort(_drawCallSets[b]);
                _drawCallSetDepVals[b] = model._renderer->GetDependencyValidation();
            } CATCH(const ::Assets::Exceptions::AssetException&) {}
            CATCH_END
        }

        // if we got a reload somewhere in the middle, we need to reset again
        if (cache.GetReloadId() != _modelCacheReloadId) {
            _drawCallSets.clear();
            _drawCallSetDepVals.clear();
        }
    }

    void VegetationSpawnManager::Render(
        Metal::DeviceContext& context, LightingParserContext& parserContext,
        unsigned techniqueIndex, RenderCore::Assets::DelayStep delayStep)
    {
        if (_pimpl->_cfg._objectTypes.empty()) return;

        _pimpl->FillInDrawCallSets();

        auto& sharedStates = _pimpl->_modelCache->GetSharedStateSet();
        auto captureMarker = sharedStates.CaptureState(
            context, parserContext.GetStateSetResolver(), parserContext.GetStateSetEnvironment());
        auto& state = parserContext.GetTechniqueContext()._runtimeState;
        state.SetParameter(u("SPAWNED_INSTANCE"), 1);
        state.SetParameter(u("GEO_INSTANCE_ALIGN_UP"), unsigned(_pimpl->_resources->_alignToTerrainUp));
        auto cleanup = MakeAutoCleanup(
            [&state]() { state.SetParameter(u("SPAWNED_INSTANCE"), 0); });

        auto& resources = *_pimpl->_resources;
        for (unsigned b=0; b<unsigned(_pimpl->_drawCallSets.size()); ++b)
            ModelRenderer::RenderPrepared(
                RenderCore::Assets::ModelRendererContext(context, parserContext, techniqueIndex),
                sharedStates, _pimpl->_drawCallSets[b], delayStep,
                [&context, b, &resources](ModelRenderer::DrawCallEvent evnt)
                {
                    VegetationSpawn_DrawInstances(
                        context, resources, b, 
                        evnt._indexCount, evnt._firstIndex, evnt._firstVertex);
                });
    }

    bool VegetationSpawnManager::HasContent(RenderCore::Assets::DelayStep delayStep) const
    {
        if (_pimpl->_cfg._objectTypes.empty()) return false;

        bool hasContent = false;
        for (unsigned b=0; b<unsigned(_pimpl->_drawCallSets.size()); ++b)
            hasContent |= !_pimpl->_drawCallSets[b].IsEmpty(delayStep);
        return hasContent;
    }

    void VegetationSpawnManager::Load(const VegetationSpawnConfig& cfg)
    {
        _pimpl->_cfg = cfg;
        _pimpl->_drawCallSets.clear();
        _pimpl->_drawCallSetDepVals.clear();
        _pimpl->_resources = std::make_unique<VegetationSpawnResources>(
            VegetationSpawnResources::Desc(
                (unsigned)cfg._objectTypes.size(), 
                cfg._alignToTerrainUp));
    }

    void VegetationSpawnManager::Reset()
    {
        _pimpl->_cfg = VegetationSpawnConfig();
        _pimpl->_drawCallSets.clear();
        _pimpl->_drawCallSetDepVals.clear();
        _pimpl->_resources.reset();
    }

    const VegetationSpawnConfig& VegetationSpawnManager::GetConfig() const
    {
        return _pimpl->_cfg;
    }

    std::shared_ptr<ILightingParserPlugin> VegetationSpawnManager::GetParserPlugin()
    {
        return _pimpl->_parserPlugin;
    }

    VegetationSpawnManager::VegetationSpawnManager(std::shared_ptr<RenderCore::Assets::ModelCache> modelCache)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_modelCache = std::move(modelCache);
        _pimpl->_modelCacheReloadId = _pimpl->_modelCache->GetReloadId();
        _pimpl->_parserPlugin = std::make_shared<VegetationSpawnPlugin>(*_pimpl);
    }

    VegetationSpawnManager::~VegetationSpawnManager() {}

    

}


namespace SceneEngine
{
    void IndirectDrawBuffer::Draw(Metal::DeviceContext& metalContext)
    {
        metalContext.GetUnderlying()->DrawIndexedInstancedIndirect(_indirectArgsBuffer.GetUnderlying(), 0);
    }

    bool IndirectDrawBuffer::WriteParams(
        Metal::DeviceContext& metalContext, 
        unsigned indexCountPerinstance, unsigned startIndexLocation, 
        unsigned baseVertexLocation, unsigned instanceCount)
    {
        D3D11_MAPPED_SUBRESOURCE mappedSub;
        auto hresult = metalContext.GetUnderlying()->Map(_indirectArgsBuffer.GetUnderlying(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSub);
        if (!SUCCEEDED(hresult))
            return false;

        auto& args = *(DrawIndexedInstancedIndirectArgs*)mappedSub.pData;
        args.IndexCountPerInstance = indexCountPerinstance;
        args.StartIndexLocation = startIndexLocation;
        args.BaseVertexLocation = baseVertexLocation;
        args.StartInstanceLocation = 0;
        args.InstanceCount = instanceCount;
        metalContext.GetUnderlying()->Unmap(_indirectArgsBuffer.GetUnderlying(), 0);
        return true;
    }

    void IndirectDrawBuffer::CopyInstanceCount(Metal::DeviceContext& metalContext, Metal::UnorderedAccessView& src)
    {
            // copy the "structure count" from the UAV into the indirect args buffer
        metalContext.GetUnderlying()->CopyStructureCount(
            _indirectArgsBuffer.GetUnderlying(), 
            unsigned(&((DrawIndexedInstancedIndirectArgs*)nullptr)->InstanceCount), 
            src.GetUnderlying());
    }

    IndirectDrawBuffer::IndirectDrawBuffer()
    {
        using namespace BufferUploads;
        auto indirectArgsBufferDesc = CreateDesc(
            BindFlag::DrawIndirectArgs | BindFlag::VertexBuffer,
            CPUAccess::WriteDynamic, GPUAccess::Read | GPUAccess::Write,
            LinearBufferDesc::Create(4*5, 4*5),
            "IndirectArgsBuffer");

        auto& uploads = GetBufferUploads();
        auto indirectArgsRes = uploads.Transaction_Immediate(indirectArgsBufferDesc);
        _indirectArgsBuffer = indirectArgsRes->AdoptUnderlying();
    }

    IndirectDrawBuffer::~IndirectDrawBuffer()
    {
    }
}
