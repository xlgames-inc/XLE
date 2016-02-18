// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ObjectPlaceholders.h"
#include "ExportedNativeTypes.h"
#include "../EntityInterface/RetainedEntities.h"
#include "../ToolsRig/VisualisationGeo.h"
#include "../../RenderCore/Assets/ModelScaffoldInternal.h"
#include "../../RenderCore/Assets/ModelRunTime.h"
#include "../../RenderCore/Assets/ModelImmutableData.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/ResourceBox.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../RenderCore/Techniques/TechniqueMaterial.h"
#include "../../RenderCore/Techniques/CommonBindings.h"
#include "../../RenderCore/Techniques/PredefinedCBLayout.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/Buffer.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../Assets/Assets.h"
#include "../../ConsoleRig/Console.h"
#include "../../Math/Transformations.h"
#include "../../Math/Geometry.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/Streams/FileUtils.h"

namespace GUILayer
{
    using namespace RenderCore;
    using namespace RenderCore::Techniques;

    namespace Parameters
    {
        static const auto Transform = ParameterBox::MakeParameterNameHash("Transform");
        static const auto Translation = ParameterBox::MakeParameterNameHash("Translation");
        static const auto Visible = ParameterBox::MakeParameterNameHash("Visible");
        static const auto ShowMarker = ParameterBox::MakeParameterNameHash("ShowMarker");
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	class SimpleModel
	{
	public:
		void Render(
			Metal::DeviceContext& devContext,
			ParsingContext& parserContext,
			Metal::ConstantBufferPacket localTransform,
			unsigned techniqueIndex, const ::Assets::ResChar techniqueConfig[],
			const ParameterBox& materialParams) const;

		const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }

		SimpleModel(const RenderCore::Assets::RawGeometry& geo, const ::Assets::ResChar filename[], unsigned largeBlocksOffset);
		SimpleModel(const ::Assets::ResChar filename[]);
		~SimpleModel();
	private:
		Metal::VertexBuffer _vb;
		Metal::IndexBuffer _ib;
		std::vector<RenderCore::Assets::DrawCallDesc> _drawCalls;
		TechniqueMaterial _material;
		unsigned _vbStride;
		Metal::NativeFormat::Enum _ibFormat;
		::Assets::DepValPtr _depVal;

		void Build(const RenderCore::Assets::RawGeometry& geo, const ::Assets::ResChar filename[], unsigned largeBlocksOffset);
	};

	void SimpleModel::Render(
		Metal::DeviceContext& devContext,
		ParsingContext& parserContext,
		Metal::ConstantBufferPacket localTransform,
		unsigned techniqueIndex, const ::Assets::ResChar techniqueConfig[],
		const ParameterBox& materialParams) const
	{
		auto shader = _material.FindVariation(parserContext, techniqueIndex, techniqueConfig);
		if (shader._shader._shaderProgram) {
			auto matParams0 = shader._cbLayout->BuildCBDataAsPkt(materialParams);
			Float3 col0 = Float3(.66f, 0.2f, 0.f);
			Float3 col1 = Float3(0.44f, 0.6f, 0.1f);
			static float time = 0.f;
			time += 3.14159f / 3.f / 60.f;
			ParameterBox p; p.SetParameter(u("MaterialDiffuse"), LinearInterpolate(col0, col1, 0.5f + 0.5f * XlCos(time)));
			auto matParams1 = shader._cbLayout->BuildCBDataAsPkt(p);

			devContext.Bind(MakeResourceList(_vb), _vbStride, 0);
			devContext.Bind(_ib, _ibFormat, 0);

			if (_drawCalls.size() >= 2) {
				shader._shader.Apply(devContext, parserContext, { localTransform, matParams0 });
				devContext.DrawIndexed(_drawCalls[0]._indexCount, _drawCalls[0]._firstIndex, _drawCalls[0]._firstVertex);

				shader._shader.Apply(devContext, parserContext, { localTransform, matParams1 });
				devContext.DrawIndexed(_drawCalls[1]._indexCount, _drawCalls[1]._firstIndex, _drawCalls[1]._firstVertex);
			}
		}
	}

	template<typename T> static T ReadFromFile(BasicFile& file, size_t size, size_t offset)
	{
		file.Seek(offset, SEEK_SET);
		auto data = std::make_unique<uint8[]>(size);
		file.Read(data.get(), size, 1);
		return T(data.get(), size);
	}

	SimpleModel::SimpleModel(const RenderCore::Assets::RawGeometry& geo, const ::Assets::ResChar filename[], unsigned largeBlocksOffset)
	{
		Build(geo, filename, largeBlocksOffset);
	}

	SimpleModel::SimpleModel(const ::Assets::ResChar filename[])
	{
		auto& scaffold = ::Assets::GetAssetComp<RenderCore::Assets::ModelScaffold>(filename);
		if (scaffold.ImmutableData()._geoCount > 0)
			Build(*scaffold.ImmutableData()._geos, scaffold.Filename().c_str(), scaffold.LargeBlocksOffset());
		_depVal = scaffold.GetDependencyValidation();
	}

	void SimpleModel::Build(const RenderCore::Assets::RawGeometry& geo, const ::Assets::ResChar filename[], unsigned largeBlocksOffset)
	{
		// load the vertex buffer & index buffer from the geo input, and copy draw calls data
		BasicFile file(filename, "rb");
		_vb = ReadFromFile<Metal::VertexBuffer>(file, geo._vb._size, geo._vb._offset + largeBlocksOffset);
		_ib = ReadFromFile<Metal::IndexBuffer>(file, geo._ib._size, geo._ib._offset + largeBlocksOffset);
		_drawCalls.insert(_drawCalls.begin(), geo._drawCalls.cbegin(), geo._drawCalls.cend());
		_vbStride = geo._vb._ia._vertexStride;
		_ibFormat = Metal::NativeFormat::Enum(geo._ib._format);

		// also construct a technique material for the geometry format
		std::vector<Metal::InputElementDesc> eles;
		for (const auto&i:geo._vb._ia._elements)
			eles.push_back(Metal::InputElementDesc(i._semanticName, i._semanticIndex, Metal::NativeFormat::Enum(i._nativeFormat), 0, i._alignedByteOffset));
		_material = TechniqueMaterial(
			std::make_pair(AsPointer(eles.cbegin()), eles.size()),
			{ ObjectCB::LocalTransform, ObjectCB::BasicMaterialConstants }, ParameterBox());

		_depVal = std::make_shared<::Assets::DependencyValidation>();
		::Assets::RegisterFileDependency(_depVal, filename);
	}

	SimpleModel::~SimpleModel() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    using EntityInterface::RetainedEntities;
    using EntityInterface::RetainedEntity;

    class VisGeoBox
    {
    public:
        class Desc {};

        Metal::VertexBuffer     _cubeVB;
        unsigned                _cubeVBCount;
        unsigned                _cubeVBStride;
        TechniqueMaterial       _material;
        TechniqueMaterial       _materialP;

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const   { return _depVal; }
        VisGeoBox(const Desc&);
        ~VisGeoBox();
    protected:
        std::shared_ptr<::Assets::DependencyValidation> _depVal;
    };
    
    VisGeoBox::VisGeoBox(const Desc&)
    : _material(
        ToolsRig::Vertex3D_InputLayout, 
        { ObjectCB::LocalTransform, ObjectCB::BasicMaterialConstants },
        ParameterBox())
    , _materialP(
        Metal::GlobalInputLayouts::P,
        { ObjectCB::LocalTransform, ObjectCB::BasicMaterialConstants },
        ParameterBox())
    {
        auto cubeVertices = ToolsRig::BuildCube();
        _cubeVBCount = (unsigned)cubeVertices.size();
        _cubeVBStride = (unsigned)sizeof(decltype(cubeVertices)::value_type);
        _cubeVB = Metal::VertexBuffer(AsPointer(cubeVertices.cbegin()), _cubeVBCount * _cubeVBStride);
        _depVal = std::make_shared<::Assets::DependencyValidation>();
    }

    VisGeoBox::~VisGeoBox() {}

    static Float4x4 GetTransform(const RetainedEntity& obj)
    {
        auto xform = obj._properties.GetParameter<Float4x4>(Parameters::Transform);
        if (xform.first) return Transpose(xform.second);

        auto transl = obj._properties.GetParameter<Float3>(Parameters::Translation);
        if (transl.first) {
            return AsFloat4x4(transl.second);
        }
        return Identity<Float4x4>();
    }

    static bool GetShowMarker(const RetainedEntity& obj)
    {
        return obj._properties.GetParameter(Parameters::ShowMarker, true);
    }

    static void DrawObject(
        Metal::DeviceContext& devContext, ParsingContext& parserContext, const VisGeoBox& visBox,
        const TechniqueMaterial::Variation& shader, Metal::ConstantBufferPacket localTransform,
		unsigned techniqueIndex, const ::Assets::ResChar technique[], const ParameterBox& matParams)
    {
		
		TRY
		{
			auto& asset = ::Assets::GetAssetDep<SimpleModel>("game/model/simple/spherestandin.dae");
			asset.Render(devContext, parserContext, localTransform, techniqueIndex, technique, matParams);
			return;
		} 
		CATCH(const ::Assets::Exceptions::AssetException&) {}
		CATCH_END

		if (shader._shader._shaderProgram) {
			shader._shader.Apply(devContext, parserContext, { localTransform, shader._cbLayout->BuildCBDataAsPkt(matParams) } );
			devContext.Bind(MakeResourceList(visBox._cubeVB), visBox._cubeVBStride, 0);
			devContext.Draw(visBox._cubeVBCount);
		}
    }

    static void DrawTriMeshMarker(
        Metal::DeviceContext& devContext,
        ParsingContext& parserContext,
        const VisGeoBox& visBox,
        const TechniqueMaterial::Variation& shader, const RetainedEntity& obj,
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
            ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::UInt32, indexListType._arrayCount));
        if (!success) return;

        const auto& chld = obj._children;
        if (!chld.size()) return;

        auto vbData = std::make_unique<Float3[]>(chld.size());
        for (size_t c=0; c<chld.size(); ++c) {
            const auto* e = objs.GetEntity(obj._doc, chld[c]);
            if (e) {
                vbData[c] = ExtractTranslation(GetTransform(*e));
            } else {
                vbData[c] = Zero<Float3>();
            }
        }
        
        shader._shader.Apply(devContext, parserContext,
            {
                MakeLocalTransformPacket(
                    GetTransform(obj),
                    ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld)),
                shader._cbLayout->BuildCBDataAsPkt(ParameterBox())
            });

        devContext.Bind(Techniques::CommonResources()._blendAdditive);
        devContext.Bind(Techniques::CommonResources()._dssReadOnly);
        devContext.Bind(Techniques::CommonResources()._cullDisable);
        
        Metal::VertexBuffer vb(vbData.get(), sizeof(Float3)*chld.size());
        Metal::IndexBuffer ib(ibData.get(), sizeof(unsigned)*indexListType._arrayCount);

        devContext.Bind(MakeResourceList(vb), sizeof(Float3), 0);
        devContext.Bind(ib, Metal::NativeFormat::R32_UINT);
        devContext.Bind(Metal::Topology::TriangleList);
        devContext.DrawIndexed(indexListType._arrayCount);
    }

    void ObjectPlaceholders::Render(
        Metal::DeviceContext& metalContext, 
        ParsingContext& parserContext,
        unsigned techniqueIndex)
    {
        auto& visBox = FindCachedBoxDep<VisGeoBox>(VisGeoBox::Desc());

		if (Tweakable("DrawMarkers", true)) {
			auto* baseTechnique = "game/xleres/techniques/illum.tech";
			auto shader = visBox._material.FindVariation(parserContext, techniqueIndex, baseTechnique);
			for (auto a=_cubeAnnotations.cbegin(); a!=_cubeAnnotations.cend(); ++a) {
				auto objects = _objects->FindEntitiesOfType(a->_typeId);
				for (const auto&o:objects) {
					if (!o->_properties.GetParameter(Parameters::Visible, true) || !GetShowMarker(*o)) continue;

					auto localTransform = MakeLocalTransformPacket(
						GetTransform(*o), ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld));

					ParameterBox matParams;

					// bit of a hack -- copy from the "Diffuse" parameter to the "MaterialDiffuse" shader constant
					auto diffuseName = ParameterBox::MakeParameterNameHash(u("Diffuse"));
					unsigned c = o->_properties.GetParameter(diffuseName, ~0u);
					matParams.SetParameter(u("MaterialDiffuse"), Float3(((c>>16)&0xff)/255.f, ((c>>8)&0xff)/255.f, ((c>>0)&0xff)/255.f));

					DrawObject(metalContext, parserContext, visBox, shader, localTransform, techniqueIndex, baseTechnique, matParams);
				}
			}
		}

        auto shaderP = visBox._materialP.FindVariation(parserContext, techniqueIndex, "game/xleres/techniques/meshmarker.tech");
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

    auto ObjectPlaceholders::IntersectionTester::FirstRayIntersection(
        const SceneEngine::IntersectionTestContext& context,
        std::pair<Float3, Float3> worldSpaceRay) const -> Result
    {
        using namespace SceneEngine;

        for (auto a=_placeHolders->_cubeAnnotations.cbegin(); a!=_placeHolders->_cubeAnnotations.cend(); ++a) {
            auto objects = _placeHolders->_objects->FindEntitiesOfType(a->_typeId);
            for (auto o=objects.cbegin(); o!=objects.cend(); ++o) {

                auto transform = GetTransform(**o);
                if (RayVsAABB(worldSpaceRay, transform, Float3(-1.f, -1.f, -1.f), Float3(1.f, 1.f, 1.f))) {
                    Result result;
                    result._type = IntersectionTestScene::Type::Extra;
                    result._worldSpaceCollision = worldSpaceRay.first;
                    result._distance = 0.f;
                    result._objectGuid = std::make_pair((*o)->_doc, (*o)->_id);
                    result._drawCallIndex = 0;
                    result._materialGuid =0;
                    return result;
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

    ObjectPlaceholders::ObjectPlaceholders(std::shared_ptr<RetainedEntities> objects)
    : _objects(std::move(objects))
    {}

    ObjectPlaceholders::~ObjectPlaceholders() {}

}

