// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DrawableDelegates.h"
#include "PredefinedCBLayout.h"

namespace RenderCore { namespace Techniques
{
	class MaterialDelegate_Basic : public IMaterialDelegate
    {
    public:
        virtual RenderCore::UniformsStreamInterface GetInterface(const void* objectContext) const;
        virtual uint64_t GetInterfaceHash(const void* objectContext) const;
		virtual const ParameterBox* GetShaderSelectors(const void* objectContext) const;
        virtual void ApplyUniforms(
            ParsingContext& context,
            RenderCore::Metal::DeviceContext& devContext,
            const RenderCore::Metal::BoundUniforms& boundUniforms,
            unsigned streamIdx,
            const void* objectContext) const;

        MaterialDelegate_Basic();
		~MaterialDelegate_Basic();

	private:
		// PredefinedCBLayout _cbLayout;
    };

	class TechniqueDelegate_Basic : public ITechniqueDelegate
	{
	public:
		virtual RenderCore::Metal::ShaderProgram* GetShader(
			ParsingContext& context,
			StringSection<::Assets::ResChar> techniqueCfgFile,
			const ParameterBox* shaderSelectors[],		// ShaderSelectors::Source::Max
			unsigned techniqueIndex);

		TechniqueDelegate_Basic();
		~TechniqueDelegate_Basic();
	};

	class GlobalCBDelegate : public IUniformBufferDelegate
    {
    public:
        ConstantBufferView WriteBuffer(ParsingContext& context, const void* objectContext);
        IteratorRange<const ConstantBufferElementDesc*> GetLayout() const;
		GlobalCBDelegate(unsigned cbIndex = 0) : _cbIndex(cbIndex) {}
	private:
		unsigned _cbIndex = 0;
    };

}}
