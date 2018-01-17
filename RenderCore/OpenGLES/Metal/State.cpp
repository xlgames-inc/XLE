// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "State.h"
#include "IncludeGLES.h"

namespace RenderCore { namespace Metal_OpenGLES
{

    void SamplerState::Apply(unsigned bindingTarget) const never_throws
    {
        glTexParameteri(bindingTarget, GL_TEXTURE_MIN_FILTER, _minFilter);
        glTexParameteri(bindingTarget, GL_TEXTURE_MAG_FILTER, _maxFilter);
        glTexParameteri(bindingTarget, GL_TEXTURE_WRAP_S, _wrapS);
        glTexParameteri(bindingTarget, GL_TEXTURE_WRAP_T, _wrapT);
        glTexParameteri(bindingTarget, GL_TEXTURE_WRAP_R, _wrapR);

        glTexParameteri(bindingTarget, GL_TEXTURE_COMPARE_MODE, _compareMode);
        glTexParameteri(bindingTarget, GL_TEXTURE_COMPARE_FUNC, _compareFunc);

        // (border color set elsewhere. Anisotrophy requires an extension)
    }

    static GLenum AsGLenum(AddressMode addressMode)
    {
        switch (addressMode) {
        default:
        case AddressMode::Wrap: return GL_REPEAT;
        case AddressMode::Mirror: return GL_MIRRORED_REPEAT;
        case AddressMode::Clamp: return GL_CLAMP_TO_EDGE;
        case AddressMode::Border:
            assert(0);      // (not supported by opengles?)
            return GL_CLAMP_TO_EDGE;
        }
    }

    SamplerState::SamplerState(
        FilterMode filter,
        AddressMode addressU, AddressMode addressV, AddressMode addressW,
        CompareOp comparison,
        bool enableMipmapping)
    {
        switch (filter) {
        case FilterMode::Bilinear:
        case FilterMode::ComparisonBilinear:
            _minFilter = enableMipmapping ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR;
            _maxFilter = GL_LINEAR;
            break;

        case FilterMode::Trilinear:
        case FilterMode::Anisotropic:
            _minFilter = GL_LINEAR_MIPMAP_NEAREST;
            _maxFilter = GL_LINEAR;
            break;

        default:
        case FilterMode::Point:
            _minFilter = enableMipmapping ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST;
            _maxFilter = GL_NEAREST;
            break;
        }

        _compareMode = GL_COMPARE_REF_TO_TEXTURE;
        switch (comparison) {
        case CompareOp::Less:
            _compareFunc = GL_LESS;
            break;

        case CompareOp::Equal:
            _compareFunc = GL_EQUAL;
            break;

        case CompareOp::LessEqual:
            _compareFunc = GL_LEQUAL;
            break;

        case CompareOp::Greater:
            _compareFunc = GL_GREATER;
            break;

        case CompareOp::NotEqual:
            _compareFunc = GL_NOTEQUAL;
            break;

        case CompareOp::GreaterEqual:
            _compareFunc = GL_GEQUAL;
            break;

        case CompareOp::Always:
            _compareFunc = GL_ALWAYS;
            break;

        case CompareOp::Never:
            _compareMode = GL_NONE;
            _compareFunc = GL_NEVER;
            break;
        }

        _wrapS = AsGLenum(addressU);
        _wrapT = AsGLenum(addressV);
        _wrapR = AsGLenum(addressW);
    }

    BlendState::BlendState() {}
    void BlendState::Apply() const never_throws
    {
        glBlendColor(0.f, 0.f, 0.f, 0.f);
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
        glEnable(GL_BLEND);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        glDisable(GL_SAMPLE_COVERAGE);
        glSampleCoverage(1.f, GL_FALSE);
    }

    ViewportDesc::ViewportDesc(DeviceContext& viewport)
    {
            // in OpenGL, viewport coordinates are always integers
        GLint viewportParameters[4];
        glGetIntegerv(GL_VIEWPORT, viewportParameters);
        TopLeftX     = float(viewportParameters[0]);
        TopLeftY     = float(viewportParameters[1]);
        Width        = float(viewportParameters[2]);
        Height       = float(viewportParameters[3]);

        glGetFloatv(GL_DEPTH_RANGE, &MinDepth); // (get MinDepth & MaxDepth)
    }
    
}}
