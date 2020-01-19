// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialVisualisation.h"
#include "VisualisationUtils.h"		// for IVisContent
#include "VisualisationGeo.h"

#include "../ShaderParser/ShaderPatcher.h"
#include "../ShaderParser/ShaderInstantiation.h"

#include "../../SceneEngine/SceneParser.h"

#include "../../RenderCore/Techniques/CommonBindings.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/DrawableDelegates.h"
#include "../../RenderCore/Techniques/ShaderVariationSet.h"
#include "../../RenderCore/Techniques/TechniqueDelegates.h"
#include "../../RenderCore/Techniques/CompiledShaderPatchCollection.h"
#include "../../RenderCore/Techniques/PipelineAccelerator.h"
#include "../../RenderCore/Techniques/DescriptorSetAccelerator.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Metal/ObjectFactory.h"
#include "../../RenderCore/Assets/AssetUtils.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderCore/Assets/ShaderPatchCollection.h"
#include "../../RenderCore/Assets/MaterialScaffold.h"
#include "../../RenderCore/Assets/PredefinedDescriptorSetLayout.h"
#include "../../RenderCore/MinimalShaderSource.h"

#include "../../RenderCore/UniformsStream.h"
#include "../../RenderCore/BufferView.h"
#include "../../RenderCore/IThreadContext.h"
#include "../../RenderCore/ShaderService.h"

#include "../../Math/Transformations.h"
#include "../../Assets/Assets.h"
#include "../../ConsoleRig/ResourceBox.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Threading/Mutex.h"

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
        _sphereBuffer = RenderCore::Assets::CreateStaticVertexBuffer(MakeIteratorRange(sphereGeometry));
        _sphereVCount = unsigned(sphereGeometry.size());
        auto cubeGeometry = BuildCube();
        _cubeBuffer = RenderCore::Assets::CreateStaticVertexBuffer(MakeIteratorRange(cubeGeometry));
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
			if (drawFnContext._boundUniforms->_boundUniformBufferSlots[3] != 0) {
				ConstantBufferView cbvs[] = {
					Techniques::MakeLocalTransformPacket(
						Identity<Float4x4>(), 
						ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld))};
				drawFnContext.ApplyUniforms(UniformsStream{MakeIteratorRange(cbvs)});
			}

				// disable blending to avoid problem when rendering single component stuff 
                //  (ie, nodes that output "float", not "float4")
			// assert(!drawFnContext._pipeline);	// note -- won't work with pipelines
            // drawFnContext._metalContext->Bind(Techniques::CommonResources()._blendOpaque);

			assert(!drawable._geo->_ib);
			drawFnContext.Draw(drawable._vertexCount);
		}
	};

    class MaterialVisualizationScene : public SceneEngine::IScene, public IVisContent
    {
    public:
        void Draw(  IThreadContext& threadContext, 
                    SceneEngine::SceneExecuteContext& executeContext,
                    IteratorRange<Techniques::DrawablesPacket** const> pkts) const
        {
			auto usi = std::make_shared<UniformsStreamInterface>();
			usi->BindConstantBuffer(0, {Techniques::ObjectCB::LocalTransform});

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

				auto& drawable = *pkts[unsigned(RenderCore::Techniques::BatchFilter::General)]->_drawables.Allocate<MaterialSceneParserDrawable>();
				drawable._descriptorSet = _descriptorSet;
				drawable._pipeline = _pipelineAccelerator;
				drawable._geo = std::make_shared<Techniques::DrawableGeo>();
				drawable._geo->_vertexStreams[0]._resource = RenderCore::Assets::CreateStaticVertexBuffer(*threadContext.GetDevice(), MakeIteratorRange(vertices));
				drawable._geo->_vertexStreams[0]._vertexElements = Vertex3D_MiniInputLayout;
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

				auto& drawable = *pkts[unsigned(RenderCore::Techniques::BatchFilter::General)]->_drawables.Allocate<MaterialSceneParserDrawable>();
				drawable._descriptorSet = _descriptorSet;
				drawable._pipeline = _pipelineAccelerator;
				drawable._geo = std::make_shared<Techniques::DrawableGeo>();
				drawable._geo->_vertexStreams[0]._resource = vb;
				drawable._geo->_vertexStreams[0]._vertexElements = Vertex3D_MiniInputLayout;
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
			const std::shared_ptr<RenderCore::Techniques::PipelineAcceleratorPool>& pipelineAcceleratorPool,
			const std::shared_ptr<RenderCore::Techniques::CompiledShaderPatchCollection>& patchCollectionOverride,
			const std::shared_ptr<RenderCore::Assets::MaterialScaffoldMaterial>& material)
        : _settings(settings), _material(material)
		{
			_depVal = std::make_shared<::Assets::DependencyValidation>();
			if (!_material)
				_material = std::make_shared<RenderCore::Assets::MaterialScaffoldMaterial>();

			auto matSelectors = _material->_matParams;

			if (patchCollectionOverride) {
				const auto* descriptorSetLayout = patchCollectionOverride->GetInterface().GetMaterialDescriptorSet().get();
				if (!descriptorSetLayout) {
					descriptorSetLayout = &RenderCore::Techniques::GetFallbackMaterialDescriptorSetLayout();
				}
				auto descriptorSetFuture = RenderCore::Techniques::MakeDescriptorSetAccelerator(
					_material->_constants, _material->_bindings,
					*descriptorSetLayout,
					"MaterialVisualizationScene");
				_descriptorSet = descriptorSetFuture->Actualize();
			
				// Also append the "RES_HAS_" constants for each resource that is both in the descriptor set and that we have a binding for
				for (const auto&r:descriptorSetLayout->_resources)
					if (_material->_bindings.HasParameter(MakeStringSection(r._name)))
						matSelectors.SetParameter(MakeStringSection(std::string{"RES_HAS_"} + r._name).Cast<utf8>(), 1);
			}

			_pipelineAccelerator =
				pipelineAcceleratorPool->CreatePipelineAccelerator(
					patchCollectionOverride,
					matSelectors,
					Vertex3D_InputLayout,
					Topology::TriangleList,
					_material->_stateSet);
		}

		const ::Assets::DepValPtr& GetDependencyValidation() { return _depVal; }

    protected:
        MaterialVisSettings  _settings;
		std::shared_ptr<RenderCore::Assets::MaterialScaffoldMaterial> _material;
		::Assets::DepValPtr _depVal;
		std::shared_ptr<RenderCore::Techniques::PipelineAccelerator> _pipelineAccelerator;
		std::shared_ptr<RenderCore::Techniques::DescriptorSetAccelerator> _descriptorSet;
    };

	::Assets::FuturePtr<SceneEngine::IScene> MakeScene(
		const std::shared_ptr<RenderCore::Techniques::PipelineAcceleratorPool>& pipelineAcceleratorPool,
		const MaterialVisSettings& visObject,
		const std::shared_ptr<RenderCore::Techniques::CompiledShaderPatchCollection>& patchCollectionOverride,
		const std::shared_ptr<RenderCore::Assets::MaterialScaffoldMaterial>& material)
	{
		auto result = std::make_shared<::Assets::AssetFuture<MaterialVisualizationScene>>("MaterialVisualization");
		::Assets::AutoConstructToFuture(*result, visObject, pipelineAcceleratorPool, patchCollectionOverride, material);
		return std::reinterpret_pointer_cast<::Assets::AssetFuture<SceneEngine::IScene>>(result);
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	static const auto s_perPixel = Hash64("PerPixel");
	static const auto s_earlyRejectionTest = Hash64("EarlyRejectionTest");
	static uint64_t s_patchExp_perPixelAndEarlyRejection[] = { s_perPixel, s_earlyRejectionTest };
	static uint64_t s_patchExp_perPixel[] = { s_perPixel };
	static uint64_t s_patchExp_earlyRejection[] = { s_earlyRejectionTest };

	static void TryRegisterDependency(
		::Assets::DepValPtr& dst,
		const std::shared_ptr<::Assets::AssetFuture<CompiledShaderByteCode>>& future)
	{
		auto futureDepVal = future->GetDependencyValidation();
		if (futureDepVal)
			::Assets::RegisterAssetDependency(dst, futureDepVal);
	}

	class ShaderVariationFactory : public RenderCore::Techniques::IShaderVariationFactory
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

		::Assets::FuturePtr<Metal::ShaderProgram> MakeShaderVariation(StringSection<> defines) override
		{
			using namespace RenderCore::Techniques;
			auto patchCollection = RenderCore::Techniques::AssembleShader(
				*_patchCollection, _patchExpansions, 
				defines);

			auto vsCode = MakeByteCodeFuture(ShaderStage::Vertex, patchCollection._processedSource, _vsTechniqueCode, "vs_main", defines);
			auto psCode = MakeByteCodeFuture(ShaderStage::Pixel, patchCollection._processedSource, _psTechniqueCode, "ps_main", defines);

			auto future = std::make_shared<::Assets::AssetFuture<Metal::ShaderProgram>>("ShaderPatchFactory");
			future->SetPollingFunction(
				[vsCode, psCode](::Assets::AssetFuture<RenderCore::Metal::ShaderProgram>& thatFuture) -> bool {

					auto vsActual = vsCode->TryActualize();
					auto psActual = psCode->TryActualize();

					if (!vsActual || !psActual) {
						auto vsState = vsCode->GetAssetState();
						auto psState = psCode->GetAssetState();
						if (vsState == ::Assets::AssetState::Invalid || psState == ::Assets::AssetState::Invalid) {
							auto depVal = std::make_shared<::Assets::DependencyValidation>();
							TryRegisterDependency(depVal, vsCode);
							TryRegisterDependency(depVal, psCode);
							std::stringstream log;
							if (vsState == ::Assets::AssetState::Invalid) log << "Vertex shader is invalid with message: " << std::endl << ::Assets::AsString(vsCode->GetActualizationLog()) << std::endl;
							if (psState == ::Assets::AssetState::Invalid) log << "Pixel shader is invalid with message: " << std::endl << ::Assets::AsString(psCode->GetActualizationLog()) << std::endl;
							thatFuture.SetInvalidAsset(depVal, ::Assets::AsBlob(log.str()));
							return false;
						}
						return true;
					}

					auto newShaderProgram = std::make_shared<RenderCore::Metal::ShaderProgram>(
						RenderCore::Metal::GetObjectFactory(), *vsActual, *psActual);
					thatFuture.SetAsset(std::move(newShaderProgram), {});
					return false;
				});
			return future;
		}

		const RenderCore::Techniques::CompiledShaderPatchCollection* _patchCollection;
		IteratorRange<const uint64_t*> _patchExpansions;
		std::shared_ptr<RenderCore::ShaderService::IShaderSource> _shaderSource;
		std::string _vsTechniqueCode;
		std::string _psTechniqueCode;
	};

	class GraphPreviewTechniqueDelegate : public RenderCore::Techniques::ITechniqueDelegate
	{
	public:
#if 0
		virtual RenderCore::Metal::ShaderProgram* GetShader(
			RenderCore::Techniques::ParsingContext& context,
			StringSection<::Assets::ResChar> techniqueCfgFile,
			const ParameterBox* shaderSelectors[],
			unsigned techniqueIndex)
		{
			if (_technique->GetDependencyValidation()->GetValidationIndex() != 0) {
				_technique = ::Assets::ActualizePtr<RenderCore::Techniques::Technique>(s_techFile);
			}

			/*const auto& shaderFuture = _resolvedShaders.FindVariation(_technique->GetEntry(techniqueIndex), shaderSelectors);
			if (!shaderFuture) return nullptr;
			// In this case we want invalid / pending shaders to get registered as such. So use the "throwing" version of
			// the Actualize call
			shaderFuture->OnFrameBarrier();		// hack -- we still need to invoke the OnFrameBarrier call, because the future is not registered with the asset manager
			return shaderFuture->Actualize().get();*/
			assert(0);
			return nullptr;
		}

		static ::Assets::FuturePtr<RenderCore::CompiledShaderByteCode> GeneratePixelPreviewShader(
			StringSection<> psName, StringSection<> definesTable,
			const std::shared_ptr<GraphLanguage::INodeGraphProvider>& provider,
			const std::string& psMainName,
			const std::shared_ptr<MessageRelay>& logMessages,
			RenderCore::ShaderService::IShaderSource& shaderSource)
		{
			auto future = std::make_shared<::Assets::AssetFuture<RenderCore::CompiledShaderByteCode>>(psName.AsString());
			TRY
			{
				ShaderSourceParser::InstantiatedShader fragments;

				// todo -- refactor this slightly so that we're instantiating all of the root
				// functions available in a standard graph file
				auto psNameSplit = MakeFileNameSplitter(psName);
				const char* entryPoint;
				assert(0);
#if 0
				enum class StructureType { DeferredPass, ColorFromWorldCoords };
				StructureType structureType = StructureType::DeferredPass;

				auto sig = provider->FindSignature(psMainName);
				if (sig.has_value()) {
					const auto& sigValue = sig.value();
					if (XlFindString(sigValue._signature.GetImplements(), "CoordinatesToColor"))
						structureType = StructureType::ColorFromWorldCoords;
				}

				if (structureType == StructureType::DeferredPass) {
					auto earlyRejection = ShaderSourceParser::InstantiationRequest { "xleres/Techniques/Graph/Pass_Standard.sh::EarlyRejectionTest_Default", {} };

					auto overridePerPixel = ShaderSourceParser::InstantiationRequest { psMainName, {} };
					overridePerPixel._customProvider = provider;

					ShaderSourceParser::InstantiationRequest instParams {
						{ "rejectionTest", earlyRejection },
						{ "perPixel", overridePerPixel }
					};
					entryPoint = "deferred_pass_main";
					fragments = ShaderSourceParser::InstantiateShader(
						psNameSplit.AllExceptParameters(), entryPoint,
						instParams,
						RenderCore::Techniques::GetDefaultShaderLanguage());
				} else {
					auto overridePerPixel = ShaderSourceParser::InstantiationRequest { psMainName, {} };
					overridePerPixel._customProvider = provider;

					entryPoint = "deferred_pass_color_from_worldcoords";
					fragments = ShaderSourceParser::InstantiateShader(
						psNameSplit.AllExceptParameters(), entryPoint,
						ShaderSourceParser::InstantiationRequest {
							{ "perPixel", overridePerPixel }
						},
						RenderCore::Techniques::GetDefaultShaderLanguage());
				}
#endif

				fragments._sourceFragments.insert(fragments._sourceFragments.begin(), "#include \"xleres/System/Prefix.h\"\n");

				std::string mergedFragments;
				size_t mergedSize = 0;
				for (auto&f:fragments._sourceFragments)
					mergedSize += f.size();
				mergedFragments.reserve(mergedSize);
				for (auto&f:fragments._sourceFragments)
					mergedFragments.insert(mergedFragments.end(), f.begin(), f.end());

				auto pendingCompile = shaderSource.CompileFromMemory(
					mergedFragments, entryPoint, psNameSplit.Parameters(), definesTable);

				future->SetPollingFunction(
					[pendingCompile, logMessages, mergedFragments](::Assets::AssetFuture<RenderCore::CompiledShaderByteCode>& thatFuture) -> bool {
						auto state = pendingCompile->GetAssetState();
						if (state == ::Assets::AssetState::Pending) return true;

						if (state == ::Assets::AssetState::Invalid) {
							auto artifacts = pendingCompile->GetArtifacts();
							if (!artifacts.empty()) {
								auto blog = ::Assets::GetErrorMessage(*pendingCompile);
								thatFuture.SetInvalidAsset(artifacts[0].second->GetDependencyValidation(), blog);
								if (logMessages)
									logMessages->AddMessage(std::string("Got error during shader compile:\n") + ::Assets::AsString(blog) + "\n");
							} else {
								thatFuture.SetInvalidAsset(nullptr, nullptr);
							}
							return false;
						}

						assert(state == ::Assets::AssetState::Ready);
						auto& artifact = *pendingCompile->GetArtifacts()[0].second;
						AutoConstructToFutureDirect(thatFuture, artifact.GetBlob(), artifact.GetDependencyValidation(), artifact.GetRequestParameters());
						if (logMessages) {
							std::stringstream str;
							str << "Completed shader:\n" << std::endl;
							str << mergedFragments << std::endl;
							logMessages->AddMessage(str.str());
						}
						return false;
					});
			} CATCH (const ::Assets::Exceptions::ConstructionError& e) {
				future->SetInvalidAsset(
					e.GetDependencyValidation(),
					e.GetActualizationLog());
				if (logMessages)
					logMessages->AddMessage(std::string("Got exception during shader construction:\n") + ::Assets::AsString(e.GetActualizationLog()) + "\n");
			} CATCH (const std::exception& e) {
				future->SetInvalidAsset(
					std::make_shared<::Assets::DependencyValidation>(),
					::Assets::AsBlob(e.what()));
				if (logMessages)
					logMessages->AddMessage(std::string("Got exception during shader construction:\n") + e.what() + "\n");
			} CATCH_END
			return future;
		}

#endif

		::Assets::FuturePtr<RenderCore::Metal::ShaderProgram> ResolveVariation(
			IteratorRange<const ParameterBox**> selectors,
			const RenderCore::Techniques::TechniqueEntry& techEntry,
			IteratorRange<const uint64_t*> patchExpansions)
		{
			using namespace RenderCore::Techniques;

			auto filteredDefines = MakeFilteredDefinesTable(selectors, techEntry._selectorFiltering, _customPatchCollection->GetInterface().GetSelectorRelevance());
			
			ShaderVariationFactory factory;
			factory._patchCollection = _customPatchCollection.get();
			factory._patchExpansions = patchExpansions;
			factory._vsTechniqueCode = std::string{"#include \""} + techEntry._vertexShaderName + "\"";
			factory._psTechniqueCode = std::string{"#include \""} + techEntry._pixelShaderName + "\"";
			factory._shaderSource = _shaderSource;
			return factory.MakeShaderVariation(filteredDefines);
		}

		ResolvedTechnique Resolve(
			const std::shared_ptr<RenderCore::Techniques::CompiledShaderPatchCollection>&,
			IteratorRange<const ParameterBox**> selectors,
			const RenderCore::Assets::RenderStateSet& stateSet) override
		{
			using namespace RenderCore::Techniques;
			IteratorRange<const uint64_t*> patchExpansions = {};
			const TechniqueEntry* techEntry = &_noPatches;
			switch (CalculateIllumType(*_customPatchCollection)) {
			case IllumType::PerPixel:
				techEntry = &_perPixel;
				patchExpansions = MakeIteratorRange(s_patchExp_perPixel);
				break;
			case IllumType::PerPixelAndEarlyRejection:
				techEntry = &_perPixelAndEarlyRejection;
				patchExpansions = MakeIteratorRange(s_patchExp_perPixelAndEarlyRejection);
				break;
			default:
				break;
			}

			ResolvedTechnique result;
			result._shaderProgram = ResolveVariation(selectors, *techEntry, patchExpansions);
			result._rasterization = BuildDefaultRastizerDesc(stateSet);

			if (stateSet._flag & RenderCore::Assets::RenderStateSet::Flag::ForwardBlend) {
                result._blend = AttachmentBlendDesc {
					stateSet._forwardBlendOp != BlendOp::NoBlending,
					stateSet._forwardBlendSrc, stateSet._forwardBlendDst, stateSet._forwardBlendOp };
            } else {
                result._blend = Techniques::CommonResources()._abOpaque;
            }
			return result;
		}

		GraphPreviewTechniqueDelegate(
			const std::shared_ptr<RenderCore::Techniques::CompiledShaderPatchCollection>& patchCollection)
		{
			// const char GraphPreviewTechniqueDelegate::s_techFile[] = "xleres/Techniques/Graph/graph.tech";
			_techniqueSet = ::Assets::AutoConstructAsset<RenderCore::Techniques::TechniqueSetFile>("xleres/Techniques/New/Illum.tech");

			const auto noPatchesHash = Hash64("Deferred_NoPatches");
			const auto perPixelHash = Hash64("Deferred_PerPixel");
			const auto perPixelAndEarlyRejectionHash = Hash64("Deferred_PerPixelAndEarlyRejection");
			auto* noPatchesSrc = _techniqueSet->FindEntry(noPatchesHash);
			auto* perPixelSrc = _techniqueSet->FindEntry(perPixelHash);
			auto* perPixelAndEarlyRejectionSrc = _techniqueSet->FindEntry(perPixelAndEarlyRejectionHash);
			if (!noPatchesSrc || !perPixelSrc || !perPixelAndEarlyRejectionSrc) {
				Throw(std::runtime_error("Could not construct technique delegate because required configurations were not found"));
			}

			_noPatches = *noPatchesSrc;
			_perPixel = *perPixelSrc;
			_perPixelAndEarlyRejection = *perPixelAndEarlyRejectionSrc;

			_shaderSource = std::make_shared<RenderCore::MinimalShaderSource>(
				RenderCore::Metal::CreateLowLevelShaderCompiler(RenderCore::Assets::Services::GetDevice()),
				RenderCore::MinimalShaderSource::Flags::CompileInBackground);

			_customPatchCollection = patchCollection;
		}
	private:
		std::shared_ptr<RenderCore::Techniques::TechniqueSharedResources> _sharedResources;
		std::shared_ptr<RenderCore::Techniques::TechniqueSetFile> _techniqueSet;

		std::shared_ptr<RenderCore::ShaderService::IShaderSource> _shaderSource;

		RenderCore::Techniques::TechniqueEntry _noPatches;
		RenderCore::Techniques::TechniqueEntry _perPixel;
		RenderCore::Techniques::TechniqueEntry _perPixelAndEarlyRejection;

		std::shared_ptr<RenderCore::Techniques::CompiledShaderPatchCollection> _customPatchCollection;
	};

	std::unique_ptr<RenderCore::Techniques::ITechniqueDelegate> MakeNodeGraphPreviewDelegateDefaultLink(
		const std::shared_ptr<RenderCore::Techniques::CompiledShaderPatchCollection>& patchCollection)
	{
		return std::make_unique<GraphPreviewTechniqueDelegate>(patchCollection);
	}

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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	static std::string InstantiatePreviewStructure(
		const RenderCore::Techniques::CompiledShaderPatchCollection& mainInstantiation,
		const ShaderSourceParser::PreviewOptions& previewOptions)
	{
		assert(mainInstantiation.GetInterface().GetPatches().size() == 1);	// only tested with a single entry point
		auto structureForPreview = GenerateStructureForPreview(
			"preview_graph", *mainInstantiation.GetInterface().GetPatches()[0]._signature, previewOptions);
		return "#include \"xleres/System/Prefix.h\"\n" + structureForPreview;
	}

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

			auto structureForPreview = InstantiatePreviewStructure(shaderPatches, _previewOptions);
			
			ShaderVariationFactory factory;
			factory._patchCollection = &shaderPatches;
			// factory._patchExpansions = patchExpansions;
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

			if (stateSet._flag & RenderCore::Assets::RenderStateSet::Flag::ForwardBlend) {
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
		const std::string& subGraphName,
		uint32_t previewNodeId,
		const std::shared_ptr<GraphLanguage::INodeGraphProvider>& subProvider,
		const std::shared_ptr<MessageRelay>& logMessages)
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

