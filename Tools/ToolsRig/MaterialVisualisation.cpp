// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialVisualisation.h"
#include "VisualisationUtils.h"		// for IVisContent
#include "VisualisationGeo.h"

#include "../ShaderParser/ShaderPatcher.h"

#include "../../SceneEngine/SceneParser.h"

#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/DrawableDelegates.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/TechniqueDelegates.h"
#include "../../RenderCore/Techniques/CompiledShaderPatchCollection.h"
#include "../../RenderCore/Techniques/CommonUtils.h"
#include "../../RenderCore/Techniques/PipelineAccelerator.h"
#include "../../RenderCore/Techniques/DescriptorSetAccelerator.h"
#include "../../RenderCore/Metal/Shader.h" // (for CreateLowLevelShaderCompiler)
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderCore/Assets/ShaderPatchCollection.h"
#include "../../RenderCore/Assets/MaterialScaffold.h"
#include "../../RenderCore/Assets/PredefinedDescriptorSetLayout.h"
#include "../../RenderCore/MinimalShaderSource.h"

#include "../../RenderCore/UniformsStream.h"
#include "../../RenderCore/BufferView.h"
#include "../../RenderCore/ShaderService.h"

#include "../../Math/Transformations.h"
#include "../../Assets/Assets.h"
#include "../../ConsoleRig/ResourceBox.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Threading/Mutex.h"
#include "../../Utility/Threading/ThreadingUtils.h"
#include "../../Utility/Threading/CompletionThreadPool.h"

namespace RenderCore { namespace Techniques
{
	const RenderCore::Assets::PredefinedDescriptorSetLayout& GetFallbackMaterialDescriptorSetLayout();
}}

namespace ToolsRig
{
    using namespace RenderCore;

///////////////////////////////////////////////////////////////////////////////////////////////////

    class CachedVisGeo
    {
    public:
        class Desc {};

        IResourcePtr _cubeBuffer;
        IResourcePtr _sphereBuffer;
        unsigned _cubeVCount;
        unsigned _sphereVCount;

        CachedVisGeo(const Desc&);
    };

    CachedVisGeo::CachedVisGeo(const Desc&)
    {
        auto sphereGeometry = BuildGeodesicSphere();
        _sphereBuffer = RenderCore::Techniques::CreateStaticVertexBuffer(MakeIteratorRange(sphereGeometry));
        _sphereVCount = unsigned(sphereGeometry.size());
        auto cubeGeometry = BuildCube();
        _cubeBuffer = RenderCore::Techniques::CreateStaticVertexBuffer(MakeIteratorRange(cubeGeometry));
        _cubeVCount = unsigned(cubeGeometry.size());
    }

	class MaterialSceneParserDrawable : public Techniques::Drawable
	{
	public:
		unsigned	_vertexCount;

		static void DrawFn(
			Techniques::ParsingContext& parserContext,
			const DrawFunctionContext& drawFnContext,
			const MaterialSceneParserDrawable& drawable)
		{
			if (drawFnContext.UniformBindingBitField() != 0) {
				ConstantBufferView cbvs[] = {
					Techniques::MakeLocalTransformPacket(
						Identity<Float4x4>(), 
						ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld))};
				drawFnContext.ApplyUniforms(UniformsStream{MakeIteratorRange(cbvs)});
			}

			assert(!drawable._geo->_ib);
			drawFnContext.Draw(drawable._vertexCount);
		}
	};

    class MaterialVisualizationScene : public SceneEngine::IScene, public IVisContent, public ::Assets::IAsyncMarker
    {
    public:
        void Draw(  IThreadContext& threadContext, 
                    SceneEngine::SceneExecuteContext& executeContext,
                    IteratorRange<Techniques::DrawablesPacket** const> pkts) const
        {
			auto usi = std::make_shared<UniformsStreamInterface>();
			usi->BindConstantBuffer(0, {Techniques::ObjectCB::LocalTransform});

			auto& pkt = *pkts[unsigned(RenderCore::Techniques::BatchFilter::General)];

            auto geoType = _settings._geometryType;
            if (geoType == MaterialVisSettings::GeometryType::Plane2D) {

                const Internal::Vertex3D    vertices[] = 
                {
                    { Float3(-1.f, -1.f, 0.f),  Float3(0.f, 0.f, 1.f), Float2(0.f, 1.f), Float4(1.f, 0.f, 0.f, 1.f) },
                    { Float3( 1.f, -1.f, 0.f),  Float3(0.f, 0.f, 1.f), Float2(1.f, 1.f), Float4(1.f, 0.f, 0.f, 1.f) },
                    { Float3(-1.f,  1.f, 0.f),  Float3(0.f, 0.f, 1.f), Float2(0.f, 0.f), Float4(1.f, 0.f, 0.f, 1.f) },
					{ Float3(-1.f,  1.f, 0.f),  Float3(0.f, 0.f, 1.f), Float2(0.f, 0.f), Float4(1.f, 0.f, 0.f, 1.f) },
					{ Float3( 1.f, -1.f, 0.f),  Float3(0.f, 0.f, 1.f), Float2(1.f, 1.f), Float4(1.f, 0.f, 0.f, 1.f) },
                    { Float3( 1.f,  1.f, 0.f),  Float3(0.f, 0.f, 1.f), Float2(1.f, 0.f), Float4(1.f, 0.f, 0.f, 1.f) }
                };

				auto space = pkt.AllocateStorage(Techniques::DrawablesPacket::Storage::VB, sizeof(vertices));
				std::memcpy(space._data.begin(), vertices, sizeof(vertices));

				auto& drawable = *pkt._drawables.Allocate<MaterialSceneParserDrawable>();
				drawable._descriptorSet = _pipeline->_descriptorSet ? _pipeline->_descriptorSet->TryActualize() : nullptr;
				drawable._pipeline = _pipeline->_pipelineAccelerator;
				drawable._geo = std::make_shared<Techniques::DrawableGeo>();
				drawable._geo->_vertexStreams[0]._vbOffset = space._startOffset;
				drawable._geo->_vertexStreamCount = 1;
				drawable._drawFn = (Techniques::Drawable::ExecuteDrawFn*)&MaterialSceneParserDrawable::DrawFn;
				drawable._vertexCount = (unsigned)dimof(vertices);
				drawable._uniformsInterface = usi;

            } else {

                unsigned count = 0;
                const auto& cachedGeo = ConsoleRig::FindCachedBox2<CachedVisGeo>();
				RenderCore::IResourcePtr vb;
                if (geoType == MaterialVisSettings::GeometryType::Sphere) {
					vb = cachedGeo._sphereBuffer;
                    count = cachedGeo._sphereVCount;
                } else if (geoType == MaterialVisSettings::GeometryType::Cube) {
					vb = cachedGeo._cubeBuffer;
                    count = cachedGeo._cubeVCount;
                } else return;

				auto& drawable = *pkt._drawables.Allocate<MaterialSceneParserDrawable>();
				drawable._descriptorSet = _pipeline->_descriptorSet ? _pipeline->_descriptorSet->TryActualize() : nullptr;
				drawable._pipeline = _pipeline->_pipelineAccelerator;
				drawable._geo = std::make_shared<Techniques::DrawableGeo>();
				drawable._geo->_vertexStreams[0]._resource = vb;
				drawable._geo->_vertexStreamCount = 1;
				drawable._drawFn = (Techniques::Drawable::ExecuteDrawFn*)&MaterialSceneParserDrawable::DrawFn;
				drawable._vertexCount = count;
				drawable._uniformsInterface = usi;

            }
        }

		virtual void ExecuteScene(
            RenderCore::IThreadContext& threadContext,
			SceneEngine::SceneExecuteContext& executeContext) const
		{
			for (unsigned v=0; v<executeContext.GetViews().size(); ++v) {
				RenderCore::Techniques::DrawablesPacket* pkts[unsigned(RenderCore::Techniques::BatchFilter::Max)];
				for (unsigned c=0; c<unsigned(RenderCore::Techniques::BatchFilter::Max); ++c)
					pkts[c] = executeContext.GetDrawablesPacket(v, RenderCore::Techniques::BatchFilter(c));

				Draw(threadContext, executeContext, MakeIteratorRange(pkts));
			}
		}

		std::pair<Float3, Float3> GetBoundingBox() const 
		{ 
			return { Float3{-1.0f, 1.0f, 1.0f}, Float3{1.0f, 1.0f, 1.0f} };
		}

		DrawCallDetails GetDrawCallDetails(unsigned drawCallIndex, uint64_t materialGuid) const
		{
			return { {}, {} };
		}
		std::shared_ptr<RenderCore::Techniques::SimpleModelRenderer::IPreDrawDelegate> SetPreDrawDelegate(const std::shared_ptr<RenderCore::Techniques::SimpleModelRenderer::IPreDrawDelegate>& delegate) { return nullptr; }
		void RenderSkeleton(
			RenderCore::IThreadContext& context, 
			RenderCore::Techniques::ParsingContext& parserContext, 
			bool drawBoneNames) const {}
		void BindAnimationState(const std::shared_ptr<VisAnimationState>& animState) {}
		bool HasActiveAnimation() const { return false; }

		MaterialVisualizationScene(
			const MaterialVisSettings& settings,
			const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAcceleratorPool,
			PatchCollectionFuture&& patchCollectionFuture,
			const std::shared_ptr<RenderCore::Assets::MaterialScaffoldMaterial>& material)
        : _settings(settings)
		{
			_depVal = std::make_shared<::Assets::DependencyValidation>();

			auto mat = material;
			if (!mat)
				mat = std::make_shared<RenderCore::Assets::MaterialScaffoldMaterial>();

			std::weak_ptr<RenderCore::Techniques::IPipelineAcceleratorPool> weakPipelineAcceleratorPool = pipelineAcceleratorPool;

			_pipelineFuture = patchCollectionFuture.then(
				[weakPipelineAcceleratorPool, mat](const std::shared_ptr<RenderCore::Techniques::CompiledShaderPatchCollection>& patchCollection) {

					auto strongPipelineAcceleratorPool = weakPipelineAcceleratorPool.lock();
					if (!strongPipelineAcceleratorPool)
						throw std::runtime_error("Aborting because pipeline accelerator pool has been destroyed");

					auto pendingPipeline = std::make_shared<PendingPipeline>();
					pendingPipeline->_depVal = std::make_shared<::Assets::DependencyValidation>();

					std::tie(pendingPipeline->_pipelineAccelerator, pendingPipeline->_descriptorSet) = RenderCore::Techniques::CreatePipelineAccelerator(
						*strongPipelineAcceleratorPool,
						patchCollection,
						*mat,
						Vertex3D_InputLayout,
						Topology::TriangleList);

					if (pendingPipeline->_descriptorSet)
						::Assets::RegisterAssetDependency(pendingPipeline->_depVal, pendingPipeline->_descriptorSet->GetDependencyValidation());

					return pendingPipeline;
				});
		}

		::Assets::AssetState GetAssetState() const
		{
			if (_pipeline)
				return ::Assets::AssetState::Ready;

			if (_pipelineFuture.is_ready()) {
				return StallWhilePending(std::chrono::milliseconds{0}).value();
			}

			if (_pipelineFuture.valid())
				return ::Assets::AssetState::Pending;

			return ::Assets::AssetState::Invalid;
		}

		std::optional<::Assets::AssetState>   StallWhilePending(std::chrono::milliseconds timeout) const
		{
			if (!_pipeline) {
				try {
					_pipeline = _pipelineFuture.get();
				} catch (const std::exception& e) {
					_actualizationLog = e.what();
				}
			}
			
			if (_pipelineFuture.valid())
				return ::Assets::AssetState::Pending;

			return ::Assets::AssetState::Invalid;
		}

		const ::Assets::DepValPtr& GetDependencyValidation() const 
		{
			if (_pipeline)
				_pipeline->_depVal;
			return _depVal; 
		}

    protected:
        MaterialVisSettings		_settings;

		struct PendingPipeline
		{
			::Assets::DepValPtr		_depVal;
			std::shared_ptr<RenderCore::Assets::MaterialScaffoldMaterial> _material;
			std::shared_ptr<RenderCore::Techniques::PipelineAccelerator> _pipelineAccelerator;
			::Assets::FuturePtr<RenderCore::Techniques::DescriptorSetAccelerator> _descriptorSet;
		};
		mutable Threading::ContinuationFuture<std::shared_ptr<PendingPipeline>> _pipelineFuture;
		mutable std::shared_ptr<PendingPipeline> _pipeline;

		::Assets::DepValPtr		_depVal;
		mutable std::string				_actualizationLog;
    };

	std::shared_ptr<SceneEngine::IScene> MakeScene(
		const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAcceleratorPool,
		const MaterialVisSettings& visObject,
		const PatchCollectionFuture& patchCollectionOverride,
		const std::shared_ptr<RenderCore::Assets::MaterialScaffoldMaterial>& material)
	{
		return std::make_shared<MaterialVisualizationScene>(visObject, pipelineAcceleratorPool, PatchCollectionFuture{patchCollectionOverride}, material);
	}

	::Assets::FuturePtr<SceneEngine::IScene> ConvertToFuture(const std::shared_ptr<SceneEngine::IScene>& scene)
	{
		// HACK -- we have to use MaterialVisualizationScene as in intermediate type, 
		// because AssetFuture requires GetDependencyValidation()
		auto result = std::make_shared<::Assets::AssetFuture<MaterialVisualizationScene>>("SceneFuture");
		if (dynamic_cast<::Assets::IAsyncMarker*>(scene.get())) {
			result->SetPollingFunction(
				[scene](::Assets::AssetFuture<MaterialVisualizationScene>& thatFuture) {
					auto marker = std::dynamic_pointer_cast<MaterialVisualizationScene>(scene);
					auto state = marker->GetAssetState();
					if (state == ::Assets::AssetState::Pending)
						return true;

					if (state == ::Assets::AssetState::Ready) {
						thatFuture.SetAsset(std::shared_ptr<MaterialVisualizationScene>{marker}, nullptr);
					} else {
						thatFuture.SetInvalidAsset(nullptr, nullptr);
					}
					return false;
				});
		} else {
			result->SetAsset(std::dynamic_pointer_cast<MaterialVisualizationScene>(scene), nullptr);
		}
		return std::reinterpret_pointer_cast<::Assets::AssetFuture<SceneEngine::IScene>>(result);
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	std::unique_ptr<RenderCore::Techniques::CompiledShaderPatchCollection> MakeCompiledShaderPatchCollection(
		const std::shared_ptr<GraphLanguage::INodeGraphProvider>& provider,
		const std::shared_ptr<MessageRelay>& logMessages)
	{
		ShaderSourceParser::InstantiationRequest instRequest;
		instRequest._customProvider = provider;
		ShaderSourceParser::GenerateFunctionOptions generateOptions;
		return std::make_unique<RenderCore::Techniques::CompiledShaderPatchCollection>(
			ShaderSourceParser::InstantiateShader(
				MakeIteratorRange(&instRequest, &instRequest+1),
				generateOptions,
				RenderCore::Techniques::GetDefaultShaderLanguage()));
	}

	class PatchAnalysisHelper
	{
	public:
		::Assets::FuturePtr<CompiledShaderByteCode> MakeByteCodeFuture(
			ShaderStage stage, StringSection<> patchCollectionCode, StringSection<> techniqueCode, StringSection<> entryPoint, StringSection<> definesTable)
		{
			char profileStr[] = "?s_*";
			switch (stage) {
			case ShaderStage::Vertex: profileStr[0] = 'v'; break;
			case ShaderStage::Geometry: profileStr[0] = 'g'; break;
			case ShaderStage::Pixel: profileStr[0] = 'p'; break;
			case ShaderStage::Domain: profileStr[0] = 'd'; break;
			case ShaderStage::Hull: profileStr[0] = 'h'; break;
			case ShaderStage::Compute: profileStr[0] = 'c'; break;
			}

			std::stringstream str;
			str << patchCollectionCode << std::endl << techniqueCode << std::endl;

			auto artifactFuture = _shaderSource->CompileFromMemory(
				str.str(), entryPoint,
				profileStr, definesTable);

			auto result = std::make_shared<::Assets::AssetFuture<CompiledShaderByteCode>>("");
			::Assets::AutoConstructToFuture(*result, artifactFuture);
			return result;
		}

		::Assets::FuturePtr<Metal::ShaderProgram> MakeShaderVariation(StringSection<> defines)
		{
			using namespace RenderCore::Techniques;
			auto patchCollection = RenderCore::Techniques::AssembleShader(
				*_patchCollection, _patchExpansions, 
				defines);

			auto vsCode = MakeByteCodeFuture(ShaderStage::Vertex, patchCollection._processedSource, _vsTechniqueCode, "vs_main", defines);
			auto psCode = MakeByteCodeFuture(ShaderStage::Pixel, patchCollection._processedSource, _psTechniqueCode, "ps_main", defines);
			return CreateShaderProgramFromByteCode(vsCode, psCode, "ShaderPatchFactory");
		}

		const RenderCore::Techniques::CompiledShaderPatchCollection* _patchCollection;
		IteratorRange<const uint64_t*> _patchExpansions;
		std::shared_ptr<RenderCore::ShaderService::IShaderSource> _shaderSource;
		std::string _vsTechniqueCode;
		std::string _psTechniqueCode;

		static std::string InstantiatePreviewStructure(
			const RenderCore::Techniques::CompiledShaderPatchCollection& mainInstantiation,
			const ShaderSourceParser::PreviewOptions& previewOptions)
		{
			assert(mainInstantiation.GetInterface().GetPatches().size() == 1);	// only tested with a single entry point
			auto structureForPreview = GenerateStructureForPreview(
				"preview_graph", *mainInstantiation.GetInterface().GetPatches()[0]._signature, previewOptions);
			return "#include \"xleres/System/Prefix.h\"\n" + structureForPreview;
		}
	};

	class ShaderPatchAnalysisDelegate : public RenderCore::Techniques::ITechniqueDelegate
	{
	public:
		::Assets::FuturePtr<RenderCore::Metal::ShaderProgram> ResolveVariation(
			const RenderCore::Techniques::CompiledShaderPatchCollection& shaderPatches,
			IteratorRange<const ParameterBox**> selectors)
		{
			using namespace RenderCore::Techniques;

			ShaderSelectorFiltering previewStructureFiltering;
			previewStructureFiltering._relevanceMap["GEO_PRETRANSFORMED"] = "1";
			auto filteredDefines = MakeFilteredDefinesTable(selectors, previewStructureFiltering, shaderPatches.GetInterface().GetSelectorRelevance());

			auto structureForPreview = PatchAnalysisHelper::InstantiatePreviewStructure(shaderPatches, _previewOptions);
			
			PatchAnalysisHelper factory;
			factory._patchCollection = &shaderPatches;
			factory._vsTechniqueCode = structureForPreview;
			factory._psTechniqueCode = structureForPreview;
			factory._shaderSource = _shaderSource;
			auto shader = factory.MakeShaderVariation(filteredDefines);
			shader->StallWhilePending();	// hack -- we have to stall here, or every shader we generate will be permanently pending
			return shader;
		}

		ResolvedTechnique Resolve(
			const std::shared_ptr<RenderCore::Techniques::CompiledShaderPatchCollection>& shaderPatches,
			IteratorRange<const ParameterBox**> selectors,
			const RenderCore::Assets::RenderStateSet& stateSet) override
		{
			using namespace RenderCore::Techniques;

			ResolvedTechnique result;
			result._shaderProgram = ResolveVariation(*shaderPatches, selectors);
			result._rasterization = BuildDefaultRastizerDesc(stateSet);

			#pragma warning(disable:4127) // conditional expression is constant
				// disable blending to avoid problem when rendering single component stuff 
                //  (ie, nodes that output "float", not "float4")
			const bool forceOpaqueBlend = true;
			if (!forceOpaqueBlend && (stateSet._flag & RenderCore::Assets::RenderStateSet::Flag::ForwardBlend)) {
                result._blend = AttachmentBlendDesc {
					stateSet._forwardBlendOp != BlendOp::NoBlending,
					stateSet._forwardBlendSrc, stateSet._forwardBlendDst, stateSet._forwardBlendOp };
            } else {
                result._blend = Techniques::CommonResources()._abOpaque;
            }
			return result;
		}

		ShaderPatchAnalysisDelegate(const ShaderSourceParser::PreviewOptions& previewOptions)
		: _previewOptions(previewOptions)
		{
			_shaderSource = std::make_shared<RenderCore::MinimalShaderSource>(
				RenderCore::Metal::CreateLowLevelShaderCompiler(RenderCore::Assets::Services::GetDevice()),
				RenderCore::MinimalShaderSource::Flags::CompileInBackground);
		}
	private:
		std::shared_ptr<RenderCore::ShaderService::IShaderSource> _shaderSource;
		ShaderSourceParser::PreviewOptions _previewOptions;
	};

	std::unique_ptr<RenderCore::Techniques::ITechniqueDelegate> MakeShaderPatchAnalysisDelegate(
		const ShaderSourceParser::PreviewOptions& previewOptions)
	{
		return std::make_unique<ShaderPatchAnalysisDelegate>(previewOptions);
	}

	std::unique_ptr<RenderCore::Techniques::CompiledShaderPatchCollection> MakeCompiledShaderPatchCollection(
		const GraphLanguage::NodeGraph& nodeGraph,
		const GraphLanguage::NodeGraphSignature& nodeGraphSignature,
		uint32_t previewNodeId,
		const std::shared_ptr<GraphLanguage::INodeGraphProvider>& subProvider)
	{
		ShaderSourceParser::InstantiationRequest instantiationReq {};
		ShaderSourceParser::GenerateFunctionOptions generateOptions;
		if (previewNodeId != ~0u)
			generateOptions._generateDanglingOutputs = previewNodeId;
		generateOptions._generateDanglingInputs = true;

		auto mainInstantiation = ShaderSourceParser::InstantiateShader(
			GraphLanguage::INodeGraphProvider::NodeGraph { "preview_graph", nodeGraph, nodeGraphSignature, subProvider },
			false,
			instantiationReq,
			generateOptions,
			RenderCore::Techniques::GetDefaultShaderLanguage());

		return std::make_unique<RenderCore::Techniques::CompiledShaderPatchCollection>(mainInstantiation);
	}

	template<typename Function, typename... Args>
		Utility::Threading::ContinuationFuture<std::invoke_result_t<std::decay_t<Function>, std::decay_t<Args>...>>
			ContinuationAsync(Function&& function, Args&&... args)
	{
		using PromiseResult = std::invoke_result_t<std::decay_t<Function>, std::decay_t<Args>...>;
		
		// We have to wrap up the uncopyable objects in a packet, because
		// there's a forced copy of the functor when we convert to a std::function<void>()
		struct Packet
		{
			Function _function;
			Utility::Threading::ContinuationPromise<PromiseResult> _promise;
		};
		auto pkt = std::make_shared<Packet>(Packet{std::move(function)});
		auto result = pkt->_promise.get_future();
		ConsoleRig::GlobalServices::GetInstance().GetLongTaskThreadPool().Enqueue(
			[pkt](Args&&... args) mutable -> void {
				try {
					pkt->_promise.set_value(pkt->_function(std::forward<Args>(args)...));
				} catch (...) {
					try {
						pkt->_promise.set_exception(std::current_exception());
					} catch(...) {}
				}
			},
			std::forward<Args>(args)...);

		return result;
	}

	PatchCollectionFuture MakeCompiledShaderPatchCollectionAsync(
		GraphLanguage::NodeGraph&& nodeGraph,
		GraphLanguage::NodeGraphSignature&& nodeGraphSignature,
		uint32_t previewNodeId,
		const std::shared_ptr<GraphLanguage::INodeGraphProvider>& subProvider)
	{
		return ContinuationAsync(
			[nodeGraph{std::move(nodeGraph)}, nodeGraphSignature{std::move(nodeGraphSignature)}, previewNodeId, subProvider]() {
				std::shared_ptr<RenderCore::Techniques::CompiledShaderPatchCollection> shdrPtr = MakeCompiledShaderPatchCollection(nodeGraph, nodeGraphSignature, previewNodeId, subProvider);
				return shdrPtr;
			}).share();
	}

	const PatchCollectionFuture& DeferredCompiledShaderPatchCollection::GetFuture()
	{
		return *_future;
	}

	DeferredCompiledShaderPatchCollection::DeferredCompiledShaderPatchCollection(
		GraphLanguage::NodeGraph&& nodeGraph,
		GraphLanguage::NodeGraphSignature&& nodeGraphSignature,
		uint32_t previewNodeId,
		const std::shared_ptr<GraphLanguage::INodeGraphProvider>& subProvider)
	{
		_future = std::make_unique<PatchCollectionFuture>(
			MakeCompiledShaderPatchCollectionAsync(
				std::move(nodeGraph),
				std::move(nodeGraphSignature),
				previewNodeId,
				subProvider));
	}
	DeferredCompiledShaderPatchCollection::~DeferredCompiledShaderPatchCollection() {}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class MessageRelay::Pimpl
	{
	public:
		std::vector<std::string> _messages;
		Threading::RecursiveMutex _lock;

		std::vector<std::pair<unsigned, std::shared_ptr<Utility::OnChangeCallback>>> _callbacks;
		unsigned _nextCallbackId = 1;
	};

	std::string MessageRelay::GetMessages() const
	{
		ScopedLock(_pimpl->_lock);
		size_t length = 0;
		for (const auto&m:_pimpl->_messages)
			length += m.size();
		std::string result;
		result.reserve(length);
		for (const auto&m:_pimpl->_messages)
			result.insert(result.end(), m.begin(), m.end());
		return result;
	}

	unsigned MessageRelay::AddCallback(const std::shared_ptr<Utility::OnChangeCallback>& callback)
	{
		ScopedLock(_pimpl->_lock);
		_pimpl->_callbacks.push_back(std::make_pair(_pimpl->_nextCallbackId, callback));
		return _pimpl->_nextCallbackId++;
	}

	void MessageRelay::RemoveCallback(unsigned id)
	{
		ScopedLock(_pimpl->_lock);
		auto i = std::find_if(
			_pimpl->_callbacks.begin(), _pimpl->_callbacks.end(),
			[id](const std::pair<unsigned, std::shared_ptr<Utility::OnChangeCallback>>& p) { return p.first == id; } );
		_pimpl->_callbacks.erase(i);
	}

	void MessageRelay::AddMessage(const std::string& msg)
	{
		ScopedLock(_pimpl->_lock);
		_pimpl->_messages.push_back(msg);
		for (const auto&cb:_pimpl->_callbacks)
			cb.second->OnChange();
	}

	MessageRelay::MessageRelay()
	{
		_pimpl = std::make_unique<Pimpl>();
	}

	MessageRelay::~MessageRelay()
	{}

}

