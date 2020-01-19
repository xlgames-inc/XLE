// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/AssetsCore.h"
#include <memory>
#include <vector>
#include <string>

namespace SceneEngine { class IScene; }
namespace RenderCore { namespace Techniques { class ITechniqueDelegate; class PipelineAcceleratorPool; class CompiledShaderPatchCollection; }}
namespace RenderCore { namespace Assets { class MaterialScaffoldMaterial; }}
namespace GraphLanguage { class INodeGraphProvider; class NodeGraph; class NodeGraphSignature; }
namespace ShaderSourceParser { class PreviewOptions; }
namespace Utility { class OnChangeCallback; }

namespace ToolsRig
{
    class MaterialVisSettings
    {
    public:
        enum class GeometryType { Sphere, Cube, Plane2D };
        GeometryType _geometryType = GeometryType::Sphere;
    };

	::Assets::FuturePtr<SceneEngine::IScene> MakeScene(
		const std::shared_ptr<RenderCore::Techniques::PipelineAcceleratorPool>& pipelineAcceleratorPool,
		const MaterialVisSettings& visObject, 
		const std::shared_ptr<RenderCore::Techniques::CompiledShaderPatchCollection>& patchCollectionOverride,
		const std::shared_ptr<RenderCore::Assets::MaterialScaffoldMaterial>& material = nullptr);

	class MessageRelay
	{
	public:
		std::string GetMessages() const;

		unsigned AddCallback(const std::shared_ptr<Utility::OnChangeCallback>& callback);
		void RemoveCallback(unsigned);

		void AddMessage(const std::string& msg);

		MessageRelay();
		~MessageRelay();
	private:
		class Pimpl;
		std::unique_ptr<Pimpl> _pimpl;
	};

	std::unique_ptr<RenderCore::Techniques::ITechniqueDelegate> MakeShaderPatchAnalysisDelegate(
		const ShaderSourceParser::PreviewOptions& previewOptions);

	std::unique_ptr<RenderCore::Techniques::CompiledShaderPatchCollection> MakeCompiledShaderPatchCollection(
		const std::shared_ptr<GraphLanguage::INodeGraphProvider>& provider,
		const std::shared_ptr<MessageRelay>& logMessages);

	std::unique_ptr<RenderCore::Techniques::CompiledShaderPatchCollection> MakeCompiledShaderPatchCollection(
		const GraphLanguage::NodeGraph& nodeGraph,
		const GraphLanguage::NodeGraphSignature& nodeGraphSignature,
		const std::string& subGraphName,
		uint32_t previewNodeId,
		const std::shared_ptr<GraphLanguage::INodeGraphProvider>& subProvider,
		const std::shared_ptr<MessageRelay>& logMessages);
}
