// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "State.h"
#include "DeviceContext.h"
#include "IncludeGLES.h"

namespace RenderCore { namespace Metal_OpenGLES
{

    void SamplerState::Apply(
        CapturedStates& capture,
        unsigned textureUnit, unsigned bindingTarget,
        const Resource* res,
        bool enableMipmaps) const never_throws
    {
        unsigned guid = enableMipmaps ? _guid : (_guid+1);
        if (_prebuiltSamplerMipmaps && _prebuiltSamplerNoMipmaps) {
            assert(textureUnit < capture._samplerStateBindings.size());
            assert(_gles300Factory);
            if (textureUnit < capture._samplerStateBindings.size() && capture._samplerStateBindings[textureUnit] != guid) {
                glBindSampler(textureUnit, enableMipmaps ? _prebuiltSamplerMipmaps->AsRawGLHandle() : _prebuiltSamplerNoMipmaps->AsRawGLHandle());
                capture._samplerStateBindings[textureUnit] = guid;
            }
        } else {
            #if defined(_DEBUG)
                // expecting GL_ACTIVE_TEXTURE to be already set to the expected texture unit
                // (which will normally be the case when binding texture, then sampler)
                GLint activeTexture = 0;
                glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
                assert(activeTexture == GL_TEXTURE0 + textureUnit);
            #endif

            if (_gles300Factory) {
                assert(textureUnit < capture._samplerStateBindings.size());
                glBindSampler(textureUnit, 0);
                if (textureUnit < capture._samplerStateBindings.size() && capture._samplerStateBindings[textureUnit] != 0) {
                    capture._samplerStateBindings[textureUnit] = 0;
                }
            }

            if (res) {
                // note -- here we incorporate the guid inside the capture object, to ensure that
                //      we rebind everything after every call to BeginStateCapture
                auto captureGUID = uint64_t(capture._captureGUID) << 32ull | uint64_t(guid);
                if (res->_lastBoundSamplerState == captureGUID)
                    return; // early-out because there's no change
                res->_lastBoundSamplerState = captureGUID;
            }

            glTexParameteri(bindingTarget, GL_TEXTURE_MIN_FILTER, enableMipmaps ? _minFilter : _maxFilter);
            glTexParameteri(bindingTarget, GL_TEXTURE_MAG_FILTER, _maxFilter);
            glTexParameteri(bindingTarget, GL_TEXTURE_WRAP_S, _wrapS);
            glTexParameteri(bindingTarget, GL_TEXTURE_WRAP_T, _wrapT);
            if (_gles300Factory)
                glTexParameteri(bindingTarget, GL_TEXTURE_WRAP_R, _wrapR);

            glTexParameteri(bindingTarget, GL_TEXTURE_COMPARE_MODE, _compareMode);
            glTexParameteri(bindingTarget, GL_TEXTURE_COMPARE_FUNC, _compareFunc);
        }

        CheckGLError("SamplerState::Apply");

        // (border color set elsewhere. Anisotropy requires an extension)
    }

    void SamplerState::Apply(unsigned textureUnit, unsigned bindingTarget, bool enableMipmaps) const never_throws
    {
        if (_prebuiltSamplerMipmaps && _prebuiltSamplerNoMipmaps) {
            assert(_gles300Factory);
            glBindSampler(textureUnit, enableMipmaps ? _prebuiltSamplerMipmaps->AsRawGLHandle() : _prebuiltSamplerNoMipmaps->AsRawGLHandle());
        } else {
            #if defined(_DEBUG)
                // expecting GL_ACTIVE_TEXTURE to be already set to the expected texture unit
                // (which will normally be the case when binding texture, then sampler)
                GLint activeTexture = 0;
                glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
                assert(activeTexture == GL_TEXTURE0 + textureUnit);
            #endif

            if (_gles300Factory)
                glBindSampler(textureUnit, 0);

            glTexParameteri(bindingTarget, GL_TEXTURE_MIN_FILTER, enableMipmaps ? _minFilter : _maxFilter);
            glTexParameteri(bindingTarget, GL_TEXTURE_MAG_FILTER, _maxFilter);
            glTexParameteri(bindingTarget, GL_TEXTURE_WRAP_S, _wrapS);
            glTexParameteri(bindingTarget, GL_TEXTURE_WRAP_T, _wrapT);
            if (_gles300Factory)
                glTexParameteri(bindingTarget, GL_TEXTURE_WRAP_R, _wrapR);

            glTexParameteri(bindingTarget, GL_TEXTURE_COMPARE_MODE, _compareMode);
            glTexParameteri(bindingTarget, GL_TEXTURE_COMPARE_FUNC, _compareFunc);
        }

        CheckGLError("SamplerState::Apply");

        // (border color set elsewhere. Anisotropy requires an extension)
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
    
    CompareOp AsCompareOp(GLenum comparison)
    {
        switch (comparison) {
            case GL_LESS:           return CompareOp::Less;
            case GL_EQUAL:          return CompareOp::Equal;
            case GL_LEQUAL:         return CompareOp::LessEqual;
            case GL_GREATER:        return CompareOp::Greater;
            case GL_NOTEQUAL:       return CompareOp::NotEqual;
            case GL_GEQUAL:         return CompareOp::GreaterEqual;
            case GL_ALWAYS:         return CompareOp::Always;
            default:
            case GL_NEVER:          return CompareOp::Never;
        }
    }

    static unsigned s_nextSamplerStateGUID = 1;

    SamplerState::SamplerState(
        FilterMode filter,
        AddressMode addressU, AddressMode addressV, AddressMode addressW,
        CompareOp comparison)
    : _guid(s_nextSamplerStateGUID)
    {
        s_nextSamplerStateGUID += 2;

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

        // avoid using sampler on Windows; Angle doesn't seem to handle them correctly at this moment:
        // https://bugs.chromium.org/p/angleproject/issues/detail?id=2246
        #if PLATFORMOS_TARGET != PLATFORMOS_WINDOWS
            auto& objectFactory = GetObjectFactory();
            _prebuiltSamplerMipmaps = objectFactory.CreateSampler();
            if (_prebuiltSamplerMipmaps) {
                glSamplerParameteri(_prebuiltSamplerMipmaps->AsRawGLHandle(), GL_TEXTURE_MIN_FILTER, _minFilter);
                glSamplerParameteri(_prebuiltSamplerMipmaps->AsRawGLHandle(), GL_TEXTURE_MAG_FILTER, _maxFilter);
                glSamplerParameteri(_prebuiltSamplerMipmaps->AsRawGLHandle(), GL_TEXTURE_WRAP_S, _wrapS);
                glSamplerParameteri(_prebuiltSamplerMipmaps->AsRawGLHandle(), GL_TEXTURE_WRAP_T, _wrapT);
                glSamplerParameteri(_prebuiltSamplerMipmaps->AsRawGLHandle(), GL_TEXTURE_WRAP_R, _wrapR);

                glSamplerParameteri(_prebuiltSamplerMipmaps->AsRawGLHandle(), GL_TEXTURE_COMPARE_MODE, _compareMode);
                glSamplerParameteri(_prebuiltSamplerMipmaps->AsRawGLHandle(), GL_TEXTURE_COMPARE_FUNC, _compareFunc);
            }

            _prebuiltSamplerNoMipmaps = objectFactory.CreateSampler();
            if (_prebuiltSamplerNoMipmaps) {
                glSamplerParameteri(_prebuiltSamplerNoMipmaps->AsRawGLHandle(), GL_TEXTURE_MIN_FILTER, _maxFilter); /* intentionally using maxFilter in non-mipmap case */
                glSamplerParameteri(_prebuiltSamplerNoMipmaps->AsRawGLHandle(), GL_TEXTURE_MAG_FILTER, _maxFilter);
                glSamplerParameteri(_prebuiltSamplerNoMipmaps->AsRawGLHandle(), GL_TEXTURE_WRAP_S, _wrapS);
                glSamplerParameteri(_prebuiltSamplerNoMipmaps->AsRawGLHandle(), GL_TEXTURE_WRAP_T, _wrapT);
                glSamplerParameteri(_prebuiltSamplerNoMipmaps->AsRawGLHandle(), GL_TEXTURE_WRAP_R, _wrapR);

                glSamplerParameteri(_prebuiltSamplerNoMipmaps->AsRawGLHandle(), GL_TEXTURE_COMPARE_MODE, _compareMode);
                glSamplerParameteri(_prebuiltSamplerNoMipmaps->AsRawGLHandle(), GL_TEXTURE_COMPARE_FUNC, _compareFunc);
            }
        #endif

        _gles300Factory = !!(GetObjectFactory().GetFeatureSet() & FeatureSet::GLES300);

        CheckGLError("Construct Sampler State");
    }

    SamplerState::SamplerState()
    : _guid(s_nextSamplerStateGUID)
    {
        s_nextSamplerStateGUID += 2;
        _minFilter = GL_LINEAR_MIPMAP_LINEAR;
        _maxFilter = GL_LINEAR;
        _compareFunc = AsGLenum(CompareOp::Never);
        _compareMode = GL_NONE;
        _wrapS = AsGLenum(AddressMode::Wrap);
        _wrapT = AsGLenum(AddressMode::Wrap);
        _wrapR = AsGLenum(AddressMode::Wrap);
        // Note -- default constructor must not invoke ObjectFactory::CreateSampler(), because
        // it is used pre-operator= sometimes, when the default constructor is expected to be low overhead
        _gles300Factory = !!(GetObjectFactory().GetFeatureSet() & FeatureSet::GLES300);
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
