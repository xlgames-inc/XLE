// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialVisualisation.h"
#include "VisualisationUtils.h"		// for IVisContent
#include "VisualisationGeo.h"

#include "../../SceneEngine/SceneParser.h"

#include "../../RenderCore/Techniques/CommonBindings.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/Assets/AssetUtils.h"

#include "../../RenderCore/UniformsStream.h"
#include "../../RenderCore/BufferView.h"
#include "../../RenderCore/IThreadContext.h"

#include "../../Math/Transformations.h"
#include "../../Assets/Assets.h"
#include "../../ConsoleRig/ResourceBox.h"

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
			Metal::DeviceContext& metalContext,
			Techniques::ParsingContext& parserContext,
			const MaterialSceneParserDrawable& drawable, const Metal::BoundUniforms& boundUniforms,
			const Metal::ShaderProgram&)
		{
			if (boundUniforms._boundUniformBufferSlots[3] != 0) {
				ConstantBufferView cbvs[] = {
					Techniques::MakeLocalTransformPacket(
						Identity<Float4x4>(), 
						ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld))};
				boundUniforms.Apply(metalContext, 3, UniformsStream{MakeIteratorRange(cbvs)});
			}

				// disable blending to avoid problem when rendering single component stuff 
                //  (ie, nodes that output "float", not "float4")
            metalContext.Bind(Techniques::CommonResources()._blendOpaque);

			assert(!drawable._geo->_ib);
			metalContext.Bind(drawable._topology);
			metalContext.Draw(drawable._vertexCount);
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
				drawable._material = _material.get();
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
				drawable._material = _material.get();
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

        MaterialVisualizationScene(const MaterialVisSettings& settings)
        : _settings(settings)
		{
			_depVal = std::make_shared<::Assets::DependencyValidation>();
			_material = std::make_shared<RenderCore::Techniques::Material>();
			XlCopyString(_material->_techniqueConfig, "xleres/techniques/illum.tech");
		}

		const ::Assets::DepValPtr& GetDependencyValidation() { return _depVal; }

    protected:
        MaterialVisSettings  _settings;
		std::shared_ptr<RenderCore::Techniques::Material> _material;
		::Assets::DepValPtr _depVal;
    };

	::Assets::FuturePtr<SceneEngine::IScene> MakeScene(const MaterialVisSettings& visObject)
	{
		auto result = std::make_shared<::Assets::AssetFuture<MaterialVisualizationScene>>("MaterialVisualization");
		::Assets::AutoConstructToFuture(*result, visObject);
		return std::reinterpret_pointer_cast<::Assets::AssetFuture<SceneEngine::IScene>>(result);
	}

}

