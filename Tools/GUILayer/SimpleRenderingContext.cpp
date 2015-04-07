// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EngineDevice.h"
#include "NativeEngineDevice.h"
#include "ExportedNativeTypes.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Metal/Buffer.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../Math/Vector.h"
#include "../../Math/Matrix.h"
#include "../../Utility/IteratorUtils.h"
#include "../../Core/Types.h"
#include "../../Tools/GUILayer/CLIXAutoPtr.h"
#include <vector>

#include "../../Assets/Assets.h"
#include "../../RenderCore/Techniques/Techniques.h"

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

        _vfRecord[1]._geoParams = ParameterBox({std::make_pair("GEO_HAS_COLOUR", "1")});
        _vfRecord[2]._geoParams = ParameterBox({std::make_pair("GEO_HAS_NORMAL", "1")});
        _vfRecord[3]._geoParams = ParameterBox({std::make_pair("GEO_HAS_TEXCOORD", "1")});
        _vfRecord[5]._geoParams = ParameterBox({std::make_pair("GEO_HAS_NORMAL", "1"), std::make_pair("GEO_HAS_TEXCOORD", "1")});
        _vfRecord[6]._geoParams = ParameterBox({std::make_pair("GEO_HAS_NORMAL", "1"), std::make_pair("GEO_HAS_TEXCOORD", "1"), std::make_pair("GEO_HAS_TANGENT_FRAME", "1")});

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
    public ref class SavedRenderResources
    {
    public:
        uint64  CreateVertexBuffer(void* data, size_t size, unsigned format);
        uint64  CreateIndexBuffer(void* data, size_t size);
        bool    DeleteBuffer(uint64 id);

        const RenderCore::Metal::VertexBuffer* GetVertexBuffer(uint64 id);
        const RenderCore::Metal::IndexBuffer* GetIndexBuffer(uint64 id);
        const VertexFormatRecord* GetVertexBufferFormat(uint64 id);

        SavedRenderResources(EngineDevice^ engineDevice);
        ~SavedRenderResources();
        !SavedRenderResources();
    protected:
        clix::auto_ptr<SavedRenderResourcesPimpl> _pimpl;
    };

    class BasicMaterialConstants
    {
    public:
            // fixed set of material parameters currently.
        Float3 _materialDiffuse;    float _opacity;
        Float3 _materialSpecular;   float _alphaThreshold;
    };

    static bool SetupState(
        RenderCore::Metal::DeviceContext& devContext, 
        RenderCore::Techniques::ParsingContext& parsingContext,
        const float color[], const float xform[],
        const VertexFormatRecord& vf)
    {
        TRY
        {
            using namespace RenderCore;
            const auto techniqueIndex = 0u;

            Techniques::TechniqueInterface techniqueInterface(vf._inputLayout);
            Techniques::TechniqueContext::BindGlobalUniforms(techniqueInterface);
            techniqueInterface.BindConstantBuffer(Hash64("LocalTransform"), 0, 1);
            techniqueInterface.BindConstantBuffer(Hash64("BasicMaterialConstants"), 1, 1);

            static ParameterBox materialParameters({std::make_pair("MAT_SKIP_LIGHTING_SCALE", "1")});
            const ParameterBox* state[] = {
                &vf._geoParams, &parsingContext.GetTechniqueContext()._globalEnvironmentState,
                &parsingContext.GetTechniqueContext()._runtimeState, &materialParameters
            };

            auto& shaderType = ::Assets::GetAssetDep<Techniques::ShaderType>("game/xleres/illum.txt");
            auto variation = shaderType.FindVariation(techniqueIndex, state, techniqueInterface);
            if (variation._shaderProgram == nullptr) {
                return false; // we can't render because we couldn't resolve a good shader variation
            }

            BasicMaterialConstants matConstants = 
            {
                Float3(color[0], color[1], color[2]), 1.f,
                Float3(0.f, 0.f, 0.f), 0.33f
            };

            RenderCore::Metal::ConstantBufferPacket cpkts[] = { 
                Techniques::MakeLocalTransformPacket(Transpose(*(Float4x4*)xform), Float3(0.f, 0.f, 0.f)),
                MakeSharedPkt(matConstants)
            };

            devContext.Bind(*variation._shaderProgram);
            devContext.Bind(*variation._boundLayout);
            variation._boundUniforms->Apply(devContext,
                parsingContext.GetGlobalUniformsStream(), 
                Metal::UniformsStream(cpkts, nullptr, dimof(cpkts)));
            return true;
        }
        CATCH (const ::Assets::Exceptions::InvalidResource&) {}
        CATCH (const ::Assets::Exceptions::PendingResource&) {}
        CATCH_END
        return false;
    }

    /// <summary>Context for simple rendering commands</summary>
    /// Some tools need to perform basic rendering commands: create a vertex buffer,
    /// set a technique, draw some polygons. 
    /// 
    /// For example, a manipulator might want to draw a 3D arrow or tube as part
    /// of a widget.
    ///
    /// This provides this kind of basic behaviour via the CLI interface so it
    /// can be used by C# (or other C++/CLI) code.
    public ref class SimpleRenderingContext
    {
    public:
        void DrawPrimitive(
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

            auto* vbuffer = _savedRes->GetVertexBuffer(vb);
            if (!vbuffer) return;

            auto* vfFormat = _savedRes->GetVertexBufferFormat(vb);
            if (!vfFormat) return;

            if (SetupState(*_devContext.get(), *_parsingContext, color, xform, *vfFormat)) {
                _devContext->Bind((RenderCore::Metal::Topology::Enum)primitiveType);
                _devContext->Bind(RenderCore::MakeResourceList(*vbuffer), vfFormat->_vertexStride, 0);
                _devContext->Draw(vertexCount, startVertex);
            }
        }

        void DrawIndexedPrimitive(
            unsigned primitiveType,
            uint64 vb, uint64 ib,
            unsigned startIndex,
            unsigned indexCount,
            unsigned startVertex,
            const float color[], const float xform[]) 
        {
            auto* ibuffer = _savedRes->GetIndexBuffer(ib);
            auto* vbuffer = _savedRes->GetVertexBuffer(vb);
            if (!ibuffer || !vbuffer) return;

            auto* vfFormat = _savedRes->GetVertexBufferFormat(vb);
            if (!vfFormat) return;
            
            if (SetupState(*_devContext.get(), *_parsingContext, color, xform, *vfFormat)) {
                auto& devContext = *_devContext.get();
                _devContext->Bind((RenderCore::Metal::Topology::Enum)primitiveType);
                devContext.Bind(RenderCore::MakeResourceList(*vbuffer), vfFormat->_vertexStride, 0);
                devContext.Bind(*ibuffer, RenderCore::Metal::NativeFormat::R32_UINT);   // Sony editor always uses 32 bit indices
                devContext.DrawIndexed(indexCount, startIndex, startVertex);
            }
        }

        SimpleRenderingContext(
            SavedRenderResources^ savedRes, 
            RenderCore::IThreadContext& threadContext,
            void* parsingContext);
        ~SimpleRenderingContext();
        !SimpleRenderingContext();
    protected:
        SavedRenderResources^ _savedRes;
        RenderCore::Techniques::ParsingContext* _parsingContext;
        clix::shared_ptr<RenderCore::Metal::DeviceContext> _devContext;
    };

////////////////////////////////////////////////////////////////////////////////////////////////?//

    uint64  SavedRenderResources::CreateVertexBuffer(void* data, size_t size, unsigned format)
    {
        RenderCore::Metal::VertexBuffer newBuffer(_pimpl->_objectFactory, data, size);
        _pimpl->_vertexBuffers.push_back(std::make_pair(_pimpl->_nextBufferID, std::move(newBuffer)));
        _pimpl->_vbFormat.push_back(std::make_pair(_pimpl->_nextBufferID, format));
        return _pimpl->_nextBufferID++;
    }

    uint64  SavedRenderResources::CreateIndexBuffer(void* data, size_t size)
    {
        RenderCore::Metal::IndexBuffer newBuffer(_pimpl->_objectFactory, data, size);
        _pimpl->_indexBuffers.push_back(std::make_pair(_pimpl->_nextBufferID, std::move(newBuffer)));
        return _pimpl->_nextBufferID++;
    }

    const RenderCore::Metal::VertexBuffer* SavedRenderResources::GetVertexBuffer(uint64 id)
    {
        for (auto i = _pimpl->_vertexBuffers.cbegin(); i != _pimpl->_vertexBuffers.cend(); ++i)
            if (i->first == id) return &i->second;
        return nullptr;
    }

    const VertexFormatRecord* SavedRenderResources::GetVertexBufferFormat(uint64 id)
    {
        for (auto i = _pimpl->_vbFormat.cbegin(); i != _pimpl->_vbFormat.cend(); ++i)
            if (i->first == id && i->second < dimof(_pimpl->_vfRecord)) {
                return &_pimpl->_vfRecord[i->second];
            }
        return nullptr;
    }

    const RenderCore::Metal::IndexBuffer* SavedRenderResources::GetIndexBuffer(uint64 id)
    {
        for (auto i = _pimpl->_indexBuffers.cbegin(); i != _pimpl->_indexBuffers.cend(); ++i)
            if (i->first == id) return &i->second;
        return nullptr;
    }

    bool SavedRenderResources::DeleteBuffer(uint64 id)
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

    SavedRenderResources::SavedRenderResources(EngineDevice^ engineDevice) 
    {
        _pimpl.reset(
            new SavedRenderResourcesPimpl(
                *engineDevice->GetNative().GetRenderDevice()));
    }

    SavedRenderResources::~SavedRenderResources() { _pimpl.reset(); }
    SavedRenderResources::!SavedRenderResources() { _pimpl.reset(); }

////////////////////////////////////////////////////////////////////////////////////////////////?//

    SimpleRenderingContext::SimpleRenderingContext(
        SavedRenderResources^ savedRes, 
        RenderCore::IThreadContext& threadContext,
        void* parsingContext)
    : _savedRes(savedRes), _parsingContext((RenderCore::Techniques::ParsingContext*)parsingContext)
    {
        _devContext = RenderCore::Metal::DeviceContext::Get(threadContext);
    }
    SimpleRenderingContext::~SimpleRenderingContext() { _devContext.reset(); }
    SimpleRenderingContext::!SimpleRenderingContext() { _devContext.reset(); }

}

