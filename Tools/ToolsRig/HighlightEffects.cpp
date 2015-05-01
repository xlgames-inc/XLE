// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "HighlightEffects.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/Metal/ShaderResource.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/ResourceBox.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../BufferUploads/ResourceLocator.h"
#include "../../Assets/AssetUtils.h"
#include "../../Utility/StringFormat.h"

namespace ToolsRig
{

    const UInt4 HighlightByStencilSettings::NoHighlight = UInt4(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff);

    HighlightByStencilSettings::HighlightByStencilSettings()
    {
        _outlineColor = Float3(1.5f, 1.35f, .7f);

        _highlightedMarker = NoHighlight;
        for (unsigned c=0; c<dimof(_stencilToMarkerMap); ++c) 
            _stencilToMarkerMap[c] = NoHighlight;
    }

    void ExecuteHighlightByStencil(
        RenderCore::Metal::DeviceContext& metalContext,
        RenderCore::Metal::ShaderResourceView& inputStencil,
        const HighlightByStencilSettings& settings,
        bool onlyHighlighted)
    {
        using namespace RenderCore;

        metalContext.BindPS(MakeResourceList(Metal::ConstantBuffer(&settings, sizeof(settings))));
        metalContext.BindPS(MakeResourceList(inputStencil));
        metalContext.Bind(Techniques::CommonResources()._dssDisable);
        metalContext.Bind(Techniques::CommonResources()._blendAlphaPremultiplied);
        metalContext.Bind(Metal::Topology::TriangleStrip);
        metalContext.Unbind<Metal::BoundInputLayout>();

        auto desc = BufferUploads::ExtractDesc(*inputStencil.GetResource());
        if (desc._type != BufferUploads::BufferDesc::Type::Texture) return;

        bool stencilInput = 
                desc._textureDesc._nativePixelFormat == Metal::NativeFormat::R24G8_TYPELESS
            ||  desc._textureDesc._nativePixelFormat == Metal::NativeFormat::D24_UNORM_S8_UINT
            ||  desc._textureDesc._nativePixelFormat == Metal::NativeFormat::R24_UNORM_X8_TYPELESS
            ||  desc._textureDesc._nativePixelFormat == Metal::NativeFormat::X24_TYPELESS_G8_UINT
            ;
                
        StringMeld<64, ::Assets::ResChar> params;
        params << "ONLY_HIGHLIGHTED=" << unsigned(onlyHighlighted);
        params << ";INPUT_MODE=" << (stencilInput?0:1);

        {
            auto& shader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/basic2D.vsh:fullscreen:vs_*", 
                "game/xleres/Effects/HighlightVis.psh:HighlightByStencil:ps_*",
                (const ::Assets::ResChar*)params);
                
            metalContext.Bind(shader);
            metalContext.Draw(4);
        }

        {
            auto& shader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/basic2D.vsh:fullscreen:vs_*", 
                "game/xleres/Effects/HighlightVis.psh:OutlineByStencil:ps_*",
                (const ::Assets::ResChar*)params);
                
            metalContext.Bind(shader);
            metalContext.Draw(4);
        }

        metalContext.UnbindPS<Metal::ShaderResourceView>(0, 1);
    }


///////////////////////////////////////////////////////////////////////////////////////////////////

    class CommonOffscreenTarget
    {
    public:
        class Desc
        {
        public:
            unsigned _width, _height;
            RenderCore::Metal::NativeFormat::Enum _format;
            Desc(unsigned width, unsigned height, RenderCore::Metal::NativeFormat::Enum format)
                : _width(width), _height(height), _format(format) {}
        };

        RenderCore::Metal::RenderTargetView _rtv;
        RenderCore::Metal::ShaderResourceView _srv;
        intrusive_ptr<BufferUploads::ResourceLocator> _resource;

        CommonOffscreenTarget(const Desc& desc);
        ~CommonOffscreenTarget();
    };

    CommonOffscreenTarget::CommonOffscreenTarget(const Desc& desc)
    {
            //  Still some work involved to just create a texture
            //  
        using namespace BufferUploads;
        auto bufferDesc = CreateDesc(
            BindFlag::ShaderResource|BindFlag::RenderTarget, 0, GPUAccess::Write,
            TextureDesc::Plain2D(desc._width, desc._height, desc._format),
            "CommonOffscreen");

        auto resource = SceneEngine::GetBufferUploads()->Transaction_Immediate(bufferDesc);

        RenderCore::Metal::RenderTargetView rtv(resource->GetUnderlying());
        RenderCore::Metal::ShaderResourceView srv(resource->GetUnderlying());

        _rtv = std::move(rtv);
        _srv = std::move(srv);
        _resource = std::move(resource);
    }

    CommonOffscreenTarget::~CommonOffscreenTarget() {}

    class HighlightShaders
    {
    public:
        class Desc {};

        const RenderCore::Metal::ShaderProgram* _drawHighlight;
        RenderCore::Metal::BoundUniforms _drawHighlightUniforms;

        const Assets::DependencyValidation& GetDependencyValidation() const   { return *_validationCallback; }

        HighlightShaders(const Desc&);
    protected:
        std::shared_ptr<Assets::DependencyValidation>  _validationCallback;
    };

    HighlightShaders::HighlightShaders(const Desc&)
    {
        auto* drawHighlight = &::Assets::GetAssetDep<RenderCore::Metal::ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*", 
            "game/xleres/effects/outlinehighlight.psh:main:ps_*");

        RenderCore::Metal::BoundUniforms uniforms(*drawHighlight);
        uniforms.BindConstantBuffer(Hash64("$Globals"), 0, 1);

        auto validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(validationCallback, &drawHighlight->GetDependencyValidation());

        _validationCallback = std::move(validationCallback);
        _drawHighlight = std::move(drawHighlight);
        _drawHighlightUniforms = std::move(uniforms);
    }

    class BinaryHighlight::Pimpl
    {
    public:
        SceneEngine::SavedTargets _savedTargets;
        RenderCore::Metal::ShaderResourceView _srv;

        Pimpl(RenderCore::Metal::DeviceContext& metalContext)
            : _savedTargets(&metalContext)
        {}
        ~Pimpl() {}
    };

    BinaryHighlight::BinaryHighlight(RenderCore::Metal::DeviceContext& metalContext)
    {
        _pimpl = std::make_unique<Pimpl>(metalContext);

        const auto& viewport = _pimpl->_savedTargets.GetViewports()[0];

        using namespace RenderCore;
        auto& offscreen = RenderCore::Techniques::FindCachedBox<CommonOffscreenTarget>(
            CommonOffscreenTarget::Desc(unsigned(viewport.Width), unsigned(viewport.Height), 
            Metal::NativeFormat::R8G8B8A8_UNORM));

		const bool doDepthTest = true;
		if (constant_expression<doDepthTest>::result()) {
			metalContext.Bind(Techniques::CommonResources()._dssReadOnly);
			Metal::DepthStencilView dsv(_pimpl->_savedTargets.GetDepthStencilView());
			metalContext.Bind(MakeResourceList(offscreen._rtv), &dsv);
		} else {
			metalContext.Bind(MakeResourceList(offscreen._rtv), nullptr);
		}

        metalContext.Clear(offscreen._rtv, Float4(0.f, 0.f, 0.f, 0.f));

        _pimpl->_srv = offscreen._srv;
    }

    void BinaryHighlight::FinishWithOutlineAndOverlay(RenderCore::Metal::DeviceContext& metalContext, Float3 outlineColor, unsigned overlayColor)
    {
        static Float3 highlightColO(1.5f, 1.35f, .7f);
        static unsigned overlayColO = 1;

        outlineColor = highlightColO;
        overlayColor = overlayColO;

        using namespace RenderCore;
        _pimpl->_savedTargets.ResetToOldTargets(&metalContext);

        HighlightByStencilSettings settings;
        settings._outlineColor = outlineColor;
        for (unsigned c=1; c<dimof(settings._stencilToMarkerMap); ++c) settings._stencilToMarkerMap[c] = UInt4(overlayColor, overlayColor, overlayColor, overlayColor);

        ExecuteHighlightByStencil(metalContext, _pimpl->_srv, settings, false);
    }

    void BinaryHighlight::FinishWithOutline(RenderCore::Metal::DeviceContext& metalContext, Float3 outlineColor)
    {
        using namespace RenderCore;
        _pimpl->_savedTargets.ResetToOldTargets(&metalContext);

            //  now we can render these objects over the main image, 
            //  using some filtering

        metalContext.BindPS(MakeResourceList(_pimpl->_srv));

        struct Constants
        {
            Float3 _color; unsigned _dummy;
        } constants = { outlineColor, 0 };
        RenderCore::SharedPkt pkts[] = { MakeSharedPkt(constants) };

        auto& shaders = Techniques::FindCachedBoxDep<HighlightShaders>(HighlightShaders::Desc());
        shaders._drawHighlightUniforms.Apply(
            metalContext, 
            RenderCore::Metal::UniformsStream(), 
            RenderCore::Metal::UniformsStream(pkts, nullptr, dimof(pkts)));
        metalContext.Bind(*shaders._drawHighlight);
        metalContext.Bind(Techniques::CommonResources()._blendAlphaPremultiplied);
        metalContext.Bind(Techniques::CommonResources()._dssDisable);
        metalContext.Bind(Metal::Topology::TriangleStrip);
        metalContext.Draw(4);
    }

    BinaryHighlight::~BinaryHighlight() {}

}

