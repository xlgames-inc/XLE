// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SimpleRenderingContext.h"
#include "EngineDevice.h"
#include "NativeEngineDevice.h"
#include "EditorInterfaceUtils.h"
#include "GUILayerUtil.h"
#include "LevelEditorScene.h"
#include "MathLayer.h"
#include "ExportedNativeTypes.h"
#include "../ToolsRig/ManipulatorsUtil.h"
#include "../ToolsRig/ManipulatorsRender.h"
#include "../../PlatformRig/BasicSceneParser.h"     // (PlatformRig::EnvironmentSettings destructor)
#include "../../SceneEngine/PlacementsManager.h"
#include "../../FixedFunctionModel/ShaderVariationSet.h"
#include "../../RenderOverlays/HighlightEffects.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/CommonBindings.h"
#include "../../RenderCore/Assets/PredefinedCBLayout.h"
#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/Metal/Buffer.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/Format.h"
#include "../../Math/Vector.h"
#include "../../Math/Matrix.h"
#include "../../Utility/IteratorUtils.h"
#include "../../Core/Types.h"
#include <vector>

#include "../../RenderCore/Techniques/Techniques.h"

namespace GUILayer
{
    class VertexFormatRecord
    {
    public:
        RenderCore::InputLayout _inputLayout;
        ParameterBox _geoParams;
        unsigned _vertexStride;
    };

    class SavedRenderResourcesPimpl
    {
    public:
        std::vector<std::pair<uint64, RenderCore::IResourcePtr>> _vertexBuffers;
        std::vector<std::pair<uint64, unsigned>> _vbFormat;
        std::vector<std::pair<uint64, RenderCore::IResourcePtr>> _indexBuffers;
        uint64 _nextBufferID;
        RenderCore::IDevice* _device;

        VertexFormatRecord _vfRecord[8];
        SavedRenderResourcesPimpl(RenderCore::IDevice& device);
    };

    SavedRenderResourcesPimpl::SavedRenderResourcesPimpl(RenderCore::IDevice& device)
    : _nextBufferID(1)
    , _device(&device)
    {
            //  These are the vertex formats defined by the Sony editor
        using namespace RenderCore;
        _vfRecord[0]._inputLayout = GlobalInputLayouts::P;
        _vfRecord[1]._inputLayout = GlobalInputLayouts::PC;
        _vfRecord[2]._inputLayout = GlobalInputLayouts::PN;
        _vfRecord[3]._inputLayout = GlobalInputLayouts::PT;
		_vfRecord[4]._inputLayout = {};
        _vfRecord[5]._inputLayout = GlobalInputLayouts::PNT;
        _vfRecord[6]._inputLayout = GlobalInputLayouts::PNTT;
		_vfRecord[7]._inputLayout = {};

        _vfRecord[1]._geoParams = ParameterBox({std::make_pair((const utf8*)"GEO_HAS_COLOUR", "1")});
        _vfRecord[2]._geoParams = ParameterBox({std::make_pair((const utf8*)"GEO_HAS_NORMAL", "1")});
        _vfRecord[3]._geoParams = ParameterBox({std::make_pair((const utf8*)"GEO_HAS_TEXCOORD", "1")});
        _vfRecord[5]._geoParams = ParameterBox({std::make_pair((const utf8*)"GEO_HAS_NORMAL", "1"), std::make_pair((const utf8*)"GEO_HAS_TEXCOORD", "1")});
        _vfRecord[6]._geoParams = ParameterBox({std::make_pair((const utf8*)"GEO_HAS_NORMAL", "1"), std::make_pair((const utf8*)"GEO_HAS_TEXCOORD", "1"), std::make_pair((const utf8*)"GEO_HAS_TANGENT_FRAME", "1"), std::make_pair((const utf8*)"GEO_HAS_BITANGENT", "1")});

        _vfRecord[0]._vertexStride = 3*4;
        _vfRecord[1]._vertexStride = 3*4 + 4;
        _vfRecord[2]._vertexStride = 3*4 + 3*4;
        _vfRecord[3]._vertexStride = 3*4 + 2*4;
        _vfRecord[4]._vertexStride = 3*4 + 2*4 + 4;
        _vfRecord[5]._vertexStride = 3*4 + 3*4 + 2*4;
        _vfRecord[6]._vertexStride = 3*4 + 3*4 + 2*4 + 4*4;
        _vfRecord[7]._vertexStride = 2*4;
    }

    /// <summary>Create and maintain rendering resources for SimpleRenderingContext</summary>
    /// Create & maintain vertex and index buffers. Intended for use when linking to
    /// C# GUI apps.
    public ref class RetainedRenderResources
    {
    public:
        uint64  CreateVertexBuffer(void* data, size_t size, unsigned format);
        uint64  CreateIndexBuffer(void* data, size_t size);
        bool    DeleteBuffer(uint64 id);

        RenderCore::IResource* GetVertexBuffer(uint64 id);
        RenderCore::IResource* GetIndexBuffer(uint64 id);
        const VertexFormatRecord* GetVertexBufferFormat(uint64 id);

        RetainedRenderResources(EngineDevice^ engineDevice);
        ~RetainedRenderResources();
        !RetainedRenderResources();
    protected:
        clix::auto_ptr<SavedRenderResourcesPimpl> _pimpl;
    };

    static bool SetupState(
        RenderCore::Metal::DeviceContext& devContext, 
        RenderCore::Techniques::ParsingContext& parsingContext,
        const float color[], const float xform[],
        const VertexFormatRecord& vf,
		RenderCore::IResource* vb,
		RenderCore::IResource* ib = nullptr)
    {
        CATCH_ASSETS_BEGIN
            using namespace RenderCore;
            const auto techniqueIndex = 0u;

            FixedFunctionModel::ShaderVariationSet material(
                vf._inputLayout,
                {Techniques::ObjectCB::LocalTransform, Techniques::ObjectCB::BasicMaterialConstants},
				{});

            auto variation = material.FindVariation(parsingContext, techniqueIndex, "unlit.tech");
            if (variation._shader._shaderProgram == nullptr) {
                return false; // we can't render because we couldn't resolve a good shader variation
            }

			if (ib) {
				auto* res = (RenderCore::Metal::Resource*)ib->QueryInterface(typeid(RenderCore::Metal::Resource).hash_code());
				assert(res);
				if (res)
					devContext.Bind(*res, RenderCore::Format::R32_UINT);
			}

            ParameterBox matConstants;
            matConstants.SetParameter((const utf8*)"MaterialDiffuse", Float3(color[0], color[1], color[2]));

			ConstantBufferView cbvs[] = {
                Techniques::MakeLocalTransformPacket(Transpose(*(Float4x4*)xform), Float3(0.f, 0.f, 0.f)),
                variation._cbLayout->BuildCBDataAsPkt(matConstants) };
			variation._shader._boundUniforms->Apply(devContext, 1, { MakeIteratorRange(cbvs) });
            variation._shader.Apply(devContext, parsingContext, { vb });
            return true;
        CATCH_ASSETS_END(parsingContext)
        return false;
    }

    void SimpleRenderingContext::DrawPrimitive(
        unsigned primitiveType,
        uint64 vb,
        unsigned startVertex,
        unsigned vertexCount,
        const float color[], const float xform[])
    {
        // We need to bind the technique and technique interface
        //      (including vertex input format)
        // "color" can be passed as a Float4 as the material parameter "MaterialDiffuse"
        // then we just bind the vertex buffer and call draw

        auto* vbuffer = _retainedRes->GetVertexBuffer(vb);
        if (!vbuffer) return;

        auto* vfFormat = _retainedRes->GetVertexBufferFormat(vb);
        if (!vfFormat) return;

        if (SetupState(*_devContext.get(), *_parsingContext, color, xform, *vfFormat, vbuffer)) {
            _devContext->Bind((RenderCore::Topology)primitiveType);
            _devContext->Draw(vertexCount, startVertex);
        }
    }

    void SimpleRenderingContext::DrawIndexedPrimitive(
        unsigned primitiveType,
        uint64 vb, uint64 ib,
        unsigned startIndex,
        unsigned indexCount,
        unsigned startVertex,
        const float color[], const float xform[]) 
    {
        auto* ibuffer = _retainedRes->GetIndexBuffer(ib);
        auto* vbuffer = _retainedRes->GetVertexBuffer(vb);
        if (!ibuffer || !vbuffer) return;

        auto* vfFormat = _retainedRes->GetVertexBufferFormat(vb);
        if (!vfFormat) return;
            
        if (SetupState(*_devContext.get(), *_parsingContext, color, xform, *vfFormat, vbuffer, ibuffer)) {
            auto& devContext = *_devContext.get();
            _devContext->Bind((RenderCore::Topology)primitiveType);
            devContext.DrawIndexed(indexCount, startIndex, startVertex);
        }
    }

    void SimpleRenderingContext::InitState(bool depthTest, bool depthWrite)
    {
        if (depthWrite) _devContext->Bind(RenderCore::Techniques::CommonResources()._dssReadWrite);
        else if (depthTest) _devContext->Bind(RenderCore::Techniques::CommonResources()._dssReadOnly);
        else _devContext->Bind(RenderCore::Techniques::CommonResources()._dssDisable);
                
        _devContext->Bind(RenderCore::Techniques::CommonResources()._blendStraightAlpha);
        _devContext->Bind(RenderCore::Techniques::CommonResources()._defaultRasterizer);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	static RenderCore::IResourcePtr CreateStaticVertexBuffer(RenderCore::IDevice& device, IteratorRange<const void*> data)
	{
		using namespace RenderCore;
		return device.CreateResource(
			CreateDesc(
				BindFlag::VertexBuffer, 0, GPUAccess::Read,
				LinearBufferDesc::Create(unsigned(data.size())),
				"simplecontext_vb"),
			[data](SubResourceId subres) {
				assert(subres._arrayLayer == 0 && subres._mip == 0);
				return SubResourceInitData{ data };
			});
	}

	static RenderCore::IResourcePtr CreateStaticIndexBuffer(RenderCore::IDevice& device, IteratorRange<const void*> data)
	{
		using namespace RenderCore;
		return device.CreateResource(
			CreateDesc(
				BindFlag::IndexBuffer, 0, GPUAccess::Read,
				LinearBufferDesc::Create(unsigned(data.size())),
				"simplecontext_ib"),
			[data](SubResourceId subres) {
				assert(subres._arrayLayer == 0 && subres._mip == 0);
				return SubResourceInitData{ data };
			});
	}

    uint64  RetainedRenderResources::CreateVertexBuffer(void* data, size_t size, unsigned format)
    {
		auto newBuffer = CreateStaticVertexBuffer(*_pimpl->_device, MakeIteratorRange(data, PtrAdd(data, size)));
        _pimpl->_vertexBuffers.push_back(std::make_pair(_pimpl->_nextBufferID, std::move(newBuffer)));
        _pimpl->_vbFormat.push_back(std::make_pair(_pimpl->_nextBufferID, format));
        return _pimpl->_nextBufferID++;
    }

    uint64  RetainedRenderResources::CreateIndexBuffer(void* data, size_t size)
    {
		auto newBuffer = CreateStaticIndexBuffer(*_pimpl->_device, MakeIteratorRange(data, PtrAdd(data, size)));
        _pimpl->_indexBuffers.push_back(std::make_pair(_pimpl->_nextBufferID, std::move(newBuffer)));
        return _pimpl->_nextBufferID++;
    }

    RenderCore::IResource* RetainedRenderResources::GetVertexBuffer(uint64 id)
    {
        for (auto i = _pimpl->_vertexBuffers.cbegin(); i != _pimpl->_vertexBuffers.cend(); ++i)
            if (i->first == id) return i->second.get();
        return nullptr;
    }

    const VertexFormatRecord* RetainedRenderResources::GetVertexBufferFormat(uint64 id)
    {
        for (auto i = _pimpl->_vbFormat.cbegin(); i != _pimpl->_vbFormat.cend(); ++i)
            if (i->first == id && i->second < dimof(_pimpl->_vfRecord)) {
                return &_pimpl->_vfRecord[i->second];
            }
        return nullptr;
    }

    RenderCore::IResource* RetainedRenderResources::GetIndexBuffer(uint64 id)
    {
        for (auto i = _pimpl->_indexBuffers.cbegin(); i != _pimpl->_indexBuffers.cend(); ++i)
            if (i->first == id) return i->second.get();
        return nullptr;
    }

    bool RetainedRenderResources::DeleteBuffer(uint64 id)
    {
        auto vi = LowerBound(_pimpl->_vertexBuffers, id);
        if (vi != _pimpl->_vertexBuffers.end() && vi->first == id) {
            _pimpl->_vertexBuffers.erase(vi);
            return true;
        }

        auto ii = LowerBound(_pimpl->_indexBuffers, id);
        if (ii != _pimpl->_indexBuffers.end() && ii->first == id) {
            _pimpl->_indexBuffers.erase(ii);
            return true;
        }

        return false;
    }

    RetainedRenderResources::RetainedRenderResources(EngineDevice^ engineDevice) 
    {
        _pimpl.reset(
            new SavedRenderResourcesPimpl(
                *engineDevice->GetNative().GetRenderDevice()));
    }

    RetainedRenderResources::~RetainedRenderResources() { _pimpl.reset(); }
    RetainedRenderResources::!RetainedRenderResources() { _pimpl.reset(); }

////////////////////////////////////////////////////////////////////////////////////////////////?//

    SimpleRenderingContext::SimpleRenderingContext(
        RenderCore::IThreadContext* threadContext,
        RetainedRenderResources^ savedRes, 
        void* parsingContext)
    : _retainedRes(savedRes), _parsingContext((RenderCore::Techniques::ParsingContext*)parsingContext)
    , _threadContext(threadContext)
    {
        _devContext = RenderCore::Metal::DeviceContext::Get(*threadContext);
    }
    SimpleRenderingContext::~SimpleRenderingContext() { _devContext.reset(); }
    SimpleRenderingContext::!SimpleRenderingContext() { _devContext.reset(); }

////////////////////////////////////////////////////////////////////////////////////////////////?//

    public ref class RenderingUtil
    {
    public:
        static void RenderCylinderHighlight(
            SimpleRenderingContext^ renderingContext,
            Vector3 centre, float radius)
        {
            ToolsRig::RenderCylinderHighlight(
                renderingContext->GetThreadContext(), renderingContext->GetParsingContext(),
                AsFloat3(centre), radius);
        }

        static void RenderHighlight(
            SimpleRenderingContext^ context,
            PlacementsEditorWrapper^ placements,
            PlacementsRendererWrapper^ renderer,
            ObjectSet^ highlight, uint64 materialGuid)
        {
            auto& threadContext = context->GetThreadContext();
            if (highlight == nullptr) {
                CATCH_ASSETS_BEGIN
                    ToolsRig::BinaryHighlight highlightRenderer(threadContext, context->GetParsingContext().GetFrameBufferPool(), context->GetParsingContext().GetNamedResources());
                    ToolsRig::Placements_RenderFiltered(
                        threadContext, context->GetParsingContext(), 
                        RenderCore::Techniques::TechniqueIndex::Forward,
                        renderer->GetNative(), placements->GetNative().GetCellSet(), 
                        nullptr, nullptr, materialGuid);

                    const Float3 highlightCol(.75f, .8f, 0.4f);
                    const unsigned overlayCol = 2;

					highlightRenderer.FinishWithOutlineAndOverlay(threadContext, highlightCol, overlayCol);
                CATCH_ASSETS_END(context->GetParsingContext())

            } else {
                if (highlight->IsEmpty()) return;

                ToolsRig::Placements_RenderHighlight(
                    threadContext, context->GetParsingContext(), 
                    renderer->GetNative(), placements->GetNative().GetCellSet(),
                    (const SceneEngine::PlacementGUID*)AsPointer(highlight->_nativePlacements->cbegin()),
                    (const SceneEngine::PlacementGUID*)AsPointer(highlight->_nativePlacements->cend()),
                    materialGuid);
            }
        }

        static void ClearDepthBuffer(SimpleRenderingContext^ context)
        {
            auto& metalContext = context->GetDevContext();
            RenderCore::Metal::DepthStencilView dsv(metalContext);
            using ClearFilter = RenderCore::Metal::DeviceContext::ClearFilter;
            metalContext.Clear(dsv, ClearFilter::Depth|ClearFilter::Stencil, 1.f, 0u);
        }
    };

}

