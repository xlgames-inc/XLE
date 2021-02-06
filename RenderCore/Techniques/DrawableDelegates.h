// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../UniformsStream.h"
#include "../RenderUtils.h"
#include "../Metal/Forward.h"
#include "../StateDesc.h"
#include "../../Assets/AssetsCore.h"
#include "../../Utility/IteratorUtils.h"
#include "../../Utility/StringUtils.h"

namespace RenderCore { class IResource; class ConstantBufferView; }
namespace RenderCore { namespace Assets { class RenderStateSet; } }
namespace Utility { class ParameterBox; }

namespace RenderCore { namespace Techniques 
{
	class ParsingContext;
	extern const char* g_techName;

    class IUniformBufferDelegate
    {
    public:
        virtual ConstantBufferView WriteBuffer(ParsingContext& context, const void* objectContext) = 0;
        virtual IteratorRange<const ConstantBufferElementDesc*> GetLayout() const;
        virtual ~IUniformBufferDelegate();
    };

    class IShaderResourceDelegate
    {
    public:
        using SRV = Metal::ShaderResourceView;
        using SamplerState = Metal::SamplerState;
        virtual void GetShaderResources(
			ParsingContext& context, const void* objectContext,
			IteratorRange<SRV*> dstSRVs,
			IteratorRange<SamplerState*> dstSamplers) const = 0;
        virtual IteratorRange<const uint64_t*> GetBindings() const = 0;
        virtual ~IShaderResourceDelegate();
    };

    DEPRECATED_ATTRIBUTE class IMaterialDelegate
    {
    public:
        virtual UniformsStreamInterface GetInterface(const void* objectContext) const = 0;
        virtual uint64_t GetInterfaceHash(const void* objectContext) const = 0;
		virtual const ParameterBox* GetShaderSelectors(const void* objectContext) const = 0;
        virtual void ApplyUniforms(
            ParsingContext& context,
            Metal::DeviceContext& devContext,
            const Metal::BoundUniforms& boundUniforms,
            unsigned streamIdx,
            const void* objectContext) const = 0;
        virtual ~IMaterialDelegate();
    };

	class DrawableMaterial;

	DEPRECATED_ATTRIBUTE class ITechniqueDelegate_Old
	{
	public:
		virtual Metal::ShaderProgram* GetShader(
			ParsingContext& context,
			const ParameterBox* shaderSelectors[],		// ShaderSelectorFiltering::Source::Max
			const DrawableMaterial& material);

		virtual RenderCore::Metal::ShaderProgram* GetShader(
			ParsingContext& context,
			const ParameterBox* shaderSelectors[],		// ShaderSelectorFiltering::Source::Max
			const DrawableMaterial& material,
			unsigned techniqueIndex);

		virtual ~ITechniqueDelegate_Old();
	};

	class CompiledShaderPatchCollection;

	class ITechniqueDelegate
	{
	public:
		struct ResolvedTechnique
		{
			::Assets::FuturePtr<Metal::ShaderProgram> _shaderProgram;

			DepthStencilDesc	_depthStencil;
			AttachmentBlendDesc _blend;
			RasterizationDesc	_rasterization;
		};

		virtual ResolvedTechnique Resolve(
			const std::shared_ptr<CompiledShaderPatchCollection>& shaderPatches,
			IteratorRange<const ParameterBox**> selectors,
			const RenderCore::Assets::RenderStateSet& renderStates) = 0;

		virtual ~ITechniqueDelegate();
	};

}}
