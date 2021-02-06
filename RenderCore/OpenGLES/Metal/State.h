// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ObjectFactory.h"
#include "../../Types.h"
#include "../../../Core/Exceptions.h"

namespace RenderCore { namespace Metal_OpenGLES
{
    class DeviceContext;

////////////////////////////////////////////////////////////////////////////////////////////////////

    class Resource;
    class CapturedStates;

    class SamplerState
    {
    public:
        SamplerState(   FilterMode filter,
                        AddressMode addressU = AddressMode::Wrap,
                        AddressMode addressV = AddressMode::Wrap,
                        AddressMode addressW = AddressMode::Wrap,
                        CompareOp comparison = CompareOp::Never,
                        bool enableMipmaps = true); /* this enableMipmaps argument in the constructor is ignored; the argument in Apply is what is relevant */
        SamplerState();

        void Apply(
            CapturedStates& capture,
            unsigned textureUnit, unsigned bindingTarget,
            const Resource* res,
            bool enableMipmaps) const never_throws;

        void Apply(unsigned textureUnit, unsigned bindingTarget, bool enableMipmaps) const never_throws;

        typedef SamplerState UnderlyingType;
        UnderlyingType GetUnderlying() const never_throws { return *this; }

    private:
        unsigned _minFilter, _maxFilter;
        unsigned _wrapS, _wrapT, _wrapR;
        unsigned _compareMode, _compareFunc;

        intrusive_ptr<OpenGL::Sampler> _prebuiltSamplerMipmaps;
        intrusive_ptr<OpenGL::Sampler> _prebuiltSamplerNoMipmaps;

        unsigned _guid;
        bool _gles300Factory;
    };

    class BlendState
    {
    public:
        BlendState();
        void Apply() const never_throws;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

}}

