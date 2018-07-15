// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CLIXAutoPtr.h"
#include "../../RenderCore/IThreadContext_Forward.h"
#include "../../RenderCore/Metal/Forward.h"
#include "../../Core/Types.h"
#include <memory>

namespace RenderCore { namespace Techniques { class ParsingContext; } }

namespace GUILayer
{
    ref class RetainedRenderResources;

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
            const float color[], const float xform[]);

        void DrawIndexedPrimitive(
            unsigned primitiveType,
            uint64 vb, uint64 ib,
            unsigned startIndex,
            unsigned indexCount,
            unsigned startVertex,
            const float color[], const float xform[]);

        void InitState(bool depthTest, bool depthWrite);
        RenderCore::Techniques::ParsingContext& GetParsingContext() { return *_parsingContext; }
        RenderCore::Metal::DeviceContext& GetDevContext() { return *_devContext.get(); }
        RenderCore::IThreadContext& GetThreadContext() { return *_threadContext; }

        SimpleRenderingContext(
            RenderCore::IThreadContext* threadContext,
            RetainedRenderResources^ savedRes, 
            void* parsingContext);
        ~SimpleRenderingContext();
        !SimpleRenderingContext();
    protected:
        RetainedRenderResources^ _retainedRes;
        RenderCore::Techniques::ParsingContext* _parsingContext;
        clix::shared_ptr<RenderCore::Metal::DeviceContext> _devContext;
        RenderCore::IThreadContext* _threadContext;     // note -- keeping an unprotected pointer here (SimpleRenderingContext is typically short lived). Create must be careful to manage lifetimes
    };
}

