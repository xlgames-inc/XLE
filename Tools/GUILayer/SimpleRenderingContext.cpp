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
#include "DelayedDeleteQueue.h"
#include "ExportedNativeTypes.h"
#include "../ToolsRig/ManipulatorsUtil.h"
#include "../ToolsRig/ManipulatorsRender.h"
#include "../ToolsRig/HighlightEffects.h"
#include "../../PlatformRig/BasicSceneParser.h"     // (PlatformRig::EnvironmentSettings destructor)
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/TechniqueMaterial.h"
#include "../../RenderCore/Techniques/CommonBindings.h"
#include "../../RenderCore/Techniques/PredefinedCBLayout.h"
#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/Metal/Buffer.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../Math/Vector.h"
#include "../../Math/Matrix.h"
#include "../../Utility/IteratorUtils.h"
#include "../../Core/Types.h"
#include <vector>

#include "../../Assets/Assets.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../SceneEngine/LightingParserContext.h"

namespace GUILayer
{
    class VertexFormatRecord
    {
    public:
        RenderCore::Metal::InputLayout _inputLayout;
        ParameterBox _geoParams;
        unsigned _vertexStride;
    };

    class SavedRenderResourcesPimpl
    {
    public:
        std::vector<std::pair<uint64, RenderCore::Metal::VertexBuffer>> _vertexBuffers;
        std::vector<std::pair<uint64, unsigned>> _vbFormat;
        std::vector<std::pair<uint64, RenderCore::Metal::IndexBuffer>> _indexBuffers;
        uint64 _nextBufferID;
        RenderCore::Metal::ObjectFactory _objectFactory;

        VertexFormatRecord _vfRecord[8];
        SavedRenderResourcesPimpl(RenderCore::IDevice& device);
    };

    SavedRenderResourcesPimpl::SavedRenderResourcesPimpl(RenderCore::IDevice& device)
    : _nextBufferID(1)
    , _objectFactory(&device) 
    {
            //  These are the vertex formats defined by the Sony editor
        using namespace RenderCore::Metal;
        _vfRecord[0]._inputLayout = GlobalInputLayouts::P;
        _vfRecord[1]._inputLayout = GlobalInputLayouts::PC;
        _vfRecord[2]._inputLayout = GlobalInputLayouts::PN;
        _vfRecord[3]._inputLayout = GlobalInputLayouts::PT;
        _vfRecord[4]._inputLayout = std::make_pair((const InputElementDesc*)nullptr, 0);
        _vfRecord[5]._inputLayout = GlobalInputLayouts::PNT;
        _vfRecord[6]._inputLayout = GlobalInputLayouts::PNTT;
        _vfRecord[7]._inputLayout = std::make_pair((const InputElementDesc*)nullptr, 0);

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

        const RenderCore::Metal::VertexBuffer* GetVertexBuffer(uint64 id);
        const RenderCore::Metal::IndexBuffer* GetIndexBuffer(uint64 id);
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
        const VertexFormatRecord& vf)
    {
        CATCH_ASSETS_BEGIN
            using namespace RenderCore;
            const auto techniqueIndex = 0u;

            static ParameterBox materialParameters({std::make_pair((const utf8*)"MAT_SKIP_LIGHTING_SCALE", "1")});
            Techniques::TechniqueMaterial material(
                vf._inputLayout,
                {Techniques::ObjectCB::LocalTransform, Techniques::ObjectCB::BasicMaterialConstants},
                materialParameters);

            auto variation = material.FindVariation(parsingContext, techniqueIndex, "game/xleres/techniques/illum.tech");
            if (variation._shader._shaderProgram == nullptr) {
                return false; // we can't render because we couldn't resolve a good shader variation
            }

            ParameterBox matConstants;
            matConstants.SetParameter((const utf8*)"MaterialDiffuse", Float3(color[0], color[1], color[2]));

            variation._shader.Apply(
                devContext, parsingContext,
                {
                    Techniques::MakeLocalTransformPacket(Transpose(*(Float4x4*)xform), Float3(0.f, 0.f, 0.f)),
                    variation._cbLayout->BuildCBDataAsPkt(matConstants)
                });
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

        if (SetupState(*_devContext.get(), *_parsingContext, color, xform, *vfFormat)) {
            _devContext->Bind((RenderCore::Metal::Topology::Enum)primitiveType);
            _devContext->Bind(RenderCore::MakeResourceList(*vbuffer), vfFormat->_vertexStride, 0);
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
            
        if (SetupState(*_devContext.get(), *_parsingContext, color, xform, *vfFormat)) {
            auto& devContext = *_devContext.get();
            _devContext->Bind((RenderCore::Metal::Topology::Enum)primitiveType);
            devContext.Bind(RenderCore::MakeResourceList(*vbuffer), vfFormat->_vertexStride, 0);
            devContext.Bind(*ibuffer, RenderCore::Metal::NativeFormat::R32_UINT);   // Sony editor always uses 32 bit indices
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

////////////////////////////////////////////////////////////////////////////////////////////////?//

    uint64  RetainedRenderResources::CreateVertexBuffer(void* data, size_t size, unsigned format)
    {
        RenderCore::Metal::VertexBuffer newBuffer(_pimpl->_objectFactory, data, size);
        _pimpl->_vertexBuffers.push_back(std::make_pair(_pimpl->_nextBufferID, std::move(newBuffer)));
        _pimpl->_vbFormat.push_back(std::make_pair(_pimpl->_nextBufferID, format));
        return _pimpl->_nextBufferID++;
    }

    uint64  RetainedRenderResources::CreateIndexBuffer(void* data, size_t size)
    {
        RenderCore::Metal::IndexBuffer newBuffer(_pimpl->_objectFactory, data, size);
        _pimpl->_indexBuffers.push_back(std::make_pair(_pimpl->_nextBufferID, std::move(newBuffer)));
        return _pimpl->_nextBufferID++;
    }

    const RenderCore::Metal::VertexBuffer* RetainedRenderResources::GetVertexBuffer(uint64 id)
    {
        for (auto i = _pimpl->_vertexBuffers.cbegin(); i != _pimpl->_vertexBuffers.cend(); ++i)
            if (i->first == id) return &i->second;
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

    const RenderCore::Metal::IndexBuffer* RetainedRenderResources::GetIndexBuffer(uint64 id)
    {
        for (auto i = _pimpl->_indexBuffers.cbegin(); i != _pimpl->_indexBuffers.cend(); ++i)
            if (i->first == id) return &i->second;
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
    : _retainedRes(savedRes), _parsingContext((SceneEngine::LightingParserContext*)parsingContext)
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
            if (highlight == nullptr) {
                CATCH_ASSETS_BEGIN
                    auto& metalContext = context->GetDevContext();
                    ToolsRig::BinaryHighlight highlight(metalContext);
                    ToolsRig::Placements_RenderFiltered(
                        metalContext, 
                        context->GetParsingContext(), 
                        placements->GetNative(), renderer->GetNative(),
                        nullptr, nullptr, materialGuid);

                    const Float3 highlightCol(.75f, .8f, 0.4f);
                    const unsigned overlayCol = 2;

                    highlight.FinishWithOutlineAndOverlay(metalContext, highlightCol, overlayCol);
                CATCH_ASSETS_END(context->GetParsingContext())

            } else {
                if (highlight->IsEmpty()) return;

                ToolsRig::Placements_RenderHighlight(
                    context->GetThreadContext(), context->GetParsingContext(), 
                    placements->GetNative(), renderer->GetNative(),
                    (const SceneEngine::PlacementGUID*)AsPointer(highlight->_nativePlacements->cbegin()),
                    (const SceneEngine::PlacementGUID*)AsPointer(highlight->_nativePlacements->cend()),
                    materialGuid);
            }
        }

        static void ClearDepthBuffer(SimpleRenderingContext^ context)
        {
            auto& metalContext = context->GetDevContext();
            RenderCore::Metal::DepthStencilView dsv(metalContext);
            metalContext.Clear(dsv, 1.f, 0u);
        }
    };

}

