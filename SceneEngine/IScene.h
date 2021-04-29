// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Techniques/TechniqueUtils.h"

namespace RenderCore { class IThreadContext; }
namespace RenderCore { namespace Techniques { class ParsingContext; class ProjectionDesc; class DrawablesPacket; enum class BatchFilter; } }
namespace RenderCore { namespace LightingEngine
{
    class ShadowProjectionDesc;
    class LightDesc;
    class EnvironmentalLightingDesc;
}}

namespace SceneEngine
{
#pragma warning(push)
#pragma warning(disable:4324) //  'SceneEngine::SceneView': structure was padded due to alignment specifier
	class SceneView
	{
	public:
		enum class Type { Normal, Shadow, Other };
		Type _type = SceneView::Type::Normal;
		RenderCore::Techniques::ProjectionDesc _projection;
	};
#pragma warning(pop)

    class ExecuteSceneContext
    {
    public:
        SceneView _view;
        RenderCore::Techniques::BatchFilter _batchFilter = RenderCore::Techniques::BatchFilter(0);
        RenderCore::Techniques::DrawablesPacket* _destinationPkt = nullptr;
    };

    class IScene
    {
    public:
        virtual void ExecuteScene(
            RenderCore::IThreadContext& threadContext,
            const ExecuteSceneContext& executeContext) const = 0;
		virtual ~IScene() = default;
	};

    class ToneMapSettings;

	class ILightingStateDelegate
	{
	public:
        using ProjectionDesc    = RenderCore::Techniques::ProjectionDesc;
        using ShadowProjIndex   = unsigned;
        using LightIndex        = unsigned;

        virtual ShadowProjIndex GetShadowProjectionCount() const = 0;
        virtual auto            GetShadowProjectionDesc(ShadowProjIndex index, const ProjectionDesc& mainSceneProj) const
            -> RenderCore::LightingEngine::ShadowProjectionDesc = 0;

        virtual LightIndex  GetLightCount() const = 0;
        virtual auto        GetLightDesc(LightIndex index) const -> const RenderCore::LightingEngine::LightDesc& = 0;
        virtual auto        GetEnvironmentalLightingDesc() const -> RenderCore::LightingEngine::EnvironmentalLightingDesc = 0;
        virtual auto        GetToneMapSettings() const -> ToneMapSettings = 0;

		virtual ~ILightingStateDelegate() = default;
    };
}
