// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Metal/Forward.h"
#include "../../Assets/Assets.h"

namespace RenderCore { namespace Assets 
{
    class DeferredShaderResource
    {
    public:
        static const ::Assets::ResChar* LinearSpace; 
        static const ::Assets::ResChar* SRGBSpace; 
        explicit DeferredShaderResource(const ::Assets::ResChar resourceName[], const ::Assets::ResChar sourceSpace[] = LinearSpace);
        ~DeferredShaderResource();
        const Metal::ShaderResourceView&       GetShaderResource() const;
        const ::Assets::DependencyValidation&     GetDependencyValidation() const     { return *_validationCallback; }
        const char*                     Initializer() const;
    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
        std::shared_ptr<::Assets::DependencyValidation>   _validationCallback;
        
        DEBUG_ONLY(char _initializer[512];)
    };

}}