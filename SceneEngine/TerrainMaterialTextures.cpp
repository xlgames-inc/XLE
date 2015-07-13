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

    static unsigned AddName(std::vector<::Assets::rstring>& atlas, const ::Assets::rstring& name)
    {
        unsigned atlasIndex = ~0u;
        auto e = std::find(atlas.cbegin(), atlas.cend(), name);
        if (e == atlas.cend()) {
            atlas.push_back(name);
            atlasIndex = unsigned(atlas.size()-1);
        } else {
            atlasIndex = (unsigned)std::distance(atlas.cbegin(), e);
        }
        return atlasIndex;
    }

    TerrainMaterialTextures::TerrainMaterialTextures(const TerrainMaterialScaffold& scaffold, bool useGradFlagMaterials)
    {
        _strataCount = 0;

            //  Each texture is stored as a separate file on disk. But we need to copy them
            //  all into a texture array.

        std::vector<::Assets::rstring> atlasTextureNames;
        std::vector<::Assets::rstring> procTextureNames;
        std::vector<uint8> texturingConstants;
        std::vector<uint8> procTextureConstants;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        if (useGradFlagMaterials) {

            const auto texturesPerMaterial = dimof(((TerrainMaterialScaffold::GradFlagMaterial*)nullptr)->_texture);
            auto highestMaterialId = 0u;
            for (auto& mat:scaffold._gradFlagMaterials)
                highestMaterialId = std::max(highestMaterialId, mat._id);

            if ((highestMaterialId+1) > 32)
                Throw(::Exceptions::BasicLabel("Too many material ids in terrain texturing"));

            texturingConstants.resize(sizeof(Float4) * (highestMaterialId+1) * texturesPerMaterial);
            auto* tcStart = (Float4*)AsPointer(texturingConstants.begin());
            std::fill(tcStart, &tcStart[(highestMaterialId+1) * texturesPerMaterial], Float4(1.f, 1.f, 1.f, 1.f));

            for (auto m=scaffold._gradFlagMaterials.cbegin(); m!=scaffold._gradFlagMaterials.cend(); ++m) {
                for (unsigned t=0; t<texturesPerMaterial; ++t) {

                    auto p = std::find_if(
                        scaffold._procTextures.cbegin(), scaffold._procTextures.cend(),
                        [m, t](const TerrainMaterialScaffold::ProcTextureSetting& s)
                        { return s._name == m->_texture[t]; });
                    if (p != scaffold._procTextures.cend()) {

                        auto procTextureId = AddName(procTextureNames, m->_texture[t]);
                        unsigned type = 1;
                        tcStart[m->_id * texturesPerMaterial + t] = Float4(
                            m->_mappingConstant[t], m->_mappingConstant[t],
                            *reinterpret_cast<float*>(&type), *reinterpret_cast<float*>(&procTextureId));

                    } else {

                        auto atlasIndex = AddName(atlasTextureNames, m->_texture[t]);
                        unsigned type = 0;
                        tcStart[m->_id * texturesPerMaterial + t] = Float4(
                            m->_mappingConstant[t], m->_mappingConstant[t],
                            *reinterpret_cast<float*>(&type), *reinterpret_cast<float*>(&atlasIndex));

                    }

                }
            }

        } else {

            auto strataCount = 0u;
            for (auto& mat:scaffold._strataMaterials)
                strataCount += (unsigned)mat._strata.size();

            texturingConstants.resize(sizeof(Float4) * strataCount * 2);
            auto* tcStart = (Float4*)AsPointer(texturingConstants.begin());
            std::fill(tcStart, &tcStart[strataCount*2], Float4(1.f, 1.f, 1.f, 1.f));

            const auto texturesPerStrata = dimof(((TerrainMaterialScaffold::StrataMaterial::Strata*)nullptr)->_texture);
            unsigned strataIndex = 0;
            for (auto m=scaffold._strataMaterials.cbegin(); m!=scaffold._strataMaterials.cend(); ++m) {
                for (auto s=m->_strata.cbegin(); s!=m->_strata.cend(); ++s, ++strataIndex) {

                    for (unsigned t=0; t<texturesPerStrata; ++t) {
                            //  This is a input texture. We need to build the 
                            //  diffuse, specularity and normal map names from this texture name
                        atlasTextureNames.push_back(s->_texture[t]);
                    }

                    tcStart[strataIndex] = Float4(s->_endHeight, s->_endHeight, s->_endHeight, s->_endHeight);

                    Float4 mappingConstant = Float4(1.f, 1.f, 1.f, 1.f);
                    for (unsigned c=0; c<texturesPerStrata; ++c) {
                        tcStart[strataCount + strataIndex][c] = 1.f / s->_mappingConstant[c];
                    }

                }
            }

            _strataCount = strataCount;

        }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // add the textures for the procedural textures to the atlas, as well
        if (procTextureNames.size() > 8)
            Throw(::Exceptions::BasicLabel("Too many different procedural textures used in terrain texturing"));

        procTextureConstants.resize(sizeof(unsigned)*4*procTextureNames.size(), 0);
        for (auto p=procTextureNames.cbegin(); p!=procTextureNames.cend(); ++p) {
            auto i = std::find_if(
                scaffold._procTextures.cbegin(), scaffold._procTextures.cend(),
                [p](const TerrainMaterialScaffold::ProcTextureSetting& s)
                { return s._name == *p; });
            if (i == scaffold._procTextures.cend()) continue;

            auto* a = (UInt4*)AsPointer(procTextureConstants.cbegin());
            auto index = (unsigned)std::distance(procTextureNames.cbegin(), p);
            a[index][0] = AddName(atlasTextureNames, i->_texture[0]);
            a[index][1] = AddName(atlasTextureNames, i->_texture[1]);
            a[index][2] = *reinterpret_cast<const unsigned*>(&i->_hgrid);
            a[index][3] = *reinterpret_cast<const unsigned*>(&i->_gain);
        }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

            // build the atlas textures
        using namespace BufferUploads;
        BufferDesc desc;
        desc._type = BufferDesc::Type::Texture;
        desc._bindFlags = BindFlag::ShaderResource;
        desc._cpuAccess = 0;
        desc._gpuAccess = GPUAccess::Read;
        desc._allocationRules = 0;
        XlCopyString(desc._name, "TerrainMaterialTextures");

        desc._textureDesc = BufferUploads::TextureDesc::Plain2D(
            scaffold._diffuseDims[0], scaffold._diffuseDims[1], Metal::NativeFormat::BC1_UNORM_SRGB, 
            (uint8)IntegerLog2(std::max(scaffold._diffuseDims[0], scaffold._diffuseDims[1]))-1, uint8(atlasTextureNames.size()));
        auto diffuseTextureArray = GetBufferUploads().Transaction_Immediate(desc)->AdoptUnderlying();

        desc._textureDesc = BufferUploads::TextureDesc::Plain2D(
            scaffold._normalDims[0], scaffold._normalDims[1], Metal::NativeFormat::BC5_UNORM, 
            (uint8)IntegerLog2(std::max(scaffold._normalDims[0], scaffold._normalDims[1]))-1, uint8(atlasTextureNames.size()));
        auto normalTextureArray = GetBufferUploads().Transaction_Immediate(desc)->AdoptUnderlying();

        desc._textureDesc = BufferUploads::TextureDesc::Plain2D(
            scaffold._paramDims[0], scaffold._paramDims[1], Metal::NativeFormat::BC1_UNORM, 
            (uint8)IntegerLog2(std::max(scaffold._paramDims[0], scaffold._paramDims[1]))-1, uint8(atlasTextureNames.size()));
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(_validationCallback, scaffold.GetDependencyValidation());

        for (auto i=atlasTextureNames.cbegin(); i!=atlasTextureNames.cend(); ++i) {
            TRY {
                ::Assets::ResChar resolvedFile[MaxPath];
                scaffold._searchRules.ResolveFile(
                    resolvedFile, dimof(resolvedFile), 
                    StringMeld<MaxPath, ::Assets::ResChar>() << *i << "_df.dds");
                if (resolvedFile[0]) {
                    LoadTextureIntoArray(diffuseTextureArray.get(), resolvedFile, (unsigned)std::distance(atlasTextureNames.cbegin(), i));
                    RegisterFileDependency(_validationCallback, resolvedFile);
                }
            } CATCH (const ::Assets::Exceptions::InvalidResource&) {}
            CATCH_END
        }

        for (auto i=atlasTextureNames.cbegin(); i!=atlasTextureNames.cend(); ++i) {
            TRY {
                ::Assets::ResChar resolvedFile[MaxPath];
                scaffold._searchRules.ResolveFile(
                    resolvedFile, dimof(resolvedFile), 
                    StringMeld<MaxPath, ::Assets::ResChar>() << *i << "_ddn.dds");
                if (resolvedFile[0]) {
                    LoadTextureIntoArray(normalTextureArray.get(), resolvedFile, (unsigned)std::distance(atlasTextureNames.cbegin(), i));
                    RegisterFileDependency(_validationCallback, resolvedFile);
                }
            } CATCH (const ::Assets::Exceptions::InvalidResource&) {}
            CATCH_END
        }

        for (auto i=atlasTextureNames.cbegin(); i!=atlasTextureNames.cend(); ++i) {
            bool fillInWhiteSpecular = true;
            auto index = (unsigned)std::distance(atlasTextureNames.cbegin(), i);
            TRY {
                ::Assets::ResChar resolvedFile[MaxPath];
                scaffold._searchRules.ResolveFile(
                    resolvedFile, dimof(resolvedFile), 
                    StringMeld<MaxPath, ::Assets::ResChar>() << *i << "_sp.dds");
                if (resolvedFile[0]) {
                    LoadTextureIntoArray(normalTextureArray.get(), resolvedFile, index);
                    RegisterFileDependency(_validationCallback, resolvedFile);
                    fillInWhiteSpecular = true;
                }
            } CATCH (const ::Assets::Exceptions::InvalidResource&) {
            } CATCH_END

                // on exception or missing files, we should fill int specularlity with whiteness
            if (fillInWhiteSpecular)
                FillWhite(specularityTextureArray.get(), dummyWhiteBuffer.get(), index);
        }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        RenderCore::Metal::ShaderResourceView diffuseSrv(diffuseTextureArray.get());
        RenderCore::Metal::ShaderResourceView normalSrv(normalTextureArray.get());
        RenderCore::Metal::ShaderResourceView specularitySrv(specularityTextureArray.get());
        RenderCore::Metal::ConstantBuffer texContBuffer(AsPointer(texturingConstants.cbegin()), texturingConstants.size());
        RenderCore::Metal::ConstantBuffer procTexContsBuffer(AsPointer(procTextureConstants.cbegin()), procTextureConstants.size());

        _textureArray[Diffuse] = std::move(diffuseTextureArray);
        _textureArray[Normal] = std::move(normalTextureArray);
        _textureArray[Specularity] = std::move(specularityTextureArray);
        _srv[Diffuse] = std::move(diffuseSrv);
        _srv[Normal] = std::move(normalSrv);
        _srv[Specularity] = std::move(specularitySrv);
        _texturingConstants = std::move(texContBuffer);
        _procTexContsBuffer = std::move(procTexContsBuffer);
    }

    TerrainMaterialTextures::~TerrainMaterialTextures() {}

}

