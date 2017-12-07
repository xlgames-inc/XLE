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

    RasterizerState::RasterizerState() {}
    void RasterizerState::Apply() const never_throws
    {
        glCullFace(GL_BACK);
        glEnable(GL_CULL_FACE);
        glFrontFace(GL_CCW);
        glLineWidth(1.f);
        glPolygonOffset(0.f, 0.f);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DITHER);       // (not supported in D3D11)
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

    DepthStencilState::DepthStencilState() {}
    void DepthStencilState::Apply() const never_throws
    {
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
        glDepthMask(GL_TRUE);
        glStencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0, ~GLuint(0x0));
        glStencilFuncSeparate(GL_BACK, GL_ALWAYS, 0, ~GLuint(0x0));
        glStencilMaskSeparate(GL_FRONT, ~GLuint(0x0));
        glStencilMaskSeparate(GL_BACK, ~GLuint(0x0));
        glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_KEEP);
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
