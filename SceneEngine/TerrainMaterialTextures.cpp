// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainMaterialTextures.h"
#include "TerrainMaterial.h"
#include "SceneEngineUtils.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../BufferUploads/DataPacket.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/Resource.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Assets/DeferredShaderResource.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/BitUtils.h"
#include "../Utility/StringFormat.h"

#include "../RenderCore/DX11/Metal/DX11Utils.h"
#include "../../Foreign/DirectXTex/DirectXTex/DirectXTex.h"

namespace SceneEngine
{
    using namespace RenderCore;

    static Metal::DeviceContext GetImmediateContext()
    {
        ID3D::DeviceContext* immContextTemp = nullptr;
        Metal::ObjectFactory().GetUnderlying()->GetImmediateContext(&immContextTemp);
        intrusive_ptr<ID3D::DeviceContext> immContext = moveptr(immContextTemp);
        return RenderCore::Metal::DeviceContext(std::move(immContext));
    }

    template <typename Type>
        static const Type& GetAssetImmediate(const char initializer[])
    {
        for (;;) {
            TRY {
                return ::Assets::GetAsset<Type>(initializer);
            } CATCH (::Assets::Exceptions::PendingResource&) {
                ::Assets::Services::GetAsyncMan().Update();
            } CATCH_END
        }
    }

    static void LoadTextureIntoArray(ID3D::Resource* destinationArray, const char sourceFile[], unsigned arrayIndex)
    {
            //      We want to load the given texture, and merge it into
            //      the texture array. We have to do this synchronously, otherwise the scheduling
            //      is too awkward
            //      We're also using the "immediate context" -- so this should be run in 
            //      the main rendering thread (or whatever thread is associated with the 
            //      immediate context)

        auto inputTexture = RenderCore::Assets::DeferredShaderResource::LoadImmediately(sourceFile);
        auto inputRes = Metal::ExtractResource<Metal::Underlying::Resource>(inputTexture.GetUnderlying());

        Metal::TextureDesc2D destinationDesc(destinationArray);
        const auto dstMipCount = destinationDesc.MipLevels;
        auto dstWidthPower = (int)IntegerLog2(destinationDesc.Width);

        if (!IsPowerOfTwo(destinationDesc.Width) || !IsPowerOfTwo(destinationDesc.Height)) {
            // only power-of-two textures supported (too difficult to merge them into a atlas otherwise)
            ThrowException(::Assets::Exceptions::InvalidResource(sourceFile, "Expecting power of two texture for terrain texturing"));
        }
        if (destinationDesc.Width != destinationDesc.Height) {
            ThrowException(::Assets::Exceptions::InvalidResource(sourceFile, "Expecting square texture for terrain texturing"));
        }

        Metal::TextureDesc2D sourceDesc(inputRes.get());
        auto srcWidthPower = (int)IntegerLog2(sourceDesc.Width);
        auto mipDifference = srcWidthPower - dstWidthPower;

        auto context = GetImmediateContext();
        for (unsigned m=0; m<dstMipCount; ++m) {
            
            auto sourceMip = m + mipDifference;
            if (sourceMip < 0) {

                LogWarning << 
                    "LoadTextureIntoArray -- performing resample on texture (" << sourceFile << "). All textures in the array must be the same size!\n";

                    //  We have to up-sample to get the same number of mips
                    //  Using the highest LOD from the source texture, resample into
                    //  a default texture
                const unsigned expectedWidth = destinationDesc.Width >> m;
                const unsigned expectedHeight = destinationDesc.Height >> m;

                auto destFormat = destinationDesc.Format;
                auto resamplingFormat = destFormat;
                auto compressionType = RenderCore::Metal::GetCompressionType((RenderCore::Metal::NativeFormat::Enum)destFormat);
                if (compressionType == RenderCore::Metal::FormatCompressionType::BlockCompression) {
                        // resampling via a higher precision buffer -- just for kicks.
                    resamplingFormat = (DXGI_FORMAT)RenderCore::Metal::NativeFormat::R16G16B16A16_FLOAT;
                }

                auto& bufferUploads = GetBufferUploads();
                using namespace BufferUploads;
                BufferDesc desc;
                desc._type = BufferDesc::Type::Texture;
                desc._bindFlags = BindFlag::UnorderedAccess;
                desc._cpuAccess = 0;
                desc._gpuAccess = GPUAccess::Read|GPUAccess::Write;
                desc._allocationRules = 0;
                desc._textureDesc = BufferUploads::TextureDesc::Plain2D(expectedWidth, expectedHeight, resamplingFormat);
                XlCopyString(desc._name, "ResamplingTexture");
                auto resamplingBuffer = bufferUploads.Transaction_Immediate(desc);
                Metal::UnorderedAccessView uav(resamplingBuffer->GetUnderlying());

                auto& resamplingShader = GetAssetImmediate<RenderCore::Metal::ComputeShader>("game/xleres/basic.csh:Resample:cs_*");
                context.Bind(resamplingShader);
                context.BindCS(MakeResourceList(uav));
                context.BindCS(MakeResourceList(inputTexture));
                context.Dispatch(expectedWidth/8, expectedHeight/8);
                context.UnbindCS<Metal::UnorderedAccessView>(0, 1);

                if (resamplingFormat!=destFormat) {
                        // We have to re-compress the texture. It's annoying, but we can use a library to do it
                    auto rawData = bufferUploads.Resource_ReadBack(*resamplingBuffer);
                    DirectX::Image image;
                    image.width = expectedWidth;
                    image.height = expectedHeight;
                    image.format = resamplingFormat;
                    image.rowPitch = rawData->GetPitches()._rowPitch;
                    image.slicePitch = rawData->GetPitches()._slicePitch;
                    image.pixels = (uint8_t*)rawData->GetData();

                    intrusive_ptr<ID3D::Device> device;
                    {
                        ID3D::Device* deviceTemp = nullptr;
                        context.GetUnderlying()->GetDevice(&deviceTemp);
                        device = intrusive_ptr<ID3D::Device>(deviceTemp, false);
                    }

                    DirectX::ScratchImage compressedImage;
                    auto hresult = DirectX::Compress(
                        image, destFormat, DirectX::TEX_COMPRESS_DITHER | DirectX::TEX_COMPRESS_SRGB, 0.f, compressedImage);
                    assert(SUCCEEDED(hresult)); (void)hresult;
                    assert(compressedImage.GetImageCount()==1);
                    
                    auto& final = *compressedImage.GetImage(0,0,0);
                    desc._bindFlags = BindFlag::ShaderResource;
                    desc._textureDesc._nativePixelFormat = destFormat;
                    auto compressedBuffer = bufferUploads.Transaction_Immediate(
                            desc, BufferUploads::CreateBasicPacket(final.slicePitch, final.pixels, TexturePitches(unsigned(final.rowPitch), unsigned(final.slicePitch))).get());

                    resamplingBuffer = compressedBuffer;   
                }

                context.GetUnderlying()->CopySubresourceRegion(
                    destinationArray, D3D11CalcSubresource(m, arrayIndex, dstMipCount),
                    0, 0, 0, resamplingBuffer->GetUnderlying(), 0, nullptr);

            } else {

                context.GetUnderlying()->CopySubresourceRegion(
                    destinationArray, D3D11CalcSubresource(m, arrayIndex, dstMipCount),
                    0, 0, 0, inputRes.get(), D3D11CalcSubresource(sourceMip, 0, sourceDesc.MipLevels), nullptr);

            }
        }
    }

    static void FillWhite(ID3D::Resource* destinationArray, ID3D::Resource* sourceResource, unsigned arrayIndex)
    {
            // copy dummy white data into all of the mip levels of the given array index in the
            // destination resource
        Metal::TextureDesc2D destinationDesc(destinationArray);
        const auto mipCount = destinationDesc.MipLevels;

        auto context = GetImmediateContext();
        for (unsigned m=0; m<mipCount; ++m) {
            const unsigned mipWidth = std::max(destinationDesc.Width >> m, 4u);
            const unsigned mipHeight = std::max(destinationDesc.Height >> m, 4u);

            D3D11_BOX srcBox;
            srcBox.left = srcBox.top = srcBox.front = 0;
            srcBox.right = mipWidth;
            srcBox.bottom = mipHeight;
            srcBox.back = 1;
            context.GetUnderlying()->CopySubresourceRegion(
                destinationArray, D3D11CalcSubresource(m, arrayIndex, mipCount),
                0, 0, 0, sourceResource, 0, &srcBox);
        }
    }

    TerrainMaterialTextures::TerrainMaterialTextures()
    {
        _strataCount = 0;
    }

    TerrainMaterialTextures::TerrainMaterialTextures(const TerrainMaterialScaffold& scaffold)
    {
        auto strataCount = (unsigned)scaffold._strata.size();

        auto texturingConstants = std::make_unique<Float4[]>(strataCount*2);
        std::fill(texturingConstants.get(), &texturingConstants[strataCount*2], Float4(1.f, 1.f, 1.f, 1.f));

            //  Each texture is stored as a separate file on disk. But we need to copy them
            //  all into a texture array.

        using namespace BufferUploads;
        BufferDesc desc;
        desc._type = BufferDesc::Type::Texture;
        desc._bindFlags = BindFlag::ShaderResource;
        desc._cpuAccess = 0;
        desc._gpuAccess = GPUAccess::Read;
        desc._allocationRules = 0;
        XlCopyString(desc._name, "TerrainMaterialTextures");

        const auto texturesPerStrata = dimof(((TerrainMaterialScaffold::Strata*)nullptr)->_texture);

            // todo -- there are some SRGB problems here!
            //          should we be using SRGB input texture format?
        desc._textureDesc = BufferUploads::TextureDesc::Plain2D(
            scaffold._diffuseDims[0], scaffold._diffuseDims[1], Metal::NativeFormat::BC1_UNORM, 
            (uint8)IntegerLog2(std::max(scaffold._diffuseDims[0], scaffold._diffuseDims[1]))-1, uint8(texturesPerStrata * strataCount));
        auto diffuseTextureArray = GetBufferUploads().Transaction_Immediate(desc)->AdoptUnderlying();

        desc._textureDesc = BufferUploads::TextureDesc::Plain2D(
            scaffold._normalDims[0], scaffold._normalDims[1], Metal::NativeFormat::BC5_UNORM, 
            (uint8)IntegerLog2(std::max(scaffold._normalDims[0], scaffold._normalDims[1]))-1, uint8(texturesPerStrata * strataCount));
        auto normalTextureArray = GetBufferUploads().Transaction_Immediate(desc)->AdoptUnderlying();

        desc._textureDesc = BufferUploads::TextureDesc::Plain2D(
            scaffold._paramDims[0], scaffold._paramDims[1], Metal::NativeFormat::BC1_UNORM, 
            (uint8)IntegerLog2(std::max(scaffold._paramDims[0], scaffold._paramDims[1]))-1, uint8(texturesPerStrata * strataCount));
        auto specularityTextureArray = GetBufferUploads().Transaction_Immediate(desc)->AdoptUnderlying();

        desc._textureDesc = BufferUploads::TextureDesc::Plain2D(
            scaffold._paramDims[0], scaffold._paramDims[1], Metal::NativeFormat::BC1_UNORM);
        auto tempBuffer = CreateEmptyPacket(desc);
        {
            uint16 blankColor = ((0x1f/2) << 11) | ((0x3f/2) << 5) | (0x1f/2);
            struct BC1Block { uint16 c0; uint16 c1; uint32 t; } block { blankColor, blankColor, 0 };

            auto dataSize = tempBuffer->GetDataSize();
            assert((dataSize % sizeof(BC1Block))==0);
            auto blockCount = dataSize / sizeof(BC1Block);
            auto data = (BC1Block*)tempBuffer->GetData();
            for (unsigned c=0; c<blockCount; ++c) data[c] = block;
        }
        auto dummyWhiteBuffer = GetBufferUploads().Transaction_Immediate(desc, tempBuffer.get())->AdoptUnderlying();

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(_validationCallback, scaffold.GetDependencyValidation());

        unsigned strataIndex = 0;
        for (auto s=scaffold._strata.cbegin(); s!=scaffold._strata.cend(); ++s, ++strataIndex) {

            for (unsigned t=0; t<texturesPerStrata; ++t) {
                    //  This is a input texture. We need to build the 
                    //  diffuse, specularity and normal map names from this texture name
                TRY { 
                    ::Assets::ResChar resolvedFile[MaxPath];
                    scaffold._searchRules.ResolveFile(resolvedFile, dimof(resolvedFile), StringMeld<MaxPath, ::Assets::ResChar>() << s->_texture[t] << "_df.dds");
                    if (resolvedFile[0]) {
                        LoadTextureIntoArray(diffuseTextureArray.get(), resolvedFile, (strataIndex * texturesPerStrata) + t);
                        RegisterFileDependency(_validationCallback, resolvedFile);
                    }
                } CATCH (const ::Assets::Exceptions::InvalidResource&) {}
                CATCH_END
                            
                TRY { 
                    ::Assets::ResChar resolvedFile[MaxPath];
                    scaffold._searchRules.ResolveFile(resolvedFile, dimof(resolvedFile), StringMeld<MaxPath, ::Assets::ResChar>() << s->_texture[t] << "_ddn.dds");
                    if (resolvedFile[0]) {
                        LoadTextureIntoArray(normalTextureArray.get(), resolvedFile, (strataIndex * texturesPerStrata) + t);
                        RegisterFileDependency(_validationCallback, resolvedFile);
                    }
                } CATCH (const ::Assets::Exceptions::InvalidResource&) {}
                CATCH_END
                                
                bool fillInWhiteSpecular = false;
                TRY { 
                    ::Assets::ResChar resolvedFile[MaxPath];
                    scaffold._searchRules.ResolveFile(resolvedFile, dimof(resolvedFile), StringMeld<MaxPath, ::Assets::ResChar>() << s->_texture[t] << "_sp.dds");
                    if (resolvedFile[0]) {
                        LoadTextureIntoArray(specularityTextureArray.get(), resolvedFile, (strataIndex * texturesPerStrata) + t);
                        RegisterFileDependency(_validationCallback, resolvedFile);
                    }
                } CATCH (const ::Assets::Exceptions::InvalidResource&) {
                    fillInWhiteSpecular = true;
                } CATCH_END

                if (fillInWhiteSpecular)
                    FillWhite(specularityTextureArray.get(), dummyWhiteBuffer.get(), (strataIndex * texturesPerStrata) + t);
            }

            texturingConstants[strataIndex] = Float4(s->_endHeight, s->_endHeight, s->_endHeight, s->_endHeight);

            Float4 mappingConstant = Float4(1.f, 1.f, 1.f, 1.f);
            for (unsigned c=0; c<texturesPerStrata; ++c) {
                texturingConstants[strataCount + strataIndex][c] = 1.f / s->_mappingConstant[c];
            }
        }

        RenderCore::Metal::ShaderResourceView diffuseSrv(diffuseTextureArray.get());
        RenderCore::Metal::ShaderResourceView normalSrv(normalTextureArray.get());
        RenderCore::Metal::ShaderResourceView specularitySrv(specularityTextureArray.get());
        RenderCore::Metal::ConstantBuffer texContBuffer(texturingConstants.get(), sizeof(Float4)*strataCount*2);

        _textureArray[Diffuse] = std::move(diffuseTextureArray);
        _textureArray[Normal] = std::move(normalTextureArray);
        _textureArray[Specularity] = std::move(specularityTextureArray);
        _srv[Diffuse] = std::move(diffuseSrv);
        _srv[Normal] = std::move(normalSrv);
        _srv[Specularity] = std::move(specularitySrv);
        _texturingConstants = std::move(texContBuffer);
        _strataCount = strataCount;
    }

    TerrainMaterialTextures::~TerrainMaterialTextures() {}

}

