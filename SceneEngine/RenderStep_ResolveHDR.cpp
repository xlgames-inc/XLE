// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)


#include "RenderStep.h"
#include "LightingTargets.h"
#include "LightingParserContext.h"
#include "LightingParser.h"
#include "Tonemap.h"
#include "../RenderCore/Techniques/RenderPass.h"
#include "../RenderCore/Techniques/CommonBindings.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Format.h"
#include "../RenderCore/FrameBufferDesc.h"
#include "../RenderCore/IAnnotator.h"
#include "../RenderCore/Metal/DeviceContext.h"

namespace SceneEngine
{
	using namespace RenderCore;

	class RenderStep_ResolveHDR : public IRenderStep
	{
	public:
		const RenderCore::Techniques::FrameBufferDescFragment& GetInterface() const { return _fragment; }
		void Execute(
			IThreadContext& threadContext,
			Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			Techniques::RenderPassFragment& rpi);

		RenderStep_ResolveHDR();
		~RenderStep_ResolveHDR();
	private:
		Techniques::FrameBufferDescFragment _fragment;
	};

	RenderStep_ResolveHDR::RenderStep_ResolveHDR()
	{
		auto input = _fragment.DefineAttachment(Techniques::AttachmentSemantics::HDRColor);
		auto output = _fragment.DefineAttachment(
			Techniques::AttachmentSemantics::LDRColor,
			AttachmentDesc { Format::R8G8B8A8_UNORM });

		_fragment.AddSubpass(
			SubpassDesc {
				std::vector<AttachmentViewDesc> {
					{ output, LoadStore::DontCare, LoadStore::Retain },
				},
				SubpassDesc::Unused,
				std::vector<AttachmentViewDesc> {
					{ input, LoadStore::Retain, LoadStore::DontCare },
				}
			});
	}

	RenderStep_ResolveHDR::~RenderStep_ResolveHDR() {}

	static void LightingParser_ResolveMSAA(
        Metal::DeviceContext& context, 
        LightingParserContext& parserContext,
        RenderCore::Resource& destinationTexture,
        RenderCore::Resource& sourceTexture,
        Format resolveFormat)
    {
		#if GFXAPI_ACTIVE == GFXAPI_DX11	// platformtemp
				// todo -- support custom resolve (tone-map aware)
				// See AMD post on this topic:
				//      http://gpuopen.com/optimized-reversible-tonemapper-for-resolve/
			context.GetUnderlying()->ResolveSubresource(
				Metal::AsResource(destinationTexture).GetUnderlying().get(), D3D11CalcSubresource(0,0,0),
				Metal::AsResource(sourceTexture).GetUnderlying().get(), D3D11CalcSubresource(0,0,0),
				Metal::AsDXGIFormat(resolveFormat));
		#endif
    }

	void RenderStep_ResolveHDR::Execute(
		IThreadContext& threadContext,
		Techniques::ParsingContext& parsingContext,
		LightingParserContext& lightingParserContext,
		Techniques::RenderPassFragment& rpi)
	{
		GPUAnnotation anno(threadContext, "Resolve-MSAA-HDR");

		auto* postLightingResolveInput = rpi.GetSRV(0);
		assert(postLightingResolveInput);

#if 0   // platformtemp
            //
            //      Post lighting resolve operations...
            //          we must bind the depth buffer to whatever
            //          buffer contained the correct depth information from the
            //          previous rendering (might be the default depth buffer, or might
            //          not be)
            //
        if (qualitySettings._samplingCount > 1) {
            auto inputTextureDesc = Metal::ExtractDesc(postLightingResolveTexture->GetUnderlying());
			auto& msaaResolveRes = Techniques::FindCachedBox2<FinalResolveResources>(
				inputTextureDesc._textureDesc._width, inputTextureDesc._textureDesc._height, inputTextureDesc._textureDesc._format);
            LightingParser_ResolveMSAA(
                metalContext, parserContext,
                *msaaResolveRes._postMsaaResolveTexture->GetUnderlying(),
                *postLightingResolveTexture->GetUnderlying(),
				inputTextureDesc._textureDesc._format);

                // todo -- also resolve the depth buffer...!
                //      here; we switch the active textures to the msaa resolved textures
            postLightingResolve = IMainTargets::PostMSAALightResolve;
        }

        metalContext.Bind(MakeResourceList(postLightingResolveRTV), nullptr);       // we don't have a single-sample depths target at this time (only multisample)
        LightingParser_PostProcess(metalContext, parserContext);
#endif

        auto toneMapSettings = lightingParserContext._delegate->GetToneMapSettings();
        LuminanceResult luminanceResult;
        if (toneMapSettings._flags & ToneMapSettings::Flags::EnableToneMap) {
                //  (must resolve luminance early, because we use it during the MSAA resolve)
            luminanceResult = ToneMap_SampleLuminance(
                threadContext, parsingContext, toneMapSettings, 
				*postLightingResolveInput);
        }

            //  Write final colour to output texture
            //  We have to be careful about whether "SRGB" is enabled
            //  on the back buffer we're writing to. Depending on the
            //  tone mapping method, sometimes we want the SRGB conversion,
            //  other times we don't (because some tone map operations produce
            //  SRGB results, others give linear results)

        ToneMap_Execute(
            threadContext, parsingContext, luminanceResult, toneMapSettings, 
            *postLightingResolveInput);
	}
}


