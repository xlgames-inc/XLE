// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ParsingContext.h"
#include "Techniques.h"
#include "../Metal/InputLayout.h"   // (for UniformsStream)
#include "../../Assets/AssetUtils.h"
#include "../../Utility/StringFormat.h"
#include <memory>

namespace RenderCore { namespace Techniques
{

    void ParsingContext::SetGlobalCB(
        RenderCore::Metal::DeviceContext& context, unsigned index, 
        const void* newData, size_t dataSize)
    {
        if (index >= dimof(_globalCBs)) {
            return;
        }

        if (!_globalCBs[index].IsGood()) {
            _globalCBs[index] = Metal::ConstantBuffer(newData, dataSize, false);
        } else {
            _globalCBs[index].Update(context, newData, dataSize);
        }
    }

    void ParsingContext::Process(const ::Assets::Exceptions::AssetException& e)
    {
            //  Handle a "invalid asset" and "pending asset" exception that 
            //  occurred during rendering. Normally this will just mean
            //  reporting the assert to the screen.
            //
            // These happen fairly often -- particularly when just starting up, or
            //  when changing rendering settings.
            //  at the moment, this will result in a bunch of allocations -- that's not
            //  ideal during error processing.
        auto* id = e.Initializer();
        
        auto* bufferStart = _stringHelpers->_pendingAssets;
        if (e.State() == ::Assets::AssetState::Invalid)
            bufferStart = _stringHelpers->_invalidAssets;

        static_assert(
            dimof(_stringHelpers->_pendingAssets) == dimof(_stringHelpers->_invalidAssets),
            "Assuming pending and invalid asset buffers are the same length");

        if (!XlFindStringI(bufferStart, id))
            StringMeldAppend(bufferStart, bufferStart + dimof(_stringHelpers->_pendingAssets)) << "," << id;
    }

    std::shared_ptr<IStateSetResolver> ParsingContext::SetStateSetResolver(
        std::shared_ptr<IStateSetResolver> stateSetResolver)
    {
        std::shared_ptr<IStateSetResolver> oldResolver = std::move(_stateSetResolver);
        _stateSetResolver = std::move(stateSetResolver);
        return std::move(oldResolver);
    }

    const std::shared_ptr<Utility::ParameterBox>& ParsingContext::GetStateSetEnvironment()
    {
        return _techniqueContext->_stateSetEnvironment;
    }

    ParsingContext::ParsingContext(const TechniqueContext& techniqueContext, NamedResources* namedResources)
    {
        _techniqueContext = std::make_unique<TechniqueContext>(techniqueContext);
        _stateSetResolver = _techniqueContext->_defaultStateSetResolver;
        _stringHelpers = std::make_unique<StringHelpers>();
        _namedResources = namedResources;

        _projectionDesc.reset((ProjectionDesc*)XlMemAlign(sizeof(ProjectionDesc), 16));
        #pragma push_macro("new")
        #undef new
            new(_projectionDesc.get()) ProjectionDesc();
        #pragma pop_macro("new")

        static_assert(dimof(_globalCBs) == dimof(_globalUniformsConstantBuffers), "Expecting equivalent array lengths");
        for (unsigned c=0; c<dimof(_globalCBs); ++c)
            _globalUniformsConstantBuffers[c] = &_globalCBs[c];
        _globalUniformsStream = std::make_unique<RenderCore::Metal::UniformsStream>(
            nullptr, _globalUniformsConstantBuffers, dimof(_globalUniformsConstantBuffers));
    }

    ParsingContext::~ParsingContext() {}

    ParsingContext::StringHelpers::StringHelpers()
    {
        _errorString[0] = _pendingAssets[0] = _invalidAssets[0] = _quickMetrics[0] = '\0';
    }

}}

