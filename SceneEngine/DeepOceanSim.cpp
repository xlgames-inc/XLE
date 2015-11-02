// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DeepOceanSim.h"
#include "LightingParserContext.h"
#include "SceneParser.h"
#include "SceneEngineUtils.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../BufferUploads/DataPacket.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../ConsoleRig/Console.h"
#include "../Assets/Assets.h"
#include "../Math/Vector.h"
#include "../Utility/StringFormat.h"
#include "../Utility/BitUtils.h"
#include "../Utility/ParameterBox.h"

namespace SceneEngine
{
    using namespace RenderCore;

    class StartingSpectrumBox
    {
    public:
        class Desc
        {
        public:
            unsigned    _width, _height;
            Float2      _physicalDimensions;
            Float2      _windVector;
            float       _scaleAgainstWind;
            float       _suppressionFactor;

            Desc(   unsigned width, unsigned height, const 
                    Float2& physicalDimensions, const Float2& windVector, 
                    float scaleAgainstWind, float suppressionFactor)
            {   
                _width = width; _height = height; _windVector = windVector; 
                _physicalDimensions = physicalDimensions; 
                _scaleAgainstWind = scaleAgainstWind; _suppressionFactor = suppressionFactor; 
            }
        };

        StartingSpectrumBox(const Desc& desc);
        ~StartingSpectrumBox();

        using SRV = RenderCore::Metal::ShaderResourceView;
        using ResLocator = intrusive_ptr<BufferUploads::ResourceLocator>;

        ResLocator  _inputReal;
        ResLocator  _inputImaginary;
        SRV         _inputRealShaderResource;
        SRV         _inputImaginaryShaderResource;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    void DeepOceanSim::Update(
        Metal::DeviceContext* context, LightingParserContext& parserContext, 
        const DeepOceanSimSettings& oceanSettings, unsigned bufferCounter)
    {
        const unsigned dimensions = oceanSettings._gridDimensions;
        const Float2 physicalDimensions = Float2(oceanSettings._physicalDimensions, oceanSettings._physicalDimensions);

        const Float2 calmWindVector = oceanSettings._windVelocity[0] * Float2(XlCos(oceanSettings._windAngle[0]), XlSin(oceanSettings._windAngle[0]));
        const Float2 strongWindVector = oceanSettings._windVelocity[1] * Float2(XlCos(oceanSettings._windAngle[1]), XlSin(oceanSettings._windAngle[1]));

        auto& calmSpectrum = Techniques::FindCachedBox<StartingSpectrumBox>(
            StartingSpectrumBox::Desc(dimensions,dimensions, physicalDimensions, calmWindVector, oceanSettings._scaleAgainstWind[0], oceanSettings._suppressionFactor[0]));
        auto& strongSpectrum = Techniques::FindCachedBox<StartingSpectrumBox>(
            StartingSpectrumBox::Desc(dimensions,dimensions, physicalDimensions, strongWindVector, oceanSettings._scaleAgainstWind[1], oceanSettings._suppressionFactor[1]));
    
        const char* fftDefines = "";
        auto useMirrorOptimisation = Tweakable("OceanUseMirrorOptimisation", true);
        if (useMirrorOptimisation) {
            fftDefines = "USE_MIRROR_OPT=1";
        }
        auto& fft1 = ::Assets::GetAssetDep<Metal::ComputeShader>("game/xleres/Ocean/FFT.csh:FFT2D_1:cs_*");
        auto& fft2 = ::Assets::GetAssetDep<Metal::ComputeShader>("game/xleres/Ocean/FFT.csh:FFT2D_2:cs_*");
        auto& setup = ::Assets::GetAssetDep<Metal::ComputeShader>("game/xleres/Ocean/FFT.csh:Setup:cs_*", fftDefines);

        StringMeld<64> shaderDefines;
        shaderDefines << "DO_FOAM_SIM=" << int(_foamQuantityUAV[0].IsGood());

        auto& buildNormals = ::Assets::GetAssetDep<Metal::ComputeShader>(_useDerivativesMapForNormals ? "game/xleres/Ocean/OceanNormals.csh:BuildDerivatives:cs_*" : "game/xleres/Ocean/OceanNormals.csh:BuildNormals:cs_*", shaderDefines.get());
        auto& buildNormalsMipmaps = ::Assets::GetAssetDep<Metal::ComputeShader>(_useDerivativesMapForNormals ? "game/xleres/Ocean/OceanNormals.csh:BuildDerivativesMipmap:cs_*" : "game/xleres/Ocean/OceanNormals.csh:BuildNormalsMipmap:cs_*", shaderDefines.get());
    
        const float shallowGridPhysicalDimension = Tweakable("OceanShallowPhysicalDimension", 256.f);
        // const float currentTime = parserContext.GetSceneParser()->GetTimeValue();
        auto materialConstants = Internal::BuildOceanMaterialConstants(oceanSettings, shallowGridPhysicalDimension);
        Metal::ConstantBuffer materialConstantBuffer(&materialConstants, sizeof(materialConstants));

        struct GridConstants
        {
            unsigned width; unsigned height;
            float spectrumFade; unsigned dummy;
        } gridConstants = { dimensions, dimensions, oceanSettings._spectrumFade, 0 };
        Metal::ConstantBuffer gridConstantsBuffer(&gridConstants, sizeof(gridConstants));

        Metal::BoundUniforms setupUniforms(::Assets::GetAssetDep<CompiledShaderByteCode>("game/xleres/Ocean/FFT.csh:Setup:cs_*", "DO_INVERSE=0"));
        Techniques::TechniqueContext::BindGlobalUniforms(setupUniforms);
        setupUniforms.BindConstantBuffer(Hash64("OceanGridConstants"), 0, 1);
        setupUniforms.BindConstantBuffer(Hash64("OceanMaterialSettings"), 1, 1);
        
        const Metal::ConstantBuffer* cbs[] = { &gridConstantsBuffer, &materialConstantBuffer };
        setupUniforms.Apply(*context, 
            parserContext.GetGlobalUniformsStream(),
            Metal::UniformsStream(nullptr, cbs, dimof(cbs)));

        context->BindCS(MakeResourceList(
            _workingTextureRealUVA, _workingTextureImaginaryUVA,
            _workingTextureXRealUVA, _workingTextureXImaginaryUVA,
            _workingTextureYRealUVA, _workingTextureYImaginaryUVA));
        context->BindCS(MakeResourceList(
            calmSpectrum._inputRealShaderResource, calmSpectrum._inputImaginaryShaderResource,
            strongSpectrum._inputRealShaderResource, strongSpectrum._inputImaginaryShaderResource));
        context->Bind(setup);
        if (useMirrorOptimisation) {
                    // only do half in X direction. The shader will write two outputs at a time
            context->Dispatch((dimensions + (32-1))/32/2, (dimensions + (32-1))/32);
        } else {
            context->Dispatch((dimensions + (32-1))/32, (dimensions + (32-1))/32);
        }

        context->UnbindCS<Metal::UnorderedAccessView>(0, 6);

            //  Perform FFT transform first on the heights texture, then on the X & Y displacement textures
        context->BindCS(MakeResourceList(_workingTextureRealUVA, _workingTextureImaginaryUVA));
        context->Bind(fft1); context->Dispatch((dimensions + (32-1))/32);
        context->Bind(fft2); context->Dispatch((dimensions + (32-1))/32);

        context->BindCS(MakeResourceList(_workingTextureXRealUVA, _workingTextureXImaginaryUVA));
        context->Bind(fft1); context->Dispatch((dimensions + (32-1))/32);
        context->Bind(fft2); context->Dispatch((dimensions + (32-1))/32);

        context->BindCS(MakeResourceList(_workingTextureYRealUVA, _workingTextureYImaginaryUVA));
        context->Bind(fft1); context->Dispatch((dimensions + (32-1))/32);
        context->Bind(fft2); context->Dispatch((dimensions + (32-1))/32);

        context->UnbindCS<Metal::UnorderedAccessView>(0, 6);

            //  Generate normals using the displacement textures
        if (!_normalsTextureUAV.empty()) {
            Metal::BoundUniforms buildNormalsUniforms(
                ::Assets::GetAssetDep<CompiledShaderByteCode>(
                    _useDerivativesMapForNormals 
                    ? "game/xleres/Ocean/OceanNormals.csh:BuildDerivatives:cs_*" 
                    : "game/xleres/Ocean/OceanNormals.csh:BuildNormals:cs_*",
                    shaderDefines.get()));
            Techniques::TechniqueContext::BindGlobalUniforms(buildNormalsUniforms);
            buildNormalsUniforms.BindConstantBuffer(Hash64("OceanGridConstants"), 0, 1);
            buildNormalsUniforms.BindConstantBuffer(Hash64("OceanMaterialSettings"), 1, 1);
            buildNormalsUniforms.Apply(*context, 
                parserContext.GetGlobalUniformsStream(),
                Metal::UniformsStream(nullptr, cbs, dimof(cbs)));

            context->BindCS(MakeResourceList(
                _workingTextureRealSRV, 
                _workingTextureXRealSRV, 
                _workingTextureYRealSRV,
                _foamQuantitySRV2[(bufferCounter+1)&1]));
            context->BindCS(MakeResourceList(_normalsTextureUAV[0], _foamQuantityUAV[bufferCounter&1]));
            context->Bind(buildNormals); context->Dispatch((dimensions + (32-1))/32, (dimensions + (32-1))/32);
            context->UnbindCS<Metal::UnorderedAccessView>(0, 2);

            context->Bind(buildNormalsMipmaps);
            for (unsigned step = 0; step<_normalsTextureUAV.size()-1; ++step) {
                unsigned mipDims = dimensions >> (step+1);
                unsigned constants[4] = { mipDims, mipDims, 0, 0 };
                context->BindCS(MakeResourceList(Metal::ConstantBuffer(constants, sizeof(constants))));

                context->BindCS(MakeResourceList(4, _normalsSingleMipSRV[step]));
                context->BindCS(MakeResourceList(_normalsTextureUAV[step+1]));
            
                context->Dispatch((mipDims + (32-1))/32, (mipDims + (32-1))/32);
                context->UnbindCS<Metal::UnorderedAccessView>(0, 1);
            }

            context->UnbindCS<Metal::ShaderResourceView>(0, 4);
        }
    }

    void DeepOceanSim::DrawDebugging(   
        RenderCore::Metal::DeviceContext& context, 
        LightingParserContext& parserContext,
        const DeepOceanSimSettings& oceanSettings)
    {
        using namespace RenderCore;

        const unsigned dimensions       = oceanSettings._gridDimensions;
        const Float2 physicalDimensions = Float2(oceanSettings._physicalDimensions, oceanSettings._physicalDimensions);
        const Float2 calmWindVector     = oceanSettings._windVelocity[0] * Float2(XlCos(oceanSettings._windAngle[0]), XlSin(oceanSettings._windAngle[0]));
        const Float2 strongWindVector   = oceanSettings._windVelocity[1] * Float2(XlCos(oceanSettings._windAngle[1]), XlSin(oceanSettings._windAngle[1]));

        auto& calmSpectrum = Techniques::FindCachedBox<StartingSpectrumBox>(
            StartingSpectrumBox::Desc(  dimensions,dimensions, physicalDimensions, calmWindVector, 
                                        oceanSettings._scaleAgainstWind[0], oceanSettings._suppressionFactor[0]));
        auto& strongSpectrum = Techniques::FindCachedBox<StartingSpectrumBox>(
            StartingSpectrumBox::Desc(  dimensions,dimensions, physicalDimensions, strongWindVector, 
                                        oceanSettings._scaleAgainstWind[1], oceanSettings._suppressionFactor[1]));

        SetupVertexGeneratorShader(context);
        context.Bind(Techniques::CommonResources()._blendStraightAlpha);
        context.Bind(::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*", "game/xleres/Ocean/FFTDebugging.psh:main:ps_*"));
        context.BindPS(MakeResourceList(
            _workingTextureRealSRV, _workingTextureImaginarySRV,
            calmSpectrum._inputRealShaderResource, calmSpectrum._inputImaginaryShaderResource,
            strongSpectrum._inputRealShaderResource, strongSpectrum._inputImaginaryShaderResource));
        context.Draw(4);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    namespace Internal
    {
        OceanMaterialConstants BuildOceanMaterialConstants(const DeepOceanSimSettings& oceanSettings, float shallowGridPhysicalDimension)
        {
            const Float2 physicalDimensions = Float2(oceanSettings._physicalDimensions, oceanSettings._physicalDimensions);
            OceanMaterialConstants result = {
                physicalDimensions[0], physicalDimensions[1], 
                oceanSettings._strengthConstantXY, oceanSettings._strengthConstantZ,
                shallowGridPhysicalDimension, oceanSettings._baseHeight, 
                
                oceanSettings._foamThreshold, oceanSettings._foamIncreaseSpeed,
                oceanSettings._foamIncreaseClamp, oceanSettings._foamDecrease,

                0, 0
            };
            return result;
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static std::pair<float, float> RandomGaussian(float variance)
    {
            //  calculate 2 random numbers using the box muller technique
            //  (see http://en.wikipedia.org/wiki/Box%E2%80%93Muller_transform)
            //  One form has a lot of trignometry, another has a loop in it...

        const int method = 1;
        if (constant_expression<method == 0>::result()) {
            return std::make_pair(
                LinearInterpolate(-1.f, 1.f, rand() / float(RAND_MAX)),
                LinearInterpolate(-1.f, 1.f, rand() / float(RAND_MAX)));
        }

        const bool polarMethod = method==1;
        if (constant_expression<polarMethod>::result()) {
            float w;
            float r0, r1;
            do {
                r0 = LinearInterpolate(-1.f, 1.f, rand() / float(RAND_MAX));
                r1 = LinearInterpolate(-1.f, 1.f, rand() / float(RAND_MAX));
                w = r0 * r0 + r1 * r1;
            } while (w >= 1.f);

            float scale = XlSqrt(-2.f * XlLog(w) / w);
            return std::make_pair(r0 * scale, r1 * scale);
        } else {
            float r0 = (rand()+1) / float(RAND_MAX);        // (prevent 0 result)
            r0 = -2.f * XlLog(r0);
            float r1 = 2.f * gPI * rand() / float(RAND_MAX);
            float a = XlSqrt(variance * r0);
            return std::make_pair(a * XlCos(r1), a * XlSin(r1));
        }
    }

    StartingSpectrumBox::StartingSpectrumBox(const Desc& desc) 
    {
        using namespace BufferUploads;
        auto& uploads = GetBufferUploads();

        auto realValues      = std::unique_ptr<float[]>(new float[desc._width*desc._height]);
        auto imaginaryValues = std::unique_ptr<float[]>(new float[desc._width*desc._height]);

            //
            //      Build input to FFT
            //          using Phillip's spectrum, as suggested by Tessendorf (and commonly used)
            //
        const float windVelocity = Magnitude(desc._windVector);
        Float2 windDirection = desc._windVector / windVelocity;
        const float gravitionalConstant = 9.8f;
        const float L = windVelocity * windVelocity / gravitionalConstant;
        const float Lx = desc._physicalDimensions[0], Ly = desc._physicalDimensions[1];     // physical dimensions of the water grid
        const float l = desc._suppressionFactor;
        const float A = 1.f;

        // #define DO_FREQ_BOOST 1
        #if (DO_FREQ_BOOST==1)
            const float freqBoost = 2.f;
        #else
            const float freqBoost = 1.f;
        #endif
                    
        for (unsigned y=0; y<desc._height; ++y) {
            for (unsigned x=0; x<desc._width; ++x) {
                float n = x + .5f - float(desc._width/2);
                float m = y + .5f - float(desc._height/2);

                    //  Actually, I'm not sure if the coefficient here should be 2.f or 4.f
                    //  (because n is a value between -.5f and 5.f). That's what freqBoost is
                    //  for. Even if freqBoost isn't physically accurate, it might help us get
                    //  more high frequency waves.
                Float2 kVector = (freqBoost * 2.f * gPI) * Float2(n / Lx, m / Ly);
                float k = Magnitude(kVector);

                float directionalPart = 1.f;
                float suppressionPart = 1.f; 
                float Ph = 0.f;

                if (n!=0.f || m!=0.f) {
                    directionalPart = Dot(windDirection, kVector) / k;
                    if (directionalPart < 0.f) {
                        directionalPart *= desc._scaleAgainstWind;
                    }
                    directionalPart *= directionalPart;

                    suppressionPart = XlExp(-k*k*l*l);
            
                    float k4 = k * k; k4 *= k4;
                    Ph = A * directionalPart * suppressionPart * XlExp(-1.f / (k*k*L*L)) / k4;
                }

                    //  Note that the random values returned are related to
                    //  each other slightly... It might be better if the 2 elements
                    //  of the complex number are not related at all.
                auto randomValues = RandomGaussian(1.f);
                // randomValues.second = RandomGaussian(1.f).first;        // second tap of the algorithm to guarantee good results
                float b = gReciprocalSqrt2 * XlSqrt(Ph);
                float realPart       = randomValues.first * b;
                float imaginaryPart  = randomValues.second * b;

                realValues[y*desc._width+x] = realPart;
                imaginaryValues[y*desc._width+x] = imaginaryPart;
            }
        }

        auto bufferUploadsDesc = BuildRenderTargetDesc(
            BindFlag::ShaderResource, 
            BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, Metal::NativeFormat::R32_UINT),
            "FFTWorking");
        auto inputReal = 
            uploads.Transaction_Immediate(
                bufferUploadsDesc, 
                BufferUploads::CreateBasicPacket(
                    desc._width*desc._height*sizeof(float), realValues.get(), 
                    TexturePitches(
                        unsigned(desc._width*sizeof(float)), 
                        unsigned(desc._width*desc._height*sizeof(float)))).get()
            );
        auto inputImaginary = 
            uploads.Transaction_Immediate(
                bufferUploadsDesc, 
                BufferUploads::CreateBasicPacket(
                    desc._width*desc._height*sizeof(float), imaginaryValues.get(), 
                    TexturePitches(
                        unsigned(desc._width*sizeof(float)), 
                        unsigned(desc._width*desc._height*sizeof(float)))).get()
            );

        Metal::ShaderResourceView inputRealShaderResource(inputReal->GetUnderlying());
        Metal::ShaderResourceView inputImaginaryShaderResource(inputImaginary->GetUnderlying());

        _inputReal = std::move(inputReal);
        _inputImaginary = std::move(inputImaginary);
        _inputRealShaderResource = std::move(inputRealShaderResource);
        _inputImaginaryShaderResource = std::move(inputImaginaryShaderResource);
    }

    StartingSpectrumBox::~StartingSpectrumBox() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    DeepOceanSimSettings::DeepOceanSimSettings()
    {
        _enable = false;
        _windAngle[0] = 0.734f * 2.f * gPI;
        _windAngle[1] = 0.41f * 2.f * gPI;
        _windVelocity[0] = 48.f;
        _windVelocity[1] = 50.f;
        _physicalDimensions = 256.f;
        _gridDimensions = 256;
        _strengthConstantXY = 1.f;
        _strengthConstantZ = 0.61f;
        _detailNormalsStrength = 0.65f;
        _spectrumFade = 0.f;
        _scaleAgainstWind[0] = 0.25f;
        _scaleAgainstWind[1] = 0.25f;
        _suppressionFactor[0] = 0.06f;
        _suppressionFactor[1] = 0.06f;
        _gridShiftSpeed = 0.062f;
        _baseHeight = 0.f;
        _foamThreshold = 0.3f;
        _foamIncreaseSpeed = 8.f / .33f;
        _foamIncreaseClamp = 8.f;
        _foamDecrease = 1;
    }

    #define ParamName(x) static auto x = ParameterBox::MakeParameterNameHash(#x);

    DeepOceanSimSettings::DeepOceanSimSettings(const ParameterBox& params)
    : DeepOceanSimSettings()
    {
        ParamName(Enable);
        ParamName(WindAngle);
        ParamName(WindVelocity);
        ParamName(PhysicalDimensions);
        ParamName(GridDimensions);
        ParamName(StrengthConstantXY);
        ParamName(StrengthConstantZ);
        ParamName(DetailNormalsStrength);
        ParamName(SpectrumFade);
        ParamName(ScaleAgainstWind);
        ParamName(SuppressionFactor);
        ParamName(GridShiftSpeed);
        ParamName(BaseHeight);
        ParamName(FoamThreshold);
        ParamName(FoamIncreaseSpeed);
        ParamName(FoamIncreaseClamp);
        ParamName(FoamDecrease);

        _enable = params.GetParameter(Enable, _enable);
        _windAngle[0] = params.GetParameter(WindAngle, _windAngle[0] * (180.f / gPI)) * (gPI / 180.f);
        _windVelocity[0] = params.GetParameter(WindVelocity, _windVelocity[0]);
        _physicalDimensions = params.GetParameter(PhysicalDimensions, _physicalDimensions);
        _gridDimensions = params.GetParameter(GridDimensions, _gridDimensions);
        _strengthConstantXY = params.GetParameter(StrengthConstantXY, _strengthConstantXY);
        _strengthConstantZ = params.GetParameter(StrengthConstantZ, _strengthConstantZ);
        _detailNormalsStrength = params.GetParameter(DetailNormalsStrength, _detailNormalsStrength);
        _spectrumFade = params.GetParameter(SpectrumFade, _spectrumFade);
        _scaleAgainstWind[0] = params.GetParameter(ScaleAgainstWind, _scaleAgainstWind[0]);
        _suppressionFactor[0] = params.GetParameter(SuppressionFactor, _suppressionFactor[0]);
        _gridShiftSpeed = params.GetParameter(GridShiftSpeed, _gridShiftSpeed);
        _baseHeight = params.GetParameter(BaseHeight, _baseHeight);
        _foamThreshold = params.GetParameter(FoamThreshold, _foamThreshold);
        _foamIncreaseSpeed = params.GetParameter(FoamIncreaseSpeed, _foamIncreaseSpeed);
        _foamIncreaseClamp = params.GetParameter(FoamIncreaseClamp, _foamIncreaseClamp);
        _foamDecrease = params.GetParameter(FoamDecrease, _foamDecrease);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    DeepOceanSim::Desc::Desc(unsigned width, unsigned height, bool useDerivativesMapForNormals, bool buildFoam)
    : _width(width), _height(height), _useDerivativesMapForNormals(useDerivativesMapForNormals), _buildFoam(buildFoam) {}

    DeepOceanSim::DeepOceanSim(const Desc& desc) 
    {
        using namespace BufferUploads;
        auto& uploads = GetBufferUploads();

        auto bufferUploadsDesc = BuildRenderTargetDesc(
            BindFlag::UnorderedAccess|BindFlag::RenderTarget|BindFlag::ShaderResource,
            BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, Metal::NativeFormat::R32_TYPELESS),
            "FFT");

            ////
        auto workingTextureReal = uploads.Transaction_Immediate(bufferUploadsDesc);
        Metal::UnorderedAccessView workingTextureRealUVA(workingTextureReal->GetUnderlying(), Metal::NativeFormat::R32_UINT);
        Metal::RenderTargetView workingTextureRealTarget(workingTextureReal->GetUnderlying(), Metal::NativeFormat::R32_UINT);
        Metal::ShaderResourceView workingTextureRealShaderResource(workingTextureReal->GetUnderlying(), Metal::NativeFormat::R32_FLOAT);

        auto workingTextureImaginary = uploads.Transaction_Immediate(bufferUploadsDesc);
        Metal::UnorderedAccessView workingTextureImaginaryUVA(workingTextureImaginary->GetUnderlying(), Metal::NativeFormat::R32_UINT);
        Metal::RenderTargetView workingTextureImaginaryTarget(workingTextureImaginary->GetUnderlying(), Metal::NativeFormat::R32_UINT);
        Metal::ShaderResourceView workingTextureImaginaryShaderResource(workingTextureImaginary->GetUnderlying(), Metal::NativeFormat::R32_FLOAT);

            ////
        auto workingTextureXReal = uploads.Transaction_Immediate(bufferUploadsDesc);
        Metal::UnorderedAccessView workingTextureXRealUVA(workingTextureXReal->GetUnderlying(), Metal::NativeFormat::R32_UINT);
        Metal::ShaderResourceView workingTextureXRealShaderResource(workingTextureXReal->GetUnderlying(), Metal::NativeFormat::R32_FLOAT);

        auto workingTextureXImaginary = uploads.Transaction_Immediate(bufferUploadsDesc);
        Metal::UnorderedAccessView workingTextureXImaginaryUVA(workingTextureXImaginary->GetUnderlying(), Metal::NativeFormat::R32_UINT);
        Metal::ShaderResourceView workingTextureXImaginaryShaderResource(workingTextureXImaginary->GetUnderlying(), Metal::NativeFormat::R32_FLOAT);

            ////
        auto workingTextureYReal = uploads.Transaction_Immediate(bufferUploadsDesc);
        Metal::UnorderedAccessView workingTextureYRealUVA(workingTextureYReal->GetUnderlying(), Metal::NativeFormat::R32_UINT);
        Metal::ShaderResourceView workingTextureYRealShaderResource(workingTextureYReal->GetUnderlying(), Metal::NativeFormat::R32_FLOAT);

        auto workingTextureYImaginary = uploads.Transaction_Immediate(bufferUploadsDesc);
        Metal::UnorderedAccessView workingTextureYImaginaryUVA(workingTextureYImaginary->GetUnderlying(), Metal::NativeFormat::R32_UINT);
        Metal::ShaderResourceView workingTextureYImaginaryShaderResource(workingTextureYImaginary->GetUnderlying(), Metal::NativeFormat::R32_FLOAT);

            ////
        const unsigned normalsMipCount = IntegerLog2(std::max(desc._width, desc._height));
        auto typelessNormalFormat = desc._useDerivativesMapForNormals?Metal::NativeFormat::R8G8_TYPELESS:Metal::NativeFormat::R8G8B8A8_TYPELESS;
        auto uintNormalFormat = desc._useDerivativesMapForNormals?Metal::NativeFormat::R8G8_UINT:Metal::NativeFormat::R8G8B8A8_UINT;
        auto unormNormalFormat = desc._useDerivativesMapForNormals?Metal::NativeFormat::R8G8_UNORM:Metal::NativeFormat::R8G8B8A8_UNORM;
        auto normalsBufferUploadsDesc = BuildRenderTargetDesc(
            BindFlag::UnorderedAccess|BindFlag::ShaderResource,
            BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, typelessNormalFormat, uint8(normalsMipCount)),
            "OceanNormals");
        auto normalsTexture = uploads.Transaction_Immediate(normalsBufferUploadsDesc);
        std::vector<Metal::UnorderedAccessView> normalsTextureUVA;
        std::vector<Metal::ShaderResourceView> normalsSingleMipSRV;
        normalsTextureUVA.reserve(normalsMipCount);
        normalsSingleMipSRV.reserve(normalsMipCount);
        for (unsigned c=0; c<normalsMipCount; ++c) {
            normalsTextureUVA.push_back(Metal::UnorderedAccessView(normalsTexture->GetUnderlying(), uintNormalFormat, c));
            normalsSingleMipSRV.push_back(Metal::ShaderResourceView(normalsTexture->GetUnderlying(), uintNormalFormat, Metal::MipSlice(c, 1)));
        }
        Metal::ShaderResourceView normalsTextureShaderResource(normalsTexture->GetUnderlying(), unormNormalFormat, Metal::MipSlice(0, normalsMipCount));

            ////
        auto foamTextureDesc = BuildRenderTargetDesc(
            BindFlag::UnorderedAccess|BindFlag::ShaderResource,
            BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, Metal::NativeFormat::R8_TYPELESS),
            "Foam");
        auto foamQuantity0 = uploads.Transaction_Immediate(foamTextureDesc, nullptr);
        auto foamQuantity1 = uploads.Transaction_Immediate(foamTextureDesc, nullptr);
        Metal::UnorderedAccessView foamQuantityUVA0(foamQuantity0->GetUnderlying(), Metal::NativeFormat::R8_UINT);
        Metal::ShaderResourceView foamQuantitySRV0(foamQuantity0->GetUnderlying(), Metal::NativeFormat::R8_UNORM);
        Metal::ShaderResourceView foamQuantitySRV20(foamQuantity0->GetUnderlying(), Metal::NativeFormat::R8_UINT);
        Metal::UnorderedAccessView foamQuantityUVA1(foamQuantity1->GetUnderlying(), Metal::NativeFormat::R8_UINT);
        Metal::ShaderResourceView foamQuantitySRV1(foamQuantity1->GetUnderlying(), Metal::NativeFormat::R8_UNORM);
        Metal::ShaderResourceView foamQuantitySRV21(foamQuantity1->GetUnderlying(), Metal::NativeFormat::R8_UINT);

            ////
        _workingTextureReal = std::move(workingTextureReal);
        _workingTextureRealUVA = std::move(workingTextureRealUVA);
        _workingTextureRealRTV = std::move(workingTextureRealTarget);
        _workingTextureRealSRV = std::move(workingTextureRealShaderResource);

        _workingTextureImaginary = std::move(workingTextureImaginary);
        _workingTextureImaginaryUVA = std::move(workingTextureImaginaryUVA);
        _workingTextureImaginaryRTV = std::move(workingTextureImaginaryTarget);
        _workingTextureImaginarySRV = std::move(workingTextureImaginaryShaderResource);

            ////
        _workingTextureXReal = std::move(workingTextureXReal);
        _workingTextureXRealUVA = std::move(workingTextureXRealUVA);
        _workingTextureXRealSRV = std::move(workingTextureXRealShaderResource);

        _workingTextureXImaginary = std::move(workingTextureXImaginary);
        _workingTextureXImaginaryUVA = std::move(workingTextureXImaginaryUVA);
        _workingTextureXImaginarySRV = std::move(workingTextureXImaginaryShaderResource);

            ////
        _workingTextureYReal = std::move(workingTextureYReal);
        _workingTextureYRealUVA = std::move(workingTextureYRealUVA);
        _workingTextureYRealSRV = std::move(workingTextureYRealShaderResource);

        _workingTextureYImaginary = std::move(workingTextureYImaginary);
        _workingTextureYImaginaryUVA = std::move(workingTextureYImaginaryUVA);
        _workingTextureYImaginarySRV = std::move(workingTextureYImaginaryShaderResource);

            ////
        _normalsTexture = std::move(normalsTexture);
        _normalsTextureUAV = std::move(normalsTextureUVA);
        _normalsSingleMipSRV = std::move(normalsSingleMipSRV);
        _normalsTextureSRV = std::move(normalsTextureShaderResource);

            ////
        _foamQuantity[0] = std::move(foamQuantity0);
        _foamQuantity[1] = std::move(foamQuantity1);
        _foamQuantityUAV[0] = std::move(foamQuantityUVA0);
        _foamQuantityUAV[1] = std::move(foamQuantityUVA1);
        _foamQuantitySRV[0] = std::move(foamQuantitySRV0);
        _foamQuantitySRV[1] = std::move(foamQuantitySRV1);
        _foamQuantitySRV2[0] = std::move(foamQuantitySRV20);
        _foamQuantitySRV2[1] = std::move(foamQuantitySRV21);

        _useDerivativesMapForNormals = desc._useDerivativesMapForNormals;
    }

    DeepOceanSim::~DeepOceanSim() {}

}

