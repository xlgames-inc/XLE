// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ParsingContext.h"
#include "Techniques.h"
#include "../../Assets/AssetUtils.h"
#include "../Metal/InputLayout.h"
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

        if (!_globalCBs[index].GetUnderlying()) {
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
        std::string id = e.Initializer();
        std::vector<std::string>* set = &_pendingAssets;
        if (e.State() == ::Assets::AssetState::Invalid)
            set = &_invalidAssets;

        auto i = std::lower_bound(set->begin(), set->end(), id);
        if (i == set->end() || *i != id)
            set->insert(i, id);
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

    ParsingContext::ParsingContext(const TechniqueContext& techniqueContext)
    {
        _techniqueContext = std::make_unique<TechniqueContext>(techniqueContext);
        _stateSetResolver = _techniqueContext->_defaultStateSetResolver;

        _projectionDesc.reset((ProjectionDesc*)XlMemAlign(sizeof(ProjectionDesc), 16));
        #pragma push_macro("new")
        #undef new
            new(_projectionDesc.get()) ProjectionDesc();
        #pragma pop_macro("new")

        for (unsigned c=0; c<dimof(_globalCBs); ++c) {
            _globalUniformsConstantBuffers.push_back(&_globalCBs[c]);
        }
        _globalUniformsStream = std::make_unique<RenderCore::Metal::UniformsStream>(
            nullptr, AsPointer(_globalUniformsConstantBuffers.begin()), _globalUniformsConstantBuffers.size());
    }

    ParsingContext::~ParsingContext() {}

}}

