// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

// iOS vs macOS
#if __IPHONE_OS_VERSION_MAX_ALLOWED
    #define HACK_PLATFORM_IOS 1
#endif

// ES3 vs desktop
#if __arm__ || __aarch64__
    #define HACK_GLES_3 1
#endif

// ES3-only definitions
#if !HACK_GLES_3
extern void glTexStorage2D (uint32_t target, int32_t levels, uint32_t internalformat, int32_t width, int32_t height);

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

#define GL_INT_SAMPLER_2D                                0x8DCA
#define GL_UNSIGNED_INT_SAMPLER_2D                       0x8DD2
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
#endif // !HACK_GLES_3
