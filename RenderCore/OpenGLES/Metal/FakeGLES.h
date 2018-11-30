// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IncludeGLES.h"

// We can compile against a desktop version of OpenGL, and use part of the feature set, by stubbing
// out some functions and features. These features won't work correctly -- but it can be useful
// for just getting basic functionality on
#if !defined(GL_ES_VERSION_2_0) && !defined(GL_ES_VERSION_3_0)

#include <stdint.h>

#if defined(__cplusplus)
    extern "C" {
#endif

extern void glTexStorage2D (uint32_t target, int32_t levels, uint32_t internalformat, int32_t width, int32_t height);
extern void* glMapBufferRange (uint32_t target, intptr_t offset, intptr_t length, uint32_t access);
extern void glBindSampler (uint32_t unit, uint32_t sampler);
extern void glSamplerParameteri (uint32_t sampler, uint32_t pname, int32_t param);
extern void glGenSamplers (int32_t count, uint32_t* samplers);
extern void glDeleteSamplers (int32_t count, const uint32_t* samplers);
extern uint8_t glIsSampler (uint32_t);
extern uint8_t glIsTransformFeedback (uint32_t);
extern uint8_t glIsVertexArray (uint32_t);
extern void glClearBufferfv (uint32_t buffer, int32_t drawbuffer, const float *value);
extern void glClearBufferfi (uint32_t buffer, int32_t drawbuffer, float depth, int32_t stencil);
extern void glClearBufferiv (uint32_t buffer, int32_t drawbuffer, const int32_t *value);
extern void glClearBufferuiv (uint32_t buffer, int32_t drawbuffer, const uint32_t *value);
extern void glInvalidateFramebuffer (uint32_t target, int32_t numAttachments, const uint32_t* attachments);
extern void glVertexAttribDivisor(uint32_t index, uint32_t divisor);
extern void glBindVertexArray (uint32_t array);
extern void glDeleteVertexArrays (int32_t n, const uint32_t *arrays);
extern void glGenVertexArrays (int32_t n, uint32_t *arrays);

#define glClearDepthf       glClearDepth

#if defined(__cplusplus)
    }
#endif

#define GL_TEXTURE_2D_ARRAY                              0x8C1A
#define GL_TEXTURE_BINDING_2D_ARRAY                      0x8C1D
#define GL_COPY_READ_BUFFER                              0x8F36
#define GL_COPY_WRITE_BUFFER                             0x8F37
#define GL_COPY_READ_BUFFER_BINDING                      GL_COPY_READ_BUFFER
#define GL_COPY_WRITE_BUFFER_BINDING                     GL_COPY_WRITE_BUFFER
#define GL_TRANSFORM_FEEDBACK_BUFFER                     0x8C8E
#define GL_TRANSFORM_FEEDBACK_BUFFER_BINDING             0x8C8F
#define GL_UNIFORM_BUFFER                                0x8A11
#define GL_UNIFORM_BUFFER_BINDING                        0x8A28

#define GL_FIXED                                         0x140C
#define GL_INT_2_10_10_10_REV                            0x8D9F
#define GL_UNSIGNED_INT_10F_11F_11F_REV                  0x8C3B
#define GL_UNSIGNED_INT_5_9_9_9_REV                      0x8C3E
#define GL_RGBA_INTEGER                                  0x8D99
#define GL_RED_INTEGER                                   0x8D94
#define GL_SAMPLER_2D_ARRAY                              0x8DC1
#define GL_SAMPLER_2D_ARRAY_SHADOW                       0x8DC4
#define GL_SAMPLER_CUBE_SHADOW                           0x8DC5
#define GL_INT_SAMPLER_2D                                0x8DCA
#define GL_INT_SAMPLER_3D                                0x8DCB
#define GL_INT_SAMPLER_CUBE                              0x8DCC
#define GL_INT_SAMPLER_2D_ARRAY                          0x8DCF
#define GL_UNSIGNED_INT_SAMPLER_2D                       0x8DD2
#define GL_UNSIGNED_INT_SAMPLER_3D                       0x8DD3
#define GL_UNSIGNED_INT_SAMPLER_CUBE                     0x8DD4
#define GL_UNSIGNED_INT_SAMPLER_2D_ARRAY                 0x8DD7
#define GL_R8_SNORM                                      0x8F94
#define GL_RG8_SNORM                                     0x8F95
#define GL_RGB8_SNORM                                    0x8F96
#define GL_RGB8UI                                        0x8D7D
#define GL_RGB8I                                         0x8D8F
#define GL_R11F_G11F_B10F                                0x8C3A
#define GL_RGB9_E5                                       0x8C3D
#define GL_RGB16F                                        0x881B
#define GL_RGB32F                                        0x8815
#define GL_RGB16UI                                       0x8D77
#define GL_RGB16I                                        0x8D89
#define GL_RGB32UI                                       0x8D71
#define GL_RGB32I                                        0x8D83
#define GL_RGBA8_SNORM                                   0x8F97
#define GL_RGBA8UI                                       0x8D7C
#define GL_RGBA8I                                        0x8D8E
#define GL_RGB10_A2UI                                    0x906F
#define GL_RGBA16F                                       0x881A
#define GL_RGBA32F                                       0x8814
#define GL_RGBA16UI                                      0x8D76
#define GL_RGBA16I                                       0x8D88
#define GL_RGBA32I                                       0x8D82
#define GL_RGBA32UI                                      0x8D70

#define GL_RGBA4                                         0x8056
#define GL_RGB5_A1                                       0x8057
#define GL_RGB565                                        0x8D62
#define GL_RGB_INTEGER                                   0x8D98

#define GL_COMPRESSED_R11_EAC                            0x9270
#define GL_COMPRESSED_SIGNED_R11_EAC                     0x9271
#define GL_COMPRESSED_RG11_EAC                           0x9272
#define GL_COMPRESSED_SIGNED_RG11_EAC                    0x9273
#define GL_COMPRESSED_RGB8_ETC2                          0x9274
#define GL_COMPRESSED_SRGB8_ETC2                         0x9275
#define GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2      0x9276
#define GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2     0x9277
#define GL_COMPRESSED_RGBA8_ETC2_EAC                     0x9278
#define GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC              0x9279
#define GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG                      0x8C00
#define GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG                      0x8C01
#define GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG                     0x8C02
#define GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG                     0x8C03

#define GL_COMPARE_REF_TO_TEXTURE                        0x884E

#define GL_MAP_READ_BIT                                  0x0001
#define GL_MAP_WRITE_BIT                                 0x0002
#define GL_MAP_INVALIDATE_RANGE_BIT                      0x0004
#define GL_MAP_INVALIDATE_BUFFER_BIT                     0x0008
#define GL_MAP_FLUSH_EXPLICIT_BIT                        0x0010
#define GL_MAP_UNSYNCHRONIZED_BIT                        0x0020

#define GL_VERTEX_ARRAY_BINDING                         0x85B5

#define GL_ACTIVE_UNIFORM_BLOCKS                        0x8A36
#define GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH         0x8A35
#define GL_PROGRAM_BINARY_RETRIEVABLE_HINT              0x8257
#define GL_TRANSFORM_FEEDBACK_BUFFER_MODE               0x8C7F
#define GL_TRANSFORM_FEEDBACK_VARYINGS                  0x8C83
#define GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH        0x8C76

#endif


