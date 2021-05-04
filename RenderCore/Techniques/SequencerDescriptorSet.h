// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Drawables.h"
#include "DrawableDelegates.h"
#include "ParsingContext.h"
#include "../UniformsStream.h"
#include "../BufferView.h"
#include "../../Utility/ArithmeticUtils.h"
#include "../../Utility/BitUtils.h"

namespace RenderCore { namespace Techniques
{
	struct SequencerUniformsHelper
	{
		UniformsStreamInterface _finalUSI;
		uint64_t _slotsQueried_ResourceViews = 0ull;
		uint64_t _slotsQueried_Samplers = 0ull;
		uint64_t _slotsQueried_ImmediateDatas = 0ull;

		std::vector<IResourceView*> _queriedResources;
		std::vector<ISampler*> _queriedSamplers;
		std::vector<UniformsStream::ImmediateData> _queriedImmediateDatas;

		std::vector<uint8_t> _tempDataBuffer;

		size_t _workingTempBufferSize = 0;
		static constexpr unsigned s_immediateDataAlignment = 8;

		////////////////////////////////////////////////////////////////////////////////////
		struct ShaderResourceDelegateBinding
		{
			IShaderResourceDelegate* _delegate = nullptr;
			std::vector<std::pair<size_t, size_t>> _immediateDataBeginAndEnd;

			uint64_t _usiSlotsFilled_ResourceViews = 0ull;
			uint64_t _usiSlotsFilled_Samplers = 0ull;
			uint64_t _usiSlotsFilled_ImmediateDatas = 0ull;

			std::vector<unsigned> _resourceInterfaceToUSI;
			std::vector<unsigned> _immediateDataInterfaceToUSI;
			std::vector<unsigned> _samplerInterfaceToUSI;
		};
		std::vector<ShaderResourceDelegateBinding> _srBindings;
		void Prepare(IShaderResourceDelegate& del, ParsingContext& parsingContext);
		void QueryResources(ParsingContext& parsingContext, uint64_t resourcesToQuery, ShaderResourceDelegateBinding& del);
		void QuerySamplers(ParsingContext& parsingContext, uint64_t samplersToQuery, ShaderResourceDelegateBinding& del);
		void QueryImmediateDatas(ParsingContext& parsingContext, uint64_t immediateDatasToQuery, ShaderResourceDelegateBinding& del);

		////////////////////////////////////////////////////////////////////////////////////
		struct UniformBufferDelegateBinding
		{
			IUniformBufferDelegate* _delegate = nullptr;
			size_t _size = 0;

			unsigned _usiSlotFilled = 0;
			size_t _tempBufferOffset = 0;
		};
		std::vector<UniformBufferDelegateBinding> _uBindings;
		void Prepare(IUniformBufferDelegate& del, uint64_t delBinding);
		void QueryImmediateDatas(ParsingContext& parsingContext, uint64_t immediateDatasToQuery, UniformBufferDelegateBinding& del);

		////////////////////////////////////////////////////////////////////////////////////
		void QueryResources(ParsingContext& parsingContext, uint64_t resourcesToQuery);
		void QuerySamplers(ParsingContext& parsingContext, uint64_t samplersToQuery);
		void QueryImmediateDatas(ParsingContext& parsingContext, uint64_t immediateDatasToQuery);

		////////////////////////////////////////////////////////////////////////////////////
		SequencerUniformsHelper(ParsingContext& parsingContext, const SequencerContext& sequencerTechnique);
	};

	std::shared_ptr<IDescriptorSet> CreateSequencerDescriptorSet(
		IDevice& device,
		ParsingContext& parsingContext,
		SequencerUniformsHelper& uniformHelper,
		const RenderCore::Assets::PredefinedDescriptorSetLayout& descSetLayout);

}}
