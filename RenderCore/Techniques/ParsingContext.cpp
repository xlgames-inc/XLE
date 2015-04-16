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

    void ParsingContext::SetGlobalCB(unsigned index, RenderCore::Metal::DeviceContext* context, const void* newData, size_t dataSize)
    {
        if (index >= dimof(_globalCBs)) {
            return;
        }

        if (!_globalCBs[index].GetUnderlying()) {
            _globalCBs[index] = Metal::ConstantBuffer(newData, dataSize, false);
        } else {
            _globalCBs[index].Update(*context, newData, dataSize);
        }
    }

    void ParsingContext::Process(const ::Assets::Exceptions::InvalidResource& e)
    {
            //  Handle a "invalid resource" exception that 
            //  occurred during rendering. Normally this will just mean
            //  reporting the invalid resource to the screen.
        std::string id = e.ResourceId();
        auto i = std::lower_bound(_invalidResources.begin(), _invalidResources.end(), id);
        if (i == _invalidResources.end() || *i != id) {
            _invalidResources.insert(i, id);
        }
    }

    void ParsingContext::Process(const ::Assets::Exceptions::PendingResource& e)
    {
            //  Handle a "pending resource" exception that 
            //  occurred during rendering. Normally this will just mean
            //  reporting the invalid resource to the screen.
            //  These happen fairly often -- particularly when just starting up, or
            //  when changing rendering settings.
            //  at the moment, this will result in a bunch of allocations -- that's not
            //  ideal during error processing.
        std::string id = e.ResourceId();
        auto i = std::lower_bound(_pendingResources.begin(), _pendingResources.end(), id);
        if (i == _pendingResources.end() || *i != id) {
            _pendingResources.insert(i, id);
        }
    }

    ParsingContext::ParsingContext(const TechniqueContext& techniqueContext)
    {
        _techniqueContext = std::make_unique<TechniqueContext>(techniqueContext);

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

