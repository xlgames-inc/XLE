// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Tonemap.h"
#include "SceneEngineUtils.h"
#include "LightingParserContext.h"
#include "SceneParser.h"
#include "LightDesc.h"
#include "GestaltResource.h"
#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Assets/DeferredShaderResource.h"
#include "../RenderCore/Metal/Format.h"
#include "../RenderCore/Metal/RenderTargetView.h"
#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/RenderUtils.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/DataPacket.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../ConsoleRig/Console.h"
#include "../Utility/BitUtils.h"
#include "../Utility/ParameterBox.h"
#include "../Utility/StringFormat.h"
#include "../Utility/Meta/ClassAccessorsImpl.h"

// #include "../RenderCore/Metal/DeviceContextImpl.h"
#include "../RenderCore/DX11/Metal/DX11Utils.h"

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
    ToneMapSettingsConstants AsConstants(const ToneMapSettings& settings);

    class BloomStepBuffer
    {
    public:
        GestaltTypes::RTVUAVSRV _bloomBuffer;
        unsigned                _width, _height;

        BloomStepBuffer(unsigned width, unsigned height, Metal::NativeFormat::Enum format);
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

    BloomStepBuffer::BloomStepBuffer(unsigned width, unsigned height, Metal::NativeFormat::Enum format)
    : _width(width), _height(height)
    , _bloomBuffer(
        GestaltTypes::RTVUAVSRV(
            BufferUploads::TextureDesc::Plain2D(width, height, Metal::AsDXGIFormat(format)),
            "BloomLumin"))
    {}

    BloomStepBuffer::BloomStepBuffer()  {}
    BloomStepBuffer::~BloomStepBuffer() {}

    class ToneMappingResources
    {
    public:
        using NativeFormat = Metal::NativeFormat::Enum;
        class Desc
        {
        public:
            Desc(
                unsigned width, unsigned height, 
                NativeFormat bloomBufferFormat, 
                unsigned sampleCount, bool useMSAASamplers)
            {
                std::fill((byte*)this, (byte*)PtrAdd(this, sizeof(*this)), 0);
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

        std::vector<GestaltTypes::UAVSRV> _luminanceBuffers;
        GestaltTypes::UAVSRV            _propertiesBuffer;
        std::vector<BloomStepBuffer>    _bloomBuffers;
        BloomStepBuffer                 _bloomTempBuffer;

        const Metal::ComputeShader*		_sampleInitialLuminance;
        const Metal::ComputeShader*		_luminanceStepDown;
        const Metal::ComputeShader*		_updateOverallLuminance;
        const Metal::ComputeShader*		_updateOverallLuminanceNoAdapt;
        const Metal::ComputeShader*		_brightPassStepDown;

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
                BufferUploads::TextureDesc::Plain2D(1<<s, 1<<(s+heightDifference), Metal::AsDXGIFormat(NativeFormat::R16_FLOAT)),
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
            "LumianceProperties", 
            BufferUploads::CreateBasicPacket(sizeof(initialData), &initialData).get(),
            BufferUploads::BindFlag::StructuredBuffer);

        StringMeld<256> shaderDefines; 
        shaderDefines << "MSAA_SAMPLES=" << desc._sampleCount;
        if (desc._useMSAASamplers) shaderDefines << ";MSAA_SAMPLERS=1";

        _sampleInitialLuminance    = &::Assets::GetAssetDep<Metal::ComputeShader>("game/xleres/postprocess/hdrluminance.csh:SampleInitialLuminance:cs_*", shaderDefines.get());
        _luminanceStepDown         = &::Assets::GetAssetDep<Metal::ComputeShader>("game/xleres/postprocess/hdrluminance.csh:LuminanceStepDown:cs_*");
        _updateOverallLuminance    = &::Assets::GetAssetDep<Metal::ComputeShader>("game/xleres/postprocess/hdrluminance.csh:UpdateOverallLuminance:cs_*");
        _brightPassStepDown        = &::Assets::GetAssetDep<Metal::ComputeShader>("game/xleres/postprocess/hdrluminance.csh:BrightPassStepDown:cs_*");

        _updateOverallLuminanceNoAdapt = &::Assets::GetAssetDep<Metal::ComputeShader>("game/xleres/postprocess/hdrluminance.csh:UpdateOverallLuminance:cs_*", "IMMEDIATE_ADAPT=1");

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(_validationCallback, _sampleInitialLuminance->GetDependencyValidation());
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
        Metal::DeviceContext& context,
        Techniques::ParsingContext& parserContext,
        ToneMappingResources& resources,
        const ToneMapSettings& settings,
        const Metal::ShaderResourceView& sourceTexture,
        bool doAdapt)
    {
            //
            //      First step in tonemapping is always to calculate the overall
            //      scene brightness. We do this in a series of step-down operations,
            //      each time reducing the image by a power of two.
            //
            //      We might use a log-average method for this, but we have to
            //      be careful about floating point accuracy.
            //
        SavedTargets savedTargets(context);
        auto resetTargets = savedTargets.MakeResetMarker(context);
        context.Bind(ResourceList<Metal::RenderTargetView, 0>(), nullptr);

        TRY {
            auto sourceDesc = Metal::TextureDesc2D(sourceTexture.GetUnderlying());
            UInt2 sourceDims(sourceDesc.Width, sourceDesc.Height);

            static unsigned frameIndex = 0; ++frameIndex;
            struct LuminanceConstants
            {
                int     _frameIndex;
                int     _totalSampleCount;
                float   _elapsedTime;
                int     _buffer;
                UInt2   _inputTextureDims;
                Float2  _initialSampleSizeRatio;
            } luminanceConstants = 
            {
                frameIndex, resources._firstStepWidth*resources._firstStepHeight, 1.0f/60.f, 0,
                sourceDims,
                Float2(float(sourceDims[0]) / float(resources._firstStepWidth), float(sourceDims[1]) / float(resources._firstStepHeight))
            };

            auto toneMapConstants = AsConstants(settings);
            context.BindCS(MakeResourceList(
                Metal::ConstantBuffer(&toneMapConstants, sizeof(toneMapConstants)),
                Metal::ConstantBuffer(&luminanceConstants, sizeof(luminanceConstants))));

            assert(!resources._luminanceBuffers.empty());

                //
                //      Initial sample from the source texture first,
                //      then make a number of down-step operations.
                // 
                //      The first step is not MSAA-aware. Only the 0th
                //      sample is considered. We could also
                //
            context.Bind(*resources._sampleInitialLuminance);
            context.BindCS(MakeResourceList(sourceTexture));
            context.BindCS(MakeResourceList(resources._luminanceBuffers[0].UAV(), resources._bloomBuffers[0]._bloomBuffer.UAV(), resources._propertiesBuffer.UAV()));
            context.Dispatch(resources._firstStepWidth/16, resources._firstStepHeight/16);
            context.UnbindCS<Metal::UnorderedAccessView>(0, 2);

            context.Bind(*resources._luminanceStepDown);
            for (size_t c=1; c<resources._luminanceBuffers.size(); ++c) {
                context.BindCS(MakeResourceList(resources._luminanceBuffers[c-1].SRV()));
                context.BindCS(MakeResourceList(resources._luminanceBuffers[c].UAV()));
                context.Dispatch(std::max(1u, (resources._firstStepWidth>>c)/16), std::max(1u, (resources._firstStepHeight>>c)/16));
                context.UnbindCS<Metal::UnorderedAccessView>(0, 1);
            }

            context.Bind(*resources._brightPassStepDown);
            for (size_t c=1; c<resources._bloomBuffers.size(); ++c) {
                context.BindCS(MakeResourceList(resources._bloomBuffers[c-1]._bloomBuffer.SRV()));
                context.BindCS(MakeResourceList(1, resources._bloomBuffers[c]._bloomBuffer.UAV()));
                context.Dispatch(std::max(1u, (resources._firstStepWidth>>c)/16), std::max(1u, (resources._firstStepHeight>>c)/16));
                context.UnbindCS<Metal::UnorderedAccessView>(1, 1);
            }

                //
                //      After we've down all of the downsample steps, we should have
                //      a 2x2 texture -- these are our final samples.
                //      Get the final luminance values, and balance it with the previous
                //      frame's values to produce the new value for this frame...
                //

            context.BindCS(MakeResourceList(resources._luminanceBuffers[resources._luminanceBuffers.size()-1].SRV()));
            if (doAdapt)    context.Bind(*resources._updateOverallLuminance);
            else            context.Bind(*resources._updateOverallLuminanceNoAdapt);
            context.Dispatch(1);
            context.UnbindCS<Metal::UnorderedAccessView>(0, 3);

                //
                //      We need to do a series of Gaussian blurs over the bloom buffers
                //      We should have a number of bloom buffers, each at different 
                //      resolutions...
                //      We blur each one, and add it to the next higher resolution one
                //

            if (settings._flags & ToneMapSettings::Flags::EnableBloom) {
                float filteringWeights[12];
                XlSetMemory(filteringWeights, 0, sizeof(filteringWeights));
                BuildGaussianFilteringWeights(filteringWeights, settings._bloomBlurStdDev, 11);
                context.BindPS(MakeResourceList(Metal::ConstantBuffer(filteringWeights, sizeof(filteringWeights))));

                context.Bind(Techniques::CommonResources()._dssDisable);

                auto& horizBlur = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                    "game/xleres/basic2D.vsh:fullscreen:vs_*", 
                    "game/xleres/Effects/SeparableFilter.psh:HorizontalBlur11:ps_*",
                    "USE_CLAMPING_WINDOW=1");
                auto& vertBlur = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                    "game/xleres/basic2D.vsh:fullscreen:vs_*", 
                    "game/xleres/Effects/SeparableFilter.psh:VerticalBlur11:ps_*",
                    "USE_CLAMPING_WINDOW=1");
                auto& copyShader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                    "game/xleres/basic2D.vsh:fullscreen:vs_*", 
                    "game/xleres/basic.psh:copy_bilinear:ps_*");

                // D3D11_VIEWPORT newViewport = { 0, 0, (float)resources._bloomBuffers[0]._width, (float)resources._bloomBuffers[0]._height, 0.f, 1.f };
                // context->GetUnderlying()->RSSetViewports(1, &newViewport);
                context.Bind(Metal::Topology::TriangleStrip);
            
                for (auto i=resources._bloomBuffers.crbegin(); ;) {

                    Metal::ViewportDesc newViewport(0, 0, (float)i->_width, (float)i->_height, 0.f, 1.f);
                    context.Bind(newViewport);

                    Float4 clampingWindow(0.f, 0.f, (float)i->_width - 1.f, (float)i->_height - 1.f);
                    context.BindPS(MakeResourceList(1, Metal::ConstantBuffer(&clampingWindow, sizeof(clampingWindow))));

                    context.Bind(Techniques::CommonResources()._blendOpaque);
                    context.Bind(MakeResourceList(resources._bloomTempBuffer._bloomBuffer.RTV()), nullptr);
                    context.BindPS(MakeResourceList(i->_bloomBuffer.SRV()));
                    context.Bind(horizBlur);
                    context.Draw(4);
            
                    context.UnbindPS<Metal::ShaderResourceView>(0, 1);
                    context.Bind(MakeResourceList(i->_bloomBuffer.RTV()), nullptr);
                    context.BindPS(MakeResourceList(resources._bloomTempBuffer._bloomBuffer.SRV()));
                    context.Bind(vertBlur);
                    context.Draw(4);
                    context.UnbindPS<Metal::ShaderResourceView>(0, 1);

                    auto oldi = i;
                    ++i;
                    if (i==resources._bloomBuffers.crend()) {
                        break;
                    }

                        // offset viewport by half a pixel -- this will make the bilinear filter catch the right pixels
                    Metal::ViewportDesc newViewport2(-.5f / float(i->_width), -.5f / float(i->_height), float(i->_width) -.5f / float(i->_width), float(i->_height) -.5f / float(i->_height), 0.f, 1.f);
                    context.Bind(newViewport2);

                        // simple copy step
                    context.Bind(Metal::BlendState(Metal::BlendOp::Add, Metal::Blend::One, Metal::Blend::One));
                    context.Bind(MakeResourceList(i->_bloomBuffer.RTV()), nullptr);
                    context.BindPS(MakeResourceList(oldi->_bloomBuffer.SRV()));
                    context.Bind(copyShader);
                    context.Draw(4);
                    context.UnbindPS<Metal::ShaderResourceView>(0, 1);
                    context.Bind(ResourceList<Metal::RenderTargetView, 0>(), nullptr);

                }
            }
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
        auto bloomBufferFormat = Metal::NativeFormat::R10G10B10A2_UNORM;
        if (bloomBufferFormatType==1) {
            bloomBufferFormat = Metal::NativeFormat::R8G8B8A8_UNORM;
        } else if (bloomBufferFormatType==2) {
            bloomBufferFormat = Metal::NativeFormat::R16G16B16A16_FLOAT;
        } else if (bloomBufferFormatType==3) {
            bloomBufferFormat = Metal::NativeFormat::R11G11B10_FLOAT;
        } else if (bloomBufferFormatType==4) {
            bloomBufferFormat = Metal::NativeFormat::R16_FLOAT;
        }
        Metal::TextureDesc2D desc(inputResource.GetUnderlying());
        auto sampleCount = desc.SampleDesc.Count;
        return Techniques::FindCachedBoxDep2<ToneMappingResources>(
            desc.Width, desc.Height, bloomBufferFormat, sampleCount, sampleCount>1);
    }

    LuminanceResult ToneMap_SampleLuminance(
        RenderCore::Metal::DeviceContext& context, 
        LightingParserContext& parserContext,
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

    LuminanceResult ToneMap_SampleLuminance(
        RenderCore::Metal::DeviceContext& context, 
        RenderCore::Techniques::ParsingContext& parserContext,
        const ToneMapSettings& settings,
        const RenderCore::Metal::ShaderResourceView& inputResource,
        bool doAdapt)
    {
        auto& toneMapRes = GetResources(inputResource);
        bool success = TryCalculateInputs(
            context, parserContext, 
            toneMapRes, settings, inputResource, doAdapt);
        return LuminanceResult(toneMapRes._propertiesBuffer.SRV(), toneMapRes._bloomBuffers[0]._bloomBuffer.SRV(), success);
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
            "game/xleres/basic2D.vsh:fullscreen:vs_*",
            "game/xleres/postprocess/tonemap.psh:main:ps_*", 
            shaderDefines.get());

        _uniforms = Metal::BoundUniforms(*_shaderProgram);
        _uniforms.BindConstantBuffers(1, {"ToneMapSettings", "ColorGradingSettings"});
        _uniforms.BindShaderResources(1, {"LuminanceBuffer", "BloomMap"});
        RenderCore::Techniques::TechniqueContext::BindGlobalUniforms(_uniforms);

        _validationCallback = _shaderProgram->GetDependencyValidation();
    }

    static void ExecuteOpaqueFullScreenPass(Metal::DeviceContext& context)
    {
        SetupVertexGeneratorShader(context);
        context.Bind(Techniques::CommonResources()._blendOpaque);
        context.Draw(4);
    }

    static bool IsSRGBTargetBound(Metal::DeviceContext& context)
    {
            //  Query the destination render target to
            //  see if SRGB conversion is enabled when writing out
        SavedTargets destinationTargets(context);
        D3D11_RENDER_TARGET_VIEW_DESC rtv;
        if (destinationTargets.GetRenderTargets()[0]) {
            destinationTargets.GetRenderTargets()[0]->GetDesc(&rtv);
            return Metal::GetComponentType(Metal::AsNativeFormat(rtv.Format)) == Metal::FormatComponentType::UNorm_SRGB;
        }
        return true;
    }

    void ToneMap_Execute(
        Metal::DeviceContext& context, 
        RenderCore::Techniques::ParsingContext& parserContext, 
        const LuminanceResult& luminanceResult,
        const ToneMapSettings& settings,
        const Metal::ShaderResourceView& inputResource)
    {
        ProtectState protectState(context, ProtectState::States::BlendState);
        bool hardwareSRGBEnabled = IsSRGBTargetBound(context);

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

                        auto& box = Techniques::FindCachedBoxDep2<ToneMapShaderBox>(
                            Tweakable("ToneMapOperator", 1), !!(settings._flags & ToneMapSettings::Flags::EnableBloom),
                            hardwareSRGBEnabled, doColorGrading, !!(colorGradingSettings._doLevelsAdustment), 
                            !!(colorGradingSettings._doSelectiveColor), !!(colorGradingSettings._doFilterColor));

                        SharedPkt cbs[] = { 
                            MakeSharedPkt(AsConstants(settings)), 
                            MakeSharedPkt(colorGradingSettings) 
                        };
                        const Metal::ShaderResourceView* srvs[] = {
                            &luminanceResult._propertiesBuffer, &luminanceResult._bloomBuffer
                        };
                        box._uniforms.Apply(context, parserContext.GetGlobalUniformsStream(), Metal::UniformsStream(cbs, srvs));
                        context.Bind(*box._shaderProgram);
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
                context.Bind(::Assets::GetAssetDep<Metal::ShaderProgram>(
                    "game/xleres/basic2D.vsh:fullscreen:vs_*", "game/xleres/basic.psh:fake_tonemap:ps_*"));
            }
            
            context.BindPS(MakeResourceList(inputResource));
            ExecuteOpaqueFullScreenPass(context);
        CATCH_ASSETS_END(parserContext)
    }

    static void DrawDebugging(Metal::DeviceContext& context, ToneMappingResources& resources)
    {
        SetupVertexGeneratorShader(context);
        context.Bind(Metal::BlendState(Metal::BlendOp::Add, Metal::Blend::One, Metal::Blend::InvSrcAlpha));
        context.BindPS(MakeResourceList(resources._propertiesBuffer.SRV()));
        for (unsigned c=0; c<std::min(size_t(3),resources._luminanceBuffers.size()); ++c) {
            context.BindPS(MakeResourceList(1+c, resources._luminanceBuffers[c].SRV()));
        }
        for (unsigned c=0; c<std::min(size_t(3),resources._bloomBuffers.size()); ++c) {
            context.BindPS(MakeResourceList(4+c, resources._bloomBuffers[c]._bloomBuffer.SRV()));
        }
        context.Bind(::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*", "game/xleres/postprocess/debugging.psh:HDRDebugging:ps_*"));
        context.Draw(4);

        context.Bind(::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/postprocess/debugging.psh:LuminanceValue:vs_*", 
            "game/xleres/utility/metricsrender.gsh:main:gs_*",
            "game/xleres/utility/metricsrender.psh:main:ps_*", ""));
        Metal::ViewportDesc mainViewportDesc(context);
        unsigned dimensions[4] = { (unsigned)mainViewportDesc.Width, (unsigned)mainViewportDesc.Height, 0, 0 };
        context.BindGS(MakeResourceList(Metal::ConstantBuffer(dimensions, sizeof(dimensions))));
        context.BindVS(MakeResourceList(resources._propertiesBuffer.SRV()));
        context.BindPS(MakeResourceList(3, ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("game/xleres/DefaultResources/metricsdigits.dds:T").GetShaderResource()));
        context.Bind(Metal::Topology::PointList);
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
            Metal::NativeFormat::Enum _format;
            Desc(unsigned width, unsigned height, Metal::NativeFormat::Enum format);
        };

        AtmosphereBlurResources(const Desc&);
        ~AtmosphereBlurResources();

        intrusive_ptr<ID3D::Resource>           _blurBuffer[2];
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

    AtmosphereBlurResources::Desc::Desc(unsigned width, unsigned height, Metal::NativeFormat::Enum format)
    {
        _width = width;
        _height = height;
        _format = format;
    }

    AtmosphereBlurResources::AtmosphereBlurResources(const Desc& desc)
    {
        using namespace BufferUploads;
        auto& uploads = GetBufferUploads();

        auto bufferDesc = BuildRenderTargetDesc(
            BindFlag::ShaderResource|BindFlag::RenderTarget,
            BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, desc._format),
            "AtmosBlur");
        auto bloomBuffer0 = uploads.Transaction_Immediate(bufferDesc)->AdoptUnderlying();
        auto bloomBuffer1 = uploads.Transaction_Immediate(bufferDesc)->AdoptUnderlying();

        Metal::RenderTargetView     bloomBufferRTV0(bloomBuffer0.get());
        Metal::ShaderResourceView   bloomBufferSRV0(bloomBuffer0.get());
        Metal::RenderTargetView     bloomBufferRTV1(bloomBuffer1.get());
        Metal::ShaderResourceView   bloomBufferSRV1(bloomBuffer1.get());

        auto* horizontalFilter = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*", 
            "game/xleres/Effects/distantblur.psh:HorizontalBlur_DistanceWeighted:ps_*");
        auto* verticalFilter = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*", 
            "game/xleres/Effects/distantblur.psh:VerticalBlur_DistanceWeighted:ps_*");

        auto horizontalFilterBinding = std::make_unique<Metal::BoundUniforms>(std::ref(*horizontalFilter));
        horizontalFilterBinding->BindConstantBuffers(1, {"Constants", "BlurConstants"});
        horizontalFilterBinding->BindShaderResources(1, {"DepthsInput", "InputTexture"});

        auto verticalFilterBinding = std::make_unique<Metal::BoundUniforms>(std::ref(*verticalFilter));
        verticalFilterBinding->BindConstantBuffers(1, {"Constants", "BlurConstants"});
        verticalFilterBinding->BindShaderResources(1, {"DepthsInput", "InputTexture"});

        auto* integrateDistantBlur = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*", 
            "game/xleres/Effects/distantblur.psh:integrate:ps_*");
        auto integrateDistantBlurBinding = std::make_unique<Metal::BoundUniforms>(std::ref(*integrateDistantBlur));
        Techniques::TechniqueContext::BindGlobalUniforms(*integrateDistantBlurBinding.get());
        integrateDistantBlurBinding->BindConstantBuffers(1, {"Constants", "BlurConstants"});
        integrateDistantBlurBinding->BindShaderResource(Hash64("DepthsInput"), 0, 1);
        integrateDistantBlurBinding->BindShaderResource(Hash64("BlurredBufferInput"), 1, 1);

        Metal::BlendState integrateBlend;
        Metal::BlendState noBlending = Metal::BlendOp::NoBlending;

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
        Metal::DeviceContext& context, LightingParserContext& parserContext,
        const AtmosphereBlurSettings& settings)
    {
            //  simple distance blur for the main camera
            //  sometimes called depth-of-field; but really it's blurring of distant objects
            //  caused by the atmosphere. Depth of field is an artifact that occurs in lens.

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
            auto& resources = Techniques::FindCachedBoxDep2<AtmosphereBlurResources>(
                blurBufferWidth, blurBufferHeight, Metal::NativeFormat::R16G16B16A16_FLOAT);

            Metal::ViewportDesc newViewport(0, 0, float(blurBufferWidth), float(blurBufferHeight), 0.f, 1.f);
            context.Bind(newViewport);
            context.Bind(MakeResourceList(resources._blurBufferRTV[1]), nullptr);
            context.Bind(resources._noBlending);
            SetupVertexGeneratorShader(context);

            auto depths = Metal::ExtractResource<ID3D::Resource>(savedTargets.GetDepthStencilView());
            Metal::ShaderResourceView depthsSRV(depths.get(), Metal::NativeFormat::R24_UNORM_X8_TYPELESS);
            auto res = Metal::ExtractResource<ID3D::Resource>(savedTargets.GetRenderTargets()[0]);
            Metal::ShaderResourceView inputSRV(res.get());

            const Metal::ShaderResourceView* blurSrvs[] = { &depthsSRV, &inputSRV };
            float blurConstants[4] = { settings._startDistance, settings._endDistance, 0.f, 0.f };
            float filteringWeights[8];
            BuildGaussianFilteringWeights(filteringWeights, settings._blurStdDev, dimof(filteringWeights));
            Metal::ConstantBufferPacket constantBufferPackets[] = { MakeSharedPkt(filteringWeights), MakeSharedPkt(blurConstants) };

                //          blur once horizontally, and once vertically
            ///////////////////////////////////////////////////////////////////////////////////////
            {
                resources._horizontalFilterBinding->Apply(
                    context, 
                    parserContext.GetGlobalUniformsStream(), 
                    Metal::UniformsStream(constantBufferPackets, blurSrvs));
                context.Bind(*resources._horizontalFilter);
                context.Draw(4);
            }
            ///////////////////////////////////////////////////////////////////////////////////////
            {
                context.Bind(MakeResourceList(resources._blurBufferRTV[0]), nullptr);
                const Metal::ShaderResourceView* blurSrvs2[] = { &depthsSRV, &resources._blurBufferSRV[1] };
                resources._verticalFilterBinding->Apply(
                    context, 
                    parserContext.GetGlobalUniformsStream(), 
                    Metal::UniformsStream(constantBufferPackets, blurSrvs2));
                context.Bind(*resources._verticalFilter);
                context.Draw(4);
            }
            ///////////////////////////////////////////////////////////////////////////////////////

                // copied blurred buffer back into main target
                //  bind output rendertarget (but not depth buffer)
            auto savedViewport = savedTargets.GetViewports()[0];
            context.GetUnderlying()->RSSetViewports(1, (D3D11_VIEWPORT*)&savedViewport);
            context.Bind(MakeResourceList(Metal::RenderTargetView(savedTargets.GetRenderTargets()[0])), nullptr);

            if (!savedTargets.GetDepthStencilView()) {
                Throw(::Exceptions::BasicLabel("No depth stencil buffer bound using atmospheric blur render"));
            }

            context.Bind(*resources._integrateDistantBlur);
            const Metal::ShaderResourceView* srvs[] = { &depthsSRV, &resources._blurBufferSRV[0] };
            resources._integrateDistantBlurBinding->Apply(
                context, 
                parserContext.GetGlobalUniformsStream(), 
                Metal::UniformsStream(constantBufferPackets, srvs));
            context.Bind(resources._integrateBlend);
            context.Draw(4);
            context.UnbindPS<Metal::ShaderResourceView>(3, 3);

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

    ToneMapSettingsConstants AsConstants(const ToneMapSettings& settings)
    {
        ToneMapSettingsConstants result;
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
        return result;
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
        props.Add(u("Flags"),                   DefaultGet(Obj, _flags),                    DefaultSet(Obj, _flags));
        props.Add(u("BloomThreshold"),          DefaultGet(Obj, _bloomThreshold),           DefaultSet(Obj, _bloomThreshold));
        props.Add(u("BloomRampingFactor"),      DefaultGet(Obj, _bloomRampingFactor),       DefaultSet(Obj, _bloomRampingFactor));
        props.Add(u("BloomDesaturationFactor"), DefaultGet(Obj, _bloomDesaturationFactor),  DefaultSet(Obj, _bloomDesaturationFactor));
        props.Add(u("SceneKey"),                DefaultGet(Obj, _sceneKey),                 DefaultSet(Obj, _sceneKey));
        props.Add(u("LuminanceMin"),            DefaultGet(Obj, _luminanceMin),             DefaultSet(Obj, _luminanceMin));
        props.Add(u("LuminanceMax"),            DefaultGet(Obj, _luminanceMax),             DefaultSet(Obj, _luminanceMax));
        props.Add(u("WhitePoint"),              DefaultGet(Obj, _whitepoint),               DefaultSet(Obj, _whitepoint));
        props.Add(u("BloomBlurStdDev"),         DefaultGet(Obj, _bloomBlurStdDev),          DefaultSet(Obj, _bloomBlurStdDev));
        props.Add(u("BloomBrightness"),         DefaultGet(Obj, _bloomBrightness),          DefaultSet(Obj, _bloomBrightness));
        props.Add(u("BloomScale"), 
            [](const Obj& obj)              { return SceneEngine::AsPackedColor(obj._bloomColor); },
            [](Obj& obj, unsigned value)    { obj._bloomColor = SceneEngine::AsFloat3Color(value); });
        
        init = true;
    }
    return props;
}
