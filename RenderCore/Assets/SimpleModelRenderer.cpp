// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SimpleModelRenderer.h"
#include "ModelRunTime.h"
#include "ModelScaffoldInternal.h"
#include "ModelImmutableData.h"
#include "AssetUtils.h"
#include "../Techniques/Drawables.h"
#include "../Techniques/TechniqueUtils.h"
#include "../Techniques/ParsingContext.h"
#include "../Techniques/CommonBindings.h"
#include "../Types.h"
#include "../UniformsStream.h"
#include "../Metal/DeviceContext.h"
#include "../Metal/InputLayout.h"
#include "../../Assets/Assets.h"
#include "../../Assets/AssetFuture.h"
#include "../../Assets/IFileSystem.h"
#include "../../Utility/VariantUtils.h"

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

	VariantArray SimpleModelRenderer::BuildDrawables(uint64_t materialFilter)
	{
		VariantArray result;

		std::string techniqueConfig = "illum";

		const auto& cmdStream = _modelScaffold->CommandStream();
        const auto& immData = _modelScaffold->ImmutableData();
        for (unsigned c = 0; c < cmdStream.GetGeoCallCount(); ++c) {
            const auto& geoCall = cmdStream.GetGeoCall(c);
            
            auto& rawGeo = immData._geos[geoCall._geoId];
            for (unsigned d = 0; d < unsigned(rawGeo._drawCalls.size()); ++d) {
                const auto& drawCall = rawGeo._drawCalls[d];

                    // reject geometry that doesn't match the material
                    // binding that we want
                if (materialFilter != 0
                    && geoCall._materialGuids[drawCall._subMaterialIndex] != materialFilter)
                    continue;

				auto& drawable = *result.Allocate<SimpleModelDrawable>(1);
				drawable._techniqueConfig = techniqueConfig;
				drawable._geo = _geos[geoCall._geoId];
				drawable._drawFn = (Techniques::Drawable::ExecuteDrawFn*)&DrawFn_SimpleModelStatic;
				drawable._drawCall = drawCall;
				drawable._uniformsInterface = _usi;

				auto machineOutput = _skeletonBinding.ModelJointToMachineOutput(geoCall._transformMarker);
                if (machineOutput < _baseTransformCount) {
                    drawable._objectToWorld = _baseTransforms[machineOutput];
                } else {
                    drawable._objectToWorld = Identity<Float4x4>();
                }
            }
        }

        for (unsigned c = 0; c < cmdStream.GetSkinCallCount(); ++c) {
            const auto& geoCall = cmdStream.GetSkinCall(c);
            
            auto& rawGeo = immData._boundSkinnedControllers[geoCall._geoId];
            for (unsigned d = 0; d < unsigned(rawGeo._drawCalls.size()); ++d) {
                const auto& drawCall = rawGeo._drawCalls[d];

                    // reject geometry that doesn't match the material
                    // binding that we want
                if (materialFilter != 0
                    && geoCall._materialGuids[drawCall._subMaterialIndex] != materialFilter)
                    continue;

                    // now we have at least once piece of geometry
                    // that we want to render... We need to bind the material,
                    // index buffer and vertex buffer and topology
                    // then we just execute the draw command

				auto& drawable = *result.Allocate<SimpleModelDrawable>(1);
				drawable._techniqueConfig = techniqueConfig;
				drawable._geo = _boundSkinnedControllers[geoCall._geoId];
				drawable._drawFn = (Techniques::Drawable::ExecuteDrawFn*)&DrawFn_SimpleModelStatic;
				drawable._drawCall = drawCall;
				drawable._uniformsInterface = _usi;

                drawable._objectToWorld = _baseTransforms[
                    _skeletonBinding.ModelJointToMachineOutput(geoCall._transformMarker)];
            }
        }

		return result;
	}

	static Techniques::DrawableGeo::VertexStream MakeVertexStream(
		const RenderCore::Assets::ModelScaffold& modelScaffold,
		const VertexData& vertices)
	{
		Techniques::DrawableGeo::VertexStream vStream;
		vStream._resource = LoadVertexBuffer(modelScaffold, vertices);
		vStream._vertexElements = BuildLowLevelInputAssembly(MakeIteratorRange(vertices._ia._elements));
		vStream._vertexElementsHash = 
			Hash64(
				AsPointer(vStream._vertexElements.begin()), 
				AsPointer(vStream._vertexElements.end()));
		vStream._vertexStride = vertices._ia._vertexStride;
		return vStream;
	}

	const ::Assets::DepValPtr& SimpleModelRenderer::GetDependencyValidation() { return _modelScaffold->GetDependencyValidation(); }

	SimpleModelRenderer::SimpleModelRenderer(const std::shared_ptr<RenderCore::Assets::ModelScaffold>& modelScaffold)
	: _modelScaffold(modelScaffold)
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

		_geos.reserve(modelScaffold->ImmutableData()._geoCount);
		for (unsigned geo=0; geo<modelScaffold->ImmutableData()._geoCount; ++geo) {
			const auto& rg = modelScaffold->ImmutableData()._geos[geo];

			auto drawableGeo = std::make_shared<Techniques::DrawableGeo>();
			drawableGeo->_vertexStreams[0] = MakeVertexStream(*modelScaffold, rg._vb);
			drawableGeo->_vertexStreamCount = 1;

			drawableGeo->_ib = LoadIndexBuffer(*modelScaffold, rg._ib);
			drawableGeo->_ibFormat = rg._ib._format;
			_geos.push_back(std::move(drawableGeo));
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

	void SimpleModelRenderer::ConstructToFuture(
		::Assets::AssetFuture<SimpleModelRenderer>& future,
		StringSection<::Assets::ResChar> modelScaffoldName)
	{
		auto scaffoldFuture = ::Assets::MakeAsset<RenderCore::Assets::ModelScaffold>(modelScaffoldName);

		future.SetPollingFunction(
			[scaffoldFuture](::Assets::AssetFuture<SimpleModelRenderer>& thatFuture) -> bool {

			auto scaffoldActual = scaffoldFuture->TryActualize();

			if (!scaffoldActual) {
				auto state = scaffoldFuture->GetAssetState();
				if (state == ::Assets::AssetState::Invalid) {
					thatFuture.SetInvalidAsset(scaffoldFuture->GetDependencyValidation(), nullptr);
					return false;
				}
				return true;
			}

			auto newModel = std::make_shared<SimpleModelRenderer>(scaffoldActual);
			thatFuture.SetAsset(std::move(newModel), {});
			return false;
		});
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
