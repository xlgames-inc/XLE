// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EngineDevice.h"
#include "NativeEngineDevice.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Metal/Buffer.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../Math/Vector.h"
#include "../../Math/Matrix.h"
#include "../../Utility/IteratorUtils.h"
#include "../../Core/Types.h"
#include "../../Tools/GUILayer/CLIXAutoPtr.h"
#include <vector>

#pragma make_public(RenderCore::IThreadContext)
#pragma make_public(RenderCore::Techniques::ProjectionDesc)

namespace GUILayer
{
    template<typename T> using AutoToShared = clix::auto_ptr<std::shared_ptr<T>>;

    class SavedRenderResourcesPimpl
    {
    public:
        std::vector<std::pair<uint64, RenderCore::Metal::VertexBuffer>> _vertexBuffers;
        std::vector<std::pair<uint64, RenderCore::Metal::IndexBuffer>> _indexBuffers;
        uint64 _currentBufferIndex;
        RenderCore::Metal::ObjectFactory _objectFactory;

        SavedRenderResourcesPimpl(RenderCore::IDevice& device)
            : _currentBufferIndex(0)
            , _objectFactory(&device) {}
    };

    /// <summary>Create and maintain rendering resources for SimpleRenderingContext</summary>
    /// Create & maintain vertex and index buffers. Intended for use when linking to
    /// C# GUI apps.
    public ref class SavedRenderResources
    {
    public:
        uint64  CreateVertexBuffer(void* data, size_t size);
        uint64  CreateIndexBuffer(void* data, size_t size);
        bool    DeleteBuffer(uint64 id);

        SavedRenderResources(EngineDevice^ engineDevice);
        ~SavedRenderResources();
        !SavedRenderResources();
    protected:
        clix::auto_ptr<SavedRenderResourcesPimpl> _pimpl;
    };

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
        }

        void DrawIndexedPrimitive(
            unsigned primitiveType,
            uint64 vb, uint64 ib,
            unsigned startIndex,
            unsigned indexCount,
            unsigned startVertex,
            const float color[], const float xform[]) 
        {
        }

        SimpleRenderingContext(
            SavedRenderResources^ savedRes, 
            RenderCore::IThreadContext& threadContext,
            RenderCore::Techniques::ParsingContext& parsingContext);
        ~SimpleRenderingContext();
        !SimpleRenderingContext();
    protected:
        SavedRenderResources^ _savedRes;
        RenderCore::Techniques::ParsingContext* _parsingContext;
        AutoToShared<RenderCore::Metal::DeviceContext> _devContext;
    };

////////////////////////////////////////////////////////////////////////////////////////////////?//

    uint64  SavedRenderResources::CreateVertexBuffer(void* data, size_t size)
    {
        RenderCore::Metal::VertexBuffer newBuffer(_pimpl->_objectFactory, data, size);
        _pimpl->_vertexBuffers.push_back(std::make_pair(_pimpl->_currentBufferIndex, std::move(newBuffer)));
        return _pimpl->_currentBufferIndex++;
    }

    uint64  SavedRenderResources::CreateIndexBuffer(void* data, size_t size)
    {
        RenderCore::Metal::IndexBuffer newBuffer(_pimpl->_objectFactory, data, size);
        _pimpl->_indexBuffers.push_back(std::make_pair(_pimpl->_currentBufferIndex, std::move(newBuffer)));
        return _pimpl->_currentBufferIndex++;
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
        RenderCore::Techniques::ParsingContext& parsingContext)
    : _savedRes(savedRes), _parsingContext(&parsingContext)
    {
        _devContext.reset(
            new std::shared_ptr<RenderCore::Metal::DeviceContext>(
                RenderCore::Metal::DeviceContext::Get(threadContext)));
    }
    SimpleRenderingContext::~SimpleRenderingContext() { _devContext.reset(); delete _savedRes; _savedRes = nullptr; }
    SimpleRenderingContext::!SimpleRenderingContext() { _devContext.reset(); delete _savedRes; _savedRes = nullptr; }

}

