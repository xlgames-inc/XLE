// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "TechniqueUtils.h"
#include "../UniformsStream.h"
#include "../BufferView.h"
#include "../Metal/Forward.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/ParameterBox.h"
#include <vector>
#include <memory>
#include <functional>

namespace Assets { namespace Exceptions { class RetrievalError; }}
namespace Utility { class ParameterBox; }
namespace RenderCore { class IResource; }

namespace RenderCore { namespace Techniques 
{
    class TechniqueContext;
	class IUniformBufferDelegate;
    class IShaderResourceDelegate;
    class AttachmentPool;
	class FrameBufferPool;
	class IPipelineAcceleratorPool;
    class SystemUniformsDelegate;
    
    /// <summary>Manages critical shader state</summary>
    /// Certain system variables are bound to the shaders, and managed by higher
    /// level code. The simpliest example is the global transform; but there are
    /// other global resources required by many shaders.
    ///
    /// Technique selection also involves some state information -- called the
    /// run-time technique state and the global technique state.
    ///
    /// This context object manages this kind of global state information.
    /// It also captures error information (such as invalid assets), which can
    /// be reported to the user after parsing.
    class ParsingContext
    {
    public:
            //  ----------------- Active projection context -----------------
        ProjectionDesc&         GetProjectionDesc()         { return *_projectionDesc; }
        const ProjectionDesc&   GetProjectionDesc() const   { return *_projectionDesc; }

            //  ----------------- Working technique context -----------------
        TechniqueContext&		GetTechniqueContext()               { return *_techniqueContext.get(); }
		ParameterBox&			GetSubframeShaderSelectors()		{ return _subframeShaderSelectors; }

		void AddUniformDelegate(uint64_t binding, const std::shared_ptr<IUniformBufferDelegate>&);
		void RemoveUniformDelegate(IUniformBufferDelegate&);
        void RemoveUniformDelegate(uint64_t binding);
        void AddShaderResourceDelegate(const std::shared_ptr<IShaderResourceDelegate>&);
		void RemoveShaderResourceDelegate(IShaderResourceDelegate&);
		auto GetUniformDelegates() const { return MakeIteratorRange(_uniformDelegates); }
        auto GetShaderResourceDelegates() const { return MakeIteratorRange(_shaderResourceDelegates); }

        SystemUniformsDelegate& GetSystemUniformsDelegate() const { return *_systemUniformsDelegate; }

        AttachmentPool& GetNamedResources() { assert(_namedResources); return *_namedResources; }
		FrameBufferPool& GetFrameBufferPool() { assert(_frameBufferPool); return *_frameBufferPool; }

			//  ----------------- Overlays for late rendering -----------------
        typedef std::function<void(RenderCore::Metal::DeviceContext&, ParsingContext&)> PendingOverlay;
        std::vector<PendingOverlay> _pendingOverlays;

            //  ----------------- Exception reporting -----------------
        class StringHelpers
        {
        public:
            char _errorString[1024];
            char _pendingAssets[1024];
            char _invalidAssets[1024];
            char _quickMetrics[4096];

            StringHelpers();
        };
        std::unique_ptr<StringHelpers> _stringHelpers;
        void Process(const ::Assets::Exceptions::RetrievalError& e);
        bool HasPendingAssets() const { return _stringHelpers->_pendingAssets[0] != '\0'; }
        bool HasInvalidAssets() const { return _stringHelpers->_invalidAssets[0] != '\0'; }
        bool HasErrorString() const { return _stringHelpers->_errorString[0] != '\0'; }

        ParsingContext(const TechniqueContext& techniqueContext, AttachmentPool* namedResources = nullptr, FrameBufferPool* frameBufferPool = nullptr);
        ~ParsingContext();

        ParsingContext& operator=(const ParsingContext&) = delete;
        ParsingContext(const ParsingContext&) = delete;

    protected:
        std::shared_ptr<IResource>					_globalCBs[5];
		ConstantBufferView							_globalCBVs[5];

        std::unique_ptr<TechniqueContext>           _techniqueContext;
        std::unique_ptr<ProjectionDesc>             _projectionDesc;

		ParameterBox								_subframeShaderSelectors;

        AttachmentPool*     _namedResources;
		FrameBufferPool*	_frameBufferPool;

		std::vector<std::pair<uint64_t, std::shared_ptr<IUniformBufferDelegate>>> _uniformDelegates;
        std::vector<std::shared_ptr<IShaderResourceDelegate>> _shaderResourceDelegates;
        std::shared_ptr<SystemUniformsDelegate> _systemUniformsDelegate;
    };

    /// <summary>Utility macros for catching asset exceptions</summary>
    /// Invalid and pending assets are common exceptions during rendering.
    /// This macros assist in creating firewalls for these exceptions
    /// (by passing them along to a ParsingContext to be recorded).
    /// 
    /// <example>
    ///     <code>\code
    ///     CATCH_ASSETS_BEGIN
    ///         DoRenderOperation(parserContext);
    ///     CATCH_ASSETS_END(parserContext)
    ///
    ///     // or:
    ///     TRY { DoRenderOperation(parserContext); } 
    ///     CATCH_ASSETS(parserContext)
    ///     CATCH (...) { HandleOtherException(); }
    ///     CATCH_END
    ///     \endcode</code>
    /// </example>
    /// @{
    #define CATCH_ASSETS(parserContext)                                                             \
        CATCH(const ::Assets::Exceptions::RetrievalError& e) { (parserContext).Process(e); }        \
        /**/

    #define CATCH_ASSETS_BEGIN TRY {
    #define CATCH_ASSETS_END(parserContext) } CATCH_ASSETS(parserContext) CATCH_END
    /// @}


	/*inline UniformsStream ParsingContext::GetGlobalUniformsStream() const
	{
		return {
			MakeIteratorRange(_globalCBVs),
			{},
			{}
		};
	}*/
}}

