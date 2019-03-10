// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SimpleModelRenderer.h"
#include "SimpleModelDeform.h"
#include "ModelScaffold.h"
#include "ModelScaffoldInternal.h"
#include "ModelImmutableData.h"
#include "MaterialScaffold.h"
#include "AssetUtils.h"
#include "Services.h"
#include "../Techniques/Drawables.h"
#include "../Techniques/TechniqueUtils.h"
#include "../Techniques/ParsingContext.h"
#include "../Techniques/CommonBindings.h"
#include "../Types.h"
#include "../ResourceDesc.h"
#include "../IDevice.h"
#include "../UniformsStream.h"
#include "../Metal/DeviceContext.h"
#include "../Metal/InputLayout.h"
#include "../../Assets/Assets.h"
#include "../../Assets/AssetFuture.h"
#include "../../Assets/IFileSystem.h"
#include "../../Utility/VariantUtils.h"

// #define BAD_MATERIAL_FALLBACK

#pragma warning(disable:4505)

namespace RenderCore { namespace Assets 
{
	static IResourcePtr LoadVertexBuffer(
        const RenderCore::Assets::ModelScaffold& scaffold,
        const RenderCore::Assets::VertexData& vb);
	static IResourcePtr LoadIndexBuffer(
        const RenderCore::Assets::ModelScaffold& scaffold,
        const RenderCore::Assets::IndexData& ib);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class SimpleModelDrawable : public Techniques::Drawable
	{
	public:
		DrawCallDesc _drawCall;
		Float4x4 _objectToWorld;
		uint64_t _materialGuid;
		unsigned _drawCallIdx;
	};

	struct DrawCallProperties
	{
		uint64_t _materialGuid;
		unsigned _drawCallIdx;
		unsigned _dummy;
	};

	static void DrawFn_SimpleModelStatic(
        Metal::DeviceContext& metalContext,
		Techniques::ParsingContext& parserContext,
        const SimpleModelDrawable& drawable, const Metal::BoundUniforms& boundUniforms,
        const Metal::ShaderProgram&)
	{
		ConstantBufferView cbvs[2];
		cbvs[0] = {
			Techniques::MakeLocalTransformPacket(
				drawable._objectToWorld, 
				ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld))};
		if (boundUniforms._boundUniformBufferSlots[3] & (1<<1)) {
			cbvs[1] = MakeSharedPkt(DrawCallProperties{drawable._materialGuid, drawable._drawCallIdx});
		}
		boundUniforms.Apply(metalContext, 3, UniformsStream{MakeIteratorRange(cbvs)});

		metalContext.Bind(drawable._drawCall._topology);
        metalContext.DrawIndexed(drawable._drawCall._indexCount, drawable._drawCall._firstIndex, drawable._drawCall._firstVertex);
	}

	#if defined(BAD_MATERIAL_FALLBACK)
		static Techniques::Material& GetDummyMaterial()
		{
			static Techniques::Material dummyMaterial;
			static bool dummyMaterialIsInitialized = false;
			if (!dummyMaterialIsInitialized) {
				XlCopyString(dummyMaterial._techniqueConfig, "xleres/techniques/illum.tech");
				dummyMaterialIsInitialized = true;
			}
			return dummyMaterial;
		}
	#endif

	void SimpleModelRenderer::BuildDrawables(
		IteratorRange<Techniques::DrawablesPacket** const> pkts,
		const Float4x4& localToWorld) const
	{
		auto* generalPkt = pkts[unsigned(Techniques::BatchFilter::General)];
		if (!generalPkt) return;

		unsigned drawCallCounter = 0;
		const auto& cmdStream = _modelScaffold->CommandStream();
        const auto& immData = _modelScaffold->ImmutableData();
        for (unsigned c = 0; c < cmdStream.GetGeoCallCount(); ++c) {
            const auto& geoCall = cmdStream.GetGeoCall(c);
            auto& rawGeo = immData._geos[geoCall._geoId];

			auto* allocatedDrawables = generalPkt->_drawables.Allocate<SimpleModelDrawable>(rawGeo._drawCalls.size());
            for (unsigned d = 0; d < unsigned(rawGeo._drawCalls.size()); ++d) {
                const auto& drawCall = rawGeo._drawCalls[d];
				auto materialGuid = geoCall._materialGuids[drawCall._subMaterialIndex];

				auto& drawable = allocatedDrawables[d];
				drawable._geo = _geos[geoCall._geoId];
				drawable._material = _materialScaffold->GetMaterial(materialGuid);
				drawable._drawFn = (Techniques::Drawable::ExecuteDrawFn*)&DrawFn_SimpleModelStatic;
				drawable._drawCall = drawCall;
				drawable._uniformsInterface = _usi;
				drawable._materialGuid = materialGuid;
				drawable._drawCallIdx = drawCallCounter;

				#if defined(BAD_MATERIAL_FALLBACK)
					if (!drawable._material)
						drawable._material = &GetDummyMaterial();
					assert(drawable._material);
				#endif

				auto machineOutput = _skeletonBinding.ModelJointToMachineOutput(geoCall._transformMarker);
                assert(machineOutput < _baseTransformCount);
                drawable._objectToWorld = Combine(_baseTransforms[machineOutput], localToWorld);

				++drawCallCounter;
            }
        }

        for (unsigned c = 0; c < cmdStream.GetSkinCallCount(); ++c) {
            const auto& geoCall = cmdStream.GetSkinCall(c);
            auto& rawGeo = immData._boundSkinnedControllers[geoCall._geoId];

			auto* allocatedDrawables = generalPkt->_drawables.Allocate<SimpleModelDrawable>(rawGeo._drawCalls.size());
            for (unsigned d = 0; d < unsigned(rawGeo._drawCalls.size()); ++d) {
                const auto& drawCall = rawGeo._drawCalls[d];
				auto materialGuid = geoCall._materialGuids[drawCall._subMaterialIndex];

                    // now we have at least once piece of geometry
                    // that we want to render... We need to bind the material,
                    // index buffer and vertex buffer and topology
                    // then we just execute the draw command

				auto& drawable = allocatedDrawables[d];
				drawable._geo = _boundSkinnedControllers[geoCall._geoId];
				drawable._material = _materialScaffold->GetMaterial(materialGuid);
				drawable._drawFn = (Techniques::Drawable::ExecuteDrawFn*)&DrawFn_SimpleModelStatic;
				drawable._drawCall = drawCall;
				drawable._uniformsInterface = _usi;
				drawable._materialGuid = materialGuid;
				drawable._drawCallIdx = drawCallCounter;

				#if defined(BAD_MATERIAL_FALLBACK)
					if (!drawable._material)
						drawable._material = &GetDummyMaterial();
					assert(drawable._material);
				#endif

				auto machineOutput = _skeletonBinding.ModelJointToMachineOutput(geoCall._transformMarker);
                assert(machineOutput < _baseTransformCount);
                drawable._objectToWorld = Combine(_baseTransforms[machineOutput], localToWorld);

				++drawCallCounter;
            }
        }
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class SimpleModelDrawable_Delegate : public SimpleModelDrawable
	{
	public:
		std::shared_ptr<SimpleModelRenderer::IPreDrawDelegate> _delegate;
	};

	static void DrawFn_SimpleModelDelegate(
        Metal::DeviceContext& metalContext,
		Techniques::ParsingContext& parserContext,
        const SimpleModelDrawable_Delegate& drawable, const Metal::BoundUniforms& boundUniforms,
        const Metal::ShaderProgram& shader)
	{
		bool delegateResult = drawable._delegate->OnDraw(metalContext, parserContext, drawable, drawable._materialGuid, drawable._drawCallIdx);
		if (delegateResult)
			DrawFn_SimpleModelStatic(metalContext, parserContext, drawable, boundUniforms, shader);
	}

	void SimpleModelRenderer::BuildDrawables(
		IteratorRange<Techniques::DrawablesPacket** const> pkts,
		const Float4x4& localToWorld,
		const std::shared_ptr<IPreDrawDelegate>& delegate) const
	{
		auto* generalPkt = pkts[unsigned(Techniques::BatchFilter::General)];
		if (!generalPkt) return;

		unsigned drawCallCounter = 0;
		const auto& cmdStream = _modelScaffold->CommandStream();
        const auto& immData = _modelScaffold->ImmutableData();
        for (unsigned c = 0; c < cmdStream.GetGeoCallCount(); ++c) {
            const auto& geoCall = cmdStream.GetGeoCall(c);
            auto& rawGeo = immData._geos[geoCall._geoId];

			auto* allocatedDrawables = generalPkt->_drawables.Allocate<SimpleModelDrawable_Delegate>(rawGeo._drawCalls.size());
            for (unsigned d = 0; d < unsigned(rawGeo._drawCalls.size()); ++d) {
                const auto& drawCall = rawGeo._drawCalls[d];
				auto materialGuid = geoCall._materialGuids[drawCall._subMaterialIndex];

				auto& drawable = allocatedDrawables[d];
				drawable._geo = _geos[geoCall._geoId];
				drawable._material = _materialScaffold->GetMaterial(materialGuid);
				drawable._drawFn = (Techniques::Drawable::ExecuteDrawFn*)&DrawFn_SimpleModelDelegate;
				drawable._drawCall = drawCall;
				drawable._uniformsInterface = _usi;
				drawable._materialGuid = materialGuid;
				drawable._drawCallIdx = drawCallCounter;
				drawable._delegate = delegate;

				#if defined(BAD_MATERIAL_FALLBACK)
					if (!drawable._material)
						drawable._material = &GetDummyMaterial();
					assert(drawable._material);
				#endif

				auto machineOutput = _skeletonBinding.ModelJointToMachineOutput(geoCall._transformMarker);
                if (machineOutput < _baseTransformCount) {
                    drawable._objectToWorld = Combine(_baseTransforms[machineOutput], localToWorld);
                } else {
                    drawable._objectToWorld = localToWorld;
                }

				++drawCallCounter;
            }
        }

        for (unsigned c = 0; c < cmdStream.GetSkinCallCount(); ++c) {
            const auto& geoCall = cmdStream.GetSkinCall(c);
            auto& rawGeo = immData._boundSkinnedControllers[geoCall._geoId];

			auto* allocatedDrawables = generalPkt->_drawables.Allocate<SimpleModelDrawable_Delegate>(rawGeo._drawCalls.size());
            for (unsigned d = 0; d < unsigned(rawGeo._drawCalls.size()); ++d) {
                const auto& drawCall = rawGeo._drawCalls[d];
				auto materialGuid = geoCall._materialGuids[drawCall._subMaterialIndex];

                    // now we have at least once piece of geometry
                    // that we want to render... We need to bind the material,
                    // index buffer and vertex buffer and topology
                    // then we just execute the draw command

				auto& drawable = allocatedDrawables[d];
				drawable._geo = _boundSkinnedControllers[geoCall._geoId];
				drawable._material = _materialScaffold->GetMaterial(materialGuid);
				drawable._drawFn = (Techniques::Drawable::ExecuteDrawFn*)&DrawFn_SimpleModelDelegate;
				drawable._drawCall = drawCall;
				drawable._uniformsInterface = _usi;
				drawable._materialGuid = materialGuid;
				drawable._drawCallIdx = drawCallCounter;
				drawable._delegate = delegate;

				#if defined(BAD_MATERIAL_FALLBACK)
					if (!drawable._material)
						drawable._material = &GetDummyMaterial();
					assert(drawable._material);
				#endif

                drawable._objectToWorld = Combine(
					_baseTransforms[_skeletonBinding.ModelJointToMachineOutput(geoCall._transformMarker)], 
					localToWorld);

				++drawCallCounter;
            }
        }
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	struct SimpleModelRenderer::DeformOp
	{
		std::shared_ptr<IDeformOperation> _deformOp;
		unsigned _dynVBBegin = 0, _dynVBEnd = 0;
		unsigned _stride = 0;
		std::vector<MiniInputElementDesc> _elements;
	};

	void SimpleModelRenderer::GenerateDeformBuffer(IThreadContext& context)
	{
		if (!_dynVB) return;
		auto& metalContext = *RenderCore::Metal::DeviceContext::Get(context);
		auto dst = ((Metal::Buffer*)_dynVB.get())->Map(metalContext);
		for (const auto&d:_deformOps) {
			auto partRange = MakeIteratorRange(PtrAdd(dst.begin(), d._dynVBBegin), PtrAdd(dst.begin(), d._dynVBEnd));
			assert(partRange.begin() < partRange.end() && PtrDiff(partRange.end(), dst.begin()) <= ptrdiff_t(dst.size()));

			IDeformOperation::VertexElementRange elementRanges[16];
			assert(d._elements.size() <= dimof(elementRanges));
			unsigned elementOffsetIterator = 0;
			for (unsigned c=0; c<d._elements.size(); ++c) {
				elementRanges[c] = MakeVertexIteratorRangeConst(
					MakeIteratorRange(PtrAdd(partRange.begin(), elementOffsetIterator), partRange.end()),
					d._stride, d._elements[c]._nativeFormat);
				elementOffsetIterator += BitsPerPixel(d._elements[c]._nativeFormat) / 8;
			}

			// Execute the actual deform op
			d._deformOp->Execute(MakeIteratorRange(elementRanges, &elementRanges[d._elements.size()]));
		}
		((Metal::Buffer*)_dynVB.get())->Unmap(metalContext);
	}

	unsigned SimpleModelRenderer::DeformOperationCount() const { return (unsigned)_deformOps.size(); }
	IDeformOperation& SimpleModelRenderer::DeformOperation(unsigned idx) { return *_deformOps[idx]._deformOp; } 

	static bool IsSorted(IteratorRange<const uint64_t*> suppressedElements)
	{
		for (auto i=suppressedElements.begin()+1; i<suppressedElements.end(); ++i)
			if (*(i-1) > *i) return false;
		return true;
	}

	static Techniques::DrawableGeo::VertexStream MakeVertexStream(
		const RenderCore::Assets::ModelScaffold& modelScaffold,
		const VertexData& vertices,
		IteratorRange<const uint64_t*> suppressedElements = {})
	{
		Techniques::DrawableGeo::VertexStream vStream;
		vStream._vertexElements = BuildLowLevelInputAssembly(MakeIteratorRange(vertices._ia._elements));

		// Remove any elements listed in the suppressedElements array (note that we're assuming
		// this is in sorted order)
		assert(IsSorted(suppressedElements));
		for (auto i=vStream._vertexElements.begin(); i!=vStream._vertexElements.end();) {
			auto hit = std::lower_bound(suppressedElements.begin(), suppressedElements.end(), i->_semanticHash);
			if (hit != suppressedElements.end() && *hit == i->_semanticHash) {
				// We can't remove it entirely because the vertex stride and offsets depend on the elements in the list. If we remove it, the offsets will suddently all be incorrect
				// But we can change the semantic so that it just doesn't get bound
				i->_semanticHash = 0;
				++i;
			} else {
				++i;
			}
		}
		if (vStream._vertexElements.empty())
			return {};

		vStream._resource = LoadVertexBuffer(modelScaffold, vertices);
		vStream._vertexElementsHash = 
			Hash64(
				AsPointer(vStream._vertexElements.begin()), 
				AsPointer(vStream._vertexElements.end()));
		vStream._vertexStride = vertices._ia._vertexStride;
		return vStream;
	}

	const ::Assets::DepValPtr& SimpleModelRenderer::GetDependencyValidation() { return _modelScaffold->GetDependencyValidation(); }

	struct NascentDeformStream
	{
		RenderCore::Techniques::DrawableGeo::VertexStream _vStream;
		SimpleModelRenderer::DeformOp _deformOp;
		std::vector<uint64_t> _suppressedElements;
	};

	static NascentDeformStream BuildSuppressedElements(
		IteratorRange<const DeformOperationInstantiation*> deformAttachments,
		unsigned geoId,
		unsigned vertexCount,
		unsigned& dynVBIterator)
	{
		// Calculate which elements are suppressed by the deform operations
		// We can only support a single deform operation per geo
		const DeformOperationInstantiation* deformAttachment = nullptr;
		for (const auto& def:deformAttachments)
			if (def._geoId == geoId) {
				assert(!deformAttachment);
				deformAttachment = &def;
			}

		if (!deformAttachment) return {};

		NascentDeformStream result;
		result._suppressedElements = { deformAttachment->_suppressElements.begin(), deformAttachment->_suppressElements.end() };
		std::sort(result._suppressedElements.begin(), result._suppressedElements.end());
		result._suppressedElements.erase(
			std::unique(result._suppressedElements.begin(), result._suppressedElements.end()),
			result._suppressedElements.end());

		result._vStream._vertexElements = deformAttachment->_generatedElements;
		result._vStream._vertexElementsHash = 
			Hash64(
				AsPointer(result._vStream._vertexElements.begin()), 
				AsPointer(result._vStream._vertexElements.end()));
		result._vStream._vertexStride = CalculateVertexStride(MakeIteratorRange(result._vStream._vertexElements));
		result._vStream._vbOffset = dynVBIterator;

		// register the deform operation
		result._deformOp._deformOp = deformAttachment->_operation;
		result._deformOp._stride = result._vStream._vertexStride;
		result._deformOp._elements = result._vStream._vertexElements;
		result._deformOp._dynVBBegin = dynVBIterator;
		dynVBIterator += result._vStream._vertexStride * vertexCount;
		result._deformOp._dynVBEnd = dynVBIterator;

		return result;
	}

	SimpleModelRenderer::SimpleModelRenderer(
		const std::shared_ptr<RenderCore::Assets::ModelScaffold>& modelScaffold,
		const std::shared_ptr<RenderCore::Assets::MaterialScaffold>& materialScaffold,
		IteratorRange<const DeformOperationInstantiation*> deformAttachments,
		const std::string& modelScaffoldName,
		const std::string& materialScaffoldName)
	: _modelScaffold(modelScaffold)
	, _materialScaffold(materialScaffold)
	, _modelScaffoldName(modelScaffoldName)
	, _materialScaffoldName(materialScaffoldName)
	{
		using namespace RenderCore::Assets;

		// const auto& skeletonScaff = ::Assets::GetAssetComp<SkeletonScaffold>(modelFile);
        // skeletonScaff.StallWhilePending();
        // const auto& skeleton = skeletonScaff.GetTransformationMachine();
        const auto& skeleton = modelScaffold->EmbeddedSkeleton();

        _skeletonBinding = SkeletonBinding(
            skeleton.GetOutputInterface(),
            modelScaffold->CommandStream().GetInputInterface());

        _baseTransformCount = skeleton.GetOutputMatrixCount();
        _baseTransforms = std::make_unique<Float4x4[]>(_baseTransformCount);
        skeleton.GenerateOutputTransforms(MakeIteratorRange(_baseTransforms.get(), _baseTransforms.get() + _baseTransformCount), &skeleton.GetDefaultParameters());

		unsigned dynVBIterator = 0;

		_geos.reserve(modelScaffold->ImmutableData()._geoCount);
		for (unsigned geo=0; geo<modelScaffold->ImmutableData()._geoCount; ++geo) {
			const auto& rg = modelScaffold->ImmutableData()._geos[geo];

			unsigned vertexCount = rg._vb._size / rg._vb._ia._vertexStride;
			auto deform = BuildSuppressedElements(deformAttachments, geo, vertexCount, dynVBIterator);

			// Build the main non-deformed vertex stream
			auto drawableGeo = std::make_shared<Techniques::DrawableGeo>();
			drawableGeo->_vertexStreams[0] = MakeVertexStream(*modelScaffold, rg._vb, MakeIteratorRange(deform._suppressedElements));
			drawableGeo->_vertexStreamCount = 1;

			// Attach those vertex streams that come from the deform operation
			if (deform._deformOp._deformOp) {
				drawableGeo->_vertexStreams[drawableGeo->_vertexStreamCount] = std::move(deform._vStream);
				++drawableGeo->_vertexStreamCount;
				_deformOps.emplace_back(std::move(deform._deformOp));
			}

			drawableGeo->_ib = LoadIndexBuffer(*modelScaffold, rg._ib);
			drawableGeo->_ibFormat = rg._ib._format;
			_geos.push_back(std::move(drawableGeo));
		}

		_boundSkinnedControllers.reserve(modelScaffold->ImmutableData()._boundSkinnedControllerCount);
		for (unsigned geo=0; geo<modelScaffold->ImmutableData()._boundSkinnedControllerCount; ++geo) {
			const auto& rg = modelScaffold->ImmutableData()._boundSkinnedControllers[geo];

			unsigned vertexCount = rg._vb._size / rg._vb._ia._vertexStride;
			auto deform = BuildSuppressedElements(deformAttachments, geo + (unsigned)modelScaffold->ImmutableData()._geoCount, vertexCount, dynVBIterator);

			// Build the main non-deformed vertex stream
			auto drawableGeo = std::make_shared<Techniques::DrawableGeo>();
			drawableGeo->_vertexStreams[0] = MakeVertexStream(*modelScaffold, rg._vb, MakeIteratorRange(deform._suppressedElements));
			drawableGeo->_vertexStreams[1] = MakeVertexStream(*modelScaffold, rg._animatedVertexElements, MakeIteratorRange(deform._suppressedElements));
			drawableGeo->_vertexStreamCount = 2;

			// Attach those vertex streams that come from the deform operation
			if (deform._deformOp._deformOp) {
				drawableGeo->_vertexStreams[drawableGeo->_vertexStreamCount] = std::move(deform._vStream);
				++drawableGeo->_vertexStreamCount;
				_deformOps.emplace_back(std::move(deform._deformOp));
			}

			drawableGeo->_ib = LoadIndexBuffer(*modelScaffold, rg._ib);
			drawableGeo->_ibFormat = rg._ib._format;
			_boundSkinnedControllers.push_back(std::move(drawableGeo));
		}

		// Create the dynamic VB and assign it to all of the slots it needs to go to
		if (dynVBIterator) {
			_dynVB = RenderCore::Assets::Services::GetDevice().CreateResource(
				CreateDesc(
					BindFlag::VertexBuffer,
					CPUAccess::WriteDynamic, GPUAccess::Read,
					LinearBufferDesc::Create(dynVBIterator),
					"ModelRendererDynVB"));

			for (auto&g:_geos)
				for (unsigned s=0; s<g->_vertexStreamCount; ++s)
					if (!g->_vertexStreams[s]._resource)
						g->_vertexStreams[s]._resource = _dynVB;

			for (auto&g:_boundSkinnedControllers)
				for (unsigned s=0; s<g->_vertexStreamCount; ++s)
					if (!g->_vertexStreams[s]._resource)
						g->_vertexStreams[s]._resource = _dynVB;
		}

		_usi = std::make_shared<UniformsStreamInterface>();
		_usi->BindConstantBuffer(0, {Techniques::ObjectCB::LocalTransform});
		_usi->BindConstantBuffer(1, {Techniques::ObjectCB::DrawCallProperties});
	}

	SimpleModelRenderer::~SimpleModelRenderer() {}

	struct DeformConstructionFuture
	{
	public:
		std::string _pendingConstruction;
		std::vector<DeformOperationInstantiation> _deformOps;
	};

	void SimpleModelRenderer::ConstructToFuture(
		::Assets::AssetFuture<SimpleModelRenderer>& future,
		StringSection<> modelScaffoldName,
		StringSection<> materialScaffoldName,
		StringSection<> deformOperations)
	{
		auto scaffoldFuture = ::Assets::MakeAsset<RenderCore::Assets::ModelScaffold>(modelScaffoldName);
		auto materialFuture = ::Assets::MakeAsset<RenderCore::Assets::MaterialScaffold>(materialScaffoldName, modelScaffoldName);
		auto deformFuture = std::make_shared<DeformConstructionFuture>();
		if (DeformOperationFactory::HasInstance())
			deformFuture->_pendingConstruction = deformOperations.AsString();

		std::string modelScaffoldNameString = modelScaffoldName.AsString();
		std::string materialScaffoldNameString = materialScaffoldName.AsString();

		future.SetPollingFunction(
			[scaffoldFuture, materialFuture, deformFuture, modelScaffoldNameString, materialScaffoldNameString](::Assets::AssetFuture<SimpleModelRenderer>& thatFuture) -> bool {

			auto scaffoldActual = scaffoldFuture->TryActualize();
			auto materialActual = materialFuture->TryActualize();

			if (!scaffoldActual) {
				auto state = scaffoldFuture->GetAssetState();
				if (state == ::Assets::AssetState::Invalid) {
					std::stringstream str;
					str << "ModelScaffold failed to actualize: ";
					const auto& actLog = scaffoldFuture->GetActualizationLog();
					str << (actLog ? ::Assets::AsString(actLog) : std::string("<<no log>>"));
					thatFuture.SetInvalidAsset(scaffoldFuture->GetDependencyValidation(), ::Assets::AsBlob(str.str()));
					return false;
				}
				return true;
			}

			if (!deformFuture->_pendingConstruction.empty()) {
				deformFuture->_deformOps = DeformOperationFactory::GetInstance().CreateDeformOperations(
					MakeStringSection(deformFuture->_pendingConstruction),
					scaffoldActual);
				deformFuture->_pendingConstruction = std::string();
			}

			if (!materialActual) {
				auto state = materialFuture->GetAssetState();
				if (state == ::Assets::AssetState::Invalid) {
					std::stringstream str;
					str << "MaterialScaffold failed to actualize: ";
					const auto& actLog = materialFuture->GetActualizationLog();
					str << (actLog ? ::Assets::AsString(actLog) : std::string("<<no log>>"));
					thatFuture.SetInvalidAsset(materialFuture->GetDependencyValidation(), ::Assets::AsBlob(str.str()));
					return false;
				}
				return true;
			}

			auto newModel = std::make_shared<SimpleModelRenderer>(scaffoldActual, materialActual, MakeIteratorRange(deformFuture->_deformOps), modelScaffoldNameString, materialScaffoldNameString);
			thatFuture.SetAsset(std::move(newModel), {});
			return false;
		});
	}

	void SimpleModelRenderer::ConstructToFuture(
		::Assets::AssetFuture<SimpleModelRenderer>& future,
		StringSection<> modelScaffoldName)
	{
		ConstructToFuture(future, modelScaffoldName, modelScaffoldName);
	}

	static IResourcePtr LoadVertexBuffer(
        const RenderCore::Assets::ModelScaffold& scaffold,
        const RenderCore::Assets::VertexData& vb)
    {
        auto buffer = std::make_unique<uint8[]>(vb._size);
		{
            auto inputFile = scaffold.OpenLargeBlocks();
            inputFile->Seek(vb._offset, Utility::FileSeekAnchor::Current);
            inputFile->Read(buffer.get(), vb._size, 1);
        }
		return RenderCore::Assets::CreateStaticVertexBuffer(
			MakeIteratorRange(buffer.get(), PtrAdd(buffer.get(), vb._size)));
    }

    static IResourcePtr LoadIndexBuffer(
        const RenderCore::Assets::ModelScaffold& scaffold,
        const RenderCore::Assets::IndexData& ib)
    {
        auto buffer = std::make_unique<uint8[]>(ib._size);
        {
            auto inputFile = scaffold.OpenLargeBlocks();
            inputFile->Seek(ib._offset, Utility::FileSeekAnchor::Current);
            inputFile->Read(buffer.get(), ib._size, 1);
        }
		return RenderCore::Assets::CreateStaticIndexBuffer(
			MakeIteratorRange(buffer.get(), PtrAdd(buffer.get(), ib._size)));
    }

	SimpleModelRenderer::IPreDrawDelegate::~IPreDrawDelegate() {}

}}
