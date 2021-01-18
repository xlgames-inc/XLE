// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Screenshot.h"

#if 0

#include "../SceneEngine/LightingParserContext.h"
#include "../SceneEngine/LightingParser.h"
#include "../SceneEngine/GestaltResource.h"
#include "../SceneEngine/SceneParser.h"
#include "../SceneEngine/Tonemap.h"
#include "../SceneEngine/SceneEngineUtils.h"
#include "../SceneEngine/PreparedScene.h"
#include "../RenderCore/Techniques/TechniqueUtils.h"
#include "../RenderCore/IThreadContext.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Assets/Services.h"
#include "../RenderCore/Format.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/DataPacket.h"
#include "../ConsoleRig/Console.h"
#include "../Utility/BitUtils.h"
#include "../Utility/Conversion.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/StringFormat.h"
#include "../OSServices/BasicFile.h"

#include "../OSServices/WinAPI/IncludeWindows.h"
#include "../Foreign/DirectXTex/DirectXTex/DirectXTex.h"
#include "../Foreign/half-1.9.2/include/half.hpp"
#include "../RenderCore/DX11/Metal/Format.h"
// #include <wincodec.h>        (avoiding an extra header from the winsdk by hard coding GUID_ContainerFormatTiff below)

namespace PlatformRig
{
    using namespace SceneEngine;
    using namespace RenderCore;

    static void SaveImage(
        const char destinationFile[],
        const void* imageData,
        VectorPattern<unsigned,2> dimensions,
        unsigned rowPitch,
        Format format)
    {
            // using DirectXTex to save to disk as a TGA file...
        DirectX::Image image {
            dimensions[0], dimensions[1],
            Metal_DX11::AsDXGIFormat(format),
            rowPitch, rowPitch * dimensions[1],
            (uint8_t*)imageData };
        auto fn = Conversion::Convert<std::basic_string<utf16>>(std::string(destinationFile));

        // DirectX::SaveToDDSFile(image, DirectX::DDS_FLAGS_NONE, fn.c_str());

        const GUID GUID_ContainerFormatTiff = 
            { 0x163bcc30, 0xe2e9, 0x4f0b, { 0x96, 0x1d, 0xa3, 0xe9, 0xfd, 0xb7, 0x88, 0xa3 }};
        auto hresult = DirectX::SaveToWICFile(
            image, DirectX::WIC_FLAGS_NONE,
            GUID_ContainerFormatTiff,
            (const wchar_t*)fn.c_str());
        (void)hresult;
        assert(SUCCEEDED(hresult));
    }

    static intrusive_ptr<BufferUploads::DataPacket> RenderTiled(
        IThreadContext& context,
        LightingParserContext& parserContext,
        ISceneParser& sceneParser,
        const Techniques::CameraDesc& camera,
        const SceneTechniqueDesc& qualitySettings,
        UInt2 sampleCount,
        Format format, bool interleavedTiles)
    {
        // We want to separate the view into several tiles, and render
        // each as a separate high-res render. Then we will stitch them
        // together and write out one extremely high-res result.

        UInt2 tileDims;
        unsigned tilesX, tilesY;
        unsigned skirt = 0;     // we need to ignore the outermost pixels when not in interleaved mode... This is because AO often has the wrong values on the edge of the screen
        UInt2 activeDims;
        if (!interleavedTiles) {
            tileDims = UInt2(2048u, 2048u);
            skirt = Tweakable("ScreenshotSkirt", 32);
            activeDims = UInt2(tileDims[0]-2*skirt, tileDims[1]-2*skirt);
            tilesX = CeilToMultiple(qualitySettings._dimensions[0] * sampleCount[0], activeDims[0]) / (activeDims[0]);
            tilesY = CeilToMultiple(qualitySettings._dimensions[1] * sampleCount[1], activeDims[1]) / (activeDims[1]);
        } else {
            tilesX = sampleCount[0];
            tilesY = sampleCount[1];
            tileDims = UInt2(qualitySettings._dimensions[0], qualitySettings._dimensions[1]);
            activeDims = tileDims;
        }
        auto tileQualSettings = qualitySettings;

            // Note that we should write out to a linear format
            // so that downsampling can be done in linear space
            // Because it's linear, we need a little extra precision
            // to avoid banding post gamma correction.
        using TargetType = GestaltTypes::RTVSRV;
        std::vector<TargetType> targets;
        targets.resize(tilesX*tilesY);

        auto metalContext = RenderCore::Metal::DeviceContext::Get(context);

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

                unsigned samplingPassIndex = 0, samplingPassCount = 1;
                unsigned viewWidth, viewHeight;
                if (!interleavedTiles) {
                    viewWidth  = std::min((x+1)*activeDims[0], qualitySettings._dimensions[0] * sampleCount[0]) - (x*activeDims[0]);
                    viewHeight = std::min((y+1)*activeDims[1], qualitySettings._dimensions[1] * sampleCount[1]) - (y*activeDims[1]);
                } else {
                    viewWidth = activeDims[0];
                    viewHeight = activeDims[1];
                    samplingPassIndex = x + y*tilesX;
                    samplingPassCount = tilesX*tilesY;
                }
                tileQualSettings._dimensions = {viewWidth+2*skirt, viewHeight+2*skirt};
                auto rtDesc = TextureDesc::Plain2D(viewWidth+2*skirt, viewHeight+2*skirt, format);
                target = TargetType(rtDesc, "HighResScreenShot");

                auto sceneMarker = LightingParser_SetupScene(
                    *metalContext, parserContext, 
                    &sceneParser, samplingPassIndex, samplingPassCount);

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
                Float4x4 customProjectionMatrix;
                if (!interleavedTiles) {
                    customProjectionMatrix = PerspectiveProjection(
                        LinearInterpolate(l, r, (int(x*activeDims[0]              - skirt))/float(qualitySettings._dimensions[0] * sampleCount[0])),
                        LinearInterpolate(t, b, (int(y*activeDims[1]              - skirt))/float(qualitySettings._dimensions[1] * sampleCount[1])),
                        LinearInterpolate(l, r, (int(x*activeDims[0] +  viewWidth + skirt))/float(qualitySettings._dimensions[0] * sampleCount[0])),
                        LinearInterpolate(t, b, (int(y*activeDims[1] + viewHeight + skirt))/float(qualitySettings._dimensions[1] * sampleCount[1])),
                        camera._nearClip, camera._farClip, Techniques::GetDefaultClipSpaceType());
                } else {
                    Float2 subpixelOffset(
                        ((x+.5f)/float(tilesX) - 0.5f)/float(rtDesc._width), 
                        ((y+.5f)/float(tilesY) - 0.5f)/float(rtDesc._height));
                    customProjectionMatrix = PerspectiveProjection(
                        l + n * subpixelOffset[0], t + n * subpixelOffset[1],
                        r + n * subpixelOffset[0], b + n * subpixelOffset[1],
                        camera._nearClip, camera._farClip, Techniques::GetDefaultClipSpaceType());
                }

                auto projDesc = BuildProjectionDesc(camera, UInt2(rtDesc._width, rtDesc._height), &customProjectionMatrix);

                    // now we can just render, using the normal process.
                parserContext.Reset();
                metalContext->Bind(MakeResourceList(target.RTV()), nullptr);
                metalContext->Bind(Metal::ViewportDesc(0.f, 0.f, float(rtDesc._width), float(rtDesc._height)));
                LightingParser_SetGlobalTransform(*metalContext, parserContext, projDesc);
                sceneParser.PrepareScene(context, parserContext, sceneMarker.GetPreparedScene());
                LightingParser_ExecuteScene(context, parserContext, tileQualSettings, sceneMarker.GetPreparedScene());
            }

        doToneMap = oldDoToneMap;

        auto& uploads = RenderCore::Techniques::Services::GetBufferUploads();

            // Now pull the data over to the CPU, and stitch together
            // We will write out the raw data in some simple format
            //      -- the user can complete processing in a image editing application
        UInt2 finalImageDims(qualitySettings._dimensions[0]*sampleCount[0], qualitySettings._dimensions[1]*sampleCount[1]);
        auto bpp = BitsPerPixel(format);
        auto finalRowPitch = finalImageDims[0]*bpp/8;
        auto rawData = BufferUploads::CreateBasicPacket(
            finalImageDims[1]*finalRowPitch, nullptr,
            BufferUploads::TexturePitches{finalRowPitch, finalImageDims[1]*finalRowPitch});
        auto* rawDataEnd = PtrAdd(rawData->GetData(), rawData->GetDataSize());
        (void)rawDataEnd;

        for (unsigned y=0; y<tilesY; ++y)
            for (unsigned x=0; x<tilesX; ++x) {
                auto& target = targets[y*tilesX+x];
                {
                    auto readback = uploads.Resource_ReadBack(target.Locator());
                    auto* readbackEnd = PtrAdd(readback->GetData(), readback->GetDataSize());
                    (void)readbackEnd;

                    unsigned viewWidth, viewHeight;
                    if (!interleavedTiles) {
                        viewWidth  = std::min((x+1)*activeDims[0], qualitySettings._dimensions[0] * sampleCount[0]) - (x*activeDims[0]);
                        viewHeight = std::min((y+1)*activeDims[1], qualitySettings._dimensions[1] * sampleCount[1]) - (y*activeDims[1]);
                    } else {
                        viewWidth = activeDims[0];
                        viewHeight = activeDims[1];
                    }

                        // copy each row of the tile into the correct spot in the output texture
                    for (unsigned r=0; r<viewHeight; ++r) {
                        const void* rowSrc = PtrAdd(readback->GetData(), (r+skirt)*readback->GetPitches()._rowPitch + skirt*bpp/8);
                        void* rowDst = PtrAdd(rawData->GetData(), (y*activeDims[1]+r)*finalRowPitch + x*activeDims[0]*bpp/8);
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

///////////////////////////////////////////////////////////////////////////////////////////////////

    static intrusive_ptr<BufferUploads::DataPacket> BoxFilterR16G16B16A16F(
		BufferUploads::DataPacket& highRes, VectorPattern<unsigned,2> srcDims, VectorPattern<unsigned,2> downsample,
        bool interleavedTiles)
    {
        const auto bpp = unsigned(sizeof(uint16)*4*8);
        UInt2 downsampledSize(srcDims[0] / downsample[0], srcDims[1] / downsample[1]);
        auto downsampledRowPitch = downsampledSize[0] * bpp / 8;
        auto rawData = BufferUploads::CreateBasicPacket(
            downsampledSize[1]*downsampledRowPitch, nullptr,
            BufferUploads::TexturePitches{downsampledRowPitch, downsampledSize[1]*downsampledRowPitch});

        const uint16* srcData = (const uint16*)highRes.GetData();
        const auto srcRowPitch = highRes.GetPitches()._rowPitch;

        uint16* dstData = (uint16*)rawData->GetData();
        const auto dstRowPitch = rawData->GetPitches()._rowPitch;

            // note that we can do this in-place (or tile-by-tile) to save some memory
        const unsigned weightDiv = downsample[0] * downsample[1];
        for (unsigned y=0; y<downsampledSize[1]; ++y)
            for (unsigned x=0; x<downsampledSize[0]; ++x) {
                Float4 dst(0, 0, 0, 0);

                if (!interleavedTiles) {
                    for (unsigned sy=0; sy<downsample[1]; ++sy)
                        for (unsigned sx=0; sx<downsample[0]; ++sx) {
                            UInt2 src(x*downsample[0]+sx, y*downsample[1]+sy);
                            auto* s = PtrAdd(srcData, src[1] * srcRowPitch + src[0] * bpp/8);
                            dst[0] += Float16AsFloat32(s[0]);
                            dst[1] += Float16AsFloat32(s[1]);
                            dst[2] += Float16AsFloat32(s[2]);
                        }
                } else {
                    for (unsigned sy=0; sy<downsample[1]; ++sy)
                        for (unsigned sx=0; sx<downsample[0]; ++sx) {
                            UInt2 src(x+sx*downsampledSize[0], y+sy*downsampledSize[1]);
                            auto* s = PtrAdd(srcData, src[1] * srcRowPitch + src[0] * bpp/8);
                            dst[0] += Float16AsFloat32(s[0]);
                            dst[1] += Float16AsFloat32(s[1]);
                            dst[2] += Float16AsFloat32(s[2]);
                        }
                }

                dst[0] /= float(weightDiv);
                dst[1] /= float(weightDiv);
                dst[2] /= float(weightDiv);
                dst[3] = 1.f;

                auto* d = PtrAdd(dstData, y*dstRowPitch+x*bpp/8);
                d[0] = Float32AsFloat16(dst[0]);
                d[1] = Float32AsFloat16(dst[1]);
                d[2] = Float32AsFloat16(dst[2]);
                d[3] = Float32AsFloat16(dst[3]);
            }

        return std::move(rawData);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static intrusive_ptr<BufferUploads::DataPacket> DoToneMap(
        IThreadContext& context,
        LightingParserContext& parserContext,
		BufferUploads::DataPacket& inputImage, VectorPattern<unsigned,2> dimensions,
		Format preFilterFormat,
        Format postFilterFormat,
        const SceneEngine::ToneMapSettings& toneMapSettings)
    {
                    // Now we want to do HDR resolve (on the GPU)
            // We should end up with an 8 bit SRGB image.
            // We have to do both the luminance sample and final tone map on
            // the post-aa image (the operations would otherwise require special
            // 
        SceneEngine::GestaltTypes::SRV preToneMap(
            TextureDesc::Plain2D(dimensions[0], dimensions[1], preFilterFormat),
            "SS-PreToneMap", &inputImage);
        SceneEngine::GestaltTypes::RTV postToneMap(
            TextureDesc::Plain2D(dimensions[0], dimensions[1], postFilterFormat),
            "SS-PostToneMap");

        auto metalContext = RenderCore::Metal::DeviceContext::Get(context);
        auto luminanceRes = 
            SceneEngine::ToneMap_SampleLuminance(
                *metalContext, 
                (RenderCore::Techniques::ParsingContext&)parserContext,
                toneMapSettings, preToneMap.SRV(), false);

#if 0 // platformtemp
        {
            SceneEngine::ProtectState protectState(
                *metalContext, 
                SceneEngine::ProtectState::States::RenderTargets | SceneEngine::ProtectState::States::Viewports);

            metalContext->Bind(MakeResourceList(postToneMap.RTV()), nullptr);
            metalContext->Bind(Metal::ViewportDesc(0, 0, float(dimensions[0]), float(dimensions[1])));
            ToneMap_Execute(
                *metalContext, parserContext,
                luminanceRes, toneMapSettings,
                preToneMap.SRV());
        }
#endif

        auto& uploads = RenderCore::Techniques::Services::GetBufferUploads();
        return uploads.Resource_ReadBack(postToneMap.Locator());
    }

    std::string FindOutputFilename()
    {
        CreateDirectoryRecursive("int/screenshots");
        unsigned index = 0;
        for (;;index++) {
            StringMeld<256> fn;
            fn << "int/screenshots/shot" << index << ".tiff";
            if (!DoesFileExist(fn))
                return fn.get();
        }
    }

    void TiledScreenshot(
        IThreadContext& context,
		Techniques::ParsingContext& parserContext,
        ISceneParser& sceneParser,
        const Techniques::CameraDesc& camera,
        const SceneTechniqueDesc& qualitySettings,
        UInt2 sampleCount)
    {
        auto preFilterFormat = Format::R16G16B16A16_FLOAT;
        auto postFilterFormat = Format::R8G8B8A8_UNORM_SRGB;
        auto highResQual = qualitySettings;
        highResQual._dimensions[0] *= sampleCount[0];
        highResQual._dimensions[1] *= sampleCount[1];
        const bool interleavedTiles = Tweakable("ScreenshotInterleaved", false);

        auto image = RenderTiled(
            context, parserContext, sceneParser,
            camera, qualitySettings, sampleCount, 
            preFilterFormat, interleavedTiles);

            // Save the unfiltered image (this is a 16 bit depth linear image)
            // We can use a program like "Luminance HDR" to run custom tonemapping
            // on the unfiltered image...
        // SaveImage(
        //     "screenshot_unfiltered.tiff",
        //     image->GetData(), highResQual._dimensions, 
        //     image->GetPitches()._rowPitch, preFilterFormat);

            // Do a box filter on the CPU to shrink the result down to
            // the output size. We could consider other filters. But (assuming
            // we're doing an integer downsample) the box filter will mean that
            // each sample point is equally weighted, and it will avoid any
            // blurring to the image.
        image = BoxFilterR16G16B16A16F(*image, highResQual._dimensions, sampleCount, interleavedTiles);

        // SaveImage(
        //     "pretonemap.tiff",
        //     image->GetData(), qualitySettings._dimensions, 
        //     image->GetPitches()._rowPitch, preFilterFormat);

        auto postToneMapImage = DoToneMap(
            context, parserContext,
            *image, qualitySettings._dimensions, preFilterFormat, postFilterFormat,
            sceneParser.GetToneMapSettings());

        SaveImage(
            FindOutputFilename().c_str(),
            postToneMapImage->GetData(), qualitySettings._dimensions, 
            postToneMapImage->GetPitches()._rowPitch, postFilterFormat);
    }
}

#else

namespace PlatformRig
{
    void TiledScreenshot(
        RenderCore::IThreadContext& context,
        RenderCore::Techniques::ParsingContext& parserContext,
        SceneEngine::IScene& sceneParser,
        const RenderCore::Techniques::CameraDesc& camera,
        const SceneEngine::CompiledSceneTechnique& qualitySettings,
        UInt2 sampleCount) {}
}

#endif

