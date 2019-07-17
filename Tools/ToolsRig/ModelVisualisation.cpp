// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ModelVisualisation.h"
#include "VisualisationUtils.h"
#include "../../RenderCore/Assets/ModelScaffold.h"
#include "../../RenderCore/Assets/MaterialScaffold.h"
#include "../../RenderCore/Assets/SimpleModelRenderer.h"
#include "../../RenderCore/Assets/SimpleModelDeform.h"
#include "../../RenderCore/Assets/SkinDeformer.h"
#include "../../RenderOverlays/AnimationVisualization.h"
#include "../../SceneEngine/SceneParser.h"
#include "../../Assets/Assets.h"
#include "../../Assets/AssetFuture.h"
#include "../../Utility/TimeUtils.h"

#pragma warning(disable:4505)

namespace ToolsRig
{
	using RenderCore::Assets::ModelScaffold;
    using RenderCore::Assets::MaterialScaffold;
	using RenderCore::Assets::AnimationSetScaffold;
	using RenderCore::Assets::SkeletonScaffold;
    using RenderCore::Assets::SkeletonMachine;
	using RenderCore::Assets::SimpleModelRenderer;

///////////////////////////////////////////////////////////////////////////////////////////////////

	uint64_t GetAnimationHash(StringSection<> name)
	{
		const char* parseEnd = nullptr;
		auto parsedValue = XlAtoUI64(name.begin(), &parseEnd, 16);
		if (parseEnd == name.end())
			return parsedValue;
		return Hash64(name);
	}

	class MaterialFilterDelegate : public RenderCore::Assets::SimpleModelRenderer::IPreDrawDelegate
	{
	public:
		virtual bool OnDraw( 
			RenderCore::Metal::DeviceContext& metalContext, RenderCore::Techniques::ParsingContext&,
			const RenderCore::Techniques::Drawable&,
			uint64_t materialGuid, unsigned drawCallIdx) override
		{
			// Note that we're rejecting other draw calls very late in the pipeline here. But it
			// helps avoid extra overhead in the more common cases
			return materialGuid == _activeMaterial;
		}

		MaterialFilterDelegate(uint64_t activeMaterial) : _activeMaterial(activeMaterial) {}
	private:
		uint64_t _activeMaterial;
	};

	class ModelScene : public SceneEngine::IScene, public IVisContent
    {
    public:
		virtual void ExecuteScene(
            RenderCore::IThreadContext& threadContext,
			SceneEngine::SceneExecuteContext& executeContext) const override
		{
			auto skeletonMachine = GetSkeletonMachine();
			assert(skeletonMachine);

			std::vector<Float4x4> skeletonMachineOutput(skeletonMachine->GetOutputMatrixCount());

			if (_animationScaffold && _animationState && _animationState->_state != VisAnimationState::State::BindPose) {
				auto& animData = _animationScaffold->ImmutableData();

				auto animHash = GetAnimationHash(_animationState->_activeAnimation);
				auto foundAnimation = animData._animationSet.FindAnimation(animHash);
				float time = _animationState->_animationTime;
				if (_animationState->_state == VisAnimationState::State::Playing)
					time += (Millisecond_Now() - _animationState->_anchorTime) / 1000.f;
				time = fmodf(time - foundAnimation._beginTime, foundAnimation._endTime - foundAnimation._beginTime) + foundAnimation._beginTime;

				auto params = animData._animationSet.BuildTransformationParameterSet(
					{time, animHash},
					*skeletonMachine, _animSetBinding,
					MakeIteratorRange(animData._curves));

				skeletonMachine->GenerateOutputTransforms(
					MakeIteratorRange(skeletonMachineOutput),
					&params);
			} else {
				skeletonMachine->GenerateOutputTransforms(
					MakeIteratorRange(skeletonMachineOutput),
					&skeletonMachine->GetDefaultParameters());
			}

			for (unsigned c=0; c<_renderer->DeformOperationCount(); ++c) {
				auto* skinDeformOp = dynamic_cast<RenderCore::Assets::SkinDeformer*>(&_renderer->DeformOperation(c));
				if (!skinDeformOp) continue;
				skinDeformOp->FeedInSkeletonMachineResults(
					MakeIteratorRange(skeletonMachineOutput),
					skeletonMachine->GetOutputInterface());
			}
			_renderer->GenerateDeformBuffer(threadContext);

			for (unsigned v=0; v<executeContext.GetViews().size(); ++v) {
				RenderCore::Techniques::DrawablesPacket* pkts[unsigned(RenderCore::Techniques::BatchFilter::Max)];
				for (unsigned c=0; c<unsigned(RenderCore::Techniques::BatchFilter::Max); ++c)
					pkts[c] = executeContext.GetDrawablesPacket(v, RenderCore::Techniques::BatchFilter(c));

				_renderer->BuildDrawables(MakeIteratorRange(pkts), Identity<Float4x4>(), _preDrawDelegate);
			}
		}

		DrawCallDetails GetDrawCallDetails(unsigned drawCallIndex, uint64_t materialGuid) const
		{
			if (_renderer) {
				auto matName = _renderer->GetMaterialScaffold()->GetMaterialName(materialGuid).AsString();
				if (matName.empty())
					matName = _renderer->GetMaterialScaffoldName();
				return { _renderer->GetModelScaffoldName(), matName };
			} else {
				return { {}, {} };
			}
		}
		std::pair<Float3, Float3> GetBoundingBox() const { return _renderer->GetModelScaffold()->GetStaticBoundingBox(); }

		std::shared_ptr<SimpleModelRenderer::IPreDrawDelegate> SetPreDrawDelegate(const std::shared_ptr<SimpleModelRenderer::IPreDrawDelegate>& delegate)
		{
			auto oldDelegate = delegate;
			std::swap(_preDrawDelegate, oldDelegate);
			return oldDelegate;
		}

		void RenderSkeleton(
			RenderCore::IThreadContext& context, 
			RenderCore::Techniques::ParsingContext& parserContext, 
			bool drawBoneNames) const
		{
			auto skeletonMachine = GetSkeletonMachine();

			if (_animationScaffold && _animationState && _animationState->_state != VisAnimationState::State::BindPose) {
				auto& animData = _animationScaffold->ImmutableData();

				auto animHash = GetAnimationHash(_animationState->_activeAnimation);
				auto foundAnimation = animData._animationSet.FindAnimation(animHash);
				float time = _animationState->_animationTime;
				if (_animationState->_state == VisAnimationState::State::Playing) {
					time += (Millisecond_Now() - _animationState->_anchorTime) / 1000.f;
					time = fmodf(time - foundAnimation._beginTime, foundAnimation._endTime - foundAnimation._beginTime) + foundAnimation._beginTime;
				}

				auto params = animData._animationSet.BuildTransformationParameterSet(
					{time, animHash},
					*skeletonMachine, _animSetBinding,
					MakeIteratorRange(animData._curves));

				RenderOverlays::RenderSkeleton(
					context,
					parserContext,
					*skeletonMachine,
					params,
					Identity<Float4x4>(),
					drawBoneNames);
			} else {
				RenderOverlays::RenderSkeleton(
					context,
					parserContext,
					*skeletonMachine,
					Identity<Float4x4>(),
					drawBoneNames);
			}
		}

		void BindAnimationState(const std::shared_ptr<VisAnimationState>& animState)
		{
			_animationState = animState;
			if (_animationState) {
				_animationState->_animationList.clear();
				if (_animationScaffold) {
					for (const auto&anim:_animationScaffold->ImmutableData()._animationSet.GetAnimations()) {
						auto name = _animationScaffold->ImmutableData()._animationSet.LookupStringName(anim.first);
						if (!name.IsEmpty()) {
							_animationState->_animationList.push_back({name.AsString(), anim.second._beginTime, anim.second._endTime});
						} else {
							char buffer[64];
							XlUI64toA(anim.first, buffer, dimof(buffer), 16);
							_animationState->_animationList.push_back({buffer, anim.second._beginTime, anim.second._endTime});
						}
					}
				}
				_animationState->_changeEvent.Invoke();
			}
		}

		bool HasActiveAnimation() const
		{
			return _animationScaffold && _animationState && _animationState->_state == VisAnimationState::State::Playing;
		}

		static void ConstructToFuture(
			::Assets::AssetFuture<ModelScene>& future,
			const ModelVisSettings& settings)
		{
			auto rendererFuture = ::Assets::MakeAsset<SimpleModelRenderer>(settings._modelName, settings._materialName, "skin");

			::Assets::FuturePtr<AnimationSetScaffold> animationSetFuture;
			::Assets::FuturePtr<SkeletonScaffold> skeletonFuture;

			if (!settings._animationFileName.empty())
				animationSetFuture = ::Assets::MakeAsset<AnimationSetScaffold>(settings._animationFileName);

			if (!settings._skeletonFileName.empty())
				skeletonFuture = ::Assets::MakeAsset<SkeletonScaffold>(settings._skeletonFileName);

			uint64_t materialBindingFilter = settings._materialBindingFilter;

			future.SetPollingFunction(
				[rendererFuture, animationSetFuture, skeletonFuture, materialBindingFilter](::Assets::AssetFuture<ModelScene>& thatFuture) -> bool {

					bool stillPending = false;
					auto rendererActual = rendererFuture->TryActualize();
					if (!rendererActual) {
						auto state = rendererFuture->GetAssetState();
						if (state == ::Assets::AssetState::Invalid) {
							std::stringstream str;
							str << "SimpleModelRenderer failed to actualize: ";
							const auto& actLog = rendererFuture->GetActualizationLog();
							str << (actLog ? ::Assets::AsString(actLog) : std::string("<<no log>>"));
							thatFuture.SetInvalidAsset(rendererFuture->GetDependencyValidation(), ::Assets::AsBlob(str.str()));
							return false;
						}
						stillPending = true;
					}

					std::shared_ptr<AnimationSetScaffold> animationSetActual;
					std::shared_ptr<SkeletonScaffold> skeletonActual;

					if (animationSetFuture) {
						animationSetActual = animationSetFuture->TryActualize();
						if (!animationSetActual) {
							auto state = animationSetFuture->GetAssetState();
							if (state == ::Assets::AssetState::Invalid) {
								std::stringstream str;
								str << "AnimationSet failed to actualize: ";
								const auto& actLog = animationSetFuture->GetActualizationLog();
								str << (actLog ? ::Assets::AsString(actLog) : std::string("<<no log>>"));
								thatFuture.SetInvalidAsset(animationSetFuture->GetDependencyValidation(), ::Assets::AsBlob(str.str()));
								return false;
							}
							stillPending = true;
						}
					}

					if (skeletonFuture) {
						skeletonActual = skeletonFuture->TryActualize();
						if (!skeletonFuture) {
							auto state = skeletonFuture->GetAssetState();
							if (state == ::Assets::AssetState::Invalid) {
								std::stringstream str;
								str << "Skeleton failed to actualize: ";
								const auto& actLog = skeletonFuture->GetActualizationLog();
								str << (actLog ? ::Assets::AsString(actLog) : std::string("<<no log>>"));
								thatFuture.SetInvalidAsset(skeletonFuture->GetDependencyValidation(), ::Assets::AsBlob(str.str()));
								return false;
							}
							stillPending = true;
						}
					}

					if (stillPending)
						return true;

					std::shared_ptr<SimpleModelRenderer::IPreDrawDelegate> preDrawDelegate;
					if (materialBindingFilter)
						preDrawDelegate = std::make_shared<MaterialFilterDelegate>(materialBindingFilter);

					auto newModel = std::make_shared<ModelScene>(rendererActual, animationSetActual, skeletonActual, preDrawDelegate);
					thatFuture.SetAsset(std::move(newModel), {});
					return false;
				});
		}

		ModelScene(
			const std::shared_ptr<SimpleModelRenderer>& renderer,
			const std::shared_ptr<AnimationSetScaffold>& animationScaffold,
			const std::shared_ptr<SkeletonScaffold>& skeletonScaffold,
			const std::shared_ptr<SimpleModelRenderer::IPreDrawDelegate>& preDrawDelegate = nullptr)
		: _renderer(renderer), _animationScaffold(animationScaffold), _skeletonScaffold(skeletonScaffold), _preDrawDelegate(preDrawDelegate)
		{
			if (!_skeletonScaffold)
				_modelScaffoldForEmbeddedSkeleton = _renderer->GetModelScaffold();

			if (_animationScaffold) {
				_animSetBinding = RenderCore::Assets::AnimationSetBinding(
					_animationScaffold->ImmutableData()._animationSet.GetOutputInterface(), 
					GetSkeletonMachine()->GetInputInterface());
			}

			_depVal = std::make_shared<::Assets::DependencyValidation>();
			::Assets::RegisterAssetDependency(_depVal, _renderer->GetDependencyValidation());
			if (_animationScaffold)
				::Assets::RegisterAssetDependency(_depVal, _animationScaffold->GetDependencyValidation());
			if (_skeletonScaffold)
				::Assets::RegisterAssetDependency(_depVal, _skeletonScaffold->GetDependencyValidation());
        }

        ~ModelScene() {}

		const ::Assets::DepValPtr& GetDependencyValidation() { return _depVal; }

    protected:
		std::shared_ptr<SimpleModelRenderer>		_renderer;
		std::shared_ptr<SimpleModelRenderer::IPreDrawDelegate>			_preDrawDelegate;
		std::shared_ptr<AnimationSetScaffold>		_animationScaffold;

		std::shared_ptr<ModelScaffold>				_modelScaffoldForEmbeddedSkeleton;
		std::shared_ptr<SkeletonScaffold>			_skeletonScaffold;
		RenderCore::Assets::AnimationSetBinding		_animSetBinding;

		std::shared_ptr<VisAnimationState>			_animationState;

		::Assets::DepValPtr		_depVal;

		const SkeletonMachine* GetSkeletonMachine() const
		{
			const SkeletonMachine* skeletonMachine = nullptr;
			if (_skeletonScaffold) {
				skeletonMachine = &_skeletonScaffold->GetTransformationMachine();
			} else if (_modelScaffoldForEmbeddedSkeleton)
				skeletonMachine = &_modelScaffoldForEmbeddedSkeleton->EmbeddedSkeleton();
			return skeletonMachine;
		}
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

	::Assets::FuturePtr<SceneEngine::IScene> MakeScene(const ModelVisSettings& settings)
	{
		auto modelScene = std::make_shared<::Assets::AssetFuture<ModelScene>>("ModelVisualization");
		::Assets::AutoConstructToFuture(*modelScene, settings);
		return std::reinterpret_pointer_cast<::Assets::AssetFuture<SceneEngine::IScene>>(modelScene);
	}

	ModelVisSettings::ModelVisSettings()
    {
        _modelName = "game/model/galleon/galleon.dae";
        _materialName = "game/model/galleon/galleon.material";
		_levelOfDetail = 0;
		_materialBindingFilter = 0;

		// _modelName = "data/meshes/actors/dragon/character assets/alduin.nif";
		// _materialName = "data/meshes/actors/dragon/character assets/alduin.nif";
		// _animationFileName = "data/meshes/actors/dragon/animations/special_alduindeathagony.hkx";

        // _envSettingsFile = "defaultenv.txt:environment";
    }

#if 0

	using FixedFunctionModel::ModelRenderer;
	using FixedFunctionModel::SharedStateSet;
    using FixedFunctionModel::ModelCache;
	using FixedFunctionModel::ModelCacheModel;
    using FixedFunctionModel::DelayedDrawCallSet;

    static void RenderWithEmbeddedSkeleton(
        const FixedFunctionModel::ModelRendererContext& context,
        const ModelRenderer& model,
        const SharedStateSet& sharedStateSet,
        const ModelScaffold* scaffold)
    {
        if (scaffold) {
            model.Render(
                context, sharedStateSet, Identity<Float4x4>(), 
                FixedFunctionModel::EmbeddedSkeletonPose(*scaffold).GetMeshToModel());
        } else {
            model.Render(context, sharedStateSet, Identity<Float4x4>());
        }
    }

    static void PrepareWithEmbeddedSkeleton(
        DelayedDrawCallSet& dest, 
        const ModelRenderer& model,
        const SharedStateSet& sharedStateSet,
        const ModelScaffold* scaffold)
    {
        if (scaffold) {
            model.Prepare(
                dest, sharedStateSet, Identity<Float4x4>(), 
                FixedFunctionModel::EmbeddedSkeletonPose(*scaffold).GetMeshToModel());
        } else {
            model.Prepare(dest, sharedStateSet, Identity<Float4x4>());
        }
    }
    
    class FixedFunctionModelSceneParser : public SceneEngine::IScene
    {
    public:
        void ExecuteScene(
            RenderCore::IThreadContext& context,
			RenderCore::Techniques::ParsingContext& parserContext,
            SceneEngine::LightingParserContext& lightingParserContext, 
            RenderCore::Techniques::BatchFilter batchFilter,
            SceneEngine::PreparedScene& preparedPackets,
            unsigned techniqueIndex) const 
        {
            auto delaySteps = SceneEngine::AsDelaySteps(batchFilter);
            if (delaySteps.empty()) return;

            auto metalContext = RenderCore::Metal::DeviceContext::Get(context);

            FixedFunctionModel::SharedStateSet::CaptureMarker captureMarker;
            if (_sharedStateSet)
				captureMarker = _sharedStateSet->CaptureState(context, parserContext.GetRenderStateDelegate(), {});

            using namespace RenderCore;
            Metal::ConstantBuffer drawCallIndexBuffer(
				Metal::GetObjectFactory(), 
				CreateDesc(BindFlag::ConstantBuffer, CPUAccess::WriteDynamic, GPUAccess::Read, LinearBufferDesc::Create(sizeof(unsigned)*4), "drawCallIndex"));
            metalContext->GetNumericUniforms(ShaderStage::Geometry).Bind(MakeResourceList(drawCallIndexBuffer));

            if (Tweakable("RenderSkinned", false)) {
                if (delaySteps[0] == FixedFunctionModel::DelayStep::OpaqueRender) {
                    auto preparedAnimation = _model->CreatePreparedAnimation();
                    FixedFunctionModel::SkinPrepareMachine prepareMachine(
                        *_modelScaffold, _modelScaffold->EmbeddedSkeleton());
                    RenderCore::Assets::AnimationState animState = {0.f, 0u};
                    prepareMachine.PrepareAnimation(context, *preparedAnimation, animState);
                    _model->PrepareAnimation(context, *preparedAnimation, prepareMachine.GetSkeletonBinding());

                    FixedFunctionModel::MeshToModel meshToModel(
                        *preparedAnimation, &prepareMachine.GetSkeletonBinding());

                    _model->Render(
                        FixedFunctionModel::ModelRendererContext(*metalContext, parserContext, techniqueIndex),
                        *_sharedStateSet, Identity<Float4x4>(), 
                        meshToModel, preparedAnimation.get());

                    if (Tweakable("RenderSkeleton", false)) {
                        prepareMachine.RenderSkeleton(
                            context, parserContext, 
                            animState, Identity<Float4x4>());
                    }
                }
            } else {
                const bool fillInStencilInfo = (_settings->_colourByMaterial != 0);

                for (auto i:delaySteps)
                    ModelRenderer::RenderPrepared(
                        FixedFunctionModel::ModelRendererContext(*metalContext.get(), parserContext, techniqueIndex),
                        *_sharedStateSet, _delayedDrawCalls, i,
                        [&metalContext, &drawCallIndexBuffer, &fillInStencilInfo](ModelRenderer::DrawCallEvent evnt)
                        {
                            if (fillInStencilInfo) {
                                // hack -- we just adjust the depth stencil state to enable the stencil buffer
                                //          no way to do this currently without dropping back to low level API
                                #if GFXAPI_ACTIVE == GFXAPI_DX11
                                    Metal::DepthStencilState dss(*metalContext);
                                    D3D11_DEPTH_STENCIL_DESC desc;
                                    dss.GetUnderlying()->GetDesc(&desc);
                                    desc.StencilEnable = true;
                                    desc.StencilWriteMask = 0xff;
                                    desc.StencilReadMask = 0xff;
                                    desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
                                    desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
                                    desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
                                    desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
                                    desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
                                    desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
                                    desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
                                    desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
                                    auto newDSS = Metal::GetObjectFactory().CreateDepthStencilState(&desc);
                                    metalContext->GetUnderlying()->OMSetDepthStencilState(newDSS.get(), 1+evnt._drawCallIndex);
                                #endif
                            }

                            unsigned drawCallIndexB[4] = { evnt._drawCallIndex, 0, 0, 0 };
                            drawCallIndexBuffer.Update(*metalContext, drawCallIndexB, sizeof(drawCallIndexB));

                            metalContext->DrawIndexed(evnt._indexCount, evnt._firstIndex, evnt._firstVertex);
                        });
            }
        }

		virtual void ExecuteScene(
            RenderCore::IThreadContext& threadContext,
			SceneEngine::SceneExecuteContext& executeContext) const override
		{
		}

		VisCameraSettings AlignCamera()
		{
			return AlignCameraToBoundingBox(40.f, _boundingBox);
		}

		FixedFunctionModelSceneParser(
			const VisOverlaySettings& settings,
			ModelRenderer& model, const std::pair<Float3, Float3>& boundingBox, SharedStateSet& sharedStateSet,
			const ModelScaffold* modelScaffold = nullptr)
		: _model(&model), _boundingBox(boundingBox), _sharedStateSet(&sharedStateSet)
		, _settings(&settings), _modelScaffold(modelScaffold) 
		, _delayedDrawCalls(typeid(ModelRenderer).hash_code())
		{
			PrepareWithEmbeddedSkeleton(
				_delayedDrawCalls, *_model,
				*_sharedStateSet, modelScaffold);
			ModelRenderer::Sort(_delayedDrawCalls);
		}
	protected:
		ModelRenderer* _model;
		SharedStateSet* _sharedStateSet;
		std::pair<Float3, Float3> _boundingBox;

		const VisOverlaySettings* _settings;
		const ModelScaffold* _modelScaffold;
		DelayedDrawCallSet _delayedDrawCalls;
	};

	std::unique_ptr<SceneEngine::IScene> CreateModelScene(const ModelCacheModel& model)
    {
        ModelVisSettings settings;
        return std::make_unique<FixedFunctionModelSceneParser>(
            settings,
            *model._renderer, model._boundingBox, *model._sharedStateSet);
    }

	static ModelCacheModel GetModel(
		ModelCache& cache,
		ModelVisSettings& settings)
	{
		std::vector<ModelCache::SupplementGUID> supplements;
		{
			const auto& s = settings._supplements;
			size_t offset = 0;
			for (;;) {
				auto comma = s.find_first_of(',', offset);
				if (comma == std::string::npos) comma = s.size();
				if (offset == comma) break;
				auto hash = ConstHash64FromString(AsPointer(s.begin()) + offset, AsPointer(s.begin()) + comma);
				supplements.push_back(hash);
				offset = comma;
			}
		}

		return cache.GetModel(
			MakeStringSection(settings._modelName), 
			MakeStringSection(settings._materialName),
			MakeIteratorRange(supplements),
			settings._levelOfDetail);
	}

#endif

}

