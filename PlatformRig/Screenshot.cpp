// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Screenshot.h"
#include "../SceneEngine/LightingParserContext.h"
#include "../SceneEngine/LightingParser.h"
#include "../SceneEngine/GestaltResource.h"
#include "../SceneEngine/SceneParser.h"
#include "../SceneEngine/Tonemap.h"
#include "../SceneEngine/SceneEngineUtils.h"
#include "../RenderCore/Techniques/TechniqueUtils.h"
#include "../RenderCore/IThreadContext.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Assets/Services.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/DataPacket.h"
#include "../ConsoleRig/Console.h"
#include "../Utility/BitUtils.h"
#include "../Utility/Conversion.h"
#include "../Utility/PtrUtils.h"

#include "../Core/WinAPI/IncludeWindows.h"
#include "../Foreign/DirectXTex/DirectXTex/DirectXTex.h"
#include "../Foreign/half-1.9.2/include/half.hpp"
// #include <wincodec.h>        (avoiding an extra header from the winsdk by hard coding GUID_ContainerFormatTiff below)

namespace PlatformRig
{
    using namespace SceneEngine;
    using namespace RenderCore;
    using namespace BufferUploads;

    static void SaveImage(
        const char destinationFile[],
        const void* imageData,
        UInt2 dimensions,
        unsigned rowPitch,
        Metal::NativeFormat::Enum format)
    {
            // using DirectXTex to save to disk as a TGA file...
        DirectX::Image image {
            dimensions[0], dimensions[1],
            Metal::AsDXGIFormat(format),
            rowPitch, rowPitch * dimensions[1],
            (uint8_t*)imageData };
        auto fn = Conversion::Convert<std::wstring>(std::string(destinationFile));

        // DirectX::SaveToDDSFile(image, DirectX::DDS_FLAGS_NONE, fn.c_str());

        const GUID GUID_ContainerFormatTiff = 
            { 0x163bcc30, 0xe2e9, 0x4f0b, { 0x96, 0x1d, 0xa3, 0xe9, 0xfd, 0xb7, 0x88, 0xa3 }};
        auto hresult = DirectX::SaveToWICFile(
            image, DirectX::WIC_FLAGS_NONE,
            GUID_ContainerFormatTiff,
            fn.c_str());
        assert(SUCCEEDED(hresult));
    }

    static intrusive_ptr<DataPacket> RenderTiled(
        IThreadContext& context,
        LightingParserContext& parserContext,
        ISceneParser& sceneParser,
        const Techniques::CameraDesc& camera,
        const RenderingQualitySettings& qualitySettings,
        Metal::NativeFormat::Enum format)
    {
        // We want to separate the view into several tiles, and render
        // each as a separate high-res render. Then we will stitch them
        // together and write out one extremely high-res result.

        const auto tileDims = 4096u;
        auto tilesX = CeilToMultiplePow2(qualitySettings._dimensions[0], tileDims) / tileDims;
        auto tilesY = CeilToMultiplePow2(qualitySettings._dimensions[1], tileDims) / tileDims;
        auto tileQualSettings = qualitySettings;

            // Note that we should write out to a linear format
            // so that downsampling can be done in linear space
            // Because it's linear, we need a little extra precision
            // to avoid banding post gamma correction.
        using TargetType = GestaltTypes::RTVSRV;
        std::vector<TargetType> targets;
        targets.resize(tilesX*tilesY);

        auto metalContext = RenderCore::Metal::DeviceContext::Get(context);

        auto sceneMarker = LightingParser_SetupScene(*metalContext, parserContext, &sceneParser);
        
        const auto coordinateSpace = GeometricCoordinateSpace::RightHanded;
        const float aspectRatio = qualitySettings._dimensions[0] / float(qualitySettings._dimensions[1]);
        const float n = camera._nearClip;
        const float h = n * XlTan(.5f * camera._verticalFieldOfView);
        const float w = h * aspectRatio;
        const float t = h, b = -h;

        float l, r;
        const auto isLH = coordinateSpace == GeometricCoordinateSpace::LeftHanded;
        if (constant_expression<isLH>::result())    { l = w; r = -w; }
        else                                        { l = -w; r = w; }

        auto& doToneMap = Tweakable("DoToneMap", true);
        auto oldDoToneMap = doToneMap;
        doToneMap = false;  // hack to disable tone mapping
        auto cleanup = MakeAutoCleanup([&doToneMap, oldDoToneMap]() { doToneMap = oldDoToneMap; });

            // Render each tile, one by one...
            // Note that there is a problem here because the tonemapping will want to
            // sample the lumiance for each tile separately. That's not ideal for us,
            // because it can mean that different tiles get different tonemapping!
            // And, anyway, we want to do down-sampling in pre-tonemapped linear space.
            // So, ideally, we should not do the tonemapping step during execute scene.
            // Instead, we will get the "lighting target" output from the lighting parser,
            // then downsampling, and then use the lighting parser to resolve HDR/gamma
            // afterwards.
        for (unsigned y=0; y<tilesY; ++y)
            for (unsigned x=0; x<tilesX; ++x) {
                auto& target = targets[y*tilesX+x];

                auto viewWidth  = std::min((x+1)*tileDims, qualitySettings._dimensions[0]) - (x*tileDims);
                auto viewHeight = std::min((y+1)*tileDims, qualitySettings._dimensions[1]) - (y*tileDims);
                tileQualSettings._dimensions = UInt2(viewWidth, viewHeight);
                auto rtDesc = TextureDesc::Plain2D(viewWidth, viewHeight, format);
                target = TargetType(rtDesc, "HighResScreenShot");

                    // We build a custom projection matrix that limits
                    // the frustum to the particular tile we're rendering.
                    //
                    // There are 2 basic ways we can do this... 
                    //      1) We can render the scene in tiles (each tile being a rectangle of the final image)
                    //      2) We can add a sub-pixel offset on each projection
                    //          (so each render covers the whole image, but at a small offset each time)
                    //
                    // The results could be quite different... Particularly for things like mipmapping, or anything
                    // that uses the screen space derivatives. Also LODs and shadows could come out differently in
                    // some cases.
                    // Also, with method 2, we don't have to just use a regular grid pattern for samples. We can
                    // use a rotated pattern to try to catch certain triangle shapes better.
                auto customProjectionMatrix = 
                    PerspectiveProjection(
                        LinearInterpolate(l, r, (x*tileDims             )/float(qualitySettings._dimensions[0])),
                        LinearInterpolate(t, b, (y*tileDims             )/float(qualitySettings._dimensions[1])),
                        LinearInterpolate(l, r, (x*tileDims +  viewWidth)/float(qualitySettings._dimensions[0])),
                        LinearInterpolate(t, b, (y*tileDims + viewHeight)/float(qualitySettings._dimensions[1])),
                        camera._nearClip, camera._farClip,
                        Techniques::GetDefaultClipSpaceType());
                auto projDesc = BuildProjectionDesc(camera, UInt2(viewWidth, viewHeight), &customProjectionMatrix);

                    // now we can just render, using the normal process.
                metalContext->Bind(MakeResourceList(target.RTV()), nullptr);
                metalContext->Bind(Metal::ViewportDesc(0.f, 0.f, float(viewWidth), float(viewHeight)));
                LightingParser_SetGlobalTransform(*metalContext, parserContext, projDesc);
                LightingParser_ExecuteScene(*metalContext, parserContext, tileQualSettings);
            }

        doToneMap = oldDoToneMap;

        auto& uploads = RenderCore::Assets::Services::GetBufferUploads();

            // Now pull the data over to the CPU, and stitch together
            // We will write out the raw data in some simple format
            //      -- the user can complete processing in a image editing application
        UInt2 finalImageDims = qualitySettings._dimensions;
        auto bpp = Metal::BitsPerPixel(format);
        auto finalRowPitch = finalImageDims[0]*bpp/8;
        auto rawData = CreateBasicPacket(
            finalImageDims[1]*finalRowPitch, nullptr,
            TexturePitches(finalRowPitch, finalImageDims[1]*finalRowPitch));
        auto* rawDataEnd = PtrAdd(rawData->GetData(), rawData->GetDataSize());

        for (unsigned y=0; y<tilesY; ++y)
            for (unsigned x=0; x<tilesX; ++x) {
                auto& target = targets[y*tilesX+x];
                {
                    auto readback = uploads.Resource_ReadBack(target.Locator());
                    auto* readbackEnd = PtrAdd(readback->GetData(), readback->GetDataSize());

                    auto viewWidth  = std::min((x+1)*tileDims, qualitySettings._dimensions[0]) - (x*tileDims);
                    auto viewHeight = std::min((y+1)*tileDims, qualitySettings._dimensions[1]) - (y*tileDims);

                        // copy each row of the tile into the correct spot in the output texture
                    for (unsigned r=0; r<viewHeight; ++r) {
                        const void* rowSrc = PtrAdd(readback->GetData(), r*readback->GetPitches()._rowPitch);
                        void* rowDst = PtrAdd(rawData->GetData(), (y*tileDims+r)*finalRowPitch + x*tileDims*bpp/8);
                        assert(PtrAdd(rowDst, viewWidth*bpp/8) <= rawDataEnd);
                        assert(PtrAdd(rowSrc, viewWidth*bpp/8) <= readbackEnd);
                        XlCopyMemory(rowDst, rowSrc, viewWidth*bpp/8);
                    }
                }
                    // destroy now to free up some memory
                target = TargetType();
            }

        return std::move(rawData);
    }

    static float Float16AsFloat32(unsigned short input) { return half_float::detail::half2float(input); }
    static unsigned short Float32AsFloat16(float input) { return half_float::detail::float2half<std::round_to_nearest>(input); }

    static intrusive_ptr<DataPacket> BoxFilterR16G16B16A16(
        DataPacket& highRes,
        UInt2 srcDims, UInt2 downsample)
    {
        const auto bpp = unsigned(sizeof(uint16)*4*8);
        UInt2 downsampledSize(
            srcDims[0] / downsample[0], 
            srcDims[1] / downsample[1]);
        auto downsampledRowPitch = downsampledSize[0] * bpp;
        auto rawData = CreateBasicPacket(
            downsampledSize[1]*downsampledRowPitch, nullptr,
            TexturePitches(downsampledRowPitch, downsampledSize[1]*downsampledRowPitch));

        const uint16* srcData = (const uint16*)highRes.GetData();
        const auto srcRowPitch = highRes.GetPitches()._rowPitch;

        uint16* dstData = (uint16*)rawData->GetData();
        const auto dstRowPitch = rawData->GetPitches()._rowPitch;

            // note that we can do this in-place (or tile-by-tile) to save some memory
        const unsigned weightDiv = downsample[0] * downsample[1];
        for (unsigned y=0; y<downsampledSize[1]; ++y)
            for (unsigned x=0; x<downsampledSize[0]; ++x) {
                // UInt4 dst(0, 0, 0, 0);
                Float4 dst(0, 0, 0, 0);
                for (unsigned sy=0; sy<downsample[1]; ++sy)
                    for (unsigned sx=0; sx<downsample[0]; ++sx) {
                        UInt2 src(x*downsample[0]+sx, y*downsample[1]+sy);
                        auto* s = PtrAdd(srcData, src[1] * srcRowPitch + src[0] * bpp / 8);
                        // dst[0] += s[0]; dst[1] += s[1]; dst[2] += s[2]; dst[3] += s[3];
                        dst[0] += Float16AsFloat32(s[0]);
                        dst[1] += Float16AsFloat32(s[1]);
                        dst[2] += Float16AsFloat32(s[2]);
                    }

                // dst[0] /= weightDiv; assert(dst[0] <= 0xffff);
                // dst[1] /= weightDiv; assert(dst[1] <= 0xffff);
                // dst[2] /= weightDiv; assert(dst[2] <= 0xffff);
                // dst[3] /= weightDiv; assert(dst[3] <= 0xffff);
                dst[0] /= float(weightDiv);
                dst[1] /= float(weightDiv);
                dst[2] /= float(weightDiv);
                dst[3] = 1.f;

                auto* d = PtrAdd(dstData, y*dstRowPitch+x*bpp/8);
                // d[0] = (uint16)dst[0]; d[1] = (uint16)dst[1]; d[2] = (uint16)dst[2]; d[3] = (uint16)dst[3];
                d[0] = Float32AsFloat16(dst[0]);
                d[1] = Float32AsFloat16(dst[1]);
                d[2] = Float32AsFloat16(dst[2]);
                d[3] = Float32AsFloat16(dst[3]);
            }

        return std::move(rawData);
    }


    void TiledScreenshot(
        IThreadContext& context,
        LightingParserContext& parserContext,
        ISceneParser& sceneParser,
        const Techniques::CameraDesc& camera,
        const RenderingQualitySettings& qualitySettings)
    {
        auto preFilterFormat = Metal::NativeFormat::R16G16B16A16_FLOAT;
        auto postFilterFormat = Metal::NativeFormat::R8G8B8A8_UNORM_SRGB;
        auto image = RenderTiled(
            context, parserContext, sceneParser,
            camera, qualitySettings, preFilterFormat);

            // Save the unfiltered image (this is a 16 bit depth linear image)
            // We can use a program like "Luminance HDR" to run custom tonemapping
            // on the unfiltered image...
        SaveImage(
            "screenshot_unfiltered.tiff",
            image->GetData(), qualitySettings._dimensions, 
            image->GetPitches()._rowPitch, preFilterFormat);

            // Do a box filter on the CPU to shrink the result down to
            // the output size. We could consider other filters. But (assuming
            // we're doing an integer downsample) the box filter will mean that
            // each sample point is equally weighted, and it will avoid any
            // blurring to the image.
        image = BoxFilterR16G16B16A16(*image, qualitySettings._dimensions, UInt2(4,4));

        UInt2 finalImageDims(qualitySettings._dimensions[0]/4, qualitySettings._dimensions[1]/4);

            // Now we want to do HDR resolve (on the GPU)
            // We should end up with an 8 bit SRGB image.
            // We have to do both the luminance sample and final tone map on
            // the post-aa image (the operations would otherwise require special
            // 
        SceneEngine::GestaltTypes::SRV preToneMap(
            BufferUploads::TextureDesc::Plain2D(finalImageDims[0], finalImageDims[1], preFilterFormat),
            "SS-PreToneMap", image.get());
        SceneEngine::GestaltTypes::RTV postToneMap(
            BufferUploads::TextureDesc::Plain2D(finalImageDims[0], finalImageDims[1], postFilterFormat),
            "SS-PostToneMap");

        auto metalContext = RenderCore::Metal::DeviceContext::Get(context);
        auto toneMapSettings = sceneParser.GetToneMapSettings();
        auto luminanceRes = 
            SceneEngine::ToneMap_SampleLuminance(
                *metalContext, 
                (RenderCore::Techniques::ParsingContext&)parserContext,
                toneMapSettings, preToneMap.SRV());

        {
            SceneEngine::ProtectState protectState(
                *metalContext, 
                SceneEngine::ProtectState::States::RenderTargets | SceneEngine::ProtectState::States::Viewports);

            metalContext->Bind(MakeResourceList(postToneMap.RTV()), nullptr);
            metalContext->Bind(Metal::ViewportDesc(0, 0, float(finalImageDims[0]), float(finalImageDims[1])));
            ToneMap_Execute(
                *metalContext, parserContext,
                luminanceRes, toneMapSettings,
                preToneMap.SRV());
        }

        auto& uploads = RenderCore::Assets::Services::GetBufferUploads();
        auto postToneMapImage = uploads.Resource_ReadBack(postToneMap.Locator());

        SaveImage(
            "screenshot.tiff",
            postToneMapImage->GetData(), finalImageDims, 
            postToneMapImage->GetPitches()._rowPitch, postFilterFormat);
    }
}

