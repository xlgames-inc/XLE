// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Tonemap.h"
#include "SceneEngineUtils.h"
#include "SceneParser.h"
#include "LightDesc.h"
#include "GestaltResource.h"
#include "MetalStubs.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/RenderPass.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Techniques/DeferredShaderResource.h"
#include "../RenderCore/Format.h"
#include "../RenderCore/Metal/TextureView.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/ObjectFactory.h"
#include "../RenderCore/RenderUtils.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/DataPacket.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../Assets/Assets.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/ResourceBox.h"
#include "../Utility/BitUtils.h"
#include "../Utility/ParameterBox.h"
#include "../Utility/StringFormat.h"
#include "../Utility/Meta/ClassAccessorsImpl.h"

// #include "../RenderCore/Metal/DeviceContextImpl.h"

#if GFXAPI_ACTIVE == GFXAPI_DX11	// platformtemp
    #include "../RenderCore/DX11/Metal/DX11Utils.h"
#endif

// #pragma warning(disable:4127)       // conditional expression is constant

namespace SceneEngine
{
    using namespace RenderCore;

    struct ColorGradingShaderConstants
    {
        Float4 _colorGradingMatrix[3];
	    Float4 _selectiveColor0;
	    Float4 _selectiveColor1;
        Float3 _filterColor;
        float _filterColorDensity;
	    float _levelsMinInput;			// ColorGradingParams0.x;
	    float _levelsGammaInput;		// 1.0 / ColorGradingParams0.y;
	    float _levelsMaxInput;			// ColorGradingParams0.z;
	    float _levelsMinOutput;			// ColorGradingParams0.w;
	    float _levelsMaxOutput;			// ColorGradingParams1.x;
        int _doLevelsAdustment;
        int _doSelectiveColor;
        int _doFilterColor;
    };
    ColorGradingShaderConstants BuildColorGradingShaderConstants(const ColorGradingSettings& settings);

    struct ToneMapSettingsConstants
    {
        Float3  _bloomScale;
        float   _bloomThreshold;
        float   _bloomRampingFactor;
        float   _bloomDesaturationFactor;
        float   _sceneKey;
	    float   _luminanceMin;
	    float   _luminanceMax;
	    float   _whitepoint;
        float   _dummy[2];
    };
    RenderCore::SharedPkt AsConstants(const ToneMapSettings& settings);

    class BloomStepBuffer
    {
    public:
        GestaltTypes::UAVSRV    _bloomBuffer;
        unsigned                _width, _height;

        BloomStepBuffer(unsigned width, unsigned height, Format format);
        BloomStepBuffer();
        ~BloomStepBuffer();

        BloomStepBuffer(BloomStepBuffer&& moveFrom) never_throws
        : _bloomBuffer(std::move(moveFrom._bloomBuffer))
        , _width(moveFrom._width), _height(moveFrom._height)
        {}
        const BloomStepBuffer& operator=(BloomStepBuffer&& moveFrom) never_throws
        {
            _bloomBuffer = std::move(moveFrom._bloomBuffer);
            _width = moveFrom._width; _height = moveFrom._height;
            return *this;
        }
    };

    BloomStepBuffer::BloomStepBuffer(unsigned width, unsigned height, Format format)
    : _width(width), _height(height)
    , _bloomBuffer(
        GestaltTypes::UAVSRV(
            BufferUploads::TextureDesc::Plain2D(width, height, format),
            "BloomLumin"))
    {}

    BloomStepBuffer::BloomStepBuffer()  {}
    BloomStepBuffer::~BloomStepBuffer() {}

    /// <summary>Tracks the "layout" state of a set of resources</summary>
    /// When using APIs that allow a single resource to be bound as both input and
    /// output at the same time (such as Vulkan, DirectX12 and OpenGL), we must insert
    /// barrier instructions to ensure that reading does not begin until writing has 
    /// finished.
    ///
    /// This provides a thin layer that will track the state of certain resources. 
    /// When a change to the "layout" of a resource is required, this will add 
    /// barriers as appropriate.
    ///
    /// Note that we could get similar behaviour in DirectX11 by just managing
    /// Binding and Unbinding in this object.
    class ResourceBarriers
    {
    public:
        class Resource
        {
        public:
            RenderCore::Resource*   _res;
            Metal::ImageLayout      _initialLayout;
            Metal::ImageLayout      _finalLayout;
        };

        class LayoutChange
        {
        public:
            unsigned            _resourceIndex;
            Metal::ImageLayout  _newLayout;
        };

        void        SetBarrier(IteratorRange<const LayoutChange*> changes);
        unsigned    AddResources(IteratorRange<const Resource*> resources);

        ResourceBarriers(Metal::DeviceContext& context, IteratorRange<const Resource*> resources);
        ~ResourceBarriers();
    private:
        std::vector<std::pair<Resource, Metal::ImageLayout>> _resources;
        Metal::DeviceContext* _attachedContext;
    };

    void        ResourceBarriers::SetBarrier(IteratorRange<const LayoutChange*> changes)
    {
        Metal::LayoutTransition transitions[16];
        unsigned transCount = 0;
        assert(changes.size() <= dimof(transitions));

        for (const auto&c:changes) {
            assert(c._resourceIndex < _resources.size());
            auto&r = _resources[c._resourceIndex];
            if (r.second != c._newLayout) {
                assert(transCount < dimof(transitions));
                transitions[transCount++] = Metal::LayoutTransition { &Metal::AsResource(*r.first._res), r.second, c._newLayout };
                r.second = c._newLayout;
            }
        }

        if (transCount != 0)
            Metal::SetImageLayouts(
                *_attachedContext, MakeIteratorRange(transitions, &transitions[transCount]));
    }

    unsigned    ResourceBarriers::AddResources(IteratorRange<const Resource*> resources)
    {
        unsigned result = (unsigned)_resources.size();
        _resources.reserve(_resources.size() + resources.size());
        for (const auto&r:resources)
            _resources.push_back(std::make_pair(r, r._initialLayout));
        return result;
    }

    ResourceBarriers::ResourceBarriers(Metal::DeviceContext& context, IteratorRange<const Resource*> resources)
    : _attachedContext(&context)
    {
        _resources.reserve(resources.size());
        for (const auto&r:resources)
            _resources.push_back(std::make_pair(r, r._initialLayout));
    }

    ResourceBarriers::~ResourceBarriers()
    {
        // Set all resources to their final layout (as long as it's not UNDEFINED)
        Metal::LayoutTransition transitions[16];
        unsigned transCount = 0;
        for (const auto&r:_resources) 
            if (r.first._finalLayout != Metal::ImageLayout::Undefined && r.second != r.first._finalLayout) {
                assert(transCount < dimof(transitions));
                transitions[transCount++] = { &Metal::AsResource(*r.first._res), r.second, r.first._finalLayout };
            }
        if (transCount != 0)
            Metal::SetImageLayouts(*_attachedContext, MakeIteratorRange(transitions, &transitions[transCount]));
    }

    class ToneMappingResources
    {
    public:
        using NativeFormat = Format;
        class Desc
        {
        public:
            Desc(
                unsigned width, unsigned height, 
                NativeFormat bloomBufferFormat, 
                unsigned sampleCount, bool useMSAASamplers)
            {
                std::fill((byte*)this, (byte*)PtrAdd(this, sizeof(*this)), '\0');
                _width = width;
                _height = height;
                _bloomBufferFormat = bloomBufferFormat;
                _sampleCount = sampleCount;
                _useMSAASamplers = useMSAASamplers;
            }

            unsigned        _width, _height, _sampleCount;
            bool            _useMSAASamplers;
            NativeFormat    _bloomBufferFormat;
        };

        using UAV = Metal::UnorderedAccessView;
        using SRV = Metal::ShaderResourceView;
        using ResLocator = intrusive_ptr<BufferUploads::ResourceLocator>;

        std::vector<GestaltTypes::UAVSRV>   _luminanceBuffers;
        GestaltTypes::UAVSRV                _propertiesBuffer;
        std::vector<BloomStepBuffer>        _bloomBuffers;
        BloomStepBuffer                     _bloomTempBuffer;

        const CompiledShaderByteCode*   _sampleInitialLuminanceByteCode;
        Metal::ComputeShader            _sampleInitialLuminance;
        const Metal::ComputeShader*		_luminanceStepDown;
        const Metal::ComputeShader*		_updateOverallLuminance;
        const Metal::ComputeShader*		_updateOverallLuminanceNoAdapt;
        const Metal::ComputeShader*		_brightPassStepDown;

        Metal::BoundUniforms        _boundUniforms;

        unsigned    _firstStepWidth;
        unsigned    _firstStepHeight;

        const ::Assets::DepValPtr&  GetDependencyValidation() const   { return _validationCallback; }

        ToneMappingResources(const Desc& desc);
        ~ToneMappingResources();

    private:
        ::Assets::DepValPtr _validationCallback;
    };

    ToneMappingResources::ToneMappingResources(const Desc& desc)
    {
            //
            //      We do the luminance calculation in a number of steps.
            //          1. go from full resolution -> first step resolution
            //              (first step must be power of two)
            //          2. keep dropping down one power of two
            //          3. finish when we get to a 2x2 texture
            //
            //      Then we execute a small single thread compute shader
            //      operation to blend the final average luminance value 
            //      with previous frame's results.
            //
            //      So we need to build temporary buffers at each power of
            //      two step.
            //
        signed log2Width = IntegerLog2(desc._width);
        signed log2Height = IntegerLog2(desc._height);
        signed firstStep = std::max(std::max(5, log2Width-1), log2Height-1)-1;   // Our texture should be at least 16x16 (simplifies compute shader work)
        signed heightDifference = log2Height - log2Width;
        heightDifference = std::min(firstStep-4, heightDifference);

        assert((1<<firstStep) >= 16);
        assert((1<<(firstStep+heightDifference)) >= 16);

            //  (we could also jsut use 2 buffers, and just use increasingly small corner
            //  areas of the texture... But maybe the textures after the first 2 are trivally small)
        _luminanceBuffers.reserve(firstStep);
        for (signed s=firstStep; s>XlAbs(heightDifference); --s)
            _luminanceBuffers.emplace_back(GestaltTypes::UAVSRV(
                BufferUploads::TextureDesc::Plain2D(1<<s, 1<<(s+heightDifference), Format::R16_FLOAT),
                "Luminance"));

            //  We need many bloom buffers as well... start at the same size as the biggest luminance
            //  buffer and work down to 32x32
        _bloomBuffers.reserve(std::max(1,firstStep-(4+XlAbs(heightDifference))));
        _bloomBuffers.push_back(BloomStepBuffer(1<<firstStep, 1<<(firstStep+heightDifference), desc._bloomBufferFormat));
        _bloomTempBuffer = BloomStepBuffer(1<<firstStep, 1<<(firstStep+heightDifference), desc._bloomBufferFormat);
        for (signed s=firstStep-1; s>(4+XlAbs(heightDifference)); --s) {
            _bloomBuffers.push_back(BloomStepBuffer(1<<s, 1<<(s+heightDifference), desc._bloomBufferFormat));
        }
    
        float initialData[] = { 1.f, 1.f };
        _propertiesBuffer = GestaltTypes::UAVSRV(
            BufferUploads::LinearBufferDesc::Create(8, 8),
            "LuminanceProperties", 
            BufferUploads::CreateBasicPacket(sizeof(initialData), &initialData).get(),
            BufferUploads::BindFlag::StructuredBuffer);

        StringMeld<256> shaderDefines; 
        shaderDefines << "MSAA_SAMPLES=" << desc._sampleCount;
        if (desc._useMSAASamplers) shaderDefines << ";MSAA_SAMPLERS=1";

        _sampleInitialLuminanceByteCode    = &::Assets::GetAssetDep<CompiledShaderByteCode>("xleres/postprocess/hdrluminance.csh:SampleInitialLuminance:cs_*", shaderDefines.get());
        _sampleInitialLuminance     = Metal::ComputeShader(Metal::GetObjectFactory(), *_sampleInitialLuminanceByteCode);
        _luminanceStepDown          = &::Assets::GetAssetDep<Metal::ComputeShader>("xleres/postprocess/hdrluminance.csh:LuminanceStepDown:cs_*");
        _updateOverallLuminance     = &::Assets::GetAssetDep<Metal::ComputeShader>("xleres/postprocess/hdrluminance.csh:UpdateOverallLuminance:cs_*");
        _brightPassStepDown         = &::Assets::GetAssetDep<Metal::ComputeShader>("xleres/postprocess/hdrluminance.csh:BrightPassStepDown:cs_*");

        _updateOverallLuminanceNoAdapt = &::Assets::GetAssetDep<Metal::ComputeShader>("xleres/postprocess/hdrluminance.csh:UpdateOverallLuminance:cs_*", "IMMEDIATE_ADAPT=1");

		UniformsStreamInterface usi;
		usi.BindConstantBuffer(0, {Hash64("ToneMapSettings")});
		usi.BindConstantBuffer(1, {Hash64("LuminanceConstants")});
		_boundUniforms = Metal::BoundUniforms(
			_sampleInitialLuminance,
			Metal::PipelineLayoutConfig{},
			UniformsStreamInterface{},
			usi);

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(_validationCallback, _sampleInitialLuminanceByteCode->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_validationCallback, _luminanceStepDown->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_validationCallback, _updateOverallLuminance->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_validationCallback, _brightPassStepDown->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_validationCallback, _updateOverallLuminanceNoAdapt->GetDependencyValidation());

        _firstStepWidth         = 1<<firstStep;
        _firstStepHeight        = 1<<(firstStep+heightDifference);
    }

    ToneMappingResources::~ToneMappingResources()
    {}

    static void DrawDebugging(Metal::DeviceContext& context, ToneMappingResources& resources);
    

        //////////////////////////////////////////////////////////////////////////////
            //      T O N E   M A P P I N G   I N P U T S                       //
        //////////////////////////////////////////////////////////////////////////////

    static bool TryCalculateInputs(
        RenderCore::IThreadContext& threadContext,
        Techniques::ParsingContext& parserContext,
        ToneMappingResources& resources,
        const ToneMapSettings& settings,
        const Metal::ShaderResourceView& sourceTexture,
        bool doAdapt)
    {
		auto& context = *Metal::DeviceContext::Get(threadContext);

            //
            //      First step in tonemapping is always to calculate the overall
            //      scene brightness. We do this in a series of step-down operations,
            //      each time reducing the image by a power of two.
            //
            //      We might use a log-average method for this, but we have to
            //      be careful about floating point accuracy.
            //
        TRY {
            auto sourceDesc = Metal::ExtractDesc(sourceTexture);
            UInt2 sourceDims(sourceDesc._textureDesc._width, sourceDesc._textureDesc._height);

            static unsigned frameIndex = 0; ++frameIndex;
            struct LuminanceConstants
            {
				unsigned    _frameIndex;
				unsigned    _totalSampleCount;
                float		_elapsedTime;
                unsigned    _buffer;
                UInt2		_inputTextureDims;
                Float2		_initialSampleSizeRatio;
            } luminanceConstants = 
            {
                frameIndex, resources._firstStepWidth*resources._firstStepHeight, 1.0f/60.f, 0u,
                sourceDims,
                Float2(float(sourceDims[0]) / float(resources._firstStepWidth), float(sourceDims[1]) / float(resources._firstStepHeight))
            };

            ConstantBufferView cbs2[] = { AsConstants(settings), MakeSharedPkt(luminanceConstants) };
			resources._boundUniforms.Apply(context, 1, UniformsStream{MakeIteratorRange(cbs2)});

            assert(!resources._luminanceBuffers.empty());

                //
                //      Initial sample from the source texture first,
                //      then make a number of down-step operations.
                // 
                //      The first step is not MSAA-aware. Only the 0th
                //      sample is considered.
                //
                //  Note -- in Vulkan we could just bind all of the textures at once
                //          (because we can bind textures as input and output at the same time)
                //          We would still need some kind of memory barrier, but it would simplify
                //          the descriptor work!
                //
            const unsigned bloomTempBufferId = 1;
            const unsigned lumianceBuffersId = 1;
            const unsigned bloomBuffersId = 1 + (unsigned)resources._luminanceBuffers.size();
            const auto UAVLayout = Metal::ImageLayout::General;
            const auto SRVLayout = Metal::ImageLayout::ShaderReadOnlyOptimal;

            // currently making no assumptions about starting or ending layouts
			SceneEngine::ResourceBarriers::Resource resList[] = {{ resources._bloomTempBuffer._bloomBuffer.Resource().get(), Metal::ImageLayout::Undefined, Metal::ImageLayout::Undefined }};
            ResourceBarriers barriers(context, MakeIteratorRange(resList));
            for (unsigned c=0; c<(unsigned)resources._luminanceBuffers.size(); ++c)
                barriers.AddResources(MakeIteratorRange<ResourceBarriers::Resource>({{resources._luminanceBuffers[c].Resource().get(), Metal::ImageLayout::Undefined, Metal::ImageLayout::Undefined }}));
            for (unsigned c=0; c<(unsigned)resources._bloomBuffers.size(); ++c)
                barriers.AddResources(MakeIteratorRange<ResourceBarriers::Resource>({{resources._bloomBuffers[c]._bloomBuffer.Resource().get(), Metal::ImageLayout::Undefined, Metal::ImageLayout::Undefined }}));

            barriers.SetBarrier(MakeIteratorRange<ResourceBarriers::LayoutChange>({{lumianceBuffersId+0, UAVLayout}, {bloomBuffersId+0, UAVLayout}}));
            context.Bind(resources._sampleInitialLuminance);
            context.GetNumericUniforms(ShaderStage::Compute).Bind(MakeResourceList(sourceTexture));
            context.GetNumericUniforms(ShaderStage::Compute).Bind(MakeResourceList(resources._luminanceBuffers[0].UAV(), resources._bloomBuffers[0]._bloomBuffer.UAV(), resources._propertiesBuffer.UAV()));
            context.Dispatch(resources._firstStepWidth/16, resources._firstStepHeight/16);
            MetalStubs::UnbindCS<Metal::UnorderedAccessView>(context, 0, 2);

            context.Bind(*resources._luminanceStepDown);
            for (unsigned c=1; c<(unsigned)resources._luminanceBuffers.size(); ++c) {
                barriers.SetBarrier(MakeIteratorRange<ResourceBarriers::LayoutChange>({{lumianceBuffersId+c-1, SRVLayout}, {lumianceBuffersId+0, UAVLayout}}));
                context.GetNumericUniforms(ShaderStage::Compute).Bind(MakeResourceList(resources._luminanceBuffers[c-1].SRV()));
                context.GetNumericUniforms(ShaderStage::Compute).Bind(MakeResourceList(resources._luminanceBuffers[c].UAV()));
                context.Dispatch(std::max(1u, (resources._firstStepWidth>>c)/16), std::max(1u, (resources._firstStepHeight>>c)/16));
                MetalStubs::UnbindCS<Metal::UnorderedAccessView>(context, 0, 1);
            }

            context.Bind(*resources._brightPassStepDown);
            for (unsigned c=1; c<(unsigned)resources._bloomBuffers.size(); ++c) {
                barriers.SetBarrier(MakeIteratorRange<ResourceBarriers::LayoutChange>({{bloomBuffersId+c-1, SRVLayout}, {bloomBuffersId+c, UAVLayout}}));

                context.GetNumericUniforms(ShaderStage::Compute).Bind(MakeResourceList(resources._bloomBuffers[c-1]._bloomBuffer.SRV()));
                context.GetNumericUniforms(ShaderStage::Compute).Bind(MakeResourceList(1, resources._bloomBuffers[c]._bloomBuffer.UAV()));
                context.Dispatch(std::max(1u, (resources._firstStepWidth>>c)/16), std::max(1u, (resources._firstStepHeight>>c)/16));
                MetalStubs::UnbindCS<Metal::UnorderedAccessView>(context, 1, 1);
            }

                //
                //      After we've down all of the downsample steps, we should have
                //      a 2x2 texture -- these are our final samples.
                //      Get the final luminance values, and balance it with the previous
                //      frame's values to produce the new value for this frame...
                //

            barriers.SetBarrier(MakeIteratorRange<ResourceBarriers::LayoutChange>({{lumianceBuffersId+(unsigned)resources._luminanceBuffers.size()-1, SRVLayout}}));
            context.GetNumericUniforms(ShaderStage::Compute).Bind(MakeResourceList(resources._luminanceBuffers[resources._luminanceBuffers.size()-1].SRV()));
            if (doAdapt)    context.Bind(*resources._updateOverallLuminance);
            else            context.Bind(*resources._updateOverallLuminanceNoAdapt);
            context.Dispatch(1);
            MetalStubs::UnbindCS<Metal::UnorderedAccessView>(context, 0, 3);

                //
                //      We need to do a series of Gaussian blurs over the bloom buffers
                //      We should have a number of bloom buffers, each at different 
                //      resolutions...
                //      We blur each one, and add it to the next higher resolution one
                //

            if (settings._flags & ToneMapSettings::Flags::EnableBloom && Tweakable("DoBloom", true)) {
                auto filteringWeights = RenderCore::MakeSharedPktSize(sizeof(float) * 12);
                XlSetMemory(filteringWeights.get(), 0, sizeof(filteringWeights));
                BuildGaussianFilteringWeights((float*)filteringWeights.get(), settings._bloomBlurStdDev, 11);

                auto& horizBlur = ::Assets::GetAssetDep<Metal::ComputeShader>(
                    "xleres/Effects/SeparableFilter.csh:HorizontalBlur11NoScale:cs_*",
                    "USE_CLAMPING_WINDOW=1");
                auto& vertBlur = ::Assets::GetAssetDep<Metal::ComputeShader>(
                    "xleres/Effects/SeparableFilter.csh:VerticalBlur11NoScale:cs_*",
                    "USE_CLAMPING_WINDOW=1");
                auto& copyShader = ::Assets::GetAssetDep<Metal::ComputeShader>(
                    "xleres/basic.csh:ResampleBilinear:cs_*");

                // We're doing this all with compute shaders to try to simplify it.
                // This avoids having to play around with viewports and blend modes
                // Nothing here fundamentally requires the graphics pipeline; though it should parallelize
                // in a way that is typical for pixel shaders...?

				UniformsStreamInterface usi;
				usi.BindConstantBuffer(0, {Hash64("Constants")});
				usi.BindConstantBuffer(1, {Hash64("ClampingWindow")});
                Metal::BoundUniforms boundUniforms(
					horizBlur,
					Metal::PipelineLayoutConfig{},
					UniformsStreamInterface{},
					usi);

                auto clampingWindowCB = MakeMetalCB(nullptr, sizeof(Float4));
                ConstantBufferView cbvs[] = { filteringWeights, &clampingWindowCB };
				boundUniforms.Apply(context, 1, UniformsStream{MakeIteratorRange(cbvs)});

                unsigned bloomBufferIndex = (unsigned)resources._bloomBuffers.size()-1;
                for (auto i=resources._bloomBuffers.crbegin(); ;) {

                    assert(i->_width >= 32 && i->_height >= 32);

                    float clampingWindow[] = { 0.f, 0.f, (float)i->_width - 1.f, (float)i->_height - 1.f };
                    clampingWindowCB.Update(context, &clampingWindow, sizeof(clampingWindow));

                    barriers.SetBarrier(MakeIteratorRange<ResourceBarriers::LayoutChange>({{bloomBuffersId+bloomBufferIndex, SRVLayout}, {bloomTempBufferId, UAVLayout}}));
                    context.GetNumericUniforms(ShaderStage::Compute).Bind(MakeResourceList(resources._bloomTempBuffer._bloomBuffer.UAV()));
                    context.GetNumericUniforms(ShaderStage::Compute).Bind(MakeResourceList(i->_bloomBuffer.SRV()));
                    context.Bind(horizBlur);
                    context.Dispatch(i->_width/16, i->_height/16);
                    MetalStubs::UnbindCS<Metal::ShaderResourceView>(context, 0, 1);

                    barriers.SetBarrier(MakeIteratorRange<ResourceBarriers::LayoutChange>({{bloomBuffersId+bloomBufferIndex, UAVLayout}, {bloomTempBufferId, SRVLayout}}));
                    context.GetNumericUniforms(ShaderStage::Compute).Bind(MakeResourceList(i->_bloomBuffer.UAV()));
                    context.GetNumericUniforms(ShaderStage::Compute).Bind(MakeResourceList(resources._bloomTempBuffer._bloomBuffer.SRV()));
                    context.Bind(vertBlur);
                    context.Dispatch(i->_width/16, i->_height/16);
                    MetalStubs::UnbindPS<Metal::ShaderResourceView>(context, 0, 1);

                    auto oldi = i;
                    ++i; --bloomBufferIndex;
                    if (i==resources._bloomBuffers.crend())
                        break;

                    // blend into the next step
                    barriers.SetBarrier(MakeIteratorRange<ResourceBarriers::LayoutChange>({{bloomBuffersId+bloomBufferIndex, UAVLayout}, {bloomBuffersId+bloomBufferIndex+1, SRVLayout}}));
                    context.GetNumericUniforms(ShaderStage::Compute).Bind(MakeResourceList(i->_bloomBuffer.UAV()));
                    context.GetNumericUniforms(ShaderStage::Compute).Bind(MakeResourceList(oldi->_bloomBuffer.SRV()));
                    context.Bind(copyShader);
                    context.Dispatch(i->_width/8, i->_height/8);

                }

                MetalStubs::UnbindCS<Metal::UnorderedAccessView>(context, 0, 1);
				MetalStubs::UnbindCS<Metal::ShaderResourceView>(context, 0, 1);
            }

            // We will read from bloom buffer 0 later down the pipeline...
            barriers.SetBarrier(MakeIteratorRange<ResourceBarriers::LayoutChange>({{bloomBuffersId+0, SRVLayout}}));

            return true;
        }
        CATCH_ASSETS(parserContext)
        CATCH(...) {}
        CATCH_END

        return false;
    }

    static ToneMappingResources& GetResources(const Metal::ShaderResourceView& inputResource)
    {
            //  Before MSAA resolve, we must setup the tone mapping (first 
            //  step is to sample the luminance)
            //      We have a number of different options for the bloom buffer format.
            //      Generally, R8G8B8A8_UNORM doesn't have enough precision. With all
            //      of the blending and blurring, we tend to get banding.
            //      Try to use a higher precision 32 bit format instead.
        const unsigned bloomBufferFormatType = Tweakable("ToneMapBloomBufferFormat", 3);
        auto bloomBufferFormat = Format::R10G10B10A2_UNORM;
        if (bloomBufferFormatType==1) {
            bloomBufferFormat = Format::R8G8B8A8_UNORM;
        } else if (bloomBufferFormatType==2) {
            bloomBufferFormat = Format::R16G16B16A16_FLOAT;
        } else if (bloomBufferFormatType==3) {
            bloomBufferFormat = Format::R11G11B10_FLOAT;
        } else if (bloomBufferFormatType==4) {
            bloomBufferFormat = Format::R16_FLOAT;
        }
        auto desc = Metal::ExtractDesc(inputResource);
        auto sampleCount = desc._textureDesc._samples._sampleCount;
        return ConsoleRig::FindCachedBoxDep2<ToneMappingResources>(
            desc._textureDesc._width, desc._textureDesc._height, bloomBufferFormat, sampleCount, sampleCount>1);
    }

    LuminanceResult ToneMap_SampleLuminance(
        RenderCore::IThreadContext& context,
		RenderCore::Techniques::ParsingContext& parserContext,
        const ToneMapSettings& settings,
        const RenderCore::Metal::ShaderResourceView& inputResource,
        bool doAdapt)
    {
        if (Tweakable("DoToneMap", true)) {
            auto& toneMapRes = GetResources(inputResource);
            bool success = TryCalculateInputs(
                context, parserContext,
                toneMapRes, settings, inputResource, doAdapt);

            if (Tweakable("ToneMapDebugging", false)) {
                parserContext._pendingOverlays.push_back(
                    std::bind(&DrawDebugging, std::placeholders::_1, std::ref(toneMapRes)));
            }

            return LuminanceResult(toneMapRes._propertiesBuffer.SRV(), toneMapRes._bloomBuffers[0]._bloomBuffer.SRV(), success);
        }

        return LuminanceResult();
    }

    LuminanceResult::LuminanceResult() : _isGood(false) {}
    LuminanceResult::LuminanceResult(const SRV& propertiesBuffer, const SRV& bloomBuffer, bool isGood)
    : _propertiesBuffer(propertiesBuffer)
    , _bloomBuffer(bloomBuffer)
    , _isGood(isGood) {}

    LuminanceResult::~LuminanceResult() {}
    LuminanceResult::LuminanceResult(LuminanceResult&& moveFrom) never_throws
    : _propertiesBuffer(std::move(moveFrom._propertiesBuffer))
    , _bloomBuffer(std::move(moveFrom._bloomBuffer))
    , _isGood(moveFrom._isGood) {}
    LuminanceResult& LuminanceResult::operator=(LuminanceResult&& moveFrom) never_throws
    {
        _propertiesBuffer = std::move(moveFrom._propertiesBuffer);
        _bloomBuffer = std::move(moveFrom._bloomBuffer);
        _isGood = moveFrom._isGood;
        return *this;
    }

        //////////////////////////////////////////////////////////////////////////////
            //      T O N E   M A P P I N G   M A I N                           //
        //////////////////////////////////////////////////////////////////////////////

    class ToneMapShaderBox
    {
    public:
        class Desc
        {
        public:
            unsigned _operator;
            bool _enableBloom;
            bool _hardwareSRGBEnabled, _doColorGrading, _doLevelsAdjustment, _doSelectiveColour, _doFilterColour;
            Desc(unsigned opr, bool enableBloom, bool hardwareSRGBEnabled, bool doColorGrading, bool doLevelsAdjustments, bool doSelectiveColour, bool doFilterColour)
                : _operator(opr), _enableBloom(enableBloom), _hardwareSRGBEnabled(hardwareSRGBEnabled), _doColorGrading(doColorGrading)
                , _doLevelsAdjustment(doLevelsAdjustments), _doSelectiveColour(doSelectiveColour), _doFilterColour(doFilterColour) {}
        };

        const Metal::ShaderProgram* _shaderProgram;
        Metal::BoundUniforms _uniforms;

        ToneMapShaderBox(const Desc& descs);

        const ::Assets::DepValPtr&  GetDependencyValidation() const   { return _validationCallback; }
    private:
        ::Assets::DepValPtr _validationCallback;
    };

    ToneMapShaderBox::ToneMapShaderBox(const Desc& desc)
    {
        StringMeld<256> shaderDefines;
        shaderDefines 
            << "OPERATOR=" << desc._operator
            << ";ENABLE_BLOOM=" << desc._enableBloom
            << ";HARDWARE_SRGB_ENABLED=" << desc._hardwareSRGBEnabled
            << ";DO_COLOR_GRADING=" << desc._doColorGrading
            << ";MAT_LEVELS_ADJUSTMENT=" << desc._doLevelsAdjustment
            << ";MAT_SELECTIVE_COLOR=" << desc._doSelectiveColour
            << ";MAT_PHOTO_FILTER=" << desc._doFilterColour
            ;

        _shaderProgram = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            "xleres/basic2D.vsh:fullscreen:vs_*",
            "xleres/postprocess/tonemap.psh:main:ps_*", 
            shaderDefines.get());

		UniformsStreamInterface usi;
		usi.BindConstantBuffer(0, {Hash64("ToneMapSettings")});
		usi.BindConstantBuffer(1, {Hash64("ColorGradingSettings")});
		usi.BindShaderResource(0, Hash64("InputTexture"));
		usi.BindShaderResource(1, Hash64("LuminanceBuffer"));
		usi.BindShaderResource(2, Hash64("BloomMap"));
        _uniforms = Metal::BoundUniforms(
			*_shaderProgram,
			Metal::PipelineLayoutConfig{},
			RenderCore::Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
			usi);

        _validationCallback = _shaderProgram->GetDependencyValidation();
    }

    void ToneMap_Execute(
        RenderCore::IThreadContext& context, 
        RenderCore::Techniques::ParsingContext& parserContext, 
        const LuminanceResult& luminanceResult,
        const ToneMapSettings& settings,
        bool hardwareSRGBEnabled,
        const Metal::ShaderResourceView& inputResource)
    {
        // ProtectState protectState(context, ProtectState::States::BlendState);
        // bool hardwareSRGBEnabled = IsSRGBTargetBound(context);

		auto& devContext = *Metal::DeviceContext::Get(context);

        CATCH_ASSETS_BEGIN
            bool bindCopyShader = true;
            if (settings._flags & ToneMapSettings::Flags::EnableToneMap) {
                if (luminanceResult._isGood) {
                        //  Bind a pixel shader that will do the tonemap operation
                        //  we do color grading operation in the same step... so make sure
                        //  we apply the correct color grading parameters a the same time.
                    TRY {

                        bool doColorGrading = Tweakable("DoColorGrading", false);
                        auto colorGradingSettings = BuildColorGradingShaderConstants(ColorGradingSettings());

                        bool enableBloom = !!(settings._flags & ToneMapSettings::Flags::EnableBloom) && Tweakable("DoBloom", true);
                        auto& box = ConsoleRig::FindCachedBoxDep2<ToneMapShaderBox>(
                            Tweakable("ToneMapOperator", 1), enableBloom,
                            hardwareSRGBEnabled, doColorGrading, !!(colorGradingSettings._doLevelsAdustment), 
                            !!(colorGradingSettings._doSelectiveColor), !!(colorGradingSettings._doFilterColor));

                        ConstantBufferView cbvs[] = { 
                            AsConstants(settings), 
                            MakeSharedPkt(colorGradingSettings) 
                        };
                        const Metal::ShaderResourceView* srvs[] = {
                            &inputResource,
                            &luminanceResult._propertiesBuffer, &luminanceResult._bloomBuffer
                        };
                        box._uniforms.Apply(devContext, 0, parserContext.GetGlobalUniformsStream());
						box._uniforms.Apply(devContext, 1, UniformsStream{MakeIteratorRange(cbvs), UniformsStream::MakeResources(MakeIteratorRange(srvs))});
                        devContext.Bind(*box._shaderProgram);
                        bindCopyShader = false;
                        
                    }
                    CATCH_ASSETS(parserContext)
                    CATCH(...) {
                        // (in this case, we'll fall back to using a copy shader)
                    } CATCH_END
                }

                if (bindCopyShader)
                    StringMeldAppend(parserContext._stringHelpers->_errorString) << "Tonemap -- falling back to copy shader\n";
            } 
        
            if (bindCopyShader) {
                    // If tone mapping is disabled (or if tonemapping failed for any reason)
                    //      -- then we have to bind a copy shader
                devContext.Bind(::Assets::GetAssetDep<Metal::ShaderProgram>(
                    "xleres/basic2D.vsh:fullscreen:vs_*", "xleres/basic.psh:fake_tonemap:ps_*"));
                devContext.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(inputResource));
            }
            
            SetupVertexGeneratorShader(devContext);
            devContext.Bind(Techniques::CommonResources()._blendOpaque);
            devContext.Draw(4);
        CATCH_ASSETS_END(parserContext)
    }

    static void DrawDebugging(Metal::DeviceContext& context, ToneMappingResources& resources)
    {
        SetupVertexGeneratorShader(context);
        context.Bind(Metal::BlendState(BlendOp::Add, Blend::One, Blend::InvSrcAlpha));
        context.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(resources._propertiesBuffer.SRV()));
        for (unsigned c=0; c<std::min(size_t(3),resources._luminanceBuffers.size()); ++c) {
            context.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(1+c, resources._luminanceBuffers[c].SRV()));
        }
        for (unsigned c=0; c<std::min(size_t(3),resources._bloomBuffers.size()); ++c) {
            context.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(4+c, resources._bloomBuffers[c]._bloomBuffer.SRV()));
        }
        context.Bind(::Assets::GetAssetDep<Metal::ShaderProgram>(
            "xleres/basic2D.vsh:fullscreen:vs_*", "xleres/postprocess/debugging.psh:HDRDebugging:ps_*"));
        context.Draw(4);

        context.Bind(::Assets::GetAssetDep<Metal::ShaderProgram>(
            "xleres/postprocess/debugging.psh:LuminanceValue:vs_*", 
            "xleres/utility/metricsrender.gsh:main:gs_*",
            "xleres/utility/metricsrender.psh:main:ps_*", ""));
        Metal::ViewportDesc mainViewportDesc(context);
        unsigned dimensions[4] = { (unsigned)mainViewportDesc.Width, (unsigned)mainViewportDesc.Height, 0, 0 };
        context.GetNumericUniforms(ShaderStage::Geometry).Bind(MakeResourceList(MakeMetalCB(dimensions, sizeof(dimensions))));
        context.GetNumericUniforms(ShaderStage::Vertex).Bind(MakeResourceList(resources._propertiesBuffer.SRV()));
        context.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(3, ::Assets::MakeAsset<RenderCore::Techniques::DeferredShaderResource>("xleres/DefaultResources/metricsdigits.dds:T")->Actualize()->GetShaderResource()));
        context.Bind(Topology::PointList);
        context.Draw(1);
    }

        //////////////////////////////////////////////////////////////////////////////
            //      A T M O S P H E R E   B L U R                               //
        //////////////////////////////////////////////////////////////////////////////

    class AtmosphereBlurResources
    {
    public:
        class Desc
        {
        public:
            unsigned _width, _height;
            Format _format;
            Desc(unsigned width, unsigned height, Format format);
        };

        AtmosphereBlurResources(const Desc&);
        ~AtmosphereBlurResources();

        intrusive_ptr<BufferUploads::ResourceLocator>           _blurBuffer[2];
        Metal::RenderTargetView     _blurBufferRTV[2];
        Metal::ShaderResourceView   _blurBufferSRV[2];

        const Metal::ShaderProgram*       _horizontalFilter;
        const Metal::ShaderProgram*       _verticalFilter;
        std::unique_ptr<Metal::BoundUniforms>   _horizontalFilterBinding;
        std::unique_ptr<Metal::BoundUniforms>   _verticalFilterBinding;

        Metal::BlendState           _integrateBlend;
        Metal::BlendState           _noBlending;

        const Metal::ShaderProgram*       _integrateDistantBlur;
        std::unique_ptr<Metal::BoundUniforms>   _integrateDistantBlurBinding;

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const   { return _validationCallback; }
    private:
        std::shared_ptr<::Assets::DependencyValidation>  _validationCallback;
    };

    AtmosphereBlurResources::Desc::Desc(unsigned width, unsigned height, Format format)
    {
        _width = width;
        _height = height;
        _format = format;
    }

    AtmosphereBlurResources::AtmosphereBlurResources(const Desc& desc)
    {
        auto& uploads = GetBufferUploads();

        auto bufferDesc = BuildRenderTargetDesc(
            BindFlag::ShaderResource|BindFlag::RenderTarget,
            BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, desc._format),
            "AtmosBlur");
        auto bloomBuffer0 = uploads.Transaction_Immediate(bufferDesc);
        auto bloomBuffer1 = uploads.Transaction_Immediate(bufferDesc);

        Metal::RenderTargetView     bloomBufferRTV0(bloomBuffer0->GetUnderlying());
        Metal::ShaderResourceView   bloomBufferSRV0(bloomBuffer0->GetUnderlying());
        Metal::RenderTargetView     bloomBufferRTV1(bloomBuffer1->GetUnderlying());
        Metal::ShaderResourceView   bloomBufferSRV1(bloomBuffer1->GetUnderlying());

        auto* horizontalFilter = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            "xleres/basic2D.vsh:fullscreen:vs_*", 
            "xleres/Effects/distantblur.psh:HorizontalBlur_DistanceWeighted:ps_*");
        auto* verticalFilter = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            "xleres/basic2D.vsh:fullscreen:vs_*", 
            "xleres/Effects/distantblur.psh:VerticalBlur_DistanceWeighted:ps_*");

        UniformsStreamInterface filterUsi;
		filterUsi.BindConstantBuffer(0, {Hash64("Constants")});
		filterUsi.BindConstantBuffer(1, {Hash64("BlurConstants")});
		filterUsi.BindShaderResource(0, Hash64("DepthsInput"));
		filterUsi.BindShaderResource(1, Hash64("InputTexture"));

		auto horizontalFilterBinding = std::make_unique<Metal::BoundUniforms>(
			std::ref(*horizontalFilter),
			Metal::PipelineLayoutConfig{},
			UniformsStreamInterface{},
			filterUsi);
		auto verticalFilterBinding = std::make_unique<Metal::BoundUniforms>(
			std::ref(*verticalFilter),
			Metal::PipelineLayoutConfig{},
			UniformsStreamInterface{},
			filterUsi);

        auto* integrateDistantBlur = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            "xleres/basic2D.vsh:fullscreen:vs_*", 
            "xleres/Effects/distantblur.psh:integrate:ps_*");
        
		UniformsStreamInterface integrateDistantBlurUsi;
		integrateDistantBlurUsi.BindConstantBuffer(0, {Hash64("Constants")});
		integrateDistantBlurUsi.BindConstantBuffer(0, {Hash64("BlurConstants")});
		integrateDistantBlurUsi.BindShaderResource(0, Hash64("DepthsInput"));
		integrateDistantBlurUsi.BindShaderResource(0, Hash64("BlurredBufferInput"));

		auto integrateDistantBlurBinding = std::make_unique<Metal::BoundUniforms>(
			std::ref(*integrateDistantBlur),
			Metal::PipelineLayoutConfig{},
			Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
			integrateDistantBlurUsi);

        Metal::BlendState integrateBlend;
        Metal::BlendState noBlending = BlendOp::NoBlending;

        auto validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(validationCallback, horizontalFilter->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(validationCallback, verticalFilter->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(validationCallback, integrateDistantBlur->GetDependencyValidation());

        _blurBuffer[0]                  = std::move(bloomBuffer0);
        _blurBuffer[1]                  = std::move(bloomBuffer1);
        _blurBufferRTV[0]               = std::move(bloomBufferRTV0);
        _blurBufferRTV[1]               = std::move(bloomBufferRTV1);
        _blurBufferSRV[0]               = std::move(bloomBufferSRV0);
        _blurBufferSRV[1]               = std::move(bloomBufferSRV1);
        _horizontalFilter               = std::move(horizontalFilter);
        _verticalFilter                 = std::move(verticalFilter);
        _horizontalFilterBinding        = std::move(horizontalFilterBinding);
        _verticalFilterBinding          = std::move(verticalFilterBinding);
        _validationCallback             = std::move(validationCallback);
        _integrateDistantBlur           = std::move(integrateDistantBlur);
        _integrateBlend                 = std::move(integrateBlend);
        _noBlending                     = std::move(noBlending);
        _integrateDistantBlurBinding    = std::move(integrateDistantBlurBinding);
    }

    AtmosphereBlurResources::~AtmosphereBlurResources() {}

    void AtmosphereBlur_Execute(
        RenderCore::IThreadContext& threadContext, RenderCore::Techniques::ParsingContext& parserContext,
        const AtmosphereBlurSettings& settings)
    {
            //  simple distance blur for the main camera
            //  sometimes called depth-of-field; but really it's blurring of distant objects
            //  caused by the atmosphere. Depth of field is an artifact that occurs in lens.

		auto& context = *RenderCore::Metal::DeviceContext::Get(threadContext);

        Metal::ViewportDesc viewport(context);
        SavedTargets savedTargets(context);
        auto targetsReset = savedTargets.MakeResetMarker(context);

        CATCH_ASSETS_BEGIN

                //  We can actually drop down to 1/3 resolution here, fine... but then
                //  the blurring becomes too much
                //  We need to use a format with an alpha channel for the weighted blur.
                //  
            unsigned blurBufferWidth = unsigned(viewport.Width/2);
            unsigned blurBufferHeight = unsigned(viewport.Height/2);
            auto& resources = ConsoleRig::FindCachedBoxDep2<AtmosphereBlurResources>(
                blurBufferWidth, blurBufferHeight, Format::R16G16B16A16_FLOAT);

            Metal::ViewportDesc newViewport(0, 0, float(blurBufferWidth), float(blurBufferHeight), 0.f, 1.f);
            context.Bind(newViewport);
            context.Bind(MakeResourceList(resources._blurBufferRTV[1]), nullptr);
            context.Bind(resources._noBlending);
            SetupVertexGeneratorShader(context);

#if GFXAPI_ACTIVE == GFXAPI_DX11	// platformtemp
            auto depths = Metal::ExtractResource<ID3D::Resource>(savedTargets.GetDepthStencilView());
            Metal::ShaderResourceView depthsSRV(Metal::AsResourcePtr(std::move(depths)), {{TextureViewDesc::Aspect::Depth}});
            auto res = Metal::ExtractResource<ID3D::Resource>(savedTargets.GetRenderTargets()[0]);
            Metal::ShaderResourceView inputSRV(Metal::AsResourcePtr(std::move(res)));
#else
			Metal::ShaderResourceView depthsSRV, inputSRV;
#endif

            const Metal::ShaderResourceView* blurSrvs[] = { &depthsSRV, &inputSRV };
            float blurConstants[4] = { settings._startDistance, settings._endDistance, 0.f, 0.f };
            float filteringWeights[8];
            BuildGaussianFilteringWeights(filteringWeights, settings._blurStdDev, dimof(filteringWeights));
            ConstantBufferView constantBufferPackets[] = { MakeSharedPkt(filteringWeights), MakeSharedPkt(blurConstants) };

                //          blur once horizontally, and once vertically
            ///////////////////////////////////////////////////////////////////////////////////////
            {
                resources._horizontalFilterBinding->Apply(context, 0, parserContext.GetGlobalUniformsStream());
				resources._horizontalFilterBinding->Apply(context, 1, UniformsStream{MakeIteratorRange(constantBufferPackets), UniformsStream::MakeResources(MakeIteratorRange(blurSrvs))});
                context.Bind(*resources._horizontalFilter);
                context.Draw(4);
            }
            ///////////////////////////////////////////////////////////////////////////////////////
            {
                context.Bind(MakeResourceList(resources._blurBufferRTV[0]), nullptr);
                const Metal::ShaderResourceView* blurSrvs2[] = { &depthsSRV, &resources._blurBufferSRV[1] };
                resources._verticalFilterBinding->Apply(context, 0, parserContext.GetGlobalUniformsStream());
				resources._verticalFilterBinding->Apply(context, 1, UniformsStream{MakeIteratorRange(constantBufferPackets), UniformsStream::MakeResources(MakeIteratorRange(blurSrvs2))});
                context.Bind(*resources._verticalFilter);
                context.Draw(4);
            }
            ///////////////////////////////////////////////////////////////////////////////////////

            resources._horizontalFilterBinding->UnbindShaderResources(context, 1);

                // copied blurred buffer back into main target
                //  bind output rendertarget (but not depth buffer)
            auto savedViewport = savedTargets.GetViewports()[0];
            context.Bind(savedViewport);
            context.Bind(MakeResourceList(Metal::RenderTargetView(savedTargets.GetRenderTargets()[0])), nullptr);

#if GFXAPI_ACTIVE == GFXAPI_DX11	// platformtemp
            if (!savedTargets.GetDepthStencilView()) {
                Throw(::Exceptions::BasicLabel("No depth stencil buffer bound using atmospheric blur render"));
            }
#endif

            context.Bind(*resources._integrateDistantBlur);
            const Metal::ShaderResourceView* srvs[] = { &depthsSRV, &resources._blurBufferSRV[0] };
            resources._integrateDistantBlurBinding->Apply(context, 0, parserContext.GetGlobalUniformsStream());
			resources._integrateDistantBlurBinding->Apply(context, 1, UniformsStream{MakeIteratorRange(constantBufferPackets), UniformsStream::MakeResources(MakeIteratorRange(srvs))});
            context.Bind(resources._integrateBlend);
            context.Draw(4);
			resources._integrateDistantBlurBinding->UnbindShaderResources(context, 1);
            // context.UnbindPS<Metal::ShaderResourceView>(3, 3);

        CATCH_ASSETS_END(parserContext)
    }

        //////////////////////////////////////////////////////////////////////////////
            //      C O N F I G U R A T I O N   S E T T I N G S                 //
        //////////////////////////////////////////////////////////////////////////////

    ColorGradingSettings DefaultColorGradingSettings()
    {
        ColorGradingSettings result;
        result._sharpenAmount           = 0.f;
        result._minInput                = 0.f;
        result._gammaInput              = 0.99999994f;
        result._maxInput                = 255.f;
        result._minOutput               = 0.f;
        result._maxOutput               = 255.f;
        result._brightness              = 1.f;
        result._contrast                = 1.f;
        result._saturation              = 1.f;
        result._filterColor             = Float3(0.59999996f, 0.82352895f, 0.57647097f);
        result._filterColorDensity      = 0.f;
        result._grain                   = 0.f;
        result._selectiveColor          = Float4(0.f, 1.f, 1.f, 1.f);
        result._selectiveColorCyans     = 0.f;
        result._selectiveColorMagentas  = 0.f;
        result._selectiveColorYellows   = 0.f;
        result._selectiveColorBlacks    = 0.f;
        return result;
    }

    ColorGradingShaderConstants BuildColorGradingShaderConstants(const ColorGradingSettings& settings)
    {
            // colour grading not currently supported...!
        ColorGradingShaderConstants result;
        return result;
    }

    RenderCore::SharedPkt AsConstants(const ToneMapSettings& settings)
    {
        auto pkt = RenderCore::MakeSharedPktSize(sizeof(ToneMapSettingsConstants));
        ToneMapSettingsConstants& result = *(ToneMapSettingsConstants*)pkt.begin();
        result._bloomScale = settings._bloomBrightness * settings._bloomColor;
        result._bloomThreshold = settings._bloomThreshold;
        result._bloomRampingFactor = settings._bloomRampingFactor;
        result._bloomDesaturationFactor = settings._bloomDesaturationFactor;
        result._sceneKey = settings._sceneKey;
	    result._luminanceMin = settings._luminanceMin;
	    result._luminanceMax = settings._luminanceMax;
	    result._whitepoint = settings._whitepoint;
        result._dummy[0] = 0.f;
        result._dummy[1] = 0.f;
        return pkt;
    }

    ToneMapSettings::ToneMapSettings()
    {
        _flags = ToneMapSettings::Flags::EnableToneMap | ToneMapSettings::Flags::EnableBloom;
        _bloomColor = Float3(1.f, 1.f, 1.f); // Float3(19.087036f, 11.582731f, 6.6070509f);
        _bloomBrightness = 1.f;
        _bloomThreshold = 10.f;
        _bloomRampingFactor = .8f;
        _bloomDesaturationFactor = .6f;
        _sceneKey = .23f;
        _luminanceMin = 0.0f;
        _luminanceMax = 20.f;
        _whitepoint = 8.f;
        _bloomBlurStdDev = 1.32f;
    }

}

template<> const ClassAccessors& GetAccessors<SceneEngine::ToneMapSettings>()
{
    using Obj = SceneEngine::ToneMapSettings;
    static ClassAccessors props(typeid(Obj).hash_code());
    static bool init = false;
    if (!init) {
        props.Add("Flags",                   DefaultGet(Obj, _flags),                    DefaultSet(Obj, _flags));
        props.Add("BloomThreshold",          DefaultGet(Obj, _bloomThreshold),           DefaultSet(Obj, _bloomThreshold));
        props.Add("BloomRampingFactor",      DefaultGet(Obj, _bloomRampingFactor),       DefaultSet(Obj, _bloomRampingFactor));
        props.Add("BloomDesaturationFactor", DefaultGet(Obj, _bloomDesaturationFactor),  DefaultSet(Obj, _bloomDesaturationFactor));
        props.Add("SceneKey",                DefaultGet(Obj, _sceneKey),                 DefaultSet(Obj, _sceneKey));
        props.Add("LuminanceMin",            DefaultGet(Obj, _luminanceMin),             DefaultSet(Obj, _luminanceMin));
        props.Add("LuminanceMax",            DefaultGet(Obj, _luminanceMax),             DefaultSet(Obj, _luminanceMax));
        props.Add("WhitePoint",              DefaultGet(Obj, _whitepoint),               DefaultSet(Obj, _whitepoint));
        props.Add("BloomBlurStdDev",         DefaultGet(Obj, _bloomBlurStdDev),          DefaultSet(Obj, _bloomBlurStdDev));
        props.Add("BloomBrightness",         DefaultGet(Obj, _bloomBrightness),          DefaultSet(Obj, _bloomBrightness));
        props.Add("BloomScale", 
            [](const Obj& obj)              { return SceneEngine::AsPackedColor(obj._bloomColor); },
            [](Obj& obj, unsigned value)    { obj._bloomColor = SceneEngine::AsFloat3Color(value); });
        
        init = true;
    }
    return props;
}
