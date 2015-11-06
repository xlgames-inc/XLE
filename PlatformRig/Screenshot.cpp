// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Screenshot.h"
#include "../SceneEngine/LightingParserContext.h"
#include "../SceneEngine/LightingParser.h"
#include "../SceneEngine/GestaltResource.h"
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
// #include <wincodec.h>        (avoiding an extra header from the winsdk by hard coding GUID_ContainerFormatTiff below)

namespace PlatformRig
{
    using namespace SceneEngine;
    using namespace RenderCore;
    using namespace BufferUploads;

    static void SaveScreenshot(
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
        DirectX::SaveToWICFile(
            image, DirectX::WIC_FLAGS_NONE,
            GUID_ContainerFormatTiff,
            fn.c_str());
    }

    void TiledScreenshot(
        IThreadContext& context,
        LightingParserContext& parserContext,
        ISceneParser& sceneParser,
        const Techniques::CameraDesc& camera,
        const RenderingQualitySettings& qualitySettings)
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
        auto format = Metal::NativeFormat::R16G16B16A16_UNORM;
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
        auto rawData = std::make_unique<uint8[]>(finalImageDims[1]*finalRowPitch);

        for (unsigned y=0; y<tilesY; ++y)
            for (unsigned x=0; x<tilesX; ++x) {
                auto& target = targets[y*tilesX+x];
                {
                    auto readback = uploads.Resource_ReadBack(target.Locator());

                    auto viewWidth  = std::min((x+1)*tileDims, qualitySettings._dimensions[0]) - (x*tileDims);
                    auto viewHeight = std::min((y+1)*tileDims, qualitySettings._dimensions[1]) - (y*tileDims);

                        // copy each row of the tile into the correct spot in the output texture
                    for (unsigned r=0; r<viewHeight; ++r) {
                        const void* rowSrc = PtrAdd(readback->GetData(), r*readback->GetPitches()._rowPitch);
                        void* rowDst = PtrAdd(rawData.get(), (y*tileDims+r)*finalRowPitch + x*tileDims*bpp/8);
                        XlCopyMemory(rowDst, rowSrc, viewWidth*bpp/8);
                    }
                }
                    // destroy now to free up some memory
                target = TargetType();
            }

        SaveScreenshot(
            "screenshot.tiff",
            rawData.get(), finalImageDims, 
            finalRowPitch, format);
    }

}

