// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "State.h"
#include "IncludeGLES.h"

namespace RenderCore { namespace Metal_OpenGLES
{

    SamplerState::SamplerState() {}
    void SamplerState::Apply(unsigned samplerIndex) const never_throws
    {
        if (samplerIndex < GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS) {
            glActiveTexture(GL_TEXTURE0 + samplerIndex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

                // (border color set elsewhere. Anisotrophy requires an extension)
        }
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
