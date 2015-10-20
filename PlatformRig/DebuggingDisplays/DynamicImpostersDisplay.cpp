// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DynamicImpostersDisplay.h"
#include "../../SceneEngine/DynamicImposters.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../RenderCore/Metal/RenderTargetView.h"
#include "../../RenderCore/Metal/ShaderResource.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../Utility/FunctionUtils.h"

#include "../../RenderCore/DX11/Metal/DX11Utils.h"

namespace PlatformRig { namespace Overlays
{

    void DynamicImpostersDisplay::Render( 
        IOverlayContext* context, Layout& layout, Interactables& interactables, 
        InterfaceState& interfaceState)
    {
        auto m = _manager.lock();
        if (!m) return;

        const auto metrics = m->GetMetrics();

        Layout statsArea = layout.AllocateFullHeight(300);
        Rect textureArea = layout.AllocateFullHeightFraction(1.f);

        float borderSize = 1.f, roundedProportion = 1.f / 12.f;
        context->DrawQuad(
            ProjectionMode::P2D,
            AsPixelCoords(Coord2(statsArea.GetMaximumSize()._topLeft[0], statsArea.GetMaximumSize()._topLeft[1])),
            AsPixelCoords(Coord2(statsArea.GetMaximumSize()._bottomRight[0], statsArea.GetMaximumSize()._bottomRight[1])),
            ColorB(0x4f7f7f7f), ColorB::White,
            Float2(0.f, 0.f), Float2(1.f, 1.f), Float2(borderSize, roundedProportion), Float2(borderSize, roundedProportion),
            "ui\\dd\\shapes.sh:Paint,Shape=RoundedRectShape,Fill=RaisedRefactiveFill,Outline=SolidFill");

        const unsigned lineHeight = 18;
        statsArea.AllocateFullWidth(5 * lineHeight);
        DrawFormatText(
            context, 
            statsArea.AllocateFullWidth(lineHeight),
            nullptr, ColorB(0xffffffff),
            "SpriteCount: %i", metrics._spriteCount);
        statsArea.AllocateFullWidth(lineHeight);
        DrawFormatText(
            context, 
            statsArea.AllocateFullWidth(lineHeight),
            nullptr, ColorB(0xffffffff),
            "Pixels allocated: %.1f%%", 100.f * metrics._pixelsAllocated / float(metrics._pixelsTotal));
        DrawFormatText(
            context, 
            statsArea.AllocateFullWidth(lineHeight),
            nullptr, ColorB(0xffffffff),
            "Largest free 0: (%i, %i)", metrics._largestFreeBlockArea[0], metrics._largestFreeBlockArea[1]);
        DrawFormatText(
            context, 
            statsArea.AllocateFullWidth(lineHeight),
            nullptr, ColorB(0xffffffff),
            "Largest free 1: (%i, %i)", metrics._largestFreeBlockSide[0], metrics._largestFreeBlockSide[1]);
        DrawFormatText(
            context, 
            statsArea.AllocateFullWidth(lineHeight),
            nullptr, ColorB(0xffffffff),
            "Overflow: (%i), Pending: (%i)", metrics._overflowCounter, metrics._pendingCounter);
        statsArea.AllocateFullWidth(lineHeight);
        DrawFormatText(
            context, 
            statsArea.AllocateFullWidth(lineHeight),
            nullptr, ColorB(0xffffffff),
            "Bytes/pixel (%i) in (%i) layers", metrics._bytesPerPixel, metrics._layerCount);
        
        {
            unsigned visibleLayer = 0;
            context->ReleaseState();
            auto cleanup = MakeAutoCleanup(
                [&context]() {context->CaptureState();});

            auto threadContext = context->GetDeviceContext();
            auto metalContext = RenderCore::Metal::DeviceContext::Get(*threadContext);
            RenderCore::Metal::RenderTargetView rtv(*metalContext);
            auto srv = m->GetAtlasResource(visibleLayer);
            RenderCore::Metal::TextureDesc2D desc(srv.GetUnderlying());
            SceneEngine::ShaderBasedCopy(
                *metalContext, rtv, srv,
                std::make_pair(textureArea._topLeft, textureArea._bottomRight),
                std::make_pair(UInt2(0,0), UInt2(desc.Width, desc.Height)),
                SceneEngine::ProtectState::States::RenderTargets);
        }
    }

    bool DynamicImpostersDisplay::ProcessInput(
        InterfaceState& interfaceState, const InputSnapshot& input)
    {
        return false;
    }

    DynamicImpostersDisplay::DynamicImpostersDisplay(
        std::weak_ptr<SceneEngine::DynamicImposters> manager)
    : _manager(std::move(manager))
    {

    }

    DynamicImpostersDisplay::~DynamicImpostersDisplay() {}

}}

