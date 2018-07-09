// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../UniformsStream.h"
#include "../RenderUtils.h"
#include "../Metal/Forward.h"
#include "../../Assets/AssetsCore.h"
#include "../../Utility/IteratorUtils.h"

namespace RenderCore { class IResource; }
namespace Utility { class ParameterBox; }

namespace RenderCore { namespace Techniques 
{
	class ParsingContext;

    class IUniformBufferDelegate
    {
    public:
        virtual RenderCore::SharedPkt WriteBuffer(ParsingContext& context, const void* objectContext) = 0;
        virtual IteratorRange<const RenderCore::ConstantBufferElementDesc*> GetLayout() const = 0;
        virtual ~IUniformBufferDelegate();
    };

    class IShaderResourceDelegate
    {
    public:
        using SRV = RenderCore::Metal::ShaderResourceView;
        using SamplerState = RenderCore::Metal::SamplerState;
        virtual void GetShaderResources(
			ParsingContext& context, const void* objectContext,
			IteratorRange<SRV*> dstSRVs,
			IteratorRange<SamplerState*> dstSamplers) const = 0;
        virtual IteratorRange<const uint64_t*> GetBindings() const = 0;
        virtual ~IShaderResourceDelegate();
    };

    class IMaterialDelegate
    {
    public:
        virtual RenderCore::UniformsStreamInterface GetInterface(const void* objectContext) const = 0;
        virtual uint64_t GetInterfaceHash(const void* objectContext) const = 0;
		virtual const ParameterBox* GetShaderSelectors(const void* objectContext) const = 0;
        virtual void ApplyUniforms(
            ParsingContext& context,
            RenderCore::Metal::DeviceContext& devContext,
            const RenderCore::Metal::BoundUniforms& boundUniforms,
            unsigned streamIdx,
            const void* objectContext) const = 0;
        virtual ~IMaterialDelegate();
    };

	class ITechniqueDelegate
	{
	public:
		virtual RenderCore::Metal::ShaderProgram* GetShader(
			ParsingContext& context,
			StringSection<::Assets::ResChar> techniqueCfgFile,
			const ParameterBox* shaderSelectors[],		// ShaderSelectors::Source::Max
			unsigned techniqueIndex) = 0;
		virtual ~ITechniqueDelegate();
	};

}}
