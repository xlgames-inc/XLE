// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/IntermediateCompilers.h"
#include "../../ConsoleRig/AttachablePtr.h"
#include <memory>

namespace RenderCore
{
	class IDevice;
	class ILowLevelCompiler;
	class ShaderService;
	class MinimalShaderSource;
	class ICompiledPipelineLayout;
	class LegacyRegisterBindingDesc;
	class IShaderSource;

	namespace Assets { class PredefinedPipelineLayoutFile; }
}

namespace RenderOverlays { class FontRenderingManager; }
namespace Assets { class Services; }
namespace BufferUploads { class IManager; }

namespace RenderCore { namespace Techniques
{
	class Services;
	class IPipelineAcceleratorPool;
	class TechniqueSharedResources;
	class ITechniqueDelegate;
	class IImmediateDrawables;
	class TechniqueContext;
	class AttachmentPool;
	class FrameBufferPool;
	class SubFrameEvents;

	/** <summary>Organizes the objects required for rendering operations, and manages their lifetimes</summary>
	 * 
	 * The techniques system requires quite a few interacting objects to perform even basic rendering (including
	 * compilers and pools). Sometimes we want to construct and work with these things individually (eg, for unit
	 * tests), however often we just want to construct them all together. That's what this apparatus does -- 
	 * it construsts and manages the lifetime of objects required for rendering using techniques.
	 * 
	 */
	class DrawingApparatus
	{
	public:
		std::shared_ptr<IDevice> _device;
		std::shared_ptr<ILowLevelCompiler> _shaderCompiler;
		std::shared_ptr<ShaderService> _shaderService;
		std::shared_ptr<IShaderSource> _shaderSource;

		::Assets::IntermediateCompilers::CompilerRegistration _shaderFilteringRegistration;
		::Assets::IntermediateCompilers::CompilerRegistration _shaderCompilerRegistration;
		::Assets::IntermediateCompilers::CompilerRegistration _graphShaderCompiler2Registration;

		std::shared_ptr<TechniqueSharedResources> _techniqueSharedResources;
		std::shared_ptr<ITechniqueDelegate> _techniqueDelegateDeferred;

		std::shared_ptr<RenderCore::Assets::PredefinedPipelineLayoutFile> _pipelineLayoutFile;
		std::shared_ptr<ICompiledPipelineLayout> _compiledPipelineLayout;
		std::shared_ptr<IPipelineAcceleratorPool> _pipelineAccelerators;

		std::shared_ptr<LegacyRegisterBindingDesc> _legacyRegisterBindingDesc;

		std::shared_ptr<TechniqueContext> _techniqueContext;

		const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _depValPtr; }
		std::shared_ptr<::Assets::DependencyValidation> _depValPtr;

		ConsoleRig::AttachablePtr<Services> _techniqueServices;
		ConsoleRig::AttachablePtr<::Assets::Services> _assetServices;

		DrawingApparatus(std::shared_ptr<IDevice> device);
		~DrawingApparatus();

		DrawingApparatus(DrawingApparatus&) = delete;
		DrawingApparatus& operator=(DrawingApparatus&) = delete;
	};

	class ImmediateDrawingApparatus
	{
	public:
		std::shared_ptr<DrawingApparatus> _mainDrawingApparatus;
		std::shared_ptr<IImmediateDrawables> _immediateDrawables;

		std::shared_ptr<RenderCore::Assets::PredefinedPipelineLayoutFile> _pipelineLayoutFile;
		std::shared_ptr<ICompiledPipelineLayout> _compiledPipelineLayout;

		std::shared_ptr<RenderOverlays::FontRenderingManager> _fontRenderingManager;

		const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _depValPtr; }
		std::shared_ptr<::Assets::DependencyValidation> _depValPtr;

		ConsoleRig::AttachablePtr<::Assets::Services> _assetServices;

		ImmediateDrawingApparatus(std::shared_ptr<DrawingApparatus>);
		~ImmediateDrawingApparatus();
		ImmediateDrawingApparatus(ImmediateDrawingApparatus&) = delete;
		ImmediateDrawingApparatus& operator=(ImmediateDrawingApparatus&) = delete;
	};

	class PrimaryResourcesApparatus
	{
	public:
		std::vector<::Assets::IntermediateCompilers::RegisteredCompilerId> _modelCompilers;
		::Assets::IntermediateCompilers::CompilerRegistration _materialCompilerRegistration;

		class ContinuationExecutor;
		std::unique_ptr<ContinuationExecutor> _continuationExecutor;
		std::shared_ptr<BufferUploads::IManager> _bufferUploads;

		std::shared_ptr<SubFrameEvents> _subFrameEvents;

		const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _depValPtr; }
		std::shared_ptr<::Assets::DependencyValidation> _depValPtr;

		ConsoleRig::AttachablePtr<Services> _techniqueServices;
		ConsoleRig::AttachablePtr<::Assets::Services> _assetServices;

		PrimaryResourcesApparatus(std::shared_ptr<IDevice> device);
		~PrimaryResourcesApparatus();
		PrimaryResourcesApparatus(PrimaryResourcesApparatus&) = delete;
		PrimaryResourcesApparatus& operator=(PrimaryResourcesApparatus&) = delete;
	};

	class FrameRenderingApparatus
	{
	public:
		std::shared_ptr<AttachmentPool> _attachmentPool;
		std::shared_ptr<FrameBufferPool> _frameBufferPool;

		const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _depValPtr; }
		std::shared_ptr<::Assets::DependencyValidation> _depValPtr;

		FrameRenderingApparatus(std::shared_ptr<IDevice> device);
		~FrameRenderingApparatus();
		FrameRenderingApparatus(FrameRenderingApparatus&) = delete;
		FrameRenderingApparatus& operator=(FrameRenderingApparatus&) = delete;
	};

}}