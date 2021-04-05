// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IOverlayContext.h"
// #include "Font.h"
// #include "../RenderCore/Metal/DeviceContext.h"
// #include "../RenderCore/Techniques/TechniqueUtils.h"
// #include "../RenderCore/Types.h"
// #include "../RenderCore/UniformsStream.h"
// #include "../Math/Matrix.h"
// #include "../Utility/MemoryUtils.h"
#include "../Utility/IteratorUtils.h"
#include <vector>
#include <memory>

#pragma warning(disable:4324)

namespace RenderCore { class VertexBufferView; class IThreadContext; }
namespace RenderCore { namespace Techniques { class IImmediateDrawables; } }

namespace RenderOverlays
{
    class ImmediateOverlayContext : public IOverlayContext
    {
    public:
        void    DrawPoint      (ProjectionMode proj, const Float3& v,     const ColorB& col,      uint8 size);
        void    DrawPoints     (ProjectionMode proj, const Float3 v[],    uint32 numPoints,       const ColorB& col,    uint8 size);
        void    DrawPoints     (ProjectionMode proj, const Float3 v[],    uint32 numPoints,       const ColorB col[],   uint8 size);

        void    DrawLine       (ProjectionMode proj, const Float3& v0,    const ColorB& colV0,    const Float3& v1,     const ColorB& colV1, float thickness);
        void    DrawLines      (ProjectionMode proj, const Float3 v[],    uint32 numPoints,       const ColorB& col,    float thickness);
        void    DrawLines      (ProjectionMode proj, const Float3 v[],    uint32 numPoints,       const ColorB col[],   float thickness);

        void    DrawTriangles  (ProjectionMode proj, const Float3 v[],    uint32 numPoints,       const ColorB& col);
        void    DrawTriangles  (ProjectionMode proj, const Float3 v[],    uint32 numPoints,       const ColorB col[]);

        void    DrawTriangle   (ProjectionMode proj, const Float3& v0,    const ColorB& colV0,    const Float3& v1,     
                                const ColorB& colV1, const Float3& v2,       const ColorB& colV2);

        void    DrawQuad       (ProjectionMode proj, 
                                const Float3& mins, const Float3& maxs, 
                                ColorB color0, ColorB color1,
                                const Float2& minTex0, const Float2& maxTex0, 
                                const Float2& minTex1, const Float2& maxTex1,
								StringSection<> shaderSelectorTable);

        void    DrawQuad(
            ProjectionMode proj, 
            const Float3& mins, const Float3& maxs, 
            ColorB color,
            StringSection<> shaderSelectorTable);

        void    DrawTexturedQuad(
            ProjectionMode proj, 
            const Float3& mins, const Float3& maxs, 
            const std::string& texture,
            ColorB color, const Float2& minTex0, const Float2& maxTex0);

        float   DrawText(
            const std::tuple<Float3, Float3>& quad, 
            const std::shared_ptr<Font>& font, const TextStyle& textStyle, 
            ColorB col, TextAlignment alignment, StringSection<char> text);

        void CaptureState();
        void ReleaseState();
        void SetState(const OverlayState& state);

        // RenderCore::IThreadContext*                 GetDeviceContext();

        ImmediateOverlayContext(
            RenderCore::IThreadContext& threadContext,
            RenderCore::Techniques::IImmediateDrawables& immediateDrawables);
        ~ImmediateOverlayContext();

        class ShaderBox;

    private:
        RenderCore::Techniques::IImmediateDrawables* _immediateDrawables;
        RenderCore::IThreadContext* _threadContext;
        std::shared_ptr<Font> _defaultFont;
        OverlayState _currentState;

        class DrawCall;
        // std::vector<DrawCall>   _drawCalls;
        // void                    Flush();
        // void                    SetShader(RenderCore::Topology topology, VertexFormat format, ProjectionMode projMode, const std::string& pixelShaderName, IteratorRange<const RenderCore::VertexBufferView*> vertexBuffers);

        IteratorRange<void*>    BeginDrawCall(const DrawCall& drawCall);
    };

	std::unique_ptr<ImmediateOverlayContext>
		MakeImmediateOverlayContext(
            RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::IImmediateDrawables& immediateDrawables);
}



