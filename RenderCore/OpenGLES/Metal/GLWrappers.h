//
//  GLWrappers.h
//  cocos3d-library-iOS
//
//  Created by David Jewsbury on 10/31/17.
//  Copyright © 2017 Pocket Gems Inc. All rights reserved.
//

#pragma once

#include "IncludeGLES.h"

#if !defined(GL_FUNC_ATTRIBUTES)
    #if PLATFORMOS_TARGET == PLATFORMOS_ANDROID
        #define GL_FUNC_ATTRIBUTES __attribute__((pcs("aapcs")))
    #elif PROJECT_ANGLE
        #define GL_FUNC_ATTRIBUTES __attribute__((stdcall))
    #else
        #define GL_FUNC_ATTRIBUTES
    #endif
#endif

typedef struct glWrappersTag
{
    void (*TexImage2D)(
        GLenum target,
        GLint level,
        GLint internalFormat,
        GLsizei width,
        GLsizei height,
        GLint border,
        GLenum format,
        GLenum type,
        const GLvoid * data) GL_FUNC_ATTRIBUTES;

    void (*TexStorage2D)(
        GLenum target,
        GLsizei levels,
        GLenum internalformat,
        GLsizei width,
        GLsizei height) GL_FUNC_ATTRIBUTES;

    void (*CompressedTexImage2D)(
        GLenum target,
        GLint level,
        GLenum internalformat,
        GLsizei width,
        GLsizei height,
        GLint border,
        GLsizei imageSize,
        const GLvoid * data) GL_FUNC_ATTRIBUTES;

    void (*RenderbufferStorage)(
        GLenum target,
        GLenum internalformat,
        GLsizei width,
        GLsizei height) GL_FUNC_ATTRIBUTES;

    void (*BufferData)(
        GLenum target,
        GLsizeiptr size,
        const GLvoid * data,
        GLenum usage) GL_FUNC_ATTRIBUTES;

    void (*DeleteBuffers)(
        GLsizei n,
        const GLuint * buffers) GL_FUNC_ATTRIBUTES;

    void (*DeleteRenderbuffers)(
        GLsizei n,
        const GLuint * renderbuffers) GL_FUNC_ATTRIBUTES;

    void (*DeleteTextures)(
        GLsizei n,
        const GLuint * textures) GL_FUNC_ATTRIBUTES;
} glWrappers;

#if defined(__cplusplus)
    extern "C" {
#endif

glWrappers* GetGLWrappers();

#if !defined(_WIN32) && !defined(ENABLE_GL_WRAPPERS)
    #define ENABLE_GL_WRAPPERS 1
#endif

#if ENABLE_GL_WRAPPERS
    #define GL_WRAP(X) (GetGLWrappers()->X)
#else
    #define GL_WRAP(X) gl##X
#endif

#if defined(__cplusplus)
    }
#endif
