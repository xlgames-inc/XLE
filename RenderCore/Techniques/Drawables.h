// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/MaterialScaffold.h"		// used by DrawableMaterial below
#include "../IDevice.h"
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
	class PipelineAccelerator;
	class IPipelineAcceleratorPool;
	class DescriptorSetAccelerator;
	class SequencerConfig;

	class SequencerContext
	{
	public:
		std::vector<std::pair<uint64_t, std::shared_ptr<IUniformBufferDelegate>>> _sequencerUniforms;
		std::vector<std::shared_ptr<IShaderResourceDelegate>> _sequencerResources;

		const SequencerConfig*	_sequencerConfig = nullptr;
	};

	class DrawableGeo
    {
    public:
        struct VertexStream
        {
            IResourcePtr	_resource;
            unsigned		_vbOffset = 0u;
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

	class DrawableMaterial
	{
	public:
		RenderCore::Assets::MaterialScaffoldMaterial _material;
		std::shared_ptr<RenderCore::Assets::ShaderPatchCollection> _patchCollection;
	};

	class Drawable
	{
	public:
        std::shared_ptr<PipelineAccelerator>		_pipeline;
		std::shared_ptr<DescriptorSetAccelerator>	_descriptorSet;
        std::shared_ptr<DrawableGeo>				_geo;
		std::shared_ptr<UniformsStreamInterface>  	_looseUniformsInterface;

		class DrawFunctionContext
		{
		public:
			void		ApplyLooseUniforms(const UniformsStream&) const;
			void 		ApplyDescriptorSets(IteratorRange<const IDescriptorSet* const*>) const;

			uint64_t 	GetBoundLooseImmediateDatas() const;
			uint64_t 	GetBoundLooseResources() const;
			uint64_t 	GetBoundLooseSamplers() const;

			void        Draw(unsigned vertexCount, unsigned startVertexLocation=0) const;
			void        DrawIndexed(unsigned indexCount, unsigned startIndexLocation=0, unsigned baseVertexLocation=0) const;
			void		DrawInstances(unsigned vertexCount, unsigned instanceCount, unsigned startVertexLocation=0) const;
			void		DrawIndexedInstances(unsigned indexCount, unsigned instanceCount, unsigned startIndexLocation=0, unsigned baseVertexLocation=0) const;
			void        DrawAuto() const;
		};

        typedef void (ExecuteDrawFn)(ParsingContext&, const DrawFunctionContext&, const Drawable&);
        ExecuteDrawFn*	_drawFn;
	};

	class DrawablesPacket
	{
	public:
		VariantArray _drawables;

		enum class Storage { VB, IB };
		struct AllocateStorageResult { IteratorRange<void*> _data; unsigned _startOffset; };
		AllocateStorageResult AllocateStorage(Storage storageType, size_t size);

		void Reset() { _drawables.clear(); _vbStorage.clear(); _ibStorage.clear(); }

		IteratorRange<const void*> GetStorage(Storage storageType) const;
	private:
		std::vector<uint8_t>	_vbStorage;
		std::vector<uint8_t>	_ibStorage;
		unsigned				_storageAlignment = 0u;
	};

	void Draw(
		IThreadContext& context,
        ParsingContext& parserContext,
		const IPipelineAcceleratorPool& pipelineAccelerators,
		const SequencerContext& sequencerTechnique,
		const DrawablesPacket& drawablePkt);

	enum class BatchFilter
    {
        General,                // general rendering batch
        PostOpaque,				// forward rendering mode after the deferred render step (where alpha blending can be used)
        SortedBlending,         // blending step with pixel-accurate depth sorting
		PreDepth,               // objects that should get a pre-depth pass
		Max
    };

}}