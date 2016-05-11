// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "TechniqueUtils.h"
#include "../Metal/Forward.h"
#include "../Metal/Buffer.h"
#include "../../Utility/MemoryUtils.h"
#include <vector>
#include <memory>

namespace Assets { namespace Exceptions { class AssetException; }}
namespace Utility { class ParameterBox; }
namespace RenderCore { class NamedResources; }

namespace RenderCore { namespace Techniques 
{
    class TechniqueContext;
    class IStateSetResolver;
    
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
        TechniqueContext&               GetTechniqueContext()               { return *_techniqueContext.get(); }
        const Metal::UniformsStream&    GetGlobalUniformsStream() const     { return *_globalUniformsStream.get(); }
        Metal::ConstantBuffer&          GetGlobalTransformCB()              { return _globalCBs[0]; }
        Metal::ConstantBuffer&          GetGlobalStateCB()                  { return _globalCBs[1]; }
        void    SetGlobalCB(
            Metal::DeviceContext& context, unsigned index, 
            const void* newData, size_t dataSize);
        std::shared_ptr<IStateSetResolver> SetStateSetResolver(
            std::shared_ptr<IStateSetResolver> stateSetResolver);
        const std::shared_ptr<IStateSetResolver>& GetStateSetResolver()            { return _stateSetResolver; }
        const std::shared_ptr<Utility::ParameterBox>& GetStateSetEnvironment();
        NamedResources& GetNamedResources() { return *_namedResources; }

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
        void Process(const ::Assets::Exceptions::AssetException& e);
        bool HasPendingAssets() const { return _stringHelpers->_pendingAssets[0] != '\0'; }
        bool HasInvalidAssets() const { return _stringHelpers->_invalidAssets[0] != '\0'; }
        bool HasErrorString() const { return _stringHelpers->_errorString[0] != '\0'; }

        ParsingContext(const TechniqueContext& techniqueContext, NamedResources& namedResources);
        ~ParsingContext();

        ParsingContext& operator=(const ParsingContext&) = delete;
        ParsingContext(const ParsingContext&) = delete;

    protected:
        Metal::ConstantBuffer   _globalCBs[5];

        std::unique_ptr<TechniqueContext>   _techniqueContext;
        AlignedUniquePtr<ProjectionDesc>    _projectionDesc;
        std::shared_ptr<IStateSetResolver>  _stateSetResolver;

        std::unique_ptr<Metal::UniformsStream>      _globalUniformsStream;
        const Metal::ConstantBuffer*                _globalUniformsConstantBuffers[5];

        NamedResources*     _namedResources;
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
        CATCH(const ::Assets::Exceptions::AssetException& e) { (parserContext).Process(e); }        \
        /**/

    #define CATCH_ASSETS_BEGIN TRY {
    #define CATCH_ASSETS_END(parserContext) } CATCH_ASSETS(parserContext) CATCH_END
    /// @}

}}

