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
            } CATCH (::Assets::Exceptions::PendingAsset&) {
                ::Assets::Services::GetAsyncMan().Update();
            } CATCH_END
        }
    }

    static void LoadTextureIntoArray(Metal::DeviceContext& context, ID3D::Resource* destinationArray, const char sourceFile[], unsigned arrayIndex)
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
            Throw(::Assets::Exceptions::InvalidAsset(sourceFile, "Expecting power of two texture for terrain texturing"));
        }
        if (destinationDesc.Width != destinationDesc.Height) {
            Throw(::Assets::Exceptions::InvalidAsset(sourceFile, "Expecting square texture for terrain texturing"));
        }

        Metal::TextureDesc2D sourceDesc(inputRes.get());
        auto srcWidthPower = (int)IntegerLog2(sourceDesc.Width);
        auto mipDifference = srcWidthPower - dstWidthPower;

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

    static void FillWhite(Metal::DeviceContext& context, ID3D::Resource* destinationArray, ID3D::Resource* sourceResource, unsigned arrayIndex)
    {
            // copy dummy white data into all of the mip levels of the given array index in the
            // destination resource
        Metal::TextureDesc2D destinationDesc(destinationArray);
        const auto mipCount = destinationDesc.MipLevels;

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
    
    class ResolvedTextureFiles
    {
    public:
        ::Assets::ResolvedAssetFile _diffuse, _normals, _params;

        ResolvedTextureFiles(
            const ::Assets::ResChar baseName[],
            const ::Assets::DirectorySearchRules& searchRules);

    protected:
        void PatchFilename(
            ::Assets::ResolvedAssetFile& dest, const ::Assets::ResChar input[], 
            const ::Assets::ResChar marker[], const ::Assets::ResChar markerEnd[],
            const ::Assets::ResChar replacement[]);
    };

    ResolvedTextureFiles::ResolvedTextureFiles(
        const ::Assets::ResChar baseName[], 
        const ::Assets::DirectorySearchRules& searchRules)
    {
        auto marker = XlFindString(baseName, "_*");
        if (marker) {
            auto* markerEnd = marker+2;
            const ::Assets::ResChar d[] = "_df", n[] = "_ddn", p[] = "_sp";
            PatchFilename(_diffuse, baseName, marker, markerEnd, d);
            PatchFilename(_normals, baseName, marker, markerEnd, n);
            PatchFilename(_params, baseName, marker, markerEnd, p);

            searchRules.ResolveFile(_diffuse._fn, dimof(_diffuse._fn), _diffuse._fn);
            searchRules.ResolveFile(_normals._fn, dimof(_normals._fn), _normals._fn);
            searchRules.ResolveFile(_params._fn, dimof(_params._fn), _params._fn);
        } else {
            searchRules.ResolveFile(_diffuse._fn, dimof(_diffuse._fn), baseName);
        }
    }

    void ResolvedTextureFiles::PatchFilename(
        ::Assets::ResolvedAssetFile& dest, const ::Assets::ResChar input[], 
        const ::Assets::ResChar marker[], const ::Assets::ResChar markerEnd[],
        const ::Assets::ResChar replacement[])
    {
        auto*i = dest._fn;
        auto*iend = &dest._fn[dimof(dest._fn)];

            // fill in part before the marker
        {
            const auto* s = input;
            while (i < iend && s < marker) {
                *i = *s;
                ++i; ++s;
            }
        }

            // fill in null terminated replacement
        {
            const auto* s = replacement;
            while (i < iend && *s) {
                *i = *s;
                ++i; ++s;
            }
        }
        
            // fill in null terminated part after the marker
        {
            const auto* s = markerEnd;
            while (i < iend && *s) {
                *i = *s;
                ++i; ++s;
            }
        }

            // null terminator (even if we ran out of buffer space)
        *std::min(i, iend-1) = '\0';
    }

    static intrusive_ptr<BufferUploads::ResourceLocator> BC1Dummy(const BufferUploads::BufferDesc& desc, uint16 blankColor)
    {
        auto tempBuffer = CreateEmptyPacket(desc);
        struct BC1Block { uint16 c0; uint16 c1; uint32 t; } block { blankColor, blankColor, 0 };

        auto dataSize = tempBuffer->GetDataSize();
        assert((dataSize % sizeof(BC1Block))==0);
        auto blockCount = dataSize / sizeof(BC1Block);
        auto data = (BC1Block*)tempBuffer->GetData();
        for (unsigned c=0; c<blockCount; ++c) data[c] = block;

        return GetBufferUploads().Transaction_Immediate(desc, tempBuffer.get());
    }

    static intrusive_ptr<BufferUploads::ResourceLocator> BC5Dummy(const BufferUploads::BufferDesc& desc, uint8 x, uint8 y)
    {
        auto tempBuffer = CreateEmptyPacket(desc);
        struct BC5Block 
        { 
            uint8 x0; uint8 x1; uint8 tx[6];
            uint8 y0; uint8 y1; uint8 ty[6];
        } block {
            x, x, {0,0,0,0,0,0},
            y, y, {0,0,0,0,0,0}
        };

        auto dataSize = tempBuffer->GetDataSize();
        assert((dataSize % sizeof(BC5Block))==0);
        auto blockCount = dataSize / sizeof(BC5Block);
        auto data = (BC5Block*)tempBuffer->GetData();
        for (unsigned c=0; c<blockCount; ++c) data[c] = block;

        return GetBufferUploads().Transaction_Immediate(desc, tempBuffer.get());
    }

    TerrainMaterialTextures::TerrainMaterialTextures(
        const TerrainMaterialConfig& scaffold, 
        bool useGradFlagMaterials)
    {
        _strataCount = 0;

            //  Each texture is stored as a separate file on disk. But we need to copy them
            //  all into a texture array.

        std::vector<::Assets::rstring> atlasTextureNames;
        std::vector<::Assets::rstring> procTextureNames;
        std::vector<uint8> texturingConstants;
        std::vector<uint8> procTextureConstants;

        auto context = GetImmediateContext();

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        if (useGradFlagMaterials) {

            const auto texturesPerMaterial = dimof(((TerrainMaterialConfig::GradFlagMaterial*)nullptr)->_texture);
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
                        [m, t](const TerrainMaterialConfig::ProcTextureSetting& s)
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

            const auto texturesPerStrata = dimof(((TerrainMaterialConfig::StrataMaterial::Strata*)nullptr)->_texture);
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
                [p](const TerrainMaterialConfig::ProcTextureSetting& s)
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
            scaffold._normalDims[0], scaffold._normalDims[1], Metal::NativeFormat::BC5_UNORM);
        auto bc5Dummy = BC5Dummy(desc, 0x80, 0x80);

        desc._textureDesc = BufferUploads::TextureDesc::Plain2D(
            scaffold._paramDims[0], scaffold._paramDims[1], Metal::NativeFormat::BC1_UNORM, 
            (uint8)IntegerLog2(std::max(scaffold._paramDims[0], scaffold._paramDims[1]))-1, uint8(atlasTextureNames.size()));
        auto specularityTextureArray = GetBufferUploads().Transaction_Immediate(desc)->AdoptUnderlying();

        desc._textureDesc = BufferUploads::TextureDesc::Plain2D(
            scaffold._paramDims[0], scaffold._paramDims[1], Metal::NativeFormat::BC1_UNORM);
        auto bc1Dummy = BC1Dummy(desc, ((0x1f/2) << 11) | ((0x3f/2) << 5) | (0x1f/2));
        

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();

        for (auto i=atlasTextureNames.cbegin(); i!=atlasTextureNames.cend(); ++i) {
            ResolvedTextureFiles texFiles(i->c_str(), scaffold._searchRules);

                // --- Diffuse --->
            TRY {
                if (texFiles._diffuse.get()[0]) {
                    LoadTextureIntoArray(context, diffuseTextureArray.get(), texFiles._diffuse.get(), (unsigned)std::distance(atlasTextureNames.cbegin(), i));
                    RegisterFileDependency(_validationCallback, texFiles._diffuse.get());
                }
            } CATCH (const ::Assets::Exceptions::InvalidAsset&) {}
            CATCH_END

                // --- Normals --->
            bool fillInDummyNormals = true;
            TRY {
                if (texFiles._normals.get()[0]) {
                    LoadTextureIntoArray(context, normalTextureArray.get(), texFiles._normals.get(), (unsigned)std::distance(atlasTextureNames.cbegin(), i));
                    RegisterFileDependency(_validationCallback, texFiles._normals.get());
                    fillInDummyNormals = false;
                }
            } CATCH (const ::Assets::Exceptions::InvalidAsset&) {}
            CATCH_END

                // --- Specular params --->
            bool fillInWhiteSpecular = true;
            auto index = (unsigned)std::distance(atlasTextureNames.cbegin(), i);
            TRY {
                if (texFiles._params.get()[0]) {
                    LoadTextureIntoArray(context, normalTextureArray.get(), texFiles._params.get(), index);
                    RegisterFileDependency(_validationCallback, texFiles._params.get());
                    fillInWhiteSpecular = false;
                }
            } CATCH (const ::Assets::Exceptions::InvalidAsset&) {
            } CATCH_END

                // on exception or missing files, we should fill in default
            if (fillInDummyNormals)
                FillWhite(context, normalTextureArray.get(), bc5Dummy->GetUnderlying(), index);
            if (fillInWhiteSpecular)
                FillWhite(context, specularityTextureArray.get(), bc1Dummy->GetUnderlying(), index);
        }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        Metal::ShaderResourceView diffuseSrv(diffuseTextureArray.get());
        Metal::ShaderResourceView normalSrv(normalTextureArray.get());
        Metal::ShaderResourceView specularitySrv(specularityTextureArray.get());
        Metal::ConstantBuffer texContBuffer(AsPointer(texturingConstants.cbegin()), texturingConstants.size());
        Metal::ConstantBuffer procTexContsBuffer(AsPointer(procTextureConstants.cbegin()), procTextureConstants.size());

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

