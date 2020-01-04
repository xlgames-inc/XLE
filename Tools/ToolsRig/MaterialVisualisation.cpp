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
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Metal/ObjectFactory.h"
#include "../../RenderCore/Assets/AssetUtils.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderCore/Assets/ShaderPatchCollection.h"
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
		Topology	_topology;
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
			assert(!drawFnContext._pipeline);	// note -- won't work with pipelines
            drawFnContext._metalContext->Bind(Techniques::CommonResources()._blendOpaque);

			assert(!drawable._geo->_ib);
			// drawFnCon.Bind(drawable._topology);
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
                    { Float3( 1.f,  1.f, 0.f),  Float3(0.f, 0.f, 1.f), Float2(1.f, 0.f), Float4(1.f, 0.f, 0.f, 1.f) }
                };

				auto& drawable = *pkts[unsigned(RenderCore::Techniques::BatchFilter::General)]->_drawables.Allocate<MaterialSceneParserDrawable>();
				// drawable._material = RenderCore::Techniques::MakeDrawableMaterial(*_material, {});
				drawable._geo = std::make_shared<Techniques::DrawableGeo>();
				drawable._geo->_vertexStreams[0]._resource = RenderCore::Assets::CreateStaticVertexBuffer(*threadContext.GetDevice(), MakeIteratorRange(vertices));
				drawable._geo->_vertexStreams[0]._vertexElements = Vertex3D_MiniInputLayout;
				drawable._geo->_vertexStreamCount = 1;
				drawable._drawFn = (Techniques::Drawable::ExecuteDrawFn*)&MaterialSceneParserDrawable::DrawFn;
				drawable._topology = Topology::TriangleStrip;
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
				// drawable._material = Techniques::MakeDrawableMaterial(*_material.get(), {});
				drawable._geo = std::make_shared<Techniques::DrawableGeo>();
				drawable._geo->_vertexStreams[0]._resource = vb;
				drawable._geo->_vertexStreams[0]._vertexElements = Vertex3D_MiniInputLayout;
				drawable._geo->_vertexStreamCount = 1;
				drawable._drawFn = (Techniques::Drawable::ExecuteDrawFn*)&MaterialSceneParserDrawable::DrawFn;
				drawable._topology = Topology::TriangleList;
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
		std::shared_ptr<RenderCore::Assets::SimpleModelRenderer::IPreDrawDelegate> SetPreDrawDelegate(const std::shared_ptr<RenderCore::Assets::SimpleModelRenderer::IPreDrawDelegate>& delegate) { return nullptr; }
		void RenderSkeleton(
			RenderCore::IThreadContext& context, 
			RenderCore::Techniques::ParsingContext& parserContext, 
			bool drawBoneNames) const {}
		void BindAnimationState(const std::shared_ptr<VisAnimationState>& animState) {}
		bool HasActiveAnimation() const { return false; }

        MaterialVisualizationScene(
			const MaterialVisSettings& settings,
			const std::shared_ptr<RenderCore::Techniques::ScaffoldMaterial>& material)
        : _settings(settings), _material(material)
		{
			_depVal = std::make_shared<::Assets::DependencyValidation>();
			if (!_material) {
				_material = std::make_shared<RenderCore::Techniques::ScaffoldMaterial>();
				// XlCopyString(_material->_techniqueConfig, "xleres/techniques/illum.tech");
			}
		}

		const ::Assets::DepValPtr& GetDependencyValidation() { return _depVal; }

    protected:
        MaterialVisSettings  _settings;
		std::shared_ptr<RenderCore::Techniques::ScaffoldMaterial> _material;
		::Assets::DepValPtr _depVal;
    };

	::Assets::FuturePtr<SceneEngine::IScene> MakeScene(
		const MaterialVisSettings& visObject,
		const std::shared_ptr<RenderCore::Techniques::ScaffoldMaterial>& material)
	{
		auto result = std::make_shared<::Assets::AssetFuture<MaterialVisualizationScene>>("MaterialVisualization");
		::Assets::AutoConstructToFuture(*result, visObject, material);
		return std::reinterpret_pointer_cast<::Assets::AssetFuture<SceneEngine::IScene>>(result);
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	class GraphPreviewTechniqueDelegate : public RenderCore::Techniques::ITechniqueDelegate
	{
	public:
		virtual RenderCore::Metal::ShaderProgram* GetShader(
			RenderCore::Techniques::ParsingContext& context,
			StringSection<::Assets::ResChar> techniqueCfgFile,
			const ParameterBox* shaderSelectors[],
			unsigned techniqueIndex)
		{
			if (_technique->GetDependencyValidation()->GetValidationIndex() != 0) {
				auto future = ::Assets::MakeAsset<RenderCore::Techniques::Technique>(s_techFile);
				future->StallWhilePending();
				_technique = future->Actualize();
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

		static void TryRegisterDependency(
			::Assets::DepValPtr& dst,
			const std::shared_ptr<::Assets::AssetFuture<CompiledShaderByteCode>>& future)
		{
			auto futureDepVal = future->GetDependencyValidation();
			if (futureDepVal)
				::Assets::RegisterAssetDependency(dst, futureDepVal);
		}

		GraphPreviewTechniqueDelegate(
			const std::shared_ptr<GraphLanguage::INodeGraphProvider>& provider,
			const std::string& psMainName,
			const std::shared_ptr<MessageRelay>& logMessages)
		{
			auto future = ::Assets::MakeAsset<RenderCore::Techniques::Technique>(s_techFile);
			auto state = future->StallWhilePending();
			if (state == ::Assets::AssetState::Ready) {
				_technique = future->Actualize();
			} else {
				std::stringstream str;
				str << "Failed loading technique file (" << s_techFile << ") with message (" << ::Assets::AsString(future->GetActualizationLog()) << ")";
				logMessages->AddMessage(str.str());
			}

			auto shaderSource = std::make_shared<RenderCore::MinimalShaderSource>(
				RenderCore::Metal::CreateLowLevelShaderCompiler(RenderCore::Assets::Services::GetDevice()),
				RenderCore::MinimalShaderSource::Flags::CompileInBackground);

			assert(0);
			/*_resolvedShaders._creationFn = 
				[provider, psMainName, logMessages, shaderSource](
					StringSection<> vsName,
					StringSection<> gsName,
					StringSection<> psName,
					StringSection<> defines)
				{
					auto vsCode = ::Assets::MakeAsset<CompiledShaderByteCode>(vsName, defines);
					assert(gsName.IsEmpty());
					
					::Assets::FuturePtr<CompiledShaderByteCode> psCode;
					if (XlEqString(MakeFileNameSplitter(psName).Extension(), "graph")) {
						psCode = GeneratePixelPreviewShader(psName, defines, provider, psMainName, logMessages, *shaderSource);
					} else {
						psCode = ::Assets::MakeAsset<CompiledShaderByteCode>(psName, defines);
					}

					auto future = std::make_shared<::Assets::AssetFuture<Metal::ShaderProgram>>("GraphPreviewTechniqueDelegate");
					future->SetPollingFunction(
						[vsCode, psCode](::Assets::AssetFuture<Metal::ShaderProgram>& thatFuture) -> bool {

						auto vsActual = vsCode->TryActualize();
						auto psActual = psCode->TryActualize();

						if (!vsActual || !psActual) {
							auto vsState = vsCode->GetAssetState();
							auto psState = psCode->GetAssetState();
							if (vsState == ::Assets::AssetState::Invalid || psState == ::Assets::AssetState::Invalid) {
								auto depVal = std::make_shared<::Assets::DependencyValidation>();
								TryRegisterDependency(depVal, vsCode);
								TryRegisterDependency(depVal, psCode);
								thatFuture.SetInvalidAsset(depVal, nullptr);
								return false;
							}
							return true;
						}

						auto newShaderProgram = std::make_shared<Metal::ShaderProgram>(Metal::GetObjectFactory(), *vsActual, *psActual);
						thatFuture.SetAsset(std::move(newShaderProgram), {});
						return false;
					});

					return future;
				};*/
		}
	private:
		std::shared_ptr<RenderCore::Techniques::Technique> _technique;
		Techniques::UniqueShaderVariationSet _resolvedShaders;
		static const char s_techFile[];
	};

	const char GraphPreviewTechniqueDelegate::s_techFile[] = "xleres/Techniques/Graph/graph.tech";

	std::unique_ptr<RenderCore::Techniques::ITechniqueDelegate> MakeNodeGraphPreviewDelegate(
		const std::shared_ptr<GraphLanguage::INodeGraphProvider>& provider,
		const std::string& psMainName,
		const std::shared_ptr<MessageRelay>& logMessages)
	{
		return std::make_unique<GraphPreviewTechniqueDelegate>(provider, psMainName, logMessages);
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

