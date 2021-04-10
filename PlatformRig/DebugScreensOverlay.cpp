// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DebugScreensOverlay.h"
#include "OverlaySystem.h"
#include "MainInputHandler.h"
#include "../RenderOverlays/DebuggingDisplay.h"
#include "../RenderOverlays/OverlayContext.h"
#include "../RenderCore/IDevice.h"
#include "../RenderCore/Techniques/RenderPassUtils.h"
#include "../RenderCore/Techniques/RenderPass.h"
#include "../RenderCore/Techniques/ImmediateDrawables.h"
#include "../Math/Vector.h"

namespace PlatformRig
{
    
    class DebugScreensOverlay : public IOverlaySystem
    {
    public:
        DebugScreensOverlay(
            std::shared_ptr<RenderOverlays::DebuggingDisplay::DebugScreensSystem> debugScreensSystem,
            std::shared_ptr<RenderCore::Techniques::IImmediateDrawables> immediateDrawables,
            std::shared_ptr<RenderOverlays::FontRenderingManager> fontRenderer)
        : _debugScreensSystem(debugScreensSystem)
        , _inputListener(std::make_shared<PlatformRig::DebugScreensInputHandler>(std::move(debugScreensSystem)))
        , _immediateDrawables(std::move(immediateDrawables))
        , _fontRenderer(std::move(fontRenderer))
        {
        }

        std::shared_ptr<IInputListener> GetInputListener()  { return _inputListener; }

        void Render(
            RenderCore::IThreadContext& threadContext,
			const RenderCore::IResourcePtr& renderTarget,
            RenderCore::Techniques::ParsingContext& parserContext)
        {
            auto overlayContext = RenderOverlays::MakeImmediateOverlayContext(threadContext, *_immediateDrawables, *_fontRenderer);
            
            auto targetDesc = renderTarget->GetDesc();
            Int2 viewportDims{ targetDesc._textureDesc._width, targetDesc._textureDesc._height };
            _debugScreensSystem->Render(*overlayContext, RenderOverlays::DebuggingDisplay::Rect{ {0,0}, viewportDims });

            auto rpi = RenderCore::Techniques::RenderPassToPresentationTarget(threadContext, renderTarget, parserContext);
            _immediateDrawables->ExecuteDraws(threadContext, parserContext, rpi.GetFrameBufferDesc(), 0);
        }

        void SetActivationState(bool) {}

    private:
        std::shared_ptr<RenderOverlays::DebuggingDisplay::DebugScreensSystem> _debugScreensSystem;
        std::shared_ptr<DebugScreensInputHandler> _inputListener;
        std::shared_ptr<RenderCore::Techniques::IImmediateDrawables> _immediateDrawables;
        std::shared_ptr<RenderOverlays::FontRenderingManager> _fontRenderer;
    };

    std::shared_ptr<IOverlaySystem> CreateDebugScreensOverlay(
        std::shared_ptr<RenderOverlays::DebuggingDisplay::DebugScreensSystem> debugScreensSystem,
        std::shared_ptr<RenderCore::Techniques::IImmediateDrawables> immediateDrawables,
        std::shared_ptr<RenderOverlays::FontRenderingManager> fontRenderer)
    {
        return std::make_shared<DebugScreensOverlay>(std::move(debugScreensSystem), std::move(immediateDrawables), std::move(fontRenderer));
    }

}

