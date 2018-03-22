// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FakeGLES.h"

#if !defined(GL_ES_VERSION_2_0) && !defined(GL_ES_VERSION_3_0)

extern "C" void glTexStorage2D (uint32_t target, int32_t levels, uint32_t internalformat, int32_t width, int32_t height) {}
extern "C" void* glMapBufferRange (uint32_t target, intptr_t offset, intptr_t length, uint32_t access) { return nullptr; }
extern "C" void glBindSampler (uint32_t unit, uint32_t sampler) {}
extern "C" void glSamplerParameteri (uint32_t sampler, uint32_t pname, int32_t param) {}
extern "C" void glGenSamplers (int32_t count, uint32_t* samplers)
{
    for (unsigned c=0; c<count; ++c)
        samplers[c] = 0;
}
extern "C" void glDeleteSamplers (int32_t count, const uint32_t* samplers) {}
extern "C" uint8_t glIsSampler (uint32_t sampler) { return 0; }
extern "C" void glClearBufferfv (uint32_t buffer, int32_t drawbuffer, const float *value) {}
extern "C" void glClearBufferfi (uint32_t buffer, int32_t drawbuffer, float depth, int32_t stencil) {}
extern "C" void glClearBufferiv (uint32_t buffer, int32_t drawbuffer, const int32_t *value) {}
extern "C" void glInvalidateFramebuffer (uint32_t target, int32_t numAttachments, const uint32_t* attachments) {}
extern "C" void glVertexAttribDivisor(uint32_t index, uint32_t divisor) {}

#endif

