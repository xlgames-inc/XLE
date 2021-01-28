// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ObjectPlaceholders.h"
#include "VisualisationGeo.h"
#include "../EntityInterface/RetainedEntities.h"
#include "../../FixedFunctionModel/ModelRunTime.h"
#include "../../FixedFunctionModel/PreboundShaders.h"
#include "../../RenderCore/Assets/ModelScaffold.h"
#include "../../RenderCore/Assets/ModelScaffoldInternal.h"
#include "../../RenderCore/Assets/ModelImmutableData.h"
#include "../../RenderCore/Assets/PredefinedCBLayout.h"
#include "../../RenderCore/Assets/MaterialScaffold.h"
#include "../../RenderCore/Assets/RawMaterial.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../RenderCore/Techniques/CommonBindings.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/CommonUtils.h"
#include "../../RenderCore/Techniques/PipelineAccelerator.h"
#include "../../RenderCore/Techniques/DescriptorSetAccelerator.h"
#include "../../RenderCore/Techniques/Drawables.h"
#include "../../RenderCore/Techniques/CompiledShaderPatchCollection.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/Buffer.h"
#include "../../RenderCore/Metal/ObjectFactory.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/ResourceList.h"
#include "../../RenderCore/Format.h"
#include "../../RenderCore/Types.h"
#include "../../RenderCore/IDevice.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../Assets/Assets.h"
#include "../../Assets/IFileSystem.h"
#include "../../Assets/AssetFutureContinuation.h"
#include "../../ConsoleRig/Console.h"
#include "../../ConsoleRig/ResourceBox.h"
#include "../../Math/Transformations.h"
#include "../../Math/Geometry.h"
#include "../../Utility/StringUtils.h"
#include "../../OSServices/RawFS.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/VariantUtils.h"
#include "../../xleres/FileList.h"

namespace ToolsRig
{
    using namespace RenderCore;

    namespace Parameters
    {
        static const auto Transform = ParameterBox::MakeParameterNameHash("Transform");
        static const auto Translation = ParameterBox::MakeParameterNameHash("Translation");
        static const auto Visible = ParameterBox::MakeParameterNameHash("Visible");
        static const auto ShowMarker = ParameterBox::MakeParameterNameHash("ShowMarker");
		static const auto Shape = ParameterBox::MakeParameterNameHash("Shape");
		static const auto Diffuse = ParameterBox::MakeParameterNameHash("Diffuse");
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	class SimpleModelDrawable : public Techniques::Drawable
	{
	public:
		RenderCore::Assets::DrawCallDesc _drawCall;
		Float4x4 _objectToWorld;
		bool _indexed = true;

		static void DrawFn(
			Techniques::ParsingContext& parserContext,
			const Techniques::Drawable::DrawFunctionContext& drawFnContext,
			const SimpleModelDrawable& drawable)
		{
			ConstantBufferView cbvs[1];
			cbvs[0] = {
				Techniques::MakeLocalTransformPacket(
					drawable._objectToWorld, 
					ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld))};

			drawFnContext.ApplyUniforms(UniformsStream{MakeIteratorRange(cbvs)});
			if (drawable._indexed) {
				drawFnContext.DrawIndexed(drawable._drawCall._indexCount, drawable._drawCall._firstIndex, drawable._drawCall._firstVertex);
			} else {
				drawFnContext.Draw(drawable._drawCall._indexCount, drawable._drawCall._firstVertex);
			}
		}
	};

	class SimpleModel
	{
	public:
		void BuildDrawables(
			IteratorRange<RenderCore::Techniques::DrawablesPacket** const> pkts,
			const ParameterBox& materialParams,
			const Float4x4& localToWorld = Identity<Float4x4>());

		const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }

		SimpleModel(
			const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAcceleratorPool,
			const RenderCore::Assets::RawGeometry& geo, ::Assets::IFileInterface& largeBlocksFile);
		SimpleModel(
			const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAcceleratorPool,
			StringSection<::Assets::ResChar> filename);
		~SimpleModel();
	private:
		std::shared_ptr<RenderCore::Techniques::DrawableGeo> _drawableGeo;
		std::vector<RenderCore::Assets::DrawCallDesc> _drawCalls;
		::Assets::DepValPtr _depVal;
		std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool> _pipelineAcceleratorPool;
		
		std::shared_ptr<RenderCore::Techniques::PipelineAccelerator> _pipelineAccelerator;
		std::shared_ptr<RenderCore::Techniques::DescriptorSetAccelerator> _descriptorSetAccelerator;
		std::shared_ptr<RenderCore::UniformsStreamInterface> _usi;

		void Build(const RenderCore::Assets::RawGeometry& geo, ::Assets::IFileInterface& largeBlocksFile);
	};

	void SimpleModel::BuildDrawables(
		IteratorRange<RenderCore::Techniques::DrawablesPacket** const> pkts,
		const ParameterBox& materialParams,
		const Float4x4& localToWorld)
	{
#if 0
		auto shader = _material.FindVariation(parserContext, techniqueIndex, techniqueConfig);
		if (shader._shader._shaderProgram) {
			auto matParams0 = shader._cbLayout->BuildCBDataAsPkt(materialParams, RenderCore::Techniques::GetDefaultShaderLanguage());
			Float3 col0 = Float3(.66f, 0.2f, 0.f);
			Float3 col1 = Float3(0.44f, 0.6f, 0.1f);
			static float time = 0.f;
			time += 3.14159f / 3.f / 60.f;
			ParameterBox p; p.SetParameter("MaterialDiffuse", LinearInterpolate(col0, col1, 0.5f + 0.5f * XlCos(time)));
			auto matParams1 = shader._cbLayout->BuildCBDataAsPkt(p, RenderCore::Techniques::GetDefaultShaderLanguage());

			if (_drawCalls.size() >= 2) {
				devContext.Bind(*(Metal::Resource*)_ib->QueryInterface(typeid(Metal::Resource).hash_code()), _ibFormat);

				VertexBufferView vbv { _vb, 0 };
				shader._shader.Apply(devContext, parserContext, {vbv});

				ConstantBufferView cbvs0[] = { {localTransform}, {matParams0} };
				shader._shader._boundUniforms->Apply(devContext, 1, UniformsStream{ MakeIteratorRange(cbvs0) });
				devContext.DrawIndexed(_drawCalls[0]._indexCount, _drawCalls[0]._firstIndex, _drawCalls[0]._firstVertex);

				ConstantBufferView cbvs1[] = { {localTransform}, {matParams0} };
				shader._shader._boundUniforms->Apply(devContext, 1, UniformsStream{ MakeIteratorRange(cbvs1) });
				devContext.DrawIndexed(_drawCalls[1]._indexCount, _drawCalls[1]._firstIndex, _drawCalls[1]._firstVertex);
			}
		}
#endif

		auto* drawables = pkts[(unsigned)RenderCore::Techniques::BatchFilter::General]->_drawables.Allocate<SimpleModelDrawable>(_drawCalls.size());
		for (const auto& drawCall:_drawCalls) {
			auto& drawable = *drawables++;
			drawable._pipeline = _pipelineAccelerator;
			drawable._descriptorSet = _descriptorSetAccelerator;
			drawable._geo = _drawableGeo;
			drawable._drawFn = (Techniques::Drawable::ExecuteDrawFn*)&SimpleModelDrawable::DrawFn;
			drawable._uniformsInterface = _usi;
			drawable._drawCall = drawCall;
            drawable._objectToWorld = localToWorld;
        }
	}

	static std::vector<uint8_t> ReadFromFile(::Assets::IFileInterface& file, size_t size, size_t offset)
	{
		file.Seek(offset);
		std::vector<uint8_t> result(size);
		file.Read(result.data(), size, 1);
		return result;
	}

	SimpleModel::SimpleModel(
		const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAcceleratorPool,
		const RenderCore::Assets::RawGeometry& geo, ::Assets::IFileInterface& largeBlocksFile)
	{
		_pipelineAcceleratorPool = pipelineAcceleratorPool;
		Build(geo, largeBlocksFile);
	}

	SimpleModel::SimpleModel(
		const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAcceleratorPool,
		StringSection<::Assets::ResChar> filename)
	{
		_pipelineAcceleratorPool = pipelineAcceleratorPool;
		auto& scaffold = ::Assets::Legacy::GetAssetComp<RenderCore::Assets::ModelScaffold>(filename);
		if (scaffold.ImmutableData()._geoCount > 0) {
			auto largeBlocksFile = scaffold.OpenLargeBlocks();
			Build(*scaffold.ImmutableData()._geos, *largeBlocksFile);
		}
		_depVal = scaffold.GetDependencyValidation();
	}

	void SimpleModel::Build(const RenderCore::Assets::RawGeometry& geo, ::Assets::IFileInterface& largeBlocksFile)
	{
		// load the vertex buffer & index buffer from the geo input, and copy draw calls data
		auto largeBlocksOffset = largeBlocksFile.TellP();
		auto vbData = ReadFromFile(largeBlocksFile, geo._vb._size, geo._vb._offset + largeBlocksOffset);
		auto ibData = ReadFromFile(largeBlocksFile, geo._ib._size, geo._ib._offset + largeBlocksOffset);
		auto vb = RenderCore::Techniques::CreateStaticVertexBuffer(MakeIteratorRange(vbData));
		auto ib = RenderCore::Techniques::CreateStaticIndexBuffer(MakeIteratorRange(ibData));
		_drawableGeo = std::make_shared<RenderCore::Techniques::DrawableGeo>();
		_drawableGeo->_vertexStreams[0]._resource = vb;
		_drawableGeo->_vertexStreamCount = 1;
		_drawableGeo->_ib = ib;
		_drawableGeo->_ibFormat = geo._ib._format;
		_drawCalls.insert(_drawCalls.begin(), geo._drawCalls.cbegin(), geo._drawCalls.cend());

		// also construct a technique material for the geometry format
		std::vector<InputElementDesc> inputElements;
		for (const auto&i:geo._vb._ia._elements)
			inputElements.push_back(InputElementDesc(i._semanticName, i._semanticIndex, i._nativeFormat, 0, i._alignedByteOffset));

		auto dsf = RenderCore::Techniques::MakeDescriptorSetAccelerator(
			ParameterBox {}, ParameterBox {},
			RenderCore::Techniques::GetFallbackMaterialDescriptorSetLayout(),
			"SimpleModel descriptor set");
		dsf->StallWhilePending();
		_descriptorSetAccelerator = dsf->Actualize();

		// The topology must be the same for all draw calls
		assert(!_drawCalls.empty());
		for (unsigned c=1; c<_drawCalls.size(); ++c) {
			assert(_drawCalls[c]._topology == _drawCalls[0]._topology);
		}

		_pipelineAccelerator = _pipelineAcceleratorPool->CreatePipelineAccelerator(
			std::make_shared<RenderCore::Techniques::CompiledShaderPatchCollection>(),
			ParameterBox {},	// material selectors
			MakeIteratorRange(inputElements),
			_drawCalls[0]._topology,
			RenderCore::Assets::RenderStateSet {});

		_usi = std::make_shared<UniformsStreamInterface>();
		_usi->BindConstantBuffer(0, {Techniques::ObjectCB::LocalTransform});
	}

	SimpleModel::~SimpleModel() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    using EntityInterface::RetainedEntities;
    using EntityInterface::RetainedEntity;

    class VisGeoBox
    {
    public:
		std::shared_ptr<RenderCore::Techniques::PipelineAccelerator> _genSphere;
		std::shared_ptr<RenderCore::Techniques::PipelineAccelerator> _genTube;
		std::shared_ptr<RenderCore::Techniques::PipelineAccelerator> _genRectangle;
		std::shared_ptr<RenderCore::Techniques::DescriptorSetAccelerator> _descriptorSetAccelerator;
		std::shared_ptr<RenderCore::UniformsStreamInterface> _usi;
		std::shared_ptr<::Assets::DependencyValidation> _depVal;

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const  { return _depVal; }

        VisGeoBox();
        ~VisGeoBox();

		static void ConstructToFuture(
			::Assets::AssetFuture<VisGeoBox>&,
			const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAcceleratorPool);
    };

	static std::shared_ptr<RenderCore::Techniques::PipelineAccelerator> BuildPipelineAccelerator(
		const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAcceleratorPool,
		const RenderCore::Assets::RawMaterial& mat)
	{
		return pipelineAcceleratorPool->CreatePipelineAccelerator(
			std::make_shared<RenderCore::Techniques::CompiledShaderPatchCollection>(mat._patchCollection),
			mat._matParamBox,
			{},
			Topology::TriangleList,
			mat._stateSet);
	}
    
    void VisGeoBox::ConstructToFuture(
		::Assets::AssetFuture<VisGeoBox>& future,
		const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAcceleratorPool)
    {
		auto sphereMatFuture = ::Assets::MakeAsset<RenderCore::Assets::RawMaterial>(AREA_LIGHT_TECH":sphere");
		auto tubeMatFuture = ::Assets::MakeAsset<RenderCore::Assets::RawMaterial>(AREA_LIGHT_TECH":tube");
		auto rectangleMatFuture = ::Assets::MakeAsset<RenderCore::Assets::RawMaterial>(AREA_LIGHT_TECH":rectangle");
		auto dsf = RenderCore::Techniques::MakeDescriptorSetAccelerator(
			ParameterBox {}, ParameterBox {},
			RenderCore::Techniques::GetFallbackMaterialDescriptorSetLayout(),
			"SimpleModel descriptor set");

		::Assets::WhenAll(sphereMatFuture, tubeMatFuture, rectangleMatFuture, dsf).ThenConstructToFuture<VisGeoBox>(
			future,
			[pipelineAcceleratorPool](
				const std::shared_ptr<RenderCore::Assets::RawMaterial>& sphereMat,
				const std::shared_ptr<RenderCore::Assets::RawMaterial>& tubeMat,
				const std::shared_ptr<RenderCore::Assets::RawMaterial>& rectangleMat,
				const std::shared_ptr<RenderCore::Techniques::DescriptorSetAccelerator>& descriptorSet) {

				auto res = std::make_shared<VisGeoBox>();
				res->_genSphere = BuildPipelineAccelerator(pipelineAcceleratorPool, *sphereMat);
				res->_genTube = BuildPipelineAccelerator(pipelineAcceleratorPool, *tubeMat);
				res->_genRectangle = BuildPipelineAccelerator(pipelineAcceleratorPool, *rectangleMat);

				res->_descriptorSetAccelerator = descriptorSet;
				res->_usi = std::make_shared<UniformsStreamInterface>();
				res->_usi->BindConstantBuffer(0, {Techniques::ObjectCB::LocalTransform});
				return res;
			});
    }

	VisGeoBox::VisGeoBox() {}
    VisGeoBox::~VisGeoBox() {}

    static Float4x4 GetTransform(const RetainedEntity& obj)
    {
        auto xform = obj._properties.GetParameter<Float4x4>(Parameters::Transform);
        if (xform.has_value()) return Transpose(xform.value());

        auto transl = obj._properties.GetParameter<Float3>(Parameters::Translation);
        if (transl.has_value()) {
            return AsFloat4x4(transl.value());
        }
        return Identity<Float4x4>();
    }

    static bool GetShowMarker(const RetainedEntity& obj)
    {
        return obj._properties.GetParameter(Parameters::ShowMarker, true);
    }

	class ObjectParams
	{
	public:
		Metal::ConstantBufferPacket _localTransform;
		ParameterBox				_matParams;

		ObjectParams(const RetainedEntity& obj, Techniques::ParsingContext& parserContext, bool directionalTransform = false)
		{
			auto trans = GetTransform(obj);
			if (directionalTransform) {
					// reorient the transform similar to represent the orientation of directional lights
				auto translation = ExtractTranslation(trans);
				trans = MakeObjectToWorld(-Normalize(translation), Float3(0.f, 0.f, 1.f), translation);
			}
			_localTransform = Techniques::MakeLocalTransformPacket(
				trans, ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld));

			// bit of a hack -- copy from the "Diffuse" parameter to the "MaterialDiffuse" shader constant
			auto c = obj._properties.GetParameter(Parameters::Diffuse, ~0u);
			_matParams.SetParameter("MaterialDiffuse", Float3(((c >> 16) & 0xff) / 255.f, ((c >> 8) & 0xff) / 255.f, ((c >> 0) & 0xff) / 255.f));
		}
	};

    void DrawSphereStandIn(
		const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAcceleratorPool,
        IteratorRange<RenderCore::Techniques::DrawablesPacket** const> pkts,
		const Float4x4& localToWorld, 
		const ParameterBox& matParams = {})
    {
		auto* asset = ::Assets::MakeAsset<SimpleModel>(pipelineAcceleratorPool, "game/model/simple/spherestandin.dae")->TryActualize().get();
		if (asset)
			asset->BuildDrawables(pkts, matParams, localToWorld);
    }

	static void DrawPointerStandIn(
		const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAcceleratorPool,
        IteratorRange<RenderCore::Techniques::DrawablesPacket** const> pkts,
		const Float4x4& localToWorld, 
		const ParameterBox& matParams = {})
	{
		auto* asset = ::Assets::MakeAsset<SimpleModel>(pipelineAcceleratorPool, "game/model/simple/pointerstandin.dae")->TryActualize().get();
		if (asset)
			asset->BuildDrawables(pkts, matParams, localToWorld);
	}

	void ObjectPlaceholders::BuildDrawables(IteratorRange<RenderCore::Techniques::DrawablesPacket** const> pkts)
	{
		if (Tweakable("DrawMarkers", true)) {
			for (const auto& a:_cubeAnnotations) {
				auto objects = _objects->FindEntitiesOfType(a._typeId);
				for (const auto&o:objects) {
					if (!o->_properties.GetParameter(Parameters::Visible, true) || !GetShowMarker(*o)) continue;
					DrawSphereStandIn(_pipelineAcceleratorPool, pkts, GetTransform(*o));
				}
			}

			for (const auto& a:_directionalAnnotations) {
				auto objects = _objects->FindEntitiesOfType(a._typeId);
				for (const auto&o : objects) {
					if (!o->_properties.GetParameter(Parameters::Visible, true) || !GetShowMarker(*o)) continue;

					auto trans = GetTransform(*o);
						// reorient the transform similar to represent the orientation of directional lights
					auto translation = ExtractTranslation(trans);
					trans = MakeObjectToWorld(-Normalize(translation), Float3(0.f, 0.f, 1.f), translation);

					DrawPointerStandIn(_pipelineAcceleratorPool, pkts, trans);
				}
			}

			if (!_areaLightAnnotation.empty()) {
				auto visBox = ::Assets::MakeAsset<VisGeoBox>(_pipelineAcceleratorPool)->TryActualize();
				if (visBox) {
					for (auto a = _areaLightAnnotation.cbegin(); a != _areaLightAnnotation.cend(); ++a) {
						auto objects = _objects->FindEntitiesOfType(a->_typeId);
						for (const auto&o : objects) {
							if (!o->_properties.GetParameter(Parameters::Visible, true) || !GetShowMarker(*o)) continue;

							auto shape = o->_properties.GetParameter(Parameters::Shape, 0u);
							unsigned vertexCount = 12 * 12 * 6;	// (must agree with the shader!)

							auto& drawable = *pkts[(unsigned)RenderCore::Techniques::BatchFilter::General]->_drawables.Allocate<SimpleModelDrawable>(1);
							switch (shape) { 
							case 2: drawable._pipeline = visBox->_genTube; break;
							case 3: drawable._pipeline = visBox->_genRectangle; vertexCount = 6*6; break;
							default: drawable._pipeline = visBox->_genSphere; break;
							}
							drawable._descriptorSet = visBox->_descriptorSetAccelerator;
							drawable._drawFn = (Techniques::Drawable::ExecuteDrawFn*)&SimpleModelDrawable::DrawFn;
							drawable._drawCall = RenderCore::Assets::DrawCallDesc { 0, vertexCount };
							drawable._objectToWorld = GetTransform(*o);
							drawable._indexed = false;
							drawable._uniformsInterface = visBox->_usi;
						}
					}
				}
			}
		}
	}

	class OldVisGeoBox
    {
	public:
		RenderCore::IResourcePtr	_cubeVB;
        unsigned					_cubeVBCount;
        unsigned					_cubeVBStride;
		FixedFunctionModel::SimpleShaderVariationManager	_material;
		FixedFunctionModel::SimpleShaderVariationManager	_materialP;

		const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const  { return _depVal; }

		struct Desc {};
		OldVisGeoBox(const Desc&);
	protected:
		std::shared_ptr<::Assets::DependencyValidation> _depVal;
	};

	OldVisGeoBox::OldVisGeoBox(const Desc&)
    : _material(
        ToolsRig::Vertex3D_InputLayout, 
        { Techniques::ObjectCB::LocalTransform, Techniques::ObjectCB::BasicMaterialConstants },
        ParameterBox())
    , _materialP(
        GlobalInputLayouts::P,
        { Techniques::ObjectCB::LocalTransform, Techniques::ObjectCB::BasicMaterialConstants },
        ParameterBox())
	{
		auto cubeVertices = ToolsRig::BuildCube();
        _cubeVBCount = (unsigned)cubeVertices.size();
        _cubeVBStride = (unsigned)sizeof(decltype(cubeVertices)::value_type);
        _cubeVB = RenderCore::Techniques::CreateStaticVertexBuffer(MakeIteratorRange(cubeVertices));
        _depVal = std::make_shared<::Assets::DependencyValidation>();
	}

	static void DrawTriMeshMarker(
        Metal::DeviceContext& devContext,
        Techniques::ParsingContext& parserContext,
        const OldVisGeoBox& visBox,
        const FixedFunctionModel::SimpleShaderVariationManager::Variation& shader, const RetainedEntity& obj,
        EntityInterface::RetainedEntities& objs)
    {
        static auto IndexListHash = ParameterBox::MakeParameterNameHash("IndexList");

        if (!obj._properties.GetParameter(Parameters::Visible, true) || !GetShowMarker(obj)) return;

        // we need an index list with at least 3 indices (to make at least one triangle)
        auto indexListType = obj._properties.GetParameterType(IndexListHash);
        if (indexListType._type == ImpliedTyping::TypeCat::Void || indexListType._arrayCount < 3)
            return;

        auto ibData = std::make_unique<unsigned[]>(indexListType._arrayCount);
        bool success = obj._properties.GetParameter(
            IndexListHash, ibData.get(), 
            ImpliedTyping::TypeDesc{ImpliedTyping::TypeCat::UInt32, indexListType._arrayCount});
        if (!success) return;

        const auto& chld = obj._children;
        if (!chld.size()) return;

        auto vbData = std::make_unique<Float3[]>(chld.size());
        for (size_t c=0; c<chld.size(); ++c) {
            const auto* e = objs.GetEntity(obj._doc, chld[c].second);
            if (e) {
                vbData[c] = ExtractTranslation(GetTransform(*e));
            } else {
                vbData[c] = Zero<Float3>();
            }
        }
        
        devContext.Bind(Techniques::CommonResources()._blendAdditive);
        devContext.Bind(Techniques::CommonResources()._dssReadOnly);
        devContext.Bind(Techniques::CommonResources()._cullDisable);
        
        auto vb = Metal::MakeVertexBuffer(Metal::GetObjectFactory(), MakeIteratorRange(vbData.get(), PtrAdd(vbData.get(), sizeof(Float3)*chld.size())));
        auto ib = Metal::MakeIndexBuffer(Metal::GetObjectFactory(), MakeIteratorRange(ibData.get(), PtrAdd(ibData.get(), sizeof(unsigned)*indexListType._arrayCount)));

		VertexBufferView vbv { &vb, 0 };
		shader._shader.Apply(devContext, parserContext, {vbv});

		ConstantBufferView cbvs[] = {
			{ Techniques::MakeLocalTransformPacket(
                GetTransform(obj),
				ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld)) },
			{ shader._cbLayout->BuildCBDataAsPkt(ParameterBox{}, RenderCore::Techniques::GetDefaultShaderLanguage()) }
        };

		shader._shader._boundUniforms->Apply(devContext, 1, {MakeIteratorRange(cbvs)});

        devContext.Bind(ib, Format::R32_UINT);
        devContext.Bind(Topology::TriangleList);
        devContext.DrawIndexed(indexListType._arrayCount);
    }

    void ObjectPlaceholders::Render(
        Metal::DeviceContext& metalContext, 
        Techniques::ParsingContext& parserContext,
        unsigned techniqueIndex)
    {
        auto& visBox = ::ConsoleRig::FindCachedBox2<OldVisGeoBox>();
        auto shaderP = visBox._materialP.FindVariation(parserContext, techniqueIndex, MESH_MARKER_TECH);
        if (shaderP._shader._shaderProgram) {
            for (const auto&a:_triMeshAnnotations) {
                auto objects = _objects->FindEntitiesOfType(a._typeId);
                for (auto o=objects.cbegin(); o!=objects.cend(); ++o) {
                    DrawTriMeshMarker(metalContext, parserContext, visBox, shaderP, **o, *_objects);
                }
            }
        }
    }

    void ObjectPlaceholders::AddAnnotation(EntityInterface::ObjectTypeId typeId, const std::string& geoType)
    {
        Annotation newAnnotation;
        newAnnotation._typeId = typeId;

        if (XlEqStringI(geoType, "TriMeshMarker")) {
            _triMeshAnnotations.push_back(newAnnotation);
        } else if (XlEqStringI(geoType, "AreaLight")) {
			_areaLightAnnotation.push_back(newAnnotation);
		} else if (XlEqStringI(geoType, "PointToOrigin")) {
			_directionalAnnotations.push_back(newAnnotation);
		} else {
            _cubeAnnotations.push_back(newAnnotation);
        }
    }

    class ObjectPlaceholders::IntersectionTester : public SceneEngine::IIntersectionTester
    {
    public:
        Result FirstRayIntersection(
            const SceneEngine::IntersectionTestContext& context,
            std::pair<Float3, Float3> worldSpaceRay) const;

        void FrustumIntersection(
            std::vector<Result>& results,
            const SceneEngine::IntersectionTestContext& context,
            const Float4x4& worldToProjection) const;

        IntersectionTester(std::shared_ptr<ObjectPlaceholders> placeHolders);
        ~IntersectionTester();
    protected:
        std::shared_ptr<ObjectPlaceholders> _placeHolders;
    };

	static SceneEngine::IIntersectionTester::Result AsResult(const Float3& worldSpaceCollision, const RetainedEntity& o)
	{
		SceneEngine::IIntersectionTester::Result result;
		result._type = SceneEngine::IntersectionTestScene::Type::Extra;
		result._worldSpaceCollision = worldSpaceCollision;
		result._distance = 0.f;
		result._objectGuid = std::make_pair(o._doc, o._id);
		result._drawCallIndex = 0;
		result._materialGuid = 0;
		return result;
	}

    auto ObjectPlaceholders::IntersectionTester::FirstRayIntersection(
        const SceneEngine::IntersectionTestContext& context,
        std::pair<Float3, Float3> worldSpaceRay) const -> Result
    {
        using namespace SceneEngine;

		// note -- we always return the first intersection encountered. We should be finding the intersection
		//		closest to the start of the ray!

        for (const auto& a:_placeHolders->_cubeAnnotations) {
            for (const auto& o: _placeHolders->_objects->FindEntitiesOfType(a._typeId))
                if (RayVsAABB(worldSpaceRay, GetTransform(*o), Float3(-1.f, -1.f, -1.f), Float3(1.f, 1.f, 1.f)))
					return AsResult(worldSpaceRay.first, *o);
        }

		for (const auto& a : _placeHolders->_directionalAnnotations) {
			for (const auto& o : _placeHolders->_objects->FindEntitiesOfType(a._typeId))
				if (RayVsAABB(worldSpaceRay, GetTransform(*o), Float3(-1.f, -1.f, -1.f), Float3(1.f, 1.f, 1.f)))
					return AsResult(worldSpaceRay.first, *o);
		}

		for (const auto& a : _placeHolders->_areaLightAnnotation) {
			for (const auto& o : _placeHolders->_objects->FindEntitiesOfType(a._typeId)) {
				const auto shape = o->_properties.GetParameter(Parameters::Shape, 0);
				auto trans = GetTransform(*o); 
				if (shape == 2) {
					// Tube... We can ShortestSegmentBetweenLines to calculate if this ray
					// intersects the tube
					auto axis = ExtractForward(trans);
					auto origin = ExtractTranslation(trans);
					auto tube = std::make_pair(Float3(origin - axis), Float3(origin + axis));
					float mua, mub;
					if (ShortestSegmentBetweenLines(mua, mub, worldSpaceRay, tube)) {
						mua = Clamp(mua, 0.f, 1.f);
						mub = Clamp(mub, 0.f, 1.f);
						float distanceSq = 
							MagnitudeSquared(
									LinearInterpolate(worldSpaceRay.first, worldSpaceRay.second, mua)
								-	LinearInterpolate(tube.first, tube.second, mub));
						float radiusSq = MagnitudeSquared(ExtractRight(trans));
						if (distanceSq <= radiusSq) {
								// (not correct intersection pt)
							return AsResult(LinearInterpolate(worldSpaceRay.first, worldSpaceRay.second, mua), *o);
						}
					}
				} else if (shape == 3)  {
					// Rectangle. We treat it as a box with some small width
					const float boxWidth = 0.01f;		// 1cm
					SetUp(trans, boxWidth * ExtractUp(trans));
					if (RayVsAABB(worldSpaceRay, trans, Float3(-1.f, -1.f, -1.f), Float3(1.f, 1.f, 1.f)))
						return AsResult(worldSpaceRay.first, *o);
				} else {
					// Sphere
					float radiusSq = MagnitudeSquared(ExtractRight(trans));
					float dist;
					if (DistanceToSphereIntersection(
						dist, worldSpaceRay.first - ExtractTranslation(trans), 
						Normalize(worldSpaceRay.second - worldSpaceRay.first), radiusSq))
						return AsResult(worldSpaceRay.first, *o);
				}
			}
		}

        return Result();
    }

    void ObjectPlaceholders::IntersectionTester::FrustumIntersection(
        std::vector<Result>& results,
        const SceneEngine::IntersectionTestContext& context,
        const Float4x4& worldToProjection) const
    {}

    ObjectPlaceholders::IntersectionTester::IntersectionTester(std::shared_ptr<ObjectPlaceholders> placeHolders)
    : _placeHolders(placeHolders)
    {}

    ObjectPlaceholders::IntersectionTester::~IntersectionTester() {}

    std::shared_ptr<SceneEngine::IIntersectionTester> ObjectPlaceholders::CreateIntersectionTester()
    {
        return std::make_shared<IntersectionTester>(shared_from_this());
    }

    ObjectPlaceholders::ObjectPlaceholders(
		const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAcceleratorPool,
		const std::shared_ptr<RetainedEntities>& objects)
    : _objects(objects)
	, _pipelineAcceleratorPool(pipelineAcceleratorPool)
    {}

    ObjectPlaceholders::~ObjectPlaceholders() {}

}

