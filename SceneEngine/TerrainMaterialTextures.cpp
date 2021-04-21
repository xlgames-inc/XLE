// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainMaterialTextures.h"
#include "TerrainMaterial.h"
#include "SceneEngineUtils.h"
#include "MetalStubs.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../BufferUploads/DataPacket.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/Resource.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Techniques/DeferredShaderResource.h"
#include "../RenderCore/Format.h"
#include "../Assets/AssetServices.h"
#include "../Assets/CompileAndAsyncManager.h"
#include "../Assets/Assets.h"
#include "../OSServices/Log.h"
#include "../Utility/BitUtils.h"
#include "../Utility/StringFormat.h"
#include "../xleres/FileList.h"

#include "../RenderCore/DX11/Metal/DX11Utils.h"
#include "../RenderCore/DX11/Metal/Format.h"
#include "../../Foreign/DirectXTex/DirectXTex/DirectXTex.h"

namespace SceneEngine
{
    using namespace RenderCore;

    template <typename Type>
        static const Type& GetAssetImmediate(const char initializer[])
    {
        for (;;) {
            TRY {
                return ::Assets::Legacy::GetAsset<Type>(initializer);
            } CATCH (::Assets::Exceptions::PendingAsset&) {
                ::Assets::Services::GetAsyncMan().Update();
            } CATCH_END
        }
    }

    static void LoadTextureIntoArray(Metal::DeviceContext& context, RenderCore::Resource& destinationArray, const char sourceFile[], unsigned arrayIndex)
    {
            //      We want to load the given texture, and merge it into
            //      the texture array. We have to do this synchronously, otherwise the scheduling
            //      is too awkward
            //      We're also using the "immediate context" -- so this should be run in 
            //      the main rendering thread (or whatever thread is associated with the 
            //      immediate context)

        auto inputTexture = RenderCore::Techniques::DeferredShaderResource::LoadImmediately(sourceFile);
        auto inputRes = inputTexture.GetResource();

        auto destinationDesc = destinationArray.GetDesc();
        const auto dstMipCount = destinationDesc._textureDesc._mipCount;
        auto dstWidthPower = (int)IntegerLog2(destinationDesc._textureDesc._width);

        if (!IsPowerOfTwo(destinationDesc._textureDesc._width) || !IsPowerOfTwo(destinationDesc._textureDesc._height)) {
            // only power-of-two textures supported (too difficult to merge them into a atlas otherwise)
            Throw(::Exceptions::BasicLabel("Expecting power of two texture for terrain texturing (%s)", sourceFile));
        }
        if (destinationDesc._textureDesc._width != destinationDesc._textureDesc._height) {
            Throw(::Exceptions::BasicLabel("Expecting square texture for terrain texturing (%s)", sourceFile));
        }

        auto sourceDesc = inputRes->GetDesc();
        auto srcWidthPower = (int)IntegerLog2(sourceDesc._textureDesc._width);
        auto mipDifference = srcWidthPower - dstWidthPower;

        for (unsigned m=0; m<dstMipCount; ++m) {
            
            auto sourceMip = m + mipDifference;
            if (sourceMip < 0) {

                Log(Warning) << 
                    "LoadTextureIntoArray -- performing resample on texture (" << sourceFile << "). All textures in the array must be the same size!\n" << std::endl;

                    //  We have to up-sample to get the same number of mips
                    //  Using the highest LOD from the source texture, resample into
                    //  a default texture
                const unsigned expectedWidth = destinationDesc._textureDesc._width >> m;
                const unsigned expectedHeight = destinationDesc._textureDesc._height >> m;

                auto destFormat = destinationDesc._textureDesc._format;
                auto resamplingFormat = destFormat;
                auto compressionType = RenderCore::GetCompressionType(resamplingFormat);
                if (compressionType == RenderCore::FormatCompressionType::BlockCompression) {
                        // resampling via a higher precision buffer -- just for kicks.
                    resamplingFormat = RenderCore::Format::R16G16B16A16_FLOAT;
                }

                auto& bufferUploads = GetBufferUploads();
                ResourceDesc desc;
                desc._type = ResourceDesc::Type::Texture;
                desc._bindFlags = BindFlag::UnorderedAccess;
                desc._cpuAccess = 0;
                desc._gpuAccess = GPUAccess::Read|GPUAccess::Write;
                desc._allocationRules = 0;
                desc._textureDesc = BufferUploads::TextureDesc::Plain2D(expectedWidth, expectedHeight, resamplingFormat);
                XlCopyString(desc._name, "ResamplingTexture");
                auto resamplingBuffer = bufferUploads.Transaction_Immediate(desc);
                Metal::UnorderedAccessView uav(resamplingBuffer->GetUnderlying());

                    //	actually getting better results with point resampling.
		            //	After the compression, the bilinear sampled texture looks quite dithered
		            //	and ugly... it might be a consequence of compressing twice?
                auto& resamplingShader = GetAssetImmediate<RenderCore::Metal::ComputeShader>(BASIC_COMPUTE_HLSL ":ResamplePoint:cs_*");
                context.Bind(resamplingShader);
                context.GetNumericUniforms(ShaderStage::Compute).Bind(MakeResourceList(uav));
                context.GetNumericUniforms(ShaderStage::Compute).Bind(MakeResourceList(inputTexture));
                context.Dispatch(expectedWidth/8, expectedHeight/8);
                MetalStubs::UnbindCS<Metal::UnorderedAccessView>(context, 0, 1);

#if GFXAPI_TARGET == GFXAPI_DX11	// platformtemp
                if (resamplingFormat!=destFormat) {
                        // We have to re-compress the texture. It's annoying, but we can use a library to do it
                    auto rawData = bufferUploads.Resource_ReadBack(*resamplingBuffer);
                    DirectX::Image image;
                    image.width = expectedWidth;
                    image.height = expectedHeight;
                    image.format = Metal_DX11::AsDXGIFormat(resamplingFormat);
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
                        image, Metal_DX11::AsDXGIFormat(destFormat), DirectX::TEX_COMPRESS_DITHER | DirectX::TEX_COMPRESS_SRGB, 0.f, compressedImage);
                    assert(SUCCEEDED(hresult)); (void)hresult;
                    assert(compressedImage.GetImageCount()==1);
                    
                    auto& final = *compressedImage.GetImage(0,0,0);
                    desc._bindFlags = BindFlag::ShaderResource;
                    desc._textureDesc._format = destFormat;
                    auto compressedBuffer = bufferUploads.Transaction_Immediate(
                        desc, BufferUploads::CreateBasicPacket(final.slicePitch, final.pixels, TexturePitches{unsigned(final.rowPitch), unsigned(final.slicePitch)}).get());

                    resamplingBuffer = compressedBuffer;   
                }
#endif

                Metal::CopyPartial(
                    context, 
					Metal::CopyPartial_Dest{&destinationArray, {m, arrayIndex}},
					Metal::CopyPartial_Src{resamplingBuffer->GetUnderlying().get(), {}});

            } else {

                Metal::CopyPartial(
                    context,
					Metal::CopyPartial_Dest{&destinationArray, {m, arrayIndex}},
					Metal::CopyPartial_Src{inputRes.get(), {sourceMip, 0}});

            }
        }
    }

    static void CopyDummy(
        Metal::DeviceContext& context, RenderCore::Resource& destinationArray, RenderCore::Resource& sourceResource,
        unsigned arrayIndex, bool blockCompressed)
    {
            // copy dummy white data into all of the mip levels of the given array index in the
            // destination resource
        auto destinationDesc = destinationArray.GetDesc();
        const auto mipCount = destinationDesc._textureDesc._mipCount;

        auto minDims = blockCompressed ? 4u : 1u;
        for (auto m=0u; m<mipCount; ++m) {
            const auto mipWidth = std::max(destinationDesc._textureDesc._width >> m, minDims);
            const auto mipHeight = std::max(destinationDesc._textureDesc._height >> m, minDims);
            Metal::CopyPartial(
                context,
				Metal::CopyPartial_Dest{&destinationArray, {m, arrayIndex}},
				Metal::CopyPartial_Src{&sourceResource, {}, {}, {mipWidth, mipHeight, 1}});
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
		::Assets::ResChar _diffuse[MaxPath];
		::Assets::ResChar _normals[MaxPath];
		::Assets::ResChar _roughness[MaxPath];

        ResolvedTextureFiles(
            const ::Assets::ResChar baseName[],
            const ::Assets::DirectorySearchRules& searchRules);

    protected:
        void PatchFilename(
			::Assets::ResChar dest[], size_t destCount,
			const ::Assets::ResChar input[], 
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
            const ::Assets::ResChar d[] = "_df", n[] = "_ddn", p[] = "_r";
            PatchFilename(_diffuse, dimof(_diffuse), baseName, marker, markerEnd, d);
            PatchFilename(_normals, dimof(_normals), baseName, marker, markerEnd, n);
            PatchFilename(_roughness, dimof(_roughness), baseName, marker, markerEnd, p);

            searchRules.ResolveFile(_diffuse, dimof(_diffuse), _diffuse);
            searchRules.ResolveFile(_normals, dimof(_normals), _normals);
            searchRules.ResolveFile(_roughness, dimof(_roughness), _roughness);
        } else {
            searchRules.ResolveFile(_diffuse, dimof(_diffuse), baseName);
        }
    }

    void ResolvedTextureFiles::PatchFilename(
		::Assets::ResChar dest[], size_t destCount,
		const ::Assets::ResChar input[], 
        const ::Assets::ResChar marker[], const ::Assets::ResChar markerEnd[],
        const ::Assets::ResChar replacement[])
    {
        auto*i = dest;
        auto*iend = &dest[destCount];

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

#if 0
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
#endif

    static intrusive_ptr<BufferUploads::ResourceLocator> BC5Dummy(const BufferUploads::BufferDesc& desc, uint8 x, uint8 y)
    {
        auto tempBuffer = BufferUploads::CreateEmptyPacket(desc);
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

    static intrusive_ptr<BufferUploads::ResourceLocator> R8Dummy(const BufferUploads::BufferDesc& desc, uint8 value)
    {
        auto tempBuffer = BufferUploads::CreateEmptyPacket(desc);
        std::memset(tempBuffer->GetData(), 0x0, tempBuffer->GetDataSize());
        return GetBufferUploads().Transaction_Immediate(desc, tempBuffer.get());
    }

    TerrainMaterialTextures::TerrainMaterialTextures(
        IThreadContext& context,
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
        ResourceDesc desc;
        desc._type = ResourceDesc::Type::Texture;
        desc._bindFlags = BindFlag::ShaderResource;
        desc._cpuAccess = 0;
        desc._gpuAccess = GPUAccess::Read;
        desc._allocationRules = 0;
        XlCopyString(desc._name, "TerrainMaterialTextures");

        desc._textureDesc = BufferUploads::TextureDesc::Plain2D(
            scaffold._diffuseDims[0], scaffold._diffuseDims[1], Format::BC1_UNORM_SRGB, 
            (uint8)IntegerLog2(std::max(scaffold._diffuseDims[0], scaffold._diffuseDims[1]))-1, uint8(atlasTextureNames.size()));
        auto diffuseTextureArray = GetBufferUploads().Transaction_Immediate(desc);

        desc._textureDesc = BufferUploads::TextureDesc::Plain2D(
            scaffold._normalDims[0], scaffold._normalDims[1], Format::BC5_UNORM, 
            (uint8)IntegerLog2(std::max(scaffold._normalDims[0], scaffold._normalDims[1]))-1, uint8(atlasTextureNames.size()));
        auto normalTextureArray = GetBufferUploads().Transaction_Immediate(desc);

        desc._textureDesc = BufferUploads::TextureDesc::Plain2D(
            scaffold._normalDims[0], scaffold._normalDims[1], Format::BC5_UNORM);
        auto bc5Dummy = BC5Dummy(desc, 0x80, 0x80);

        desc._textureDesc = BufferUploads::TextureDesc::Plain2D(
            scaffold._paramDims[0], scaffold._paramDims[1], Format::R8_UNORM, 
            (uint8)IntegerLog2(std::max(scaffold._paramDims[0], scaffold._paramDims[1]))-1, uint8(atlasTextureNames.size()));
        auto roughnessTextureArray = GetBufferUploads().Transaction_Immediate(desc);

        desc._textureDesc = BufferUploads::TextureDesc::Plain2D(
            scaffold._paramDims[0], scaffold._paramDims[1], Format::R8_UNORM);
        auto r8Dummy = R8Dummy(desc, 0);
        

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        _validationCallback = ::Assets::GetDepValSys().Make();

		auto& metalContext = *RenderCore::Metal::DeviceContext::Get(context);

        for (auto i=atlasTextureNames.cbegin(); i!=atlasTextureNames.cend(); ++i) {
            ResolvedTextureFiles texFiles(i->c_str(), scaffold._searchRules);

                // --- Diffuse --->
            TRY {
                if (texFiles._diffuse[0]) {
                    LoadTextureIntoArray(metalContext, *diffuseTextureArray->GetUnderlying(), texFiles._diffuse, (unsigned)std::distance(atlasTextureNames.cbegin(), i));
                    RegisterFileDependency(_validationCallback, texFiles._diffuse);
                }
            } CATCH (const ::Assets::Exceptions::InvalidAsset&) {}
            CATCH_END

                // --- Normals --->
            bool fillInDummyNormals = true;
            TRY {
                if (texFiles._normals[0]) {
                    LoadTextureIntoArray(metalContext, *normalTextureArray->GetUnderlying(), texFiles._normals, (unsigned)std::distance(atlasTextureNames.cbegin(), i));
                    RegisterFileDependency(_validationCallback, texFiles._normals);
                    fillInDummyNormals = false;
                }
            } CATCH (const ::Assets::Exceptions::InvalidAsset&) {}
            CATCH_END

                // --- Specular params --->
            bool fillInBlackRoughness = true;
            auto index = (unsigned)std::distance(atlasTextureNames.cbegin(), i);
            TRY {
                if (texFiles._roughness[0]) {
                    LoadTextureIntoArray(metalContext, *normalTextureArray->GetUnderlying(), texFiles._roughness, index);
                    RegisterFileDependency(_validationCallback, texFiles._roughness);
                    fillInBlackRoughness = false;
                }
            } CATCH (const ::Assets::Exceptions::InvalidAsset&) {
            } CATCH_END

                // on exception or missing files, we should fill in default
            if (fillInDummyNormals)
                CopyDummy(metalContext, *normalTextureArray->GetUnderlying(), *bc5Dummy->GetUnderlying(), index, true);
            if (fillInBlackRoughness)
                CopyDummy(metalContext, *roughnessTextureArray->GetUnderlying(), *r8Dummy->GetUnderlying(), index, false);
        }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        #if defined(_DEBUG)
                // resize this buffer in debug to prevent D3D warning messages when it is smaller than the buffer in the shader code
                // it's ok for it to be smaller, because the shader will only actually read from the filled-in data... So this is
                // just to suppress the warning.
            if (texturingConstants.size() < sizeof(Float4) * 32 * 5)
                texturingConstants.resize(sizeof(Float4) * 32 * 5, 0);
        #endif

        _srv[Diffuse] = Metal::ShaderResourceView(diffuseTextureArray->GetUnderlying());
        _srv[Normal] = Metal::ShaderResourceView(normalTextureArray->GetUnderlying());
        _srv[Roughness] = Metal::ShaderResourceView(roughnessTextureArray->GetUnderlying());
		if (!texturingConstants.empty())
			_texturingConstants = MakeMetalCB(AsPointer(texturingConstants.cbegin()), texturingConstants.size());
		if (!procTextureConstants.empty())
			_procTexContsBuffer = MakeMetalCB(AsPointer(procTextureConstants.cbegin()), procTextureConstants.size());

        _textureArray[Diffuse] = std::move(diffuseTextureArray);
        _textureArray[Normal] = std::move(normalTextureArray);
        _textureArray[Roughness] = std::move(roughnessTextureArray);
    }

    TerrainMaterialTextures::~TerrainMaterialTextures() {}

}

