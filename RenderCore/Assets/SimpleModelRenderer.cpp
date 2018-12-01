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

#include "../DX11/Metal/Buffer.h"

namespace RenderCore { namespace Assets 
{
	static IResourcePtr LoadVertexBuffer(
        const RenderCore::Assets::ModelScaffold& scaffold,
        const RenderCore::Assets::VertexData& vb);
	static IResourcePtr LoadIndexBuffer(
        const RenderCore::Assets::ModelScaffold& scaffold,
        const RenderCore::Assets::IndexData& ib);

	class SimpleModelDrawable : public Techniques::Drawable
	{
	public:
		DrawCallDesc _drawCall;
		Float4x4 _objectToWorld;
	};

	static void DrawFn_SimpleModelStatic(
        Metal::DeviceContext& metalContext,
		Techniques::ParsingContext& parserContext,
        const SimpleModelDrawable& drawable, const Metal::BoundUniforms& boundUniforms,
        const Metal::ShaderProgram&)
	{
		ConstantBufferView cbvs[] = {
			Techniques::MakeLocalTransformPacket(
				drawable._objectToWorld, 
				ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld))};
		boundUniforms.Apply(metalContext, 3, UniformsStream{MakeIteratorRange(cbvs)});

		metalContext.Bind(drawable._drawCall._topology);
        metalContext.DrawIndexed(drawable._drawCall._indexCount, drawable._drawCall._firstIndex, drawable._drawCall._firstVertex);
	}

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

	VariantArray SimpleModelRenderer::BuildDrawables(
		const Float4x4& localToWorld,
		uint64_t materialFilter)
	{
		VariantArray result;

		const auto& cmdStream = _modelScaffold->CommandStream();
        const auto& immData = _modelScaffold->ImmutableData();
        for (unsigned c = 0; c < cmdStream.GetGeoCallCount(); ++c) {
            const auto& geoCall = cmdStream.GetGeoCall(c);
            
            auto& rawGeo = immData._geos[geoCall._geoId];
            for (unsigned d = 0; d < unsigned(rawGeo._drawCalls.size()); ++d) {
                const auto& drawCall = rawGeo._drawCalls[d];
				auto materialGuids = geoCall._materialGuids[drawCall._subMaterialIndex];

                    // reject geometry that doesn't match the material
                    // binding that we want
                if (materialFilter != 0 && materialGuids != materialFilter)
                    continue;

				auto& drawable = *result.Allocate<SimpleModelDrawable>(1);
				drawable._geo = _geos[geoCall._geoId];
				drawable._material = _materialScaffold->GetMaterial(materialGuids);
				drawable._drawFn = (Techniques::Drawable::ExecuteDrawFn*)&DrawFn_SimpleModelStatic;
				drawable._drawCall = drawCall;
				drawable._uniformsInterface = _usi;

				auto machineOutput = _skeletonBinding.ModelJointToMachineOutput(geoCall._transformMarker);
                if (machineOutput < _baseTransformCount) {
                    drawable._objectToWorld = Combine(_baseTransforms[machineOutput], localToWorld);
                } else {
                    drawable._objectToWorld = localToWorld;
                }
            }
        }

        for (unsigned c = 0; c < cmdStream.GetSkinCallCount(); ++c) {
            const auto& geoCall = cmdStream.GetSkinCall(c);
            
            auto& rawGeo = immData._boundSkinnedControllers[geoCall._geoId];
            for (unsigned d = 0; d < unsigned(rawGeo._drawCalls.size()); ++d) {
                const auto& drawCall = rawGeo._drawCalls[d];
				auto materialGuids = geoCall._materialGuids[drawCall._subMaterialIndex];

                    // reject geometry that doesn't match the material
                    // binding that we want
                if (materialFilter != 0 && materialGuids != materialFilter)
                    continue;

                    // now we have at least once piece of geometry
                    // that we want to render... We need to bind the material,
                    // index buffer and vertex buffer and topology
                    // then we just execute the draw command

				auto& drawable = *result.Allocate<SimpleModelDrawable>(1);
				drawable._geo = _boundSkinnedControllers[geoCall._geoId];
				drawable._material = _materialScaffold->GetMaterial(materialGuids);
				drawable._drawFn = (Techniques::Drawable::ExecuteDrawFn*)&DrawFn_SimpleModelStatic;
				drawable._drawCall = drawCall;
				drawable._uniformsInterface = _usi;

                drawable._objectToWorld = Combine(
					_baseTransforms[_skeletonBinding.ModelJointToMachineOutput(geoCall._transformMarker)], 
					localToWorld);
            }
        }

		return result;
	}

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

	SimpleModelRenderer::SimpleModelRenderer(
		const std::shared_ptr<RenderCore::Assets::ModelScaffold>& modelScaffold,
		const std::shared_ptr<RenderCore::Assets::MaterialScaffold>& materialScaffold,
		IteratorRange<const DeformOperationInstantiation*> deformAttachments)
	: _modelScaffold(modelScaffold)
	, _materialScaffold(materialScaffold)
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
        skeleton.GenerateOutputTransforms(_baseTransforms.get(), _baseTransformCount, &skeleton.GetDefaultParameters());

		unsigned dynVBIterator = 0;

		_geos.reserve(modelScaffold->ImmutableData()._geoCount);
		for (unsigned geo=0; geo<modelScaffold->ImmutableData()._geoCount; ++geo) {
			const auto& rg = modelScaffold->ImmutableData()._geos[geo];

			// Calculate which elements are suppressed by the deform operations
			// We can only support a single deform operation per geo
			const DeformOperationInstantiation* deformAttachment = nullptr;
			for (const auto& def:deformAttachments)
				if (def._geoId == geo) {
					assert(!deformAttachment);
					deformAttachment = &def;
				}
					
			std::vector<uint64_t> suppressedElements;
			if (deformAttachment)
				suppressedElements = { deformAttachment->_suppressElements.begin(), deformAttachment->_suppressElements.end() };
			std::sort(suppressedElements.begin(), suppressedElements.end());
			suppressedElements.erase(
				std::unique(suppressedElements.begin(), suppressedElements.end()),
				suppressedElements.end());

			// Build the main non-deformed vertex stream
			auto drawableGeo = std::make_shared<Techniques::DrawableGeo>();
			drawableGeo->_vertexStreams[0] = MakeVertexStream(*modelScaffold, rg._vb, MakeIteratorRange(suppressedElements));
			drawableGeo->_vertexStreamCount = 1;

			// Attach those vertex streams that come from the deform operation
			if (deformAttachment) {
				auto& vStream = drawableGeo->_vertexStreams[1];
				++drawableGeo->_vertexStreamCount;
				vStream._vertexElements = deformAttachment->_generatedElements;
				vStream._vertexElementsHash = 
					Hash64(
						AsPointer(vStream._vertexElements.begin()), 
						AsPointer(vStream._vertexElements.end()));
				vStream._vertexStride = CalculateVertexStride(MakeIteratorRange(vStream._vertexElements));

				// register the deform operation
				unsigned vertexCount = rg._vb._size;
				DeformOp deformOp;
				deformOp._deformOp = deformAttachment->_operation;
				deformOp._stride = vStream._vertexStride;
				deformOp._elements = vStream._vertexElements;
				deformOp._dynVBBegin = dynVBIterator;
				dynVBIterator += vStream._vertexStride * vertexCount;
				deformOp._dynVBEnd = dynVBIterator;
				_deformOps.emplace_back(std::move(deformOp));
			}

			drawableGeo->_ib = LoadIndexBuffer(*modelScaffold, rg._ib);
			drawableGeo->_ibFormat = rg._ib._format;
			_geos.push_back(std::move(drawableGeo));
		}

		if (dynVBIterator) {
			_dynVB = RenderCore::Assets::Services::GetDevice().CreateResource(
				CreateDesc(
					BindFlag::VertexBuffer,
					CPUAccess::WriteDynamic, GPUAccess::Read,
					LinearBufferDesc::Create(dynVBIterator),
					"ModelRendererDynVB"));

			for (auto&g:_geos)
				if (g->_vertexStreamCount > 1 && !g->_vertexStreams[1]._resource)
					g->_vertexStreams[1]._resource = _dynVB;
		}

		_boundSkinnedControllers.reserve(modelScaffold->ImmutableData()._boundSkinnedControllerCount);
		for (unsigned geo=0; geo<modelScaffold->ImmutableData()._boundSkinnedControllerCount; ++geo) {
			const auto& rg = modelScaffold->ImmutableData()._boundSkinnedControllers[geo];

			auto drawableGeo = std::make_shared<Techniques::DrawableGeo>();
			drawableGeo->_vertexStreams[0] = MakeVertexStream(*modelScaffold, rg._vb);
			drawableGeo->_vertexStreams[1] = MakeVertexStream(*modelScaffold, rg._animatedVertexElements);
			drawableGeo->_vertexStreamCount = 2;

			drawableGeo->_ib = LoadIndexBuffer(*modelScaffold, rg._ib);
			drawableGeo->_ibFormat = rg._ib._format;
			_boundSkinnedControllers.push_back(std::move(drawableGeo));
		}

		_usi = std::make_shared<UniformsStreamInterface>();
		_usi->BindConstantBuffer(0, {Techniques::ObjectCB::LocalTransform});
	}

	struct DeformConstructionFuture
	{
	public:
		std::string _pendingConstruction;
		std::vector<DeformOperationInstantiation> _deformOps;
	};

	void SimpleModelRenderer::ConstructToFuture(
		::Assets::AssetFuture<SimpleModelRenderer>& future,
		StringSection<::Assets::ResChar> modelScaffoldName,
		StringSection<::Assets::ResChar> materialScaffoldName,
		StringSection<::Assets::ResChar> deformOperations)
	{
		auto scaffoldFuture = ::Assets::MakeAsset<RenderCore::Assets::ModelScaffold>(modelScaffoldName);
		auto materialFuture = ::Assets::MakeAsset<RenderCore::Assets::MaterialScaffold>(materialScaffoldName, modelScaffoldName);
		auto deformFuture = std::make_shared<DeformConstructionFuture>();
		if (DeformOperationFactory::HasInstance())
			deformFuture->_pendingConstruction = deformOperations.AsString();

		future.SetPollingFunction(
			[scaffoldFuture, materialFuture, deformFuture](::Assets::AssetFuture<SimpleModelRenderer>& thatFuture) -> bool {

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

			auto newModel = std::make_shared<SimpleModelRenderer>(scaffoldActual, materialActual, MakeIteratorRange(deformFuture->_deformOps));
			thatFuture.SetAsset(std::move(newModel), {});
			return false;
		});
	}

	void SimpleModelRenderer::ConstructToFuture(
		::Assets::AssetFuture<SimpleModelRenderer>& future,
		StringSection<::Assets::ResChar> modelScaffoldName)
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

}}
