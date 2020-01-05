// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../IDevice.h"
#include "../Metal/Forward.h"
#include "../../Utility/VariantUtils.h"
#include "../../Utility/IteratorUtils.h"
#include <vector>
#include <memory>
#include <string>

namespace Utility { class ParameterBox; }
namespace RenderCore { class IThreadContext; class MiniInputElementDesc; class UniformsStreamInterface; class UniformsStream; }
namespace RenderCore { namespace Assets { class MaterialScaffoldMaterial; class ShaderPatchCollection; } }

namespace RenderCore { namespace Techniques
{
	class ParsingContext;
	class IUniformBufferDelegate;
	class IShaderResourceDelegate;
	class IMaterialDelegate;
	class ITechniqueDelegate;
	class IRenderStateDelegate;
	class PipelineAccelerator;
	class DescriptorSetAccelerator;

	class SequencerContext
	{
	public:
		std::vector<std::pair<uint64_t, std::shared_ptr<IUniformBufferDelegate>>> _sequencerUniforms;
		std::vector<std::shared_ptr<IShaderResourceDelegate>> _sequencerResources;

		uint64_t	_sequencerConfigId = ~0ull;
	};

	class DrawableGeo
    {
    public:
        class VertexStream
        {
        public:
            IResourcePtr	_resource;
            std::vector<MiniInputElementDesc> _vertexElements;
            unsigned		_vertexStride = 0u;
            uint64_t		_vertexElementsHash = 0ull;
            unsigned		_vbOffset = 0u;
            unsigned		_instanceStepDataRate = 0u;
        };
        VertexStream        _vertexStreams[4];
        unsigned            _vertexStreamCount = 0;

        IResourcePtr		_ib;
        Format				_ibFormat = Format(0);
        unsigned			_dynIBBegin = ~0u;
        unsigned			_dynIBEnd = 0u;

        struct Flags
        {
            enum Enum { Temporary       = 1 << 0 };
            using BitField = unsigned;
        };
        Flags::BitField     _flags = 0u;
    };

	class DrawableMaterial;
	std::shared_ptr<DrawableMaterial> MakeDrawableMaterial(
		const RenderCore::Assets::MaterialScaffoldMaterial& mat,
		const RenderCore::Assets::ShaderPatchCollection& patchCollection);

	class Drawable
	{
	public:
        std::shared_ptr<PipelineAccelerator>		_pipeline;
		std::shared_ptr<DescriptorSetAccelerator>	_descriptorSet;
        std::shared_ptr<DrawableGeo>				_geo;

		class DrawFunctionContext
		{
		public:
			Metal::DeviceContext*			_metalContext;
			const Metal::GraphicsPipeline*	_pipeline;
			const Metal::BoundUniforms*		_boundUniforms;

			void		ApplyUniforms(const UniformsStream&) const;

			void        Draw(unsigned vertexCount, unsigned startVertexLocation=0) const;
			void        DrawIndexed(unsigned indexCount, unsigned startIndexLocation=0, unsigned baseVertexLocation=0) const;
			void		DrawInstances(unsigned vertexCount, unsigned instanceCount, unsigned startVertexLocation=0) const;
			void		DrawIndexedInstances(unsigned indexCount, unsigned instanceCount, unsigned startIndexLocation=0, unsigned baseVertexLocation=0) const;
			void        DrawAuto() const;
		};

        typedef void (ExecuteDrawFn)(
			ParsingContext& parserContext,
			const DrawFunctionContext& drawFnContext,
            const Drawable&);
        ExecuteDrawFn*						_drawFn;

        std::shared_ptr<UniformsStreamInterface>  _uniformsInterface;
	};

	class DrawablesPacket
	{
	public:
		VariantArray _drawables;
	};

	void Draw(
		IThreadContext& context,
        Techniques::ParsingContext& parserContext,
		const SequencerContext& sequencerTechnique,
		const Drawable& drawable);

	enum class BatchFilter
    {
        General,                // general rendering batch
        Transparent,            // transparent objects (particularly those that require some object based sorting)
        OITransparent,          // order independent transparent
		PreDepth,               // objects that should get a pre-depth pass
        TransparentPreDepth,	// pre-depth pass for objects considered "transparent" (ie, opaque parts of transparent objects)
		Max
    };

}}