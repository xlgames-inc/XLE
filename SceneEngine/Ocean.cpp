// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4267)    // 'initializing' : conversion from 'size_t' to 'int', possible loss of data -- in cml inverse_f

#include "Ocean.h"
#include "ShallowWater.h"
#include "SceneEngineUtils.h"
#include "LightingParserContext.h"
#include "SceneParser.h"
#include "RefractionsBuffer.h"
#include "SimplePatchBox.h"
#include "Sky.h"
#include "Noise.h"
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
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/RenderUtils.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/DataPacket.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../Math/Transformations.h"
#include "../Math/ProjectionMath.h"
#include "../Math/Geometry.h"
#include "../Math/Noise.h"
#include "../ConsoleRig/Console.h"
#include "../Utility/BitUtils.h"

#include "../RenderCore/DX11/Metal/DX11Utils.h"

#pragma warning(disable:4127)       // warning C4127: conditional expression is constant
#pragma warning(disable:4505)       // warning C4505: 'SceneEngine::SortByGridIndex' : unreferenced local function has been removed

namespace SceneEngine
{
    using namespace RenderCore;
    using namespace RenderCore::Metal;

    RenderCore::Metal::ShaderResourceView OceanReflectionResource;
    Float4x4 OceanWorldToReflection = Identity<Float4x4>();
    ISurfaceHeightsProvider* MainSurfaceHeightsProvider = nullptr;
    static ParameterBox MaterialState_Blank;

///////////////////////////////////////////////////////////////////////////////////////////////////

    class FFTBufferBox
    {
    public:
        class Desc
        {
        public:
            Desc(unsigned width, unsigned height, bool useDerivativesMapForNormals);
            unsigned _width, _height;
            bool _useDerivativesMapForNormals;
        };

        FFTBufferBox(const Desc& desc);
        ~FFTBufferBox();

        intrusive_ptr<ID3D::Resource>               _workingTextureReal;
        RenderCore::Metal::UnorderedAccessView      _workingTextureRealUVA;
        RenderCore::Metal::RenderTargetView         _workingTextureRealTarget;
        RenderCore::Metal::ShaderResourceView       _workingTextureRealShaderResource;

        intrusive_ptr<ID3D::Resource>               _workingTextureImaginary;
        RenderCore::Metal::UnorderedAccessView      _workingTextureImaginaryUVA;
        RenderCore::Metal::RenderTargetView         _workingTextureImaginaryTarget;
        RenderCore::Metal::ShaderResourceView       _workingTextureImaginaryShaderResource;

        intrusive_ptr<ID3D::Resource>               _workingTextureXReal;
        RenderCore::Metal::UnorderedAccessView      _workingTextureXRealUVA;
        RenderCore::Metal::ShaderResourceView       _workingTextureXRealShaderResource;

        intrusive_ptr<ID3D::Resource>               _workingTextureXImaginary;
        RenderCore::Metal::UnorderedAccessView      _workingTextureXImaginaryUVA;
        RenderCore::Metal::ShaderResourceView       _workingTextureXImaginaryShaderResource;

        intrusive_ptr<ID3D::Resource>               _workingTextureYReal;
        RenderCore::Metal::UnorderedAccessView      _workingTextureYRealUVA;
        RenderCore::Metal::ShaderResourceView       _workingTextureYRealShaderResource;

        intrusive_ptr<ID3D::Resource>               _workingTextureYImaginary;
        RenderCore::Metal::UnorderedAccessView      _workingTextureYImaginaryUVA;
        RenderCore::Metal::ShaderResourceView       _workingTextureYImaginaryShaderResource;

        intrusive_ptr<ID3D::Resource>                       _normalsTexture;
        std::vector<RenderCore::Metal::UnorderedAccessView> _normalsTextureUAV;
        std::vector<RenderCore::Metal::ShaderResourceView>  _normalsSingleMipSRV;
        RenderCore::Metal::ShaderResourceView               _normalsTextureShaderResource;

        intrusive_ptr<ID3D::Resource>               _foamQuantity[2];
        RenderCore::Metal::UnorderedAccessView      _foamQuantityUAV[2];
        RenderCore::Metal::ShaderResourceView       _foamQuantitySRV[2];
        RenderCore::Metal::ShaderResourceView       _foamQuantitySRV2[2];

        bool _useDerivativesMapForNormals;
    };

    FFTBufferBox::Desc::Desc(unsigned width, unsigned height, bool useDerivativesMapForNormals) : _width(width), _height(height), _useDerivativesMapForNormals(useDerivativesMapForNormals) {}

    FFTBufferBox::FFTBufferBox(const Desc& desc) 
    {
        using namespace BufferUploads;
        auto& uploads = GetBufferUploads();

        auto bufferUploadsDesc = BuildRenderTargetDesc(
            BindFlag::UnorderedAccess|BindFlag::RenderTarget|BindFlag::ShaderResource,
            BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, NativeFormat::R32_TYPELESS),
            "FFT");

            ////
        auto workingTextureReal = uploads.Transaction_Immediate(bufferUploadsDesc)->AdoptUnderlying();
        UnorderedAccessView workingTextureRealUVA(workingTextureReal.get(), NativeFormat::R32_UINT);
        RenderTargetView workingTextureRealTarget(workingTextureReal.get(), NativeFormat::R32_UINT);
        ShaderResourceView workingTextureRealShaderResource(workingTextureReal.get(), NativeFormat::R32_FLOAT);

        auto workingTextureImaginary = uploads.Transaction_Immediate(bufferUploadsDesc)->AdoptUnderlying();
        UnorderedAccessView workingTextureImaginaryUVA(workingTextureImaginary.get(), NativeFormat::R32_UINT);
        RenderTargetView workingTextureImaginaryTarget(workingTextureImaginary.get(), NativeFormat::R32_UINT);
        ShaderResourceView workingTextureImaginaryShaderResource(workingTextureImaginary.get(), NativeFormat::R32_FLOAT);

            ////
        auto workingTextureXReal = uploads.Transaction_Immediate(bufferUploadsDesc)->AdoptUnderlying();
        UnorderedAccessView workingTextureXRealUVA(workingTextureXReal.get(), NativeFormat::R32_UINT);
        ShaderResourceView workingTextureXRealShaderResource(workingTextureXReal.get(), NativeFormat::R32_FLOAT);

        auto workingTextureXImaginary = uploads.Transaction_Immediate(bufferUploadsDesc)->AdoptUnderlying();
        UnorderedAccessView workingTextureXImaginaryUVA(workingTextureXImaginary.get(), NativeFormat::R32_UINT);
        ShaderResourceView workingTextureXImaginaryShaderResource(workingTextureXImaginary.get(), NativeFormat::R32_FLOAT);

            ////
        auto workingTextureYReal = uploads.Transaction_Immediate(bufferUploadsDesc)->AdoptUnderlying();
        UnorderedAccessView workingTextureYRealUVA(workingTextureYReal.get(), NativeFormat::R32_UINT);
        ShaderResourceView workingTextureYRealShaderResource(workingTextureYReal.get(), NativeFormat::R32_FLOAT);

        auto workingTextureYImaginary = uploads.Transaction_Immediate(bufferUploadsDesc)->AdoptUnderlying();
        UnorderedAccessView workingTextureYImaginaryUVA(workingTextureYImaginary.get(), NativeFormat::R32_UINT);
        ShaderResourceView workingTextureYImaginaryShaderResource(workingTextureYImaginary.get(), NativeFormat::R32_FLOAT);

            ////
        const unsigned normalsMipCount = IntegerLog2(std::max(desc._width, desc._height));
        auto typelessNormalFormat = desc._useDerivativesMapForNormals?NativeFormat::R8G8_TYPELESS:NativeFormat::R8G8B8A8_TYPELESS;
        auto uintNormalFormat = desc._useDerivativesMapForNormals?NativeFormat::R8G8_UINT:NativeFormat::R8G8B8A8_UINT;
        auto unormNormalFormat = desc._useDerivativesMapForNormals?NativeFormat::R8G8_UNORM:NativeFormat::R8G8B8A8_UNORM;
        auto normalsBufferUploadsDesc = BuildRenderTargetDesc(
            BindFlag::UnorderedAccess|BindFlag::ShaderResource,
            BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, typelessNormalFormat, uint8(normalsMipCount)),
            "OceanNormals");
        auto normalsTexture = uploads.Transaction_Immediate(normalsBufferUploadsDesc)->AdoptUnderlying();
        std::vector<UnorderedAccessView> normalsTextureUVA;
        std::vector<ShaderResourceView> normalsSingleMipSRV;
        normalsTextureUVA.reserve(normalsMipCount);
        normalsSingleMipSRV.reserve(normalsMipCount);
        for (unsigned c=0; c<normalsMipCount; ++c) {
            normalsTextureUVA.push_back(UnorderedAccessView(normalsTexture.get(), uintNormalFormat, c));
            normalsSingleMipSRV.push_back(ShaderResourceView(normalsTexture.get(), uintNormalFormat, MipSlice(c, 1)));
        }
        ShaderResourceView normalsTextureShaderResource(normalsTexture.get(), unormNormalFormat, MipSlice(0, normalsMipCount));

            ////
        auto foamTextureDesc = BuildRenderTargetDesc(
            BindFlag::UnorderedAccess|BindFlag::ShaderResource,
            BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, NativeFormat::R8_TYPELESS),
            "Foam");
        auto foamQuantity0 = uploads.Transaction_Immediate(foamTextureDesc, nullptr)->AdoptUnderlying();
        auto foamQuantity1 = uploads.Transaction_Immediate(foamTextureDesc, nullptr)->AdoptUnderlying();
        UnorderedAccessView foamQuantityUVA0(foamQuantity0.get(), NativeFormat::R8_UINT);
        ShaderResourceView foamQuantitySRV0(foamQuantity0.get(), NativeFormat::R8_UNORM);
        ShaderResourceView foamQuantitySRV20(foamQuantity0.get(), NativeFormat::R8_UINT);
        UnorderedAccessView foamQuantityUVA1(foamQuantity1.get(), NativeFormat::R8_UINT);
        ShaderResourceView foamQuantitySRV1(foamQuantity1.get(), NativeFormat::R8_UNORM);
        ShaderResourceView foamQuantitySRV21(foamQuantity1.get(), NativeFormat::R8_UINT);

            ////
        _workingTextureReal = std::move(workingTextureReal);
        _workingTextureRealUVA = std::move(workingTextureRealUVA);
        _workingTextureRealTarget = std::move(workingTextureRealTarget);
        _workingTextureRealShaderResource = std::move(workingTextureRealShaderResource);

        _workingTextureImaginary = std::move(workingTextureImaginary);
        _workingTextureImaginaryUVA = std::move(workingTextureImaginaryUVA);
        _workingTextureImaginaryTarget = std::move(workingTextureImaginaryTarget);
        _workingTextureImaginaryShaderResource = std::move(workingTextureImaginaryShaderResource);

            ////
        _workingTextureXReal = std::move(workingTextureXReal);
        _workingTextureXRealUVA = std::move(workingTextureXRealUVA);
        _workingTextureXRealShaderResource = std::move(workingTextureXRealShaderResource);

        _workingTextureXImaginary = std::move(workingTextureXImaginary);
        _workingTextureXImaginaryUVA = std::move(workingTextureXImaginaryUVA);
        _workingTextureXImaginaryShaderResource = std::move(workingTextureXImaginaryShaderResource);

            ////
        _workingTextureYReal = std::move(workingTextureYReal);
        _workingTextureYRealUVA = std::move(workingTextureYRealUVA);
        _workingTextureYRealShaderResource = std::move(workingTextureYRealShaderResource);

        _workingTextureYImaginary = std::move(workingTextureYImaginary);
        _workingTextureYImaginaryUVA = std::move(workingTextureYImaginaryUVA);
        _workingTextureYImaginaryShaderResource = std::move(workingTextureYImaginaryShaderResource);

            ////
        _normalsTexture = std::move(normalsTexture);
        _normalsTextureUAV = std::move(normalsTextureUVA);
        _normalsSingleMipSRV = std::move(normalsSingleMipSRV);
        _normalsTextureShaderResource = std::move(normalsTextureShaderResource);

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

    FFTBufferBox::~FFTBufferBox() {}

    void FFT_DoDebugging(RenderCore::Metal::DeviceContext* context)
    {
        const unsigned dimensions = 256;
        auto& box = Techniques::FindCachedBox<FFTBufferBox>(FFTBufferBox::Desc(dimensions,dimensions, false));

        SavedTargets savedTargets(context);
        ViewportDesc oldViewport(*context);

        ViewportDesc newViewport( 0, 0, float(dimensions), float(dimensions), 0.f, 1.f );
        context->Bind(newViewport);

        context->Bind(::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*", "game/xleres/Ocean/FFTDebugging.psh:copy:ps_*"));
        context->Bind(MakeResourceList(box._workingTextureRealTarget, box._workingTextureImaginaryTarget), nullptr);
        context->BindPS(MakeResourceList(::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("game/objects/env/nature/grassland/plant/co_gland_weed_a_df.dds").GetShaderResource()));
        SetupVertexGeneratorShader(context);
        context->Draw(4);
        savedTargets.ResetToOldTargets(context);
        context->Bind(oldViewport);

        auto& fft1 = ::Assets::GetAssetDep<Metal::ComputeShader>("game/xleres/Ocean/FFT.csh:FFT2D_1:cs_*");
        auto& fft2 = ::Assets::GetAssetDep<Metal::ComputeShader>("game/xleres/Ocean/FFT.csh:FFT2D_2:cs_*");

        context->BindCS(MakeResourceList(box._workingTextureRealUVA, box._workingTextureImaginaryUVA));
        unsigned constants[4] = {1, 0, 0, 0};
        context->BindCS(MakeResourceList(ConstantBuffer(constants, sizeof(constants))));
        context->Bind(fft1); context->Dispatch((dimensions + (32-1))/32);
        context->Bind(fft2); context->Dispatch((dimensions + (32-1))/32);

        context->Bind(::Assets::GetAssetDep<Metal::ComputeShader>("game/xleres/Ocean/FFT.csh:Lowpass:cs_*"));
        context->Dispatch((dimensions + (32-1))/32);

        constants[0] = 0;
        context->BindCS(MakeResourceList(ConstantBuffer(constants, sizeof(constants))));
        context->Bind(fft1); context->Dispatch((dimensions + (32-1))/32);
        context->Bind(fft2); context->Dispatch((dimensions + (32-1))/32);
        context->UnbindCS<UnorderedAccessView>(0, 2);

        context->Bind(Techniques::CommonResources()._blendStraightAlpha);
        context->Bind(::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*", "game/xleres/Ocean/FFTDebugging.psh:main:ps_*"));
        context->BindPS(MakeResourceList(box._workingTextureRealShaderResource, box._workingTextureImaginaryShaderResource));
        context->Draw(4);
    }

        ////////////////////////////////

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

            Desc(unsigned width, unsigned height, const Float2& physicalDimensions, const Float2& windVector, float scaleAgainstWind, float suppressionFactor)
                {   _width = width; _height = height; _windVector = windVector; 
                    _physicalDimensions = physicalDimensions; _scaleAgainstWind = scaleAgainstWind; _suppressionFactor = suppressionFactor; }
        };

        StartingSpectrumBox(const Desc& desc);
        ~StartingSpectrumBox();

        intrusive_ptr<ID3D::Resource>                  _inputReal;
        intrusive_ptr<ID3D::Resource>                  _inputImaginary;
        RenderCore::Metal::ShaderResourceView       _inputRealShaderResource;
        RenderCore::Metal::ShaderResourceView       _inputImaginaryShaderResource;
    };

    static std::pair<float, float> RandomGaussian(float variance)
    {
            //  calculate 2 random numbers using the box muller technique
            //  (see http://en.wikipedia.org/wiki/Box%E2%80%93Muller_transform)
            //  One form has a lot of trignometry, another has a loop in it...

        const int method = 1;
        if (method == 0) {
            return std::make_pair(
                LinearInterpolate(-1.f, 1.f, rand() / float(RAND_MAX)),
                LinearInterpolate(-1.f, 1.f, rand() / float(RAND_MAX)));
        }

        const bool polarMethod = method==1;
        if (polarMethod) {
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
            BindFlag::ShaderResource, BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, NativeFormat::R32_UINT),
            "FFTWorking");
        auto inputReal = 
            uploads.Transaction_Immediate(
                bufferUploadsDesc, 
                BufferUploads::CreateBasicPacket(
                    desc._width*desc._height*sizeof(float), realValues.get(), 
                    TexturePitches(
                        unsigned(desc._width*sizeof(float)), 
                        unsigned(desc._width*desc._height*sizeof(float)))).get()
            )->AdoptUnderlying();
        auto inputImaginary = 
            uploads.Transaction_Immediate(
                bufferUploadsDesc, 
                BufferUploads::CreateBasicPacket(
                    desc._width*desc._height*sizeof(float), imaginaryValues.get(), 
                    TexturePitches(
                        unsigned(desc._width*sizeof(float)), 
                        unsigned(desc._width*desc._height*sizeof(float)))).get()
            )->AdoptUnderlying();

        RenderCore::Metal::ShaderResourceView inputRealShaderResource(inputReal.get());
        RenderCore::Metal::ShaderResourceView inputImaginaryShaderResource(inputImaginary.get());

        _inputReal = std::move(inputReal);
        _inputImaginary = std::move(inputImaginary);
        _inputRealShaderResource = std::move(inputRealShaderResource);
        _inputImaginaryShaderResource = std::move(inputImaginaryShaderResource);
    }

    StartingSpectrumBox::~StartingSpectrumBox() {}

        ////////////////////////////////

    SimplePatchBox::Desc::Desc(unsigned width, unsigned height, bool flipAlternate) 
    {
        std::fill((char*)this, (char*)PtrAdd(this, sizeof(*this)), 0);
        _width = width; _height = height; _flipAlternate = flipAlternate; 
    }

    SimplePatchBox::SimplePatchBox(const Desc& desc) 
    {
            //  build an index buffer for drawing a simple grid patch
            //  split the full patch into a few smaller patches.. This may help
            //  post transform vertex cache performance
        const unsigned miniGridSize = 16;
        const unsigned miniGridsX = desc._width / miniGridSize;
        const unsigned miniGridsY = desc._height / miniGridSize;
        const unsigned indicesPerMiniGrid = (miniGridSize)*(miniGridSize)*6;
        auto indexBufferData = std::unique_ptr<uint32[]>(new uint32[indicesPerMiniGrid*miniGridsX*miniGridsY]);

        for (unsigned mg=0; mg<miniGridsX*miniGridsY; ++mg) {
            unsigned offsetX = (mg%miniGridsX) * miniGridSize;
            unsigned offsetY = (mg/miniGridsX) * miniGridSize;
        
            uint32* writingData = &indexBufferData[mg*indicesPerMiniGrid];
            for (unsigned y=0; y<miniGridSize; ++y) {
                for (unsigned x=0; x<miniGridSize; ++x) {
                    unsigned index00 = offsetX + x + (offsetY + y) * desc._width;
                    unsigned index10 = index00 + (((offsetX + x + 1)<desc._width)?1:0);
                    unsigned index01 = index00 + (((offsetY + y + 1)<desc._height)?desc._width:0);
                    unsigned index11 = index01 + (((offsetX + x + 1)<desc._width)?1:0);

                    if ((x + y)&1 && desc._flipAlternate) {
                            //  flip the center line on alternating grids
                            //  sometimes this can produce nicer shapes on roughly
                            //  deformed height fields
                        *writingData++ = index00;
                        *writingData++ = index10;
                        *writingData++ = index11;
                        *writingData++ = index00;
                        *writingData++ = index11;
                        *writingData++ = index01;
                    } else {
                        *writingData++ = index00;
                        *writingData++ = index10;
                        *writingData++ = index01;
                        *writingData++ = index01;
                        *writingData++ = index10;
                        *writingData++ = index11;
                    }
                }
            }
        }

        auto simplePatchSize = indicesPerMiniGrid*miniGridsX*miniGridsY*sizeof(uint32);
        RenderCore::Metal::IndexBuffer simplePatchIndexBuffer(indexBufferData.get(), simplePatchSize);

        _simplePatchIndexBuffer = std::move(simplePatchIndexBuffer);
        _simplePatchIndexCount = indicesPerMiniGrid*miniGridsX*miniGridsY;
    }

    SimplePatchBox::~SimplePatchBox() {}

            ////////////////////////////////

    static void OceanSurface_DrawDebugging(   RenderCore::Metal::DeviceContext* context, 
                                              LightingParserContext& parserContext,
                                              const OceanSettings& oceanSettings,
                                              FFTBufferBox& fftBuffer);

    class OceanRenderingConstants
    {
    public:
        unsigned _gridWidth, _gridHeight;
        float _spectrumFade;
        unsigned _useScreenSpaceGrid;
        float _detailNormalsStrength;
        Float2 _gridShift;
        unsigned _dummy;
        Float4x4 _oceanToReflection;
    };

    namespace Internal
    {
        OceanMaterialConstants BuildOceanMaterialConstants(const OceanSettings& oceanSettings, float shallowGridPhysicalDimension)
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

    static OceanRenderingConstants BuildOceanRenderingConstants(
        const OceanSettings& oceanSettings, const Float4x4& oceanToReflection,
        float currentTime)
    {
        const unsigned screenSpaceGridScale = Tweakable("OceanScreenSpaceGridScale", 6);
        const unsigned dimensions = oceanSettings._gridDimensions;
        float oceanSpectrumFade = oceanSettings._spectrumFade;

            //  Move the ocean in the wind direction
        float windAngle = LinearInterpolate(oceanSettings._windAngle[0], oceanSettings._windAngle[1], oceanSettings._spectrumFade);
        float windSpeed = LinearInterpolate(oceanSettings._windVelocity[0], oceanSettings._windVelocity[1], oceanSettings._spectrumFade);
        auto sc = XlSinCos(windAngle);
        Float2 windVector = windSpeed * Float2(std::get<0>(sc), std::get<1>(sc));
        Float2 gridShift = oceanSettings._gridShiftSpeed * currentTime * windVector  / oceanSettings._physicalDimensions;
        gridShift[0] = gridShift[0] - XlFloor(gridShift[0]);
        gridShift[1] = gridShift[1] - XlFloor(gridShift[1]);

        OceanRenderingConstants result = {
            dimensions, dimensions, 
            oceanSpectrumFade, screenSpaceGridScale!=0,
            oceanSettings._detailNormalsStrength,
            gridShift, 0,
            oceanToReflection
        };
        return result;
    }

    static unsigned OceanBufferCounter = 0;

    static void UpdateOceanSurface(RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext, const OceanSettings& oceanSettings, FFTBufferBox& fftBuffer)
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

        auto& buildNormals = ::Assets::GetAssetDep<Metal::ComputeShader>(fftBuffer._useDerivativesMapForNormals ? "game/xleres/Ocean/OceanNormals.csh:BuildDerivatives:cs_*" : "game/xleres/Ocean/OceanNormals.csh:BuildNormals:cs_*");
        auto& buildNormalsMipmaps = ::Assets::GetAssetDep<Metal::ComputeShader>(fftBuffer._useDerivativesMapForNormals ? "game/xleres/Ocean/OceanNormals.csh:BuildDerivativesMipmap:cs_*" : "game/xleres/Ocean/OceanNormals.csh:BuildNormalsMipmap:cs_*");
    
        const float shallowGridPhysicalDimension = Tweakable("OceanShallowPhysicalDimension", 256.f);
        const float currentTime = parserContext.GetSceneParser()->GetTimeValue();
        auto materialConstants = Internal::BuildOceanMaterialConstants(oceanSettings, shallowGridPhysicalDimension);
        ConstantBuffer materialConstantBuffer(&materialConstants, sizeof(materialConstants));
        auto renderingConstants = BuildOceanRenderingConstants(oceanSettings, OceanWorldToReflection, currentTime);
        ConstantBuffer renderingConstantsBuffer(&renderingConstants, sizeof(renderingConstants));
        const ConstantBuffer* cbs[] = { &renderingConstantsBuffer, &materialConstantBuffer };

        BoundUniforms setupUniforms(::Assets::GetAssetDep<CompiledShaderByteCode>("game/xleres/Ocean/FFT.csh:Setup:cs_*", "DO_INVERSE=0"));
        Techniques::TechniqueContext::BindGlobalUniforms(setupUniforms);
        setupUniforms.BindConstantBuffer(Hash64("OceanRenderingConstants"), 0, 1);
        setupUniforms.BindConstantBuffer(Hash64("OceanMaterialSettings"), 1, 1);
        setupUniforms.Apply(*context, 
            parserContext.GetGlobalUniformsStream(),
            UniformsStream(nullptr, cbs, dimof(cbs)));

        context->BindCS(MakeResourceList(
            fftBuffer._workingTextureRealUVA, fftBuffer._workingTextureImaginaryUVA,
            fftBuffer._workingTextureXRealUVA, fftBuffer._workingTextureXImaginaryUVA,
            fftBuffer._workingTextureYRealUVA, fftBuffer._workingTextureYImaginaryUVA));
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

        context->UnbindCS<UnorderedAccessView>(0, 6);

            //  Perform FFT transform first on the heights texture, then on the X & Y displacement textures
        context->BindCS(MakeResourceList(fftBuffer._workingTextureRealUVA, fftBuffer._workingTextureImaginaryUVA));
        context->Bind(fft1); context->Dispatch((dimensions + (32-1))/32);
        context->Bind(fft2); context->Dispatch((dimensions + (32-1))/32);

        context->BindCS(MakeResourceList(fftBuffer._workingTextureXRealUVA, fftBuffer._workingTextureXImaginaryUVA));
        context->Bind(fft1); context->Dispatch((dimensions + (32-1))/32);
        context->Bind(fft2); context->Dispatch((dimensions + (32-1))/32);

        context->BindCS(MakeResourceList(fftBuffer._workingTextureYRealUVA, fftBuffer._workingTextureYImaginaryUVA));
        context->Bind(fft1); context->Dispatch((dimensions + (32-1))/32);
        context->Bind(fft2); context->Dispatch((dimensions + (32-1))/32);

        context->UnbindCS<UnorderedAccessView>(0, 6);

            //  Generate normals using the displacement textures
        if (!fftBuffer._normalsTextureUAV.empty()) {
            BoundUniforms buildNormalsUniforms(
                ::Assets::GetAssetDep<CompiledShaderByteCode>(
                    fftBuffer._useDerivativesMapForNormals 
                    ? "game/xleres/Ocean/OceanNormals.csh:BuildDerivatives:cs_*" 
                    : "game/xleres/Ocean/OceanNormals.csh:BuildNormals:cs_*"));
            Techniques::TechniqueContext::BindGlobalUniforms(buildNormalsUniforms);
            buildNormalsUniforms.BindConstantBuffer(Hash64("OceanRenderingConstants"), 0, 1);
            buildNormalsUniforms.BindConstantBuffer(Hash64("OceanMaterialSettings"), 1, 1);
            buildNormalsUniforms.Apply(*context, 
                parserContext.GetGlobalUniformsStream(),
                UniformsStream(nullptr, cbs, dimof(cbs)));

            context->BindCS(MakeResourceList(
                fftBuffer._workingTextureRealShaderResource, 
                fftBuffer._workingTextureXRealShaderResource, 
                fftBuffer._workingTextureYRealShaderResource,
                fftBuffer._foamQuantitySRV2[(OceanBufferCounter+1)&1]));
            context->BindCS(MakeResourceList(fftBuffer._normalsTextureUAV[0], fftBuffer._foamQuantityUAV[OceanBufferCounter&1]));
            context->Bind(buildNormals); context->Dispatch((dimensions + (32-1))/32, (dimensions + (32-1))/32);
            context->UnbindCS<UnorderedAccessView>(0, 2);

            context->Bind(buildNormalsMipmaps);
            for (unsigned step = 0; step<fftBuffer._normalsTextureUAV.size()-1; ++step) {
                unsigned mipDims = dimensions >> (step+1);
                unsigned constants[4] = { mipDims, mipDims, 0, 0 };
                context->BindCS(MakeResourceList(ConstantBuffer(constants, sizeof(constants))));

                context->BindCS(MakeResourceList(4, fftBuffer._normalsSingleMipSRV[step]));
                context->BindCS(MakeResourceList(fftBuffer._normalsTextureUAV[step+1]));
            
                context->Dispatch((mipDims + (32-1))/32, (mipDims + (32-1))/32);
                context->UnbindCS<UnorderedAccessView>(0, 1);
            }

            context->UnbindCS<ShaderResourceView>(0, 4);
        }
    }

                //////////////////////////////////////////////////////////////////////////
        //   ================================================================================   //

    static Float4x4 BuildLocalToWorld_Cam(const Float3& position, const Float3& forward, const Float3& up)
    {
            //
            //      Our "camera" coordinate space:
            //
            //        * Right handed
            //        * +X to the right
            //        * +Y up
            //        * -Z into the screen
            //

                // (todo -- check if this is creating "right" or "left"...)
        Float3 right        = Normalize(Cross(forward, up));
        Float3 adjustedUp   = Normalize(Cross(right, forward));
        return Float4x4(
            right[0], adjustedUp[0], -forward[0], position[0],
            right[1], adjustedUp[1], -forward[1], position[1],
            right[2], adjustedUp[2], -forward[2], position[2],
            0.f, 0.f, 0.f, 1.f);
    }

    struct GridRenderingConstants
    {
        Float4x4 _gridProjection;
        Float4 _gridFrustumCorners[4];
        Float3 _gridProjectionOrigin;
        unsigned _gridPatchWidth, _gridPatchHeight;
        float _dummy0[3];
    };

    bool CalculateGridProjection(   GridRenderingConstants& result,
                                    Techniques::ProjectionDesc& mainCameraProjection, 
                                    float oceanBaseHeight)
    {
            //
            //      Calculate a projection matrix for the water grid.
            //      it should be similar to the camera projection, but not
            //      exactly the same. When the water mesh doesn't penetrate the 
            //      near or far plane of the camera frustum, projecting from the 
            //      camera position is fine. 
            //
            //      But when the water mesh does penetrate either near or far, 
            //      then we should customize the water projection transform slightly.
            //      
        Float4x4 viewToWorld = mainCameraProjection._cameraToWorld;

        auto forward    = ExtractForward_Cam(viewToWorld);
        auto position   = ExtractTranslation(viewToWorld);
        auto up         = ExtractUp_Cam(viewToWorld);

        Float4 waterPlane(0.f, 0.f, 1.f, -oceanBaseHeight);

            //
            //      We need to calculate the corners of the camera frustum (in world space)
            //      and we use this to look for intersections between the edges of the frustum
            //      and the water plane.
            //
        Float3 cameraAbsFrustumCorners[8];
        CalculateAbsFrustumCorners(cameraAbsFrustumCorners, mainCameraProjection._worldToProjection);

        const std::pair<unsigned, unsigned> edges[] = 
        {
            std::make_pair(0, 1), std::make_pair(1, 2), std::make_pair(2, 3), std::make_pair(3, 0),
            std::make_pair(4, 5), std::make_pair(5, 6), std::make_pair(6, 7), std::make_pair(7, 4),
            std::make_pair(0, 4), std::make_pair(1, 5), std::make_pair(2, 6), std::make_pair(3, 7)
        };

        bool foundEdgeIntersection = false;
        bool foundNearOrFarIntersection = false;

        float intersectionPts[dimof(edges)];
        for (unsigned c=0; c<dimof(edges); ++c) {
            intersectionPts[c] = RayVsPlane(
                cameraAbsFrustumCorners[edges[c].first], cameraAbsFrustumCorners[edges[c].second],
                waterPlane);
            if (intersectionPts[c] >= 0.f && intersectionPts[c] <= 1.f) {
                foundEdgeIntersection = true;
                    //  if there is an intersection in one of the first 8 edges, it means there
                    //  is a near or far intersection case
                foundNearOrFarIntersection |= (c < 8);  
            }
        }

        if (!foundEdgeIntersection) {
            return false;   // the water is not visible at all from here
        }

        Float3 projectionFocusPoint;

            //
            //  Find the middle of the object transform
            //      1. use the average of the intersection pts
            //      2. find the point where the camera ray intersects the water plane
            //
            //  Method 1 seems to work much better. It's more stable and produces more
            //  even results at extreme camera angles.
            //
        static bool useIntersectionAverage = true;
        if (useIntersectionAverage) {

            unsigned intersectionCount = 0;
            projectionFocusPoint = Float3(0.f, 0.f, 0.f);
            for (unsigned c=0; c<dimof(intersectionPts); ++c) {
                auto i = intersectionPts[c];
                if (i >= 0.f && i <= 1.f) {
                    projectionFocusPoint += LinearInterpolate(
                        cameraAbsFrustumCorners[edges[c].first], cameraAbsFrustumCorners[edges[c].second], i);
                    ++intersectionCount;
                    assert(std::abs((projectionFocusPoint[2] / float(intersectionCount))-oceanBaseHeight) < 0.1f);
                }
            }
            if (intersectionCount) {
                projectionFocusPoint /= float(intersectionCount);
            }

                //  Prevent the focus point from getting to far in the distance.
                //  If the focus point sits on the far clip plane, it can cause wierd artefacts.
            Float2 xyOffset(projectionFocusPoint[0] - position[0], projectionFocusPoint[1] - position[1]);
            float xyDistance = Magnitude(xyOffset);
            const float maxDistance = Tweakable("OceanProjectionXYOffset", 400.f);
            if (xyDistance > maxDistance) {
                xyOffset *= maxDistance / xyDistance;
                projectionFocusPoint[0] = xyOffset[0] + position[0];
                projectionFocusPoint[1] = xyOffset[1] + position[1];
            }

        } else {
                // use the middle of the camera to find focus point

            float intersectionWithCameraMiddle = RayVsPlane(position, position + forward, waterPlane);
            if (intersectionWithCameraMiddle < 0.f) {
                    // try flipping the Z direction of forward (which will mirror the forward direction)
                intersectionWithCameraMiddle = RayVsPlane(
                    position, position + Float3(forward[0], forward[1], -forward[2]),
                    waterPlane);
            }

                //  If the distance to the intersection point is too far, then cap the distance.
                //  in that case, we need to project the focus point down to the water plane.
            intersectionWithCameraMiddle = std::min(intersectionWithCameraMiddle, Tweakable("OceanProjectionMaxDistance", 10000.f));
            projectionFocusPoint = 
                position + intersectionWithCameraMiddle * forward;
            projectionFocusPoint[2] = oceanBaseHeight;   /// (push down to the water plane (assume normal straight up)
        }

        Float3 projectionViewPoint = 
            position + Float3(0.f, 0.f, Tweakable("OceanProjectionOffset", 100.f));

            //      We want to build a new view matrix, but our projection matrix
            //      should be only slightly modified from the main camera projection
            //      (use the same field of view, but adjust the edges to surround
            //      the intersection points we found before)

        auto viewToWorldTransform = BuildLocalToWorld_Cam(
            projectionViewPoint, Normalize(projectionFocusPoint - projectionViewPoint), up);

        auto worldToView = InvertOrthonormalTransform(viewToWorldTransform);

        float minX = FLT_MAX, minY = FLT_MAX;
        float maxX = -FLT_MAX, maxY = -FLT_MAX;

        float maxXYDisplacement = Tweakable("OceanMaxDisplacementXY", 10.f);
        float maxZDisplacement = Tweakable("OceanMaxDisplacementZ", 15.f);
        Float3 displacementBox[] = 
        {
            Float3(-maxXYDisplacement, -maxXYDisplacement, -maxZDisplacement),
            Float3(-maxXYDisplacement,  maxXYDisplacement, -maxZDisplacement),
            Float3( maxXYDisplacement, -maxXYDisplacement, -maxZDisplacement),
            Float3( maxXYDisplacement,  maxXYDisplacement, -maxZDisplacement),

            Float3(-maxXYDisplacement, -maxXYDisplacement,  maxZDisplacement),
            Float3(-maxXYDisplacement,  maxXYDisplacement,  maxZDisplacement),
            Float3( maxXYDisplacement, -maxXYDisplacement,  maxZDisplacement),
            Float3( maxXYDisplacement,  maxXYDisplacement,  maxZDisplacement)
        };

            //  adjust projection to fit in all of the intersection points
            //  we calculated earlier
        for (unsigned c=0; c<dimof(edges); ++c) {
            if (intersectionPts[c] >= 0.f && intersectionPts[c] <= 1.f) {
                Float3 intersection = LinearInterpolate(
                    cameraAbsFrustumCorners[edges[c].first], 
                    cameraAbsFrustumCorners[edges[c].second], 
                    intersectionPts[c]);

                    //  Check how this point looks after being displaced by the maximum
                    //  amount in each direction.
                for (unsigned q=0; q<dimof(displacementBox); ++q) {
                    Float3 viewSpace = Truncate(worldToView * Float4(intersection + displacementBox[q], 1.f));

                        //  When using the "average" method above, sometimes the camera can be focused at a point
                        //  that moves intersection points behind the camera. When that happens, weird things happen
                        //  to the projection. We would get a better result by using a better focus point for this
                        //  camera (maybe limiting how close the angle of the camera can get to horizontal)
                    if (viewSpace[2] < 0.f) {
                        Float2 v(viewSpace[0] / -viewSpace[2], viewSpace[1] / -viewSpace[2]);
                        minX = std::min(minX, v[0]);
                        minY = std::min(minY, v[1]);
                        maxX = std::max(maxX, v[0]);
                        maxY = std::max(maxY, v[1]);
                    }
                }
            }
        }

        auto gridPerspective = Techniques::PerspectiveProjection(
            maxX, minY, minX, maxY,     // note -- maxX, minX flipped (required to match handiness of normal projection transforms)
            1.0f, 100.f, Techniques::GetDefaultClipSpaceType());

        result._gridProjection = Combine(worldToView, gridPerspective);
        result._gridProjectionOrigin = projectionViewPoint;

            // calculate the far frustum corners (note that the order of the corners is important)
            // our invert calculation is not accurate enough to calculate the near plane coordinates correctly

        Float3 gridProjAbsFrustumCorners[8];
        CalculateAbsFrustumCorners(gridProjAbsFrustumCorners, result._gridProjection);

            // shader needs frustum corners relative to the projection view point
            //      (ie, it's the direction to the frustum corners from the projection
            //      origin).
            //  actually, it doesn't matter if we use near or far frustum corners --
            //  we just need the direction in the shader. Far might be more accurate,
            //  but it's hard to know for sure. Maybe the average is most accurate...?
        result._gridFrustumCorners[0] = Float4(LinearInterpolate(gridProjAbsFrustumCorners[0], gridProjAbsFrustumCorners[4], 0.5f) - projectionViewPoint, 0.f);
        result._gridFrustumCorners[1] = Float4(LinearInterpolate(gridProjAbsFrustumCorners[1], gridProjAbsFrustumCorners[5], 0.5f) - projectionViewPoint, 0.f);
        result._gridFrustumCorners[2] = Float4(LinearInterpolate(gridProjAbsFrustumCorners[2], gridProjAbsFrustumCorners[6], 0.5f) - projectionViewPoint, 0.f);
        result._gridFrustumCorners[3] = Float4(LinearInterpolate(gridProjAbsFrustumCorners[3], gridProjAbsFrustumCorners[7], 0.5f) - projectionViewPoint, 0.f);
        return true;
    }

    void DrawProjectorFrustums( RenderCore::Metal::DeviceContext* context,
                                LightingParserContext& parserContext,
                                Techniques::ProjectionDesc& mainCameraProjection,
                                float oceanBaseHeight)
    {
        GridRenderingConstants gridConstants;
        if (CalculateGridProjection(gridConstants, mainCameraProjection, oceanBaseHeight)) {

            Float3 cameraAbsFrustumCorners[8];
            CalculateAbsFrustumCorners(cameraAbsFrustumCorners, mainCameraProjection._worldToProjection);

            struct Vertex
            {
                Float3 position; unsigned colour;
            }
            lines[] = 
            {
                { gridConstants._gridProjectionOrigin, 0xffff0000 }, { gridConstants._gridProjectionOrigin + Truncate(gridConstants._gridFrustumCorners[0]), 0xffff0000 },
                { gridConstants._gridProjectionOrigin, 0xffff0000 }, { gridConstants._gridProjectionOrigin + Truncate(gridConstants._gridFrustumCorners[1]), 0xffff0000 },
                { gridConstants._gridProjectionOrigin, 0xffff0000 }, { gridConstants._gridProjectionOrigin + Truncate(gridConstants._gridFrustumCorners[2]), 0xffff0000 },
                { gridConstants._gridProjectionOrigin, 0xffff0000 }, { gridConstants._gridProjectionOrigin + Truncate(gridConstants._gridFrustumCorners[3]), 0xffff0000 },

                { cameraAbsFrustumCorners[0], 0xff00ff00 }, { cameraAbsFrustumCorners[4], 0xff00ff00 },
                { cameraAbsFrustumCorners[1], 0xff00ff00 }, { cameraAbsFrustumCorners[5], 0xff00ff00 },
                { cameraAbsFrustumCorners[2], 0xff00ff00 }, { cameraAbsFrustumCorners[6], 0xff00ff00 },
                { cameraAbsFrustumCorners[3], 0xff00ff00 }, { cameraAbsFrustumCorners[7], 0xff00ff00 },

                { cameraAbsFrustumCorners[0], 0xff00ff00 }, { cameraAbsFrustumCorners[1], 0xff00ff00 },
                { cameraAbsFrustumCorners[1], 0xff00ff00 }, { cameraAbsFrustumCorners[3], 0xff00ff00 },
                { cameraAbsFrustumCorners[3], 0xff00ff00 }, { cameraAbsFrustumCorners[2], 0xff00ff00 },
                { cameraAbsFrustumCorners[2], 0xff00ff00 }, { cameraAbsFrustumCorners[0], 0xff00ff00 }
            };
            VertexBuffer temporaryBuffer(lines, sizeof(lines));

            context->Bind(MakeResourceList(temporaryBuffer), sizeof(Vertex), 0);
            auto& shader = ::Assets::GetAsset<ShaderProgram>(
                "game/xleres/forward/illum.vsh:main:vs_*", 
                "game/xleres/forward/illum.psh:main:ps_*",
                "GEO_HAS_COLOUR=1");
            auto localTransform = Techniques::MakeLocalTransform(
                Identity<Float4x4>(), 
                ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld));
            context->BindVS(MakeResourceList(parserContext.GetGlobalTransformCB(), ConstantBuffer(&localTransform, sizeof(localTransform))));
            context->Bind(shader);
            context->Bind(BoundInputLayout(GlobalInputLayouts::PC, shader));
            context->Bind(Topology::LineList);
            context->Draw(dimof(lines));
        }
    }

    class WaterNoiseTexture
    {
    public:
        class Desc
        {
        public:
            float _hgrid, _gain, _lacunarity;
            unsigned _octaves;
            Desc(float hgrid, float gain, float lacunarity, unsigned octaves);
            Desc();
        };

        Metal::ShaderResourceView _srv;

        WaterNoiseTexture(const Desc& desc);
    };

    WaterNoiseTexture::Desc::Desc(float hgrid, float gain, float lacunarity, unsigned octaves)
    {
        _hgrid = hgrid;
        _gain = gain;
        _lacunarity = lacunarity;
        _octaves = octaves;
    }

    WaterNoiseTexture::Desc::Desc() 
    {
        _hgrid = Tweakable("WaterNoiseHGrid", 5.f);
        _gain = Tweakable("WaterNoiseGain", .75f);
        _lacunarity = Tweakable("WaterNoiseLacunarity", 2.1042f);
        _octaves = Tweakable("WaterNoiseOctaves", 7);
    }

    static float fbm(Float4 pos, float hgrid, float gain, float lacunarity, int octaves)
    {
	    float total = 0.0f;
	    float frequency = 1.0f/(float)hgrid;
	    float amplitude = 1.f;
	    for (int i = 0; i < octaves; ++i) {
		    total += SimplexNoise(Float4(pos * frequency)) * amplitude;
		    frequency *= lacunarity;
		    amplitude *= gain;
	    }

	    return total;
    }

    WaterNoiseTexture::WaterNoiseTexture(const Desc& desc)
    {
        using namespace BufferUploads;
        const unsigned width = 512, height = 512;
        auto tDesc = CreateDesc(
            BindFlag::ShaderResource, 0, GPUAccess::Read,
            BufferUploads::TextureDesc::Plain2D(width, height, Metal::NativeFormat::R8G8_UNORM),
            "WaterNoise");

        static float scale0 = 10.f;
        static float scale1 = 10.f;
        static float offset1 = 15.f;

            // Here is an interesting way to create a wrapping noise texture...
            // We're going to build the texture is 4D. The two coordinates in
            // the final 2D output texture are each tied to 2 coordinates in 4D
            // space. As we travel linearly in a cardinal direction in 2D space,
            // we will travel around in a circle in 4D space. The circle will
            // eventually wrap back around into itself -- and so in the output
            // texture it will wrap at that point! We have one circle for X, and 
            // another for Y -- and so the final texture wraps in all directions.
        auto pkt = CreateEmptyPacket(tDesc);
        auto data = (const uint8*)pkt->GetData(0);
        for (unsigned y=0; y<height; ++y)
            for (unsigned x=0; x<width; ++x) {
                auto* d = PtrAdd(data, (x + (y*width)) * 2);
                float a0 = x / float(width) * 2.f * 3.14159f;
                float a1 = y / float(height) * 2.f * 3.14159f;

                float noiseValue0 = fbm(
                    Float4(scale0 * XlCos(a0), scale0 * XlSin(a0), scale0 * XlCos(a1), scale0 * XlSin(a1)),
                    desc._hgrid, desc._gain, desc._lacunarity, desc._octaves);

                float noiseValue1 = fbm(
                    Float4(scale1 * XlCos(a0) + offset1, scale1 * XlSin(a0) + offset1, scale1 * XlCos(a1) + offset1, scale1 * XlSin(a1) + offset1),
                    desc._hgrid, desc._gain, desc._lacunarity, desc._octaves);

                ((uint8*)d)[0] = uint8(255.f * Clamp(.5f + 0.5f * noiseValue0, 0.f, 1.f));
                ((uint8*)d)[1] = uint8(255.f * Clamp(.5f + 0.5f * noiseValue1, 0.f, 1.f));
            }

        auto texture = GetBufferUploads().Transaction_Immediate(tDesc, pkt.get());
        _srv = Metal::ShaderResourceView(texture->GetUnderlying());
    }

        //   ================================================================================   //
                //////////////////////////////////////////////////////////////////////////

    void RenderOceanSurface(RenderCore::Metal::DeviceContext* context, 
                            LightingParserContext& parserContext,
                            const OceanSettings& oceanSettings, 
                            const OceanLightingSettings& oceanLightingSettings,
                            FFTBufferBox& fftBuffer, ShallowWaterSim* shallowWater, 
                            RefractionsBuffer* refractionsBox, 
                            RenderCore::Metal::ShaderResourceView& depthBufferSRV,
                            int techniqueIndex)
    {
        if (!Tweakable("OceanRenderSurface", true)) {
            return;
        }

        //////////////////////////////////////////////////////////////////////////////

        const unsigned screenSpaceGridScale = Tweakable("OceanScreenSpaceGridScale", 6);
        const float shallowGridPhysicalDimension = Tweakable("OceanShallowPhysicalDimension", 256.f);

        const unsigned dimensions = oceanSettings._gridDimensions;
        const Float2 physicalDimensions = Float2(oceanSettings._physicalDimensions, oceanSettings._physicalDimensions);
        const float currentTime = parserContext.GetSceneParser()->GetTimeValue();
        auto materialConstants = Internal::BuildOceanMaterialConstants(oceanSettings, shallowGridPhysicalDimension);
        auto renderingConstants = BuildOceanRenderingConstants(oceanSettings, OceanWorldToReflection, currentTime);

        //////////////////////////////////////////////////////////////////////////////
            //  draw world space grid (simple patch)

            //    -- todo --    don't align the triangles the same way in every grid
            //                  swap triangle direction with some interesting pattern

        unsigned patchWidth, patchHeight;
        if (screenSpaceGridScale!=0) {
            ViewportDesc viewport(*context);
            patchWidth = unsigned(std::ceil(float(viewport.Width / screenSpaceGridScale / 16)) * 16);
            patchHeight = unsigned(std::ceil(float(viewport.Height / screenSpaceGridScale / 16)) * 16);
        } else {
            patchWidth = patchHeight = dimensions;
        }
        auto& simplePatchBox = Techniques::FindCachedBox<SimplePatchBox>(SimplePatchBox::Desc(patchWidth, patchHeight, true));
        context->Bind(simplePatchBox._simplePatchIndexBuffer, NativeFormat::R32_UINT);

        //////////////////////////////////////////////////////////////////////////////

        const bool pause = Tweakable("Pause", false);
        GridRenderingConstants gridConstants;
        XlSetMemory(&gridConstants, 0, sizeof(gridConstants));
        gridConstants._gridPatchWidth = patchWidth;
        gridConstants._gridPatchHeight = patchHeight;
        static Techniques::ProjectionDesc savedProjection;
        if (!pause) {
            savedProjection = parserContext.GetProjectionDesc();
        }
        if (!CalculateGridProjection(gridConstants, savedProjection, oceanSettings._baseHeight)) {
            return;
        }

        //////////////////////////////////////////////////////////////////////////////
        
        unsigned skyProjectionType = 0;
        auto skyTexture = parserContext.GetSceneParser()->GetGlobalLightingDesc()._skyTexture;
        if (skyTexture[0]) {
            skyProjectionType = SkyTexture_BindPS(context, parserContext, skyTexture, 6);
        }

        bool doDynamicReflection = OceanReflectionResource.GetUnderlying() != nullptr;

        //////////////////////////////////////////////////////////////////////////////

        ConstantBuffer oceanMaterialConstants(&materialConstants, sizeof(materialConstants));
        ConstantBuffer oceanRenderingConstants(&renderingConstants, sizeof(renderingConstants));
        ConstantBuffer oceanGridConstants(&gridConstants, sizeof(gridConstants));
        ConstantBuffer oceanLightingConstants(&oceanLightingSettings, sizeof(OceanLightingSettings));

        static auto HashMaterialConstants           = Hash64("OceanMaterialSettings");
        static auto HashGridConstants               = Hash64("GridConstants");
        static auto HashRenderingConstants          = Hash64("OceanRenderingConstants");
        static auto HashLightingConstants           = Hash64("OceanLightingSettings");
        static auto HashDynamicReflectionTexture    = Hash64("DynamicReflectionTexture");
        static auto HashSurfaceSpecularity          = Hash64("SurfaceSpecularity");
        const bool useWireframeRender               = Tweakable("OceanRenderWireframe", false);
        if (!useWireframeRender) {

            auto& shaderType = ::Assets::GetAssetDep<Techniques::ShaderType>("game/xleres/ocean/oceanmaterial.txt");

            ParameterBox materialParameters;
            materialParameters.SetParameter((const utf8*)"MAT_USE_DERIVATIVES_MAP", unsigned(fftBuffer._useDerivativesMapForNormals));
            materialParameters.SetParameter((const utf8*)"MAT_USE_SHALLOW_WATER", shallowWater && unsigned(shallowWater->IsActive()));
            materialParameters.SetParameter((const utf8*)"MAT_DO_REFRACTION", refractionsBox!=nullptr);
            materialParameters.SetParameter((const utf8*)"MAT_DYNAMIC_REFLECTION", int(doDynamicReflection));
            materialParameters.SetParameter((const utf8*)"SHALLOW_WATER_TILE_DIMENSION", shallowWater?shallowWater->GetGridDimension():0);
            materialParameters.SetParameter((const utf8*)"SKY_PROJECTION", skyProjectionType);
            ParameterBox dummyBox;
            const ParameterBox* state[] = {
                &MaterialState_Blank, &parserContext.GetTechniqueContext()._globalEnvironmentState,
                &parserContext.GetTechniqueContext()._runtimeState, &materialParameters
            };
            
            Techniques::TechniqueInterface techniqueInterface;
            Techniques::TechniqueContext::BindGlobalUniforms(techniqueInterface);
            techniqueInterface.BindConstantBuffer(HashMaterialConstants, 0, 1);
            techniqueInterface.BindConstantBuffer(HashGridConstants, 1, 1);
            techniqueInterface.BindConstantBuffer(HashRenderingConstants, 2, 1);
            techniqueInterface.BindConstantBuffer(HashLightingConstants, 3, 1);
            techniqueInterface.BindShaderResource(HashDynamicReflectionTexture, 0, 1);
            techniqueInterface.BindShaderResource(HashSurfaceSpecularity, 1, 1);

            auto variation = shaderType.FindVariation(techniqueIndex, state, techniqueInterface);
            if (variation._shaderProgram != nullptr) {
                context->Bind(*variation._shaderProgram);
                if (variation._boundLayout) {
                    context->Bind(*variation._boundLayout);
                }

                //auto& surfaceSpecularity = ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("game/xleres/defaultresources/waternoise.png");
                auto& surfaceSpecularity = Techniques::FindCachedBox2<WaterNoiseTexture>();
                const ConstantBuffer* prebuiltBuffers[] = { &oceanMaterialConstants, &oceanGridConstants, &oceanRenderingConstants, &oceanLightingConstants };
                const ShaderResourceView* srvs[]        = { &OceanReflectionResource, &surfaceSpecularity._srv };
                variation._boundUniforms->Apply(
                    *context, 
                    parserContext.GetGlobalUniformsStream(),
                    UniformsStream(nullptr, prebuiltBuffers, dimof(prebuiltBuffers), srvs, dimof(srvs)));
            }

        } else {

            auto& patchRender = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/Ocean/OceanPatch.vsh:main:vs_*",
                "game/xleres/solidwireframe.gsh:main:gs_*",
                "game/xleres/solidwireframe.psh:outlinepatch:ps_*",
                "SOLIDWIREFRAME_TEXCOORD=1");
            BoundUniforms boundUniforms(patchRender);
            Techniques::TechniqueContext::BindGlobalUniforms(boundUniforms);
            boundUniforms.BindConstantBuffer(HashMaterialConstants, 0, 1);
            boundUniforms.BindConstantBuffer(HashGridConstants, 1, 1);
            boundUniforms.BindConstantBuffer(HashRenderingConstants, 2, 1);
            const ConstantBuffer* prebuiltBuffers[]             = { &oceanMaterialConstants, &oceanGridConstants, &oceanRenderingConstants };
            boundUniforms.Apply(*context, 
                parserContext.GetGlobalUniformsStream(),
                UniformsStream(nullptr, prebuiltBuffers, dimof(prebuiltBuffers)));
            context->Bind(patchRender);

        }

        context->BindVS(MakeResourceList(   fftBuffer._workingTextureRealShaderResource, 
                                            fftBuffer._workingTextureXRealShaderResource, 
                                            fftBuffer._workingTextureYRealShaderResource));
        context->BindPS(MakeResourceList(1, fftBuffer._normalsTextureShaderResource));
        context->BindPS(MakeResourceList(3, 
            fftBuffer._foamQuantitySRV[OceanBufferCounter&1], 
            // ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("game/xleres/defaultresources/waternoise.png").GetShaderResource()
            Techniques::FindCachedBox2<WaterNoiseTexture>()._srv));
        if (shallowWater) {
            shallowWater->BindForOceanRender(*context, OceanBufferCounter);
        }
        if (refractionsBox) {
                //  only need the depth buffer if we're doing refractions
            context->BindPS(MakeResourceList(9, refractionsBox->_refractionsFrontSRV, depthBufferSRV));
        }
        auto& perlinNoiseRes = Techniques::FindCachedBox<PerlinNoiseResources>(PerlinNoiseResources::Desc());
        context->BindVS(MakeResourceList(12, perlinNoiseRes._gradShaderResource, perlinNoiseRes._permShaderResource));

        context->Bind(Techniques::CommonResources()._dssReadWrite);    // make sure depth read and write are enabled
        SetupVertexGeneratorShader(context);        // (disable vertex input)
        context->Bind(Topology::TriangleList);
        context->DrawIndexed(simplePatchBox._simplePatchIndexCount);

        context->UnbindVS<ShaderResourceView>(0, 3);
        context->UnbindPS<ShaderResourceView>(0, 10);

            //  some debugging information
        if (Tweakable("OceanDebugging", false)) {
            parserContext._pendingOverlays.push_back(
                std::bind(  &OceanSurface_DrawDebugging, 
                            std::placeholders::_1, std::placeholders::_2, oceanSettings, fftBuffer));
        }

        if (pause) {
            DrawProjectorFrustums(context, parserContext, savedProjection, oceanSettings._baseHeight);
        }
    }

    static void OceanSurface_DrawDebugging(   RenderCore::Metal::DeviceContext* context, 
                                              LightingParserContext& parserContext,
                                              const OceanSettings& oceanSettings,
                                              FFTBufferBox& fftBuffer)
    {
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
        context->Bind(Techniques::CommonResources()._blendStraightAlpha);
        context->Bind(::Assets::GetAssetDep<ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*", "game/xleres/Ocean/FFTDebugging.psh:main:ps_*"));
        context->BindPS(MakeResourceList(
            fftBuffer._workingTextureRealShaderResource, fftBuffer._workingTextureImaginaryShaderResource,
            calmSpectrum._inputRealShaderResource, calmSpectrum._inputImaginaryShaderResource,
            strongSpectrum._inputRealShaderResource, strongSpectrum._inputImaginaryShaderResource));
        context->Draw(4);
    }

        ////////////////////////////////

    void Ocean_Execute( DeviceContext* context, LightingParserContext& parserContext,
                        const OceanSettings& settings,
                        const OceanLightingSettings& lightingSettings,
                        ShaderResourceView& depthBufferSRV)
    {
        if (!settings._enable) return;

        TRY {

                //      We need to take a copy of the back buffer for refractions
                //      we could blur it here -- but is it better blurred or sharp?
            ViewportDesc mainViewportDesc(*context);
            auto& refractionBox = Techniques::FindCachedBox<RefractionsBuffer>(
                RefractionsBuffer::Desc(unsigned(mainViewportDesc.Width/2.f), unsigned(mainViewportDesc.Height/2.f)));
            BuildRefractionsTexture(context, parserContext, refractionBox, 1.6f);

            auto duplicatedDepthBuffer = DuplicateResource(
                context->GetUnderlying(), ExtractResource<ID3D::Resource>(depthBufferSRV.GetUnderlying()).get());
            ShaderResourceView secondaryDepthBufferSRV(
                duplicatedDepthBuffer.get(), (NativeFormat::Enum)DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
            // context->GetUnderlying()->CopyResource(
            //     mainTargets._secondaryDepthBufferTexture, mainTargets._msaaDepthBufferTexture);

            const unsigned simulatingGridsCount = 24;
            const float shallowGridPhysicalDimension = Tweakable("OceanShallowPhysicalDimension", 256.f);
            const unsigned shallowGridDimension = Tweakable("OceanShallowGridDimension", 128);
            const bool useDerivativesMap = Tweakable("OceanNormalsBasedOnDerivatives", true);
            const bool doShallowWater = Tweakable("OceanDoShallowWater", false);
            const bool usePipeModel = Tweakable("OceanShallowPipeModel", false);
            auto& fftBuffer = Techniques::FindCachedBox<FFTBufferBox>(FFTBufferBox::Desc(
                settings._gridDimensions, settings._gridDimensions, useDerivativesMap));
            ShallowWaterSim* shallowWaterBox = nullptr;

            context->Bind(Techniques::CommonResources()._dssReadOnly);   // write disabled
            UpdateOceanSurface(context, parserContext, settings, fftBuffer);
            if (doShallowWater && MainSurfaceHeightsProvider) {
                shallowWaterBox = &Techniques::FindCachedBox<ShallowWaterSim>(
                    ShallowWaterSim::Desc(shallowGridDimension, simulatingGridsCount, usePipeModel, false));
                shallowWaterBox->ExecuteSim(
                    ShallowWaterSim::SimulationContext(
                        *context, settings, shallowGridPhysicalDimension,
                        MainSurfaceHeightsProvider,
                        &fftBuffer._workingTextureRealShaderResource,
                        ShallowWaterSim::BorderMode::GlobalWaves),
                    parserContext,
                    OceanBufferCounter);
            }
            RenderOceanSurface(
                context, parserContext, settings, lightingSettings, fftBuffer, shallowWaterBox, 
                &refractionBox, 
                //mainTargets._msaaDepthBufferSRV,
                secondaryDepthBufferSRV,
                TechniqueIndex_General);

            ++OceanBufferCounter;

        } 
        CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH_END
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    OceanSettings::OceanSettings()
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

    OceanLightingSettings::OceanLightingSettings()
    {
        _specularReflectionBrightness = Float3(2.8f, 2.4f, 2.1f);
        _foamBrightness = 0.08f;
        _opticalThickness = Float3(0.05f, 0.042f, 0.038f);
        _skyReflectionBrightness = 1.355f;

        _specularPower = 128.f;
        _upwellingScale = .075f;
        _refractiveIndex = 1.333f;
        _reflectionBumpScale = 0.1f;

        _detailNormalFrequency = 6.727f;
        _specularityFrequency = 7.1f;
        _matSpecularMin = .3f; _matSpecularMax = .6f;

        _matRoughness = .15f;
        _dummy[0] = _dummy[1] = _dummy[2] = 0;
    }

}