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

namespace Assets { namespace Exceptions { class InvalidAsset; class PendingAsset; }}

namespace RenderCore { namespace Techniques 
{
    class TechniqueContext;
    
    class ParsingContext
    {
    public:
            //  ----------------- Active projection context -----------------
        ProjectionDesc&         GetProjectionDesc()         { return *_projectionDesc; }
        const ProjectionDesc&   GetProjectionDesc() const   { return *_projectionDesc; }

            //  ----------------- Working technique context -----------------
        TechniqueContext&               GetTechniqueContext()               { return *_techniqueContext.get(); }
        const Metal::UniformsStream&    GetGlobalUniformsStream() const     { return *_globalUniformsStream.get(); }
        void                            SetGlobalCB(unsigned index, Metal::DeviceContext* context, const void* newData, size_t dataSize);
        Metal::ConstantBuffer&          GetGlobalTransformCB()              { return _globalCBs[0]; }
        Metal::ConstantBuffer&          GetGlobalStateCB()                  { return _globalCBs[1]; }

            //  ----------------- Exception reporting ----------------- 
        std::string                 _errorString;
        std::vector<std::string>    _pendingResources;
        std::vector<std::string>    _invalidResources;
        void                        Process(const ::Assets::Exceptions::InvalidAsset& e);
        void                        Process(const ::Assets::Exceptions::PendingAsset& e);

        ParsingContext(const TechniqueContext& techniqueContext);
        ~ParsingContext();

        ParsingContext& operator=(const ParsingContext&) = delete;
        ParsingContext(const ParsingContext&) = delete;

    protected:
        Metal::ConstantBuffer   _globalCBs[6];

        std::unique_ptr<TechniqueContext>   _techniqueContext;
        std::unique_ptr<ProjectionDesc, AlignedDeletor<ProjectionDesc>> _projectionDesc;

        std::unique_ptr<Metal::UniformsStream>      _globalUniformsStream;
        std::vector<const Metal::ConstantBuffer*>   _globalUniformsConstantBuffers;
    };

}}

