// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "State.h"
#include "IncludeGLES.h"

namespace RenderCore { namespace Metal_OpenGLES
{

    void SamplerState::Apply(unsigned textureUnit, unsigned bindingTarget, bool enableMipmaps) const never_throws
    {
        #if !APPORTABLE
        if (_prebuiltSamplerMipmaps) {
            glBindSampler(textureUnit, enableMipmaps ? _prebuiltSamplerMipmaps->AsRawGLHandle() : _prebuiltSamplerNoMipmaps->AsRawGLHandle());
        } else
		#endif
        {
            #if defined(_DEBUG)
                // expecting GL_ACTIVE_TEXTURE to be already set to the expected texture unit
                // (which will normally be the case when binding texture, then sampler)
                GLint activeTexture = 0;
                glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
                assert(activeTexture == GL_TEXTURE0 + textureUnit);
            #endif

            #if !APPORTABLE
                glBindSampler(textureUnit, 0);
            #endif

            glTexParameteri(bindingTarget, GL_TEXTURE_MIN_FILTER, enableMipmaps ? _minFilter : _maxFilter);
            glTexParameteri(bindingTarget, GL_TEXTURE_MAG_FILTER, _maxFilter);
            glTexParameteri(bindingTarget, GL_TEXTURE_WRAP_S, _wrapS);
            glTexParameteri(bindingTarget, GL_TEXTURE_WRAP_T, _wrapT);
            glTexParameteri(bindingTarget, GL_TEXTURE_WRAP_R, _wrapR);

            glTexParameteri(bindingTarget, GL_TEXTURE_COMPARE_MODE, _compareMode);
            glTexParameteri(bindingTarget, GL_TEXTURE_COMPARE_FUNC, _compareFunc);
        }

        CheckGLError("SamplerState::Apply");

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

    GLenum AsGLenum(CompareOp comparison)
    {
        switch (comparison) {
        case CompareOp::Less:           return GL_LESS;
        case CompareOp::Equal:          return GL_EQUAL;
        case CompareOp::LessEqual:      return GL_LEQUAL;
        case CompareOp::Greater:        return GL_GREATER;
        case CompareOp::NotEqual:       return GL_NOTEQUAL;
        case CompareOp::GreaterEqual:   return GL_GEQUAL;
        case CompareOp::Always:         return GL_ALWAYS;
        default:
        case CompareOp::Never:          return GL_NEVER;
        }
    }

    SamplerState::SamplerState(
        FilterMode filter,
        AddressMode addressU, AddressMode addressV, AddressMode addressW,
        CompareOp comparison)
    {
        CheckGLError("Construct Sampler State (start)");

        switch (filter) {
        case FilterMode::Bilinear:
        case FilterMode::ComparisonBilinear:
            _minFilter = GL_LINEAR_MIPMAP_NEAREST;
            _maxFilter = GL_LINEAR;
            break;

        case FilterMode::Trilinear:
        case FilterMode::Anisotropic:
            _minFilter = GL_LINEAR_MIPMAP_LINEAR;
            _maxFilter = GL_LINEAR;
            break;

        default:
        case FilterMode::Point:
            _minFilter = GL_NEAREST_MIPMAP_NEAREST;
            _maxFilter = GL_NEAREST;
            break;
        }

        _compareFunc = AsGLenum(comparison);
        _compareMode = (filter == FilterMode::ComparisonBilinear) ? GL_COMPARE_REF_TO_TEXTURE : GL_NONE;

        _wrapS = AsGLenum(addressU);
        _wrapT = AsGLenum(addressV);
        _wrapR = AsGLenum(addressW);

        auto& objectFactory = GetObjectFactory();
        #if !APPORTABLE
            _prebuiltSamplerMipmaps = objectFactory.CreateSampler();
        #endif
        if (_prebuiltSamplerMipmaps) {
            glSamplerParameteri(_prebuiltSamplerMipmaps->AsRawGLHandle(), GL_TEXTURE_MIN_FILTER, _minFilter);
            glSamplerParameteri(_prebuiltSamplerMipmaps->AsRawGLHandle(), GL_TEXTURE_MAG_FILTER, _maxFilter);
            glSamplerParameteri(_prebuiltSamplerMipmaps->AsRawGLHandle(), GL_TEXTURE_WRAP_S, _wrapS);
            glSamplerParameteri(_prebuiltSamplerMipmaps->AsRawGLHandle(), GL_TEXTURE_WRAP_T, _wrapT);
            glSamplerParameteri(_prebuiltSamplerMipmaps->AsRawGLHandle(), GL_TEXTURE_WRAP_R, _wrapR);

            glSamplerParameteri(_prebuiltSamplerMipmaps->AsRawGLHandle(), GL_TEXTURE_COMPARE_MODE, _compareMode);
            glSamplerParameteri(_prebuiltSamplerMipmaps->AsRawGLHandle(), GL_TEXTURE_COMPARE_FUNC, _compareFunc);
        }

        #if !APPORTABLE
            _prebuiltSamplerNoMipmaps = objectFactory.CreateSampler();
        #endif
        if (_prebuiltSamplerNoMipmaps) {
            glSamplerParameteri(_prebuiltSamplerNoMipmaps->AsRawGLHandle(), GL_TEXTURE_MIN_FILTER, _maxFilter);
            glSamplerParameteri(_prebuiltSamplerNoMipmaps->AsRawGLHandle(), GL_TEXTURE_MAG_FILTER, _maxFilter);
            glSamplerParameteri(_prebuiltSamplerNoMipmaps->AsRawGLHandle(), GL_TEXTURE_WRAP_S, _wrapS);
            glSamplerParameteri(_prebuiltSamplerNoMipmaps->AsRawGLHandle(), GL_TEXTURE_WRAP_T, _wrapT);
            glSamplerParameteri(_prebuiltSamplerNoMipmaps->AsRawGLHandle(), GL_TEXTURE_WRAP_R, _wrapR);

            glSamplerParameteri(_prebuiltSamplerNoMipmaps->AsRawGLHandle(), GL_TEXTURE_COMPARE_MODE, _compareMode);
            glSamplerParameteri(_prebuiltSamplerNoMipmaps->AsRawGLHandle(), GL_TEXTURE_COMPARE_FUNC, _compareFunc);
        }

        CheckGLError("Construct Sampler State");
    }

    SamplerState::SamplerState()
    {
        _minFilter = GL_LINEAR_MIPMAP_LINEAR;
        _maxFilter = GL_LINEAR;
        _compareFunc = AsGLenum(CompareOp::Never);
        _compareMode = GL_NONE;
        _wrapS = AsGLenum(AddressMode::Wrap);
        _wrapT = AsGLenum(AddressMode::Wrap);
        _wrapR = AsGLenum(AddressMode::Wrap);
        // Note -- default constructor must not invoke ObjectFactory::CreateSampler(), because
        // it is used pre-operator= sometimes, when the default constructor is expected to be low overhead
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

        CheckGLError("Apply BlendState");
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

        CheckGLError("GetViewport");
    }
    
}}
