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
#include "../RenderCore/Metal/DeviceContextImpl.h"
#include "../RenderCore/RenderUtils.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/DataPacket.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../ConsoleRig/Console.h"
#include "../Utility/BitUtils.h"
#include "../Utility/ParameterBox.h"
#include "../Utility/StringFormat.h"

#include "../RenderCore/DX11/Metal/DX11Utils.h"

#pragma warning(disable:4127)       // conditional expression is constant

namespace SceneEngine
{
    using namespace RenderCore;
    using namespace RenderCore::Metal;

    ColorGradingSettings        DefaultColorGradingSettings();
    extern ColorGradingSettings GlobalColorGradingSettings = DefaultColorGradingSettings();

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
        intrusive_ptr<ID3D::Resource>              _bloomBuffer;
        RenderCore::Metal::UnorderedAccessView  _bloomBufferUAV;
        RenderCore::Metal::RenderTargetView     _bloomBufferRTV;
        RenderCore::Metal::ShaderResourceView   _bloomBufferSRV;
        unsigned                                _width, _height;

        BloomStepBuffer(unsigned width, unsigned height, RenderCore::Metal::NativeFormat::Enum format);
        BloomStepBuffer();
        ~BloomStepBuffer();
    };

    BloomStepBuffer::BloomStepBuffer(unsigned width, unsigned height, RenderCore::Metal::NativeFormat::Enum format)
    {
        using namespace BufferUploads;
        auto& uploads = GetBufferUploads();

        auto basicLuminanceBufferDesc = BuildRenderTargetDesc(
            BindFlag::ShaderResource|BindFlag::UnorderedAccess|BindFlag::RenderTarget,
            BufferUploads::TextureDesc::Plain2D(width, height, AsDXGIFormat(format)),
            "BloomLumin");
        auto bloomBuffer = uploads.Transaction_Immediate(basicLuminanceBufferDesc)->AdoptUnderlying();

        RenderCore::Metal::UnorderedAccessView  bloomBufferUAV(bloomBuffer.get());
        RenderCore::Metal::RenderTargetView     bloomBufferRTV(bloomBuffer.get());
        RenderCore::Metal::ShaderResourceView   bloomBufferSRV(bloomBuffer.get());

        _bloomBuffer    = std::move(bloomBuffer);
        _bloomBufferUAV = std::move(bloomBufferUAV);
        _bloomBufferRTV = std::move(bloomBufferRTV);
        _bloomBufferSRV = std::move(bloomBufferSRV);
        _width          = width;
        _height         = height;
    }

    BloomStepBuffer::BloomStepBuffer()  {}
    BloomStepBuffer::~BloomStepBuffer() {}

    class ToneMappingResources
    {
    public:
        class Desc
        {
        public:
            Desc(unsigned width, unsigned height, RenderCore::Metal::NativeFormat::Enum bloomBufferFormat, unsigned sampleCount, bool useMSAASamplers)
            {
                std::fill((byte*)this, (byte*)PtrAdd(this, sizeof(*this)), 0);
                _width = width;
                _height = height;
                _bloomBufferFormat = bloomBufferFormat;
                _sampleCount = sampleCount;
                _useMSAASamplers = useMSAASamplers;
            }
            unsigned _width, _height, _sampleCount;
            bool _useMSAASamplers;
            RenderCore::Metal::NativeFormat::Enum _bloomBufferFormat;
        };

        std::vector<intrusive_ptr<ID3D::Resource>>             _luminanceBuffers;
        std::vector<RenderCore::Metal::UnorderedAccessView> _luminanceBufferUAV;
        std::vector<RenderCore::Metal::ShaderResourceView>  _luminanceBufferSRV;

        intrusive_ptr<ID3D::Resource>                          _propertiesBuffer;
        RenderCore::Metal::UnorderedAccessView              _propertiesBufferUAV;
        RenderCore::Metal::ShaderResourceView               _propertiesBufferSRV;

        std::vector<BloomStepBuffer>                        _bloomBuffers;
        BloomStepBuffer                                     _bloomTempBuffer;

        const RenderCore::Metal::ComputeShader*		_sampleInitialLuminance;
        const RenderCore::Metal::ComputeShader*		_luminanceStepDown;
        const RenderCore::Metal::ComputeShader*		_updateOverallLuminance;
        const RenderCore::Metal::ComputeShader*		_brightPassStepDown;

        unsigned _firstStepWidth;
        unsigned _firstStepHeight;
        bool _calculateInputsSucceeded;

        const std::shared_ptr<::Assets::DependencyValidation>&  GetDependencyValidation() const   { return _validationCallback; }

        ToneMappingResources(const Desc& desc);
        ~ToneMappingResources();

    private:
        std::shared_ptr<::Assets::DependencyValidation>  _validationCallback;
    };

    ToneMappingResources::ToneMappingResources(const Desc& desc)
    {
        using namespace BufferUploads;
        auto& uploads = GetBufferUploads();

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

        auto basicLuminanceBufferDesc = BuildRenderTargetDesc(
            BindFlag::ShaderResource|BindFlag::UnorderedAccess,
            BufferUploads::TextureDesc::Plain2D(1,1, AsDXGIFormat(NativeFormat::R16_FLOAT)),
            "Luminance");
        std::vector<intrusive_ptr<ID3D::Resource>> luminanceBuffers;
        std::vector<UnorderedAccessView> luminanceBuffersUAV;
        std::vector<ShaderResourceView> luminanceBuffersSRV;
        luminanceBuffers.reserve(firstStep);
        luminanceBuffersUAV.reserve(firstStep);
        luminanceBuffersSRV.reserve(firstStep);

            //  (we could also use 2 buffers, and just use increasingly small corner
            //  areas of the texture)
        for (signed s=firstStep; s>XlAbs(heightDifference); --s) {
            basicLuminanceBufferDesc._textureDesc._width    = 1<<s;
            basicLuminanceBufferDesc._textureDesc._height   = 1<<(s+heightDifference);
            auto newBuffer = uploads.Transaction_Immediate(basicLuminanceBufferDesc)->AdoptUnderlying();
            luminanceBuffers.push_back(newBuffer);
            luminanceBuffersUAV.push_back(UnorderedAccessView(newBuffer.get()));
            luminanceBuffersSRV.push_back(ShaderResourceView(newBuffer.get()));
        }

            //  We need many bloom buffers as well... start at the same size as the biggest luminance
            //  buffer and work down to 32x32
        std::vector<BloomStepBuffer> bloomBuffers;
        bloomBuffers.reserve(std::max(1,firstStep-(4+XlAbs(heightDifference))));
        bloomBuffers.push_back(BloomStepBuffer(1<<firstStep, 1<<(firstStep+heightDifference), desc._bloomBufferFormat));
        BloomStepBuffer bloomTempBuffer(1<<firstStep, 1<<(firstStep+heightDifference), desc._bloomBufferFormat);
        for (signed s=firstStep-1; s>(4+XlAbs(heightDifference)); --s) {
            bloomBuffers.push_back(BloomStepBuffer(1<<s, 1<<(s+heightDifference), desc._bloomBufferFormat));
        }
    
        BufferDesc structuredBufferDesc;
        structuredBufferDesc._type = BufferDesc::Type::LinearBuffer;
        structuredBufferDesc._bindFlags = BindFlag::ShaderResource|BindFlag::StructuredBuffer|BindFlag::UnorderedAccess;
        structuredBufferDesc._cpuAccess = 0;
        structuredBufferDesc._gpuAccess = GPUAccess::Write;
        structuredBufferDesc._allocationRules = 0;
        structuredBufferDesc._linearBufferDesc._sizeInBytes = 8;
        structuredBufferDesc._linearBufferDesc._structureByteSize = 8;
        float initialData[] = { 1.f, 1.f };
        auto propertiesBuffer = uploads.Transaction_Immediate(
            structuredBufferDesc, 
            BufferUploads::CreateBasicPacket(sizeof(initialData), &initialData).get())->AdoptUnderlying();

        UnorderedAccessView propertiesBufferUAV(propertiesBuffer.get());
        ShaderResourceView propertiesBufferSRV(propertiesBuffer.get());

        char shaderDefines[256]; 
        sprintf_s(
            shaderDefines, dimof(shaderDefines), "MSAA_SAMPLES=%i%s", 
            desc._sampleCount, desc._useMSAASamplers?";MSAA_SAMPLERS=1":"");

        auto* sampleInitialLuminance    = &::Assets::GetAssetDep<ComputeShader>("game/xleres/postprocess/hdrluminance.csh:SampleInitialLuminance:cs_*", shaderDefines);
        auto* luminanceStepDown         = &::Assets::GetAssetDep<ComputeShader>("game/xleres/postprocess/hdrluminance.csh:LuminanceStepDown:cs_*");
        auto* updateOverallLuminance    = &::Assets::GetAssetDep<ComputeShader>("game/xleres/postprocess/hdrluminance.csh:UpdateOverallLuminance:cs_*");
        auto* brightPassStepDown        = &::Assets::GetAssetDep<ComputeShader>("game/xleres/postprocess/hdrluminance.csh:BrightPassStepDown:cs_*");

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(_validationCallback, sampleInitialLuminance->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_validationCallback, luminanceStepDown->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_validationCallback, updateOverallLuminance->GetDependencyValidation());

        _luminanceBuffers       = std::move(luminanceBuffers);
        _luminanceBufferUAV     = std::move(luminanceBuffersUAV);
        _luminanceBufferSRV     = std::move(luminanceBuffersSRV);

        _propertiesBuffer       = std::move(propertiesBuffer);
        _propertiesBufferUAV    = std::move(propertiesBufferUAV);
        _propertiesBufferSRV    = std::move(propertiesBufferSRV);

        _bloomBuffers           = std::move(bloomBuffers);
        _bloomTempBuffer        = std::move(bloomTempBuffer);

        _sampleInitialLuminance = sampleInitialLuminance;
        _luminanceStepDown      = luminanceStepDown;
        _brightPassStepDown     = brightPassStepDown;
        _updateOverallLuminance = updateOverallLuminance;
        _firstStepWidth         = 1<<firstStep;
        _firstStepHeight        = 1<<(firstStep+heightDifference);
        _calculateInputsSucceeded = false;
    }

    ToneMappingResources::~ToneMappingResources()
    {}

    static void    ToneMapping_DrawDebugging(   RenderCore::Metal::DeviceContext& context,
                                                ToneMappingResources& resources);

        //////////////////////////////////////////////////////////////////////////////
            //      T O N E   M A P P I N G   I N P U T S                       //
        //////////////////////////////////////////////////////////////////////////////

    void ToneMapping_CalculateInputs(   RenderCore::Metal::DeviceContext* context,
                                        LightingParserContext& parserContext,
                                        ToneMappingResources& resources, 
                                        RenderCore::Metal::ShaderResourceView& sourceTexture, 
                                        RenderCore::Metal::ShaderResourceView* sampleFrequencyMap)
    {
            //
            //      First step in tonemapping is always to calculate the overall
            //      scene brightness. We do this in a series of step-down operations,
            //      each time reducing the image by a power of two.
            //
            //      We might use a log-average method for this, but we have to
            //      be careful about floating point accuracy.
            //
        SavedTargets savedTargets(*context);
        context->Bind(ResourceList<RenderTargetView, 0>(), nullptr);

        TRY {
            static unsigned frameIndex = 0; ++frameIndex;
            struct LuminanceConstants
            {
                int     _frameIndex;
                int     _totalSampleCount;
                float   _elapsedTime;
                int     _buffer;
            } luminanceConstants = { frameIndex, resources._firstStepWidth*resources._firstStepHeight, 1.0f/60.f, 0 };

            auto toneMapSettings = parserContext.GetSceneParser()->GetToneMapSettings();;
            auto toneMapConstants = AsConstants(toneMapSettings);
            context->BindCS(MakeResourceList(
                ConstantBuffer(&toneMapConstants, sizeof(toneMapConstants)),
                ConstantBuffer(&luminanceConstants, sizeof(luminanceConstants))));

            assert(!resources._luminanceBufferUAV.empty());
            assert(!resources._luminanceBufferSRV.empty());

                //
                //      Initial sample from the source texture first,
                //      then make a number of down-step operations.
                // 
                //      The first step is not MSAA-aware. Only the 0th
                //      sample is considered. We could also
                //
            context->Bind(*resources._sampleInitialLuminance);
            context->BindCS(MakeResourceList(sourceTexture));
            context->BindCS(MakeResourceList(resources._luminanceBufferUAV[0], resources._bloomBuffers[0]._bloomBufferUAV, resources._propertiesBufferUAV));
            context->Dispatch(resources._firstStepWidth/16, resources._firstStepHeight/16);
            context->UnbindCS<UnorderedAccessView>(0, 2);

            context->Bind(*resources._luminanceStepDown);
            for (size_t c=1; c<resources._luminanceBufferUAV.size(); ++c) {
                context->BindCS(MakeResourceList(resources._luminanceBufferSRV[c-1]));
                context->BindCS(MakeResourceList(resources._luminanceBufferUAV[c]));
                context->Dispatch(std::max(1u, (resources._firstStepWidth>>c)/16), std::max(1u, (resources._firstStepHeight>>c)/16));
                context->UnbindCS<UnorderedAccessView>(0, 1);
            }

            context->Bind(*resources._brightPassStepDown);
            for (size_t c=1; c<resources._bloomBuffers.size(); ++c) {
                context->BindCS(MakeResourceList(resources._bloomBuffers[c-1]._bloomBufferSRV));
                context->BindCS(MakeResourceList(1, resources._bloomBuffers[c]._bloomBufferUAV));
                context->Dispatch(std::max(1u, (resources._firstStepWidth>>c)/16), std::max(1u, (resources._firstStepHeight>>c)/16));
                context->UnbindCS<UnorderedAccessView>(1, 1);
            }

                //
                //      After we've down all of the downsample steps, we should have
                //      a 2x2 texture -- these are our final samples.
                //      Get the final luminance values, and balance it with the previous
                //      frame's values to produce the new value for this frame...
                //

            context->BindCS(MakeResourceList(resources._luminanceBufferSRV[resources._luminanceBufferSRV.size()-1]));
            context->Bind(*resources._updateOverallLuminance);
            context->Dispatch(1);
            context->UnbindCS<UnorderedAccessView>(0, 3);

                //
                //      We need to do a series of Gaussian blurs over the bloom buffers
                //      We should have a number of bloom buffers, each at different 
                //      resolutions...
                //      We blur each one, and add it to the next higher resolution one
                //

            if (toneMapSettings._flags & ToneMapSettings::Flags::EnableBloom) {
                float filteringWeights[12];
                XlSetMemory(filteringWeights, 0, sizeof(filteringWeights));
                BuildGaussianFilteringWeights(filteringWeights, toneMapSettings._bloomBlurStdDev, 11);
                context->BindPS(MakeResourceList(Metal::ConstantBuffer(filteringWeights, sizeof(filteringWeights))));

                context->Bind(Techniques::CommonResources()._dssDisable);

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
                context->Bind(Topology::TriangleStrip);
            
                for (auto i=resources._bloomBuffers.crbegin(); ;) {

                    ViewportDesc newViewport(0, 0, (float)i->_width, (float)i->_height, 0.f, 1.f);
                    context->Bind(newViewport);

                    Float4 clampingWindow(0.f, 0.f, (float)i->_width - 1.f, (float)i->_height - 1.f);
                    context->BindPS(MakeResourceList(1, Metal::ConstantBuffer(&clampingWindow, sizeof(clampingWindow))));

                    context->Bind(Techniques::CommonResources()._blendOpaque);
                    context->Bind(MakeResourceList(resources._bloomTempBuffer._bloomBufferRTV), nullptr);
                    context->BindPS(MakeResourceList(i->_bloomBufferSRV));
                    context->Bind(horizBlur);
                    context->Draw(4);
            
                    context->UnbindPS<ShaderResourceView>(0, 1);
                    context->Bind(MakeResourceList(i->_bloomBufferRTV), nullptr);
                    context->BindPS(MakeResourceList(resources._bloomTempBuffer._bloomBufferSRV));
                    context->Bind(vertBlur);
                    context->Draw(4);
                    context->UnbindPS<ShaderResourceView>(0, 1);

                    auto oldi = i;
                    ++i;
                    if (i==resources._bloomBuffers.crend()) {
                        break;
                    }

                        // offset viewport by half a pixel -- this will make the bilinear filter catch the right pixels
                    ViewportDesc newViewport2(-.5f / float(i->_width), -.5f / float(i->_height), float(i->_width) -.5f / float(i->_width), float(i->_height) -.5f / float(i->_height), 0.f, 1.f);
                    context->Bind(newViewport2);

                        // simple copy step
                    context->Bind(Metal::BlendState(Metal::BlendOp::Add, Metal::Blend::One, Metal::Blend::One));
                    context->Bind(MakeResourceList(i->_bloomBufferRTV), nullptr);
                    context->BindPS(MakeResourceList(oldi->_bloomBufferSRV));
                    context->Bind(copyShader);
                    context->Draw(4);
                    context->UnbindPS<ShaderResourceView>(0, 1);
                    context->Bind(ResourceList<RenderTargetView, 0>(), nullptr);

                }
            }
            resources._calculateInputsSucceeded = true;
        } 
        CATCH(const ::Assets::Exceptions::InvalidAsset& e) { parserContext.Process(e); resources._calculateInputsSucceeded = false; }
        CATCH(const ::Assets::Exceptions::PendingAsset& e) { parserContext.Process(e); resources._calculateInputsSucceeded = false; }
        CATCH(...) { resources._calculateInputsSucceeded = false; }
        CATCH_END

        savedTargets.ResetToOldTargets(*context);
    }

    ToneMappingResources& GetResources(ShaderResourceView& inputResource, int sampleCount)
    {
            //  Before MSAA resolve, we must setup the tone mapping (first 
            //  step is to sample the luminance)
            //      We have a number of different options for the bloom buffer format.
            //      Generally, R8G8B8A8_UNORM doesn't have enough precision. With all
            //      of the blending and blurring, we tend to get banding.
            //      Try to use a higher precision 32 bit format instead.
        const unsigned bloomBufferFormatType = Tweakable("ToneMapBloomBufferFormat", 3);
        NativeFormat::Enum bloomBufferFormat = NativeFormat::R10G10B10A2_UNORM;
        if (bloomBufferFormatType==1) {
            bloomBufferFormat = NativeFormat::R8G8B8A8_UNORM;
        } else if (bloomBufferFormatType==2) {
            bloomBufferFormat = NativeFormat::R16G16B16A16_FLOAT;
        } else if (bloomBufferFormatType==3) {
            bloomBufferFormat = NativeFormat::R11G11B10_FLOAT;
        } else if (bloomBufferFormatType==4) {
            bloomBufferFormat = NativeFormat::R16_FLOAT;
        }
        TextureDesc2D desc(inputResource.GetUnderlying());
        ToneMappingResources& toneMapRes = Techniques::FindCachedBoxDep<ToneMappingResources>(
            ToneMappingResources::Desc( desc.Width, desc.Height, 
                                        bloomBufferFormat, sampleCount, sampleCount>1));
        return toneMapRes;
    }

    void ToneMap_SampleLuminance(DeviceContext* context, LightingParserContext& parserContext, ShaderResourceView& inputResource, int sampleCount)
    {
        if (Tweakable("DoToneMap", true)) {
            auto& toneMapRes = GetResources(inputResource, sampleCount);
            ToneMapping_CalculateInputs(context, parserContext, toneMapRes, inputResource, nullptr);
        }
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
            bool _hardwareSRGBDisabled, _doColorGrading, _doLevelsAdjustment, _doSelectiveColour, _doFilterColour;
            Desc(unsigned opr, bool enableBloom, bool hardwareSRGBDisabled, bool doColorGrading, bool doLevelsAdjustments, bool doSelectiveColour, bool doFilterColour)
                : _operator(opr), _enableBloom(enableBloom), _hardwareSRGBDisabled(hardwareSRGBDisabled), _doColorGrading(doColorGrading)
                , _doLevelsAdjustment(doLevelsAdjustments), _doSelectiveColour(doSelectiveColour), _doFilterColour(doFilterColour) {}
        };

        const RenderCore::Metal::ShaderProgram* _shaderProgram;
        RenderCore::Metal::BoundUniforms _uniforms;

        ToneMapShaderBox(const Desc& descs);

        const std::shared_ptr<::Assets::DependencyValidation>&  GetDependencyValidation() const   { return _validationCallback; }
    private:
        std::shared_ptr<::Assets::DependencyValidation>  _validationCallback;
    };

    ToneMapShaderBox::ToneMapShaderBox(const Desc& desc)
    {
        char shaderDefines[256];
        sprintf_s(
            shaderDefines, dimof(shaderDefines), 
            "OPERATOR=%i;ENABLE_BLOOM=%i;HARDWARE_SRGB_DISABLED=%i;DO_COLOR_GRADING=%i;MAT_LEVELS_ADJUSTMENT=%i;MAT_SELECTIVE_COLOR=%i;MAT_PHOTO_FILTER=%i",
            Tweakable("ToneMapOperator", 1), desc._enableBloom, desc._hardwareSRGBDisabled, desc._doColorGrading, desc._doLevelsAdjustment, desc._doSelectiveColour, desc._doFilterColour);
        auto& shaderProgram = ::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*",
            "game/xleres/postprocess/tonemap.psh:main:ps_*", 
            shaderDefines);

        RenderCore::Metal::BoundUniforms uniforms(shaderProgram);
        uniforms.BindConstantBuffer(Hash64("ToneMapSettings"), 0, 1);
        uniforms.BindConstantBuffer(Hash64("ColorGradingSettings"), 1, 1);

        auto validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(validationCallback, shaderProgram.GetDependencyValidation());

        _shaderProgram = &shaderProgram;
        _uniforms = std::move(uniforms);
        _validationCallback = std::move(validationCallback);
    }

    void ToneMap_Execute(DeviceContext* context, LightingParserContext& parserContext, ShaderResourceView& inputResource, int sampleCount)
    {
        SavedBlendAndRasterizerState savedStates(*context);
        context->Bind(Techniques::CommonResources()._cullDisable);

        bool hardwareSRGBDisabled = false;
        {
                    //  Query the destination render target to
                    //  see if SRGB conversion is enabled when writing out
            SavedTargets destinationTargets(*context);
            D3D11_RENDER_TARGET_VIEW_DESC rtv;
            if (destinationTargets.GetRenderTargets()[0]) {
                destinationTargets.GetRenderTargets()[0]->GetDesc(&rtv);
                hardwareSRGBDisabled = rtv.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            }
        }

        CATCH_ASSETS_BEGIN
            context->BindPS(MakeResourceList(inputResource));
            SetupVertexGeneratorShader(*context);
            context->Bind(Techniques::CommonResources()._blendOpaque);
            bool bindCopyShader = true;
            auto settings = parserContext.GetSceneParser()->GetToneMapSettings();
            if (Tweakable("DoToneMap", true) && (settings._flags & ToneMapSettings::Flags::EnableToneMap)) {
                auto& toneMapRes = GetResources(inputResource, sampleCount);
                if (toneMapRes._calculateInputsSucceeded) {
                        //  Bind a pixel shader that will do the tonemap operation
                        //  we do color grading operation in the same step... so make sure
                        //  we apply the correct color grading parameters a the same time.
                    TRY {

                        bool doColorGrading = Tweakable("DoColorGrading", false);
                        auto colorGradingSettings = BuildColorGradingShaderConstants(GlobalColorGradingSettings);

                        auto& box = Techniques::FindCachedBoxDep<ToneMapShaderBox>(
                            ToneMapShaderBox::Desc(
                                Tweakable("ToneMapOperator", 1), !!(settings._flags & ToneMapSettings::Flags::EnableBloom),
                                hardwareSRGBDisabled, doColorGrading, !!(colorGradingSettings._doLevelsAdustment), 
                                !!(colorGradingSettings._doSelectiveColor), !!(colorGradingSettings._doFilterColor)));

                        context->Bind(*box._shaderProgram);
                        context->BindPS(MakeResourceList(1, toneMapRes._propertiesBufferSRV, toneMapRes._bloomBuffers[0]._bloomBufferSRV));
                        bindCopyShader = false;

                        const RenderCore::SharedPkt cbs[] = { 
                            MakeSharedPkt(AsConstants(settings)), 
                            MakeSharedPkt(colorGradingSettings) 
                        };
                        box._uniforms.Apply( *context, UniformsStream(), UniformsStream(cbs, nullptr, dimof(cbs)));

                        if (Tweakable("ToneMapDebugging", false)) {
                            parserContext._pendingOverlays.push_back(
                                std::bind(&ToneMapping_DrawDebugging, std::placeholders::_1, std::ref(toneMapRes)));
                        }

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
                context->Bind(::Assets::GetAssetDep<Metal::ShaderProgram>(
                    "game/xleres/basic2D.vsh:fullscreen:vs_*", "game/xleres/basic.psh:fake_tonemap:ps_*"));
            }
            context->Draw(4);
        CATCH_ASSETS_END(parserContext)

        savedStates.ResetToOldStates(*context);
    }

    static void    ToneMapping_DrawDebugging(   RenderCore::Metal::DeviceContext& context,
                                                ToneMappingResources& resources)
    {
        SetupVertexGeneratorShader(context);
        context.Bind(BlendState(BlendOp::Add, Blend::One, Blend::InvSrcAlpha));
        context.BindPS(MakeResourceList(resources._propertiesBufferSRV));
        for (unsigned c=0; c<std::min(size_t(3),resources._luminanceBufferSRV.size()); ++c) {
            context.BindPS(MakeResourceList(1+c, resources._luminanceBufferSRV[c]));
        }
        for (unsigned c=0; c<std::min(size_t(3),resources._bloomBuffers.size()); ++c) {
            context.BindPS(MakeResourceList(4+c, resources._bloomBuffers[c]._bloomBufferSRV));
        }
        context.Bind(::Assets::GetAssetDep<ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*", "game/xleres/postprocess/debugging.psh:HDRDebugging:ps_*"));
        context.Draw(4);

        context.Bind(::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/postprocess/debugging.psh:LuminanceValue:vs_*", 
            "game/xleres/utility/metricsrender.gsh:main:gs_*",
            "game/xleres/utility/metricsrender.psh:main:ps_*", ""));
        ViewportDesc mainViewportDesc(context);
        unsigned dimensions[4] = { (unsigned)mainViewportDesc.Width, (unsigned)mainViewportDesc.Height, 0, 0 };
        context.BindGS(MakeResourceList(ConstantBuffer(dimensions, sizeof(dimensions))));
        context.BindVS(MakeResourceList(resources._propertiesBufferSRV));
        context.BindPS(MakeResourceList(3, ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("game/xleres/DefaultResources/metricsdigits.dds:T").GetShaderResource()));
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
            NativeFormat::Enum _format;
            Desc(unsigned width, unsigned height, NativeFormat::Enum format);
        };

        AtmosphereBlurResources(const Desc&);
        ~AtmosphereBlurResources();

        intrusive_ptr<ID3D::Resource>           _blurBuffer[2];
        RenderCore::Metal::RenderTargetView     _blurBufferRTV[2];
        RenderCore::Metal::ShaderResourceView   _blurBufferSRV[2];

        const RenderCore::Metal::ShaderProgram*       _horizontalFilter;
        const RenderCore::Metal::ShaderProgram*       _verticalFilter;
        std::unique_ptr<RenderCore::Metal::BoundUniforms>   _horizontalFilterBinding;
        std::unique_ptr<RenderCore::Metal::BoundUniforms>   _verticalFilterBinding;

        RenderCore::Metal::BlendState           _integrateBlend;
        RenderCore::Metal::BlendState           _noBlending;

        const RenderCore::Metal::ShaderProgram*       _integrateDistantBlur;
        std::unique_ptr<RenderCore::Metal::BoundUniforms>   _integrateDistantBlurBinding;

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const   { return _validationCallback; }
    private:
        std::shared_ptr<::Assets::DependencyValidation>  _validationCallback;
    };

    AtmosphereBlurResources::Desc::Desc(unsigned width, unsigned height, NativeFormat::Enum format)
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

        RenderCore::Metal::RenderTargetView     bloomBufferRTV0(bloomBuffer0.get());
        RenderCore::Metal::ShaderResourceView   bloomBufferSRV0(bloomBuffer0.get());
        RenderCore::Metal::RenderTargetView     bloomBufferRTV1(bloomBuffer1.get());
        RenderCore::Metal::ShaderResourceView   bloomBufferSRV1(bloomBuffer1.get());

        auto* horizontalFilter = &::Assets::GetAssetDep<ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*", 
            "game/xleres/Effects/distantblur.psh:HorizontalBlur_DistanceWeighted:ps_*");
        auto* verticalFilter = &::Assets::GetAssetDep<ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*", 
            "game/xleres/Effects/distantblur.psh:VerticalBlur_DistanceWeighted:ps_*");

        auto horizontalFilterBinding = std::make_unique<BoundUniforms>(std::ref(*horizontalFilter));
        horizontalFilterBinding->BindConstantBuffers(1, {"Constants", "BlurConstants"});
        horizontalFilterBinding->BindShaderResources(1, {"DepthsInput", "InputTexture"});

        auto verticalFilterBinding = std::make_unique<BoundUniforms>(std::ref(*verticalFilter));
        verticalFilterBinding->BindConstantBuffers(1, {"Constants", "BlurConstants"});
        verticalFilterBinding->BindShaderResources(1, {"DepthsInput", "InputTexture"});

        auto* integrateDistantBlur = &::Assets::GetAssetDep<ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*", 
            "game/xleres/Effects/distantblur.psh:integrate:ps_*");
        auto integrateDistantBlurBinding = std::make_unique<BoundUniforms>(std::ref(*integrateDistantBlur));
        Techniques::TechniqueContext::BindGlobalUniforms(*integrateDistantBlurBinding.get());
        integrateDistantBlurBinding->BindConstantBuffers(1, {"Constants", "BlurConstants"});
        integrateDistantBlurBinding->BindShaderResource(Hash64("DepthsInput"), 0, 1);
        integrateDistantBlurBinding->BindShaderResource(Hash64("BlurredBufferInput"), 1, 1);

        RenderCore::Metal::BlendState integrateBlend;
        RenderCore::Metal::BlendState noBlending = RenderCore::Metal::BlendOp::NoBlending;

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
        RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext,
        const AtmosphereBlurSettings& settings)
    {
            //  simple distance blur for the main camera
            //  sometimes called depth-of-field; but really it's blurring of distant objects
            //  caused by the atmosphere. Depth of field is an artifact that occurs in lens.

        using namespace RenderCore::Metal;
        ViewportDesc viewport(*context);
        SavedTargets savedTargets(*context);

        TRY {

                //  We can actually drop down to 1/3 resolution here, fine... but then
                //  the blurring becomes too much
                //  We need to use a format with an alpha channel for the weighted blur.
                //  
            unsigned blurBufferWidth = unsigned(viewport.Width/2);
            unsigned blurBufferHeight = unsigned(viewport.Height/2);
            auto& resources = Techniques::FindCachedBoxDep<AtmosphereBlurResources>(
                AtmosphereBlurResources::Desc(blurBufferWidth, blurBufferHeight, NativeFormat::R16G16B16A16_FLOAT));

            ViewportDesc newViewport(0, 0, float(blurBufferWidth), float(blurBufferHeight), 0.f, 1.f);
            context->Bind(newViewport);
            context->Bind(MakeResourceList(resources._blurBufferRTV[1]), nullptr);
            context->Bind(resources._noBlending);
            SetupVertexGeneratorShader(*context);

            auto depths = ExtractResource<ID3D::Resource>(savedTargets.GetDepthStencilView());
            ShaderResourceView depthsSRV(depths.get(), (RenderCore::Metal::NativeFormat::Enum)DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
            auto res = ExtractResource<ID3D::Resource>(savedTargets.GetRenderTargets()[0]);
            ShaderResourceView inputSRV(res.get());

            const ShaderResourceView* blurSrvs[] = { &depthsSRV, &inputSRV };
            float blurConstants[4] = { settings._startDistance, settings._endDistance, 0.f, 0.f };
            float filteringWeights[8];
            BuildGaussianFilteringWeights(filteringWeights, settings._blurStdDev, dimof(filteringWeights));
            ConstantBufferPacket constantBufferPackets[] = { MakeSharedPkt(filteringWeights), MakeSharedPkt(blurConstants) };

                //          blur once horizontally, and once vertically
            ///////////////////////////////////////////////////////////////////////////////////////
            {
                resources._horizontalFilterBinding->Apply(
                    *context, 
                    parserContext.GetGlobalUniformsStream(), 
                    UniformsStream(constantBufferPackets, blurSrvs));
                context->Bind(*resources._horizontalFilter);
                context->Draw(4);
            }
            ///////////////////////////////////////////////////////////////////////////////////////
            {
                context->Bind(MakeResourceList(resources._blurBufferRTV[0]), nullptr);
                const ShaderResourceView* blurSrvs2[] = { &depthsSRV, &resources._blurBufferSRV[1] };
                resources._verticalFilterBinding->Apply(
                    *context, 
                    parserContext.GetGlobalUniformsStream(), 
                    UniformsStream(constantBufferPackets, blurSrvs2));
                context->Bind(*resources._verticalFilter);
                context->Draw(4);
            }
            ///////////////////////////////////////////////////////////////////////////////////////

                // copied blurred buffer back into main target
                //  bind output rendertarget (but not depth buffer)
            auto savedViewport = savedTargets.GetViewports()[0];
            context->GetUnderlying()->RSSetViewports(1, (D3D11_VIEWPORT*)&savedViewport);
            context->Bind(MakeResourceList(RenderTargetView(savedTargets.GetRenderTargets()[0])), nullptr);

            if (!savedTargets.GetDepthStencilView()) {
                Throw(::Exceptions::BasicLabel("No depth stencil buffer bound using atmospheric blur render"));
            }

            context->Bind(*resources._integrateDistantBlur);
            const ShaderResourceView* srvs[] = { &depthsSRV, &resources._blurBufferSRV[0] };
            resources._integrateDistantBlurBinding->Apply(
                *context, 
                parserContext.GetGlobalUniformsStream(), 
                UniformsStream(constantBufferPackets, srvs));
            context->Bind(resources._integrateBlend);
            context->Draw(4);
            context->UnbindPS<ShaderResourceView>(3, 3);

        } CATCH(...) {
        } CATCH_END

        savedTargets.ResetToOldTargets(*context);    //(also resets viewport)
    }

        //////////////////////////////////////////////////////////////////////////////
            //      C O N F I G U R A T I O N   S E T T I N G S                 //
        //////////////////////////////////////////////////////////////////////////////

    ToneMapSettings DefaultToneMapSettings()
    {
        ToneMapSettings result;
        result._flags = ToneMapSettings::Flags::EnableToneMap | ToneMapSettings::Flags::EnableBloom;
        result._bloomScale = Float3(19.087036f, 11.582731f, 6.6070509f);
        result._bloomThreshold = 10.f;
        result._bloomRampingFactor = .8f;
        result._bloomDesaturationFactor = .6f;
        result._sceneKey = .23f;
        result._luminanceMin = 0.06f;
        result._luminanceMax = 3.f;
        result._whitepoint = 8.f;
        result._bloomBlurStdDev = 1.32f;
        return result;
    }

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
        result._bloomScale = settings._bloomScale;
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

    ToneMapSettings::ToneMapSettings() {}

    ToneMapSettings::ToneMapSettings(const ParameterBox& paramBox)
    {
        auto defaults = DefaultToneMapSettings();

        static const auto bloomScale = ParameterBox::MakeParameterNameHash("BloomScale");
        static const auto bloomThreshold = ParameterBox::MakeParameterNameHash("BloomThreshold");
        static const auto bloomRampingFactor = ParameterBox::MakeParameterNameHash("BloomRampingFactor");
        static const auto bloomDesaturationFactor = ParameterBox::MakeParameterNameHash("BloomDesaturationFactor");
        static const auto sceneKey = ParameterBox::MakeParameterNameHash("SceneKey");
        static const auto luminanceMin = ParameterBox::MakeParameterNameHash("LuminanceMin");
        static const auto luminanceMax = ParameterBox::MakeParameterNameHash("LuminanceMax");
        static const auto whitePoint = ParameterBox::MakeParameterNameHash("WhitePoint");
        static const auto bloomBlurStdDev = ParameterBox::MakeParameterNameHash("BloomBlurStdDev");
        static const auto bloomBrightness = ParameterBox::MakeParameterNameHash("BloomBrightness");
        static const auto flags = ParameterBox::MakeParameterNameHash("Flags");

        auto flagsV = paramBox.GetParameter<unsigned>(flags);
        if (flagsV.first) _flags = flagsV.second;
        else              _flags = defaults._flags;

        auto scale = paramBox.GetParameter<unsigned>(bloomScale);
        if (scale.first)    _bloomScale = AsFloat3Color(scale.second) * paramBox.GetParameter<float>(bloomBrightness, 1.f);
        else                _bloomScale = defaults._bloomScale;

        _bloomThreshold = paramBox.GetParameter<float>(bloomThreshold, defaults._bloomThreshold);
        _bloomRampingFactor = paramBox.GetParameter<float>(bloomRampingFactor, defaults._bloomRampingFactor);
        _bloomDesaturationFactor = paramBox.GetParameter<float>(bloomDesaturationFactor, defaults._bloomDesaturationFactor);
        _sceneKey = paramBox.GetParameter<float>(sceneKey, defaults._sceneKey);
	    _luminanceMin = paramBox.GetParameter<float>(luminanceMin, defaults._luminanceMin);
	    _luminanceMax = paramBox.GetParameter<float>(luminanceMax, defaults._luminanceMax);
	    _whitepoint = paramBox.GetParameter<float>(whitePoint, defaults._whitepoint);
        _bloomBlurStdDev = paramBox.GetParameter<float>(bloomBlurStdDev, defaults._bloomBlurStdDev);
    }

}

