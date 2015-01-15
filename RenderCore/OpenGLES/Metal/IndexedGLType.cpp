// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "IndexedGLType.h"
#include "../../../Utility/StringFormat.h"
#include "../IDeviceOpenGLES.h"
#include <GLES2/gl2.h>
#include <assert.h>
#include <map>

#if PLATFORMOS_ACTIVE == PLATFORMOS_WINDOWS
    extern "C" dll_import void __stdcall OutputDebugStringA(const char*);
#endif

namespace RenderCore { namespace Metal_OpenGLES
{
    signed  IndexedGLType_AddRef(RawGLHandle object) never_throws;
    signed  IndexedGLType_Release(RawGLHandle object) never_throws;

        ////////////////////////////////////////////////////////////////////////////////////////////////

    static unsigned AsGLShaderType(ShaderType::Enum type)
    {
        switch (type) {
        case ShaderType::VertexShader:      return GL_VERTEX_SHADER;
        default:
        case ShaderType::FragmentShader:    return GL_FRAGMENT_SHADER;
        }
    }

    intrusive_ptr<GlObject<GlObject_Type::Shader> >             CreateShader(ShaderType::Enum type)
    {
        auto obj =  (GlObject<GlObject_Type::Shader>*)glCreateShader(AsGLShaderType(type));
        return intrusive_ptr<GlObject<GlObject_Type::Shader>>(obj);
    }

    intrusive_ptr<GlObject<GlObject_Type::ShaderProgram> >      CreateShaderProgram()
    {
        auto temp = (GlObject<GlObject_Type::ShaderProgram>*)glCreateProgram();
        return intrusive_ptr<GlObject<GlObject_Type::ShaderProgram> >(temp);
    }

    intrusive_ptr<GlObject<GlObject_Type::Texture> >            CreateTexture()
    {
        RawGLHandle result = RawGLHandle_Invalid;
        glGenTextures(1, &result);
        auto temp = (GlObject<GlObject_Type::Texture>*)result;
        return intrusive_ptr<GlObject<GlObject_Type::Texture> >(temp);
    }

    intrusive_ptr<GlObject<GlObject_Type::RenderBuffer> >       CreateRenderBuffer()
    {
        RawGLHandle result = RawGLHandle_Invalid;
        glGenRenderbuffers(1, &result);
        auto temp = (GlObject<GlObject_Type::RenderBuffer>*)result;
        return intrusive_ptr<GlObject<GlObject_Type::RenderBuffer> >(temp);
    }

    intrusive_ptr<GlObject<GlObject_Type::FrameBuffer> >        CreateFrameBuffer()
    {
        RawGLHandle result = RawGLHandle_Invalid;
        glGenFramebuffers(1, &result);
        auto temp = (GlObject<GlObject_Type::FrameBuffer>*)result;
        return intrusive_ptr<GlObject<GlObject_Type::FrameBuffer> >(temp);
    }

    intrusive_ptr<GlObject<GlObject_Type::Buffer> >             CreateBuffer()
    {
        RawGLHandle result = RawGLHandle_Invalid;
        glGenBuffers(1, &result);
        auto temp = (GlObject<GlObject_Type::Buffer>*)result;
        return intrusive_ptr<GlObject<GlObject_Type::Buffer> >(temp);
    }

        ////////////////////////////////////////////////////////////////////////////////////////////////

    namespace Detail
    {
        template<int Type> void Destroy(RawGLHandle handle);

        template<> void Destroy<GlObject_Type::Shader>(RawGLHandle object)
        { 
            glDeleteShader(object);
        }
    
        template<> void Destroy<GlObject_Type::ShaderProgram>(RawGLHandle object)
        { 
            glDeleteProgram(object);
        }
    
        template<> void Destroy<GlObject_Type::Texture>(RawGLHandle object)
        { 
            glDeleteTextures(1, &object);
        }
    
        template<> void Destroy<GlObject_Type::RenderBuffer>(RawGLHandle object)
        { 
            glDeleteRenderbuffers(1, &object);
        }

        template<> void Destroy<GlObject_Type::FrameBuffer>(RawGLHandle object)
        { 
            glDeleteFramebuffers(1, &object);
        }
    
        template<> void Destroy<GlObject_Type::Buffer>(RawGLHandle object)
        { 
            glDeleteBuffers(1, (GLuint*)&object); 
        }

        template<> void Destroy<GlObject_Type::Resource>(RawGLHandle object)
        {
                //
                //      We don't know the type of this resource.
                //      fall back to the simpliest possible polymorphism...
                //
                //      Just check the type and 
                //
            if (glIsTexture(object)) {
                glDeleteTextures(1, &object);
            } else if (glIsBuffer(object)) {
                glDeleteBuffers(1, &object);
            } else if (glIsFramebuffer(object)) {
                glDeleteFramebuffers(1, &object);
            } else if (glIsRenderbuffer(object)) {
                glDeleteRenderbuffers(1, &object);
            }
        }
    }

        ////////////////////////////////////////////////////////////////////////////////////////////////

    template<int Type>
        signed      GlObject<Type>::AddRef() const never_throws
    {
        return IndexedGLType_AddRef(RawGLHandle(this));
    }

    template<int Type>
        signed      GlObject<Type>::Release() const never_throws
    {
        signed result = IndexedGLType_Release(RawGLHandle(this));
        if (result == 0) {
            Detail::Destroy<Type>(RawGLHandle(this));
        }
        return result;
    }

    template<int Type>
        GlObject<Type>::GlObject()
    {
    }
        
    template<int Type>
        GlObject<Type>::~GlObject()
    {
        Release();
    }

    #if COMPILER_ACTIVE == COMPILER_TYPE_MSVC        // no explicit instantiation in GCC?
        template GlObject<GlObject_Type::Shader>;
        template GlObject<GlObject_Type::ShaderProgram>;
        template GlObject<GlObject_Type::Texture>;
        template GlObject<GlObject_Type::RenderBuffer>;
        template GlObject<GlObject_Type::FrameBuffer>;
        template GlObject<GlObject_Type::Buffer>;
        template GlObject<GlObject_Type::Resource>;
    #endif

        ////////////////////////////////////////////////////////////////////////////////////////////////

    template<> template<> GlObject<GlObject_Type::Texture>*             GlObject<GlObject_Type::Resource>::ResourceCast()
    {
        if (glIsTexture(RawGLHandle(this))) {
            return (GlObject<GlObject_Type::Texture>*)(this);
        } else {
            return (GlObject<GlObject_Type::Texture>*)RawGLHandle_Invalid;
        }
    }

    template<> template<> GlObject<GlObject_Type::RenderBuffer>*        GlObject<GlObject_Type::Resource>::ResourceCast()
    {
        if (glIsRenderbuffer(RawGLHandle(this))) {
            return (GlObject<GlObject_Type::RenderBuffer>*)(this);
        } else {
            return (GlObject<GlObject_Type::RenderBuffer>*)RawGLHandle_Invalid;
        }
    }

    template<> template<> GlObject<GlObject_Type::FrameBuffer>*         GlObject<GlObject_Type::Resource>::ResourceCast()
    {
        if (glIsFramebuffer(RawGLHandle(this))) {
            return (GlObject<GlObject_Type::FrameBuffer>*)(this);
        } else {
            return (GlObject<GlObject_Type::FrameBuffer>*)RawGLHandle_Invalid;
        }
    }

    template<> template<> GlObject<GlObject_Type::Buffer>*              GlObject<GlObject_Type::Resource>::ResourceCast()
    {
        if (glIsBuffer(RawGLHandle(this))) {
            return (GlObject<GlObject_Type::Buffer>*)(this);
        } else {
            return (GlObject<GlObject_Type::Buffer>*)RawGLHandle_Invalid;
        }
    }

    template<> template<> const GlObject<GlObject_Type::Texture>*       GlObject<GlObject_Type::Resource>::ResourceCast() const
    {
        if (glIsTexture(RawGLHandle(this))) {
            return (GlObject<GlObject_Type::Texture>*)(this);
        } else {
            return (GlObject<GlObject_Type::Texture>*)RawGLHandle_Invalid;
        }
    }

    template<> template<> const GlObject<GlObject_Type::RenderBuffer>*    GlObject<GlObject_Type::Resource>::ResourceCast() const
    {
        if (glIsRenderbuffer(RawGLHandle(this))) {
            return (GlObject<GlObject_Type::RenderBuffer>*)(this);
        } else {
            return (GlObject<GlObject_Type::RenderBuffer>*)RawGLHandle_Invalid;
        }
    }

    template<> template<> const GlObject<GlObject_Type::FrameBuffer>*     GlObject<GlObject_Type::Resource>::ResourceCast() const
    {
        if (glIsFramebuffer(RawGLHandle(this))) {
            return (GlObject<GlObject_Type::FrameBuffer>*)(this);
        } else {
            return (GlObject<GlObject_Type::FrameBuffer>*)RawGLHandle_Invalid;
        }
    }

    template<> template<> const GlObject<GlObject_Type::Buffer>*          GlObject<GlObject_Type::Resource>::ResourceCast() const
    {
        if (glIsBuffer(RawGLHandle(this))) {
            return (GlObject<GlObject_Type::Buffer>*)(this);
        } else {
            return (GlObject<GlObject_Type::Buffer>*)RawGLHandle_Invalid;
        }
    }

    template<int Type> template <int OtherType> GlObject<OtherType>*        GlObject<Type>::ResourceCast() never_throws         { return RawGLHandle_Invalid; }
    template<int Type> template <int OtherType> const GlObject<OtherType>*  GlObject<Type>::ResourceCast() const never_throws   { return RawGLHandle_Invalid; }

        ////////////////////////////////////////////////////////////////////////////////////////////////

    signed IndexedGLType_AddRef(unsigned object) never_throws
    {
        GlobalResources& globalResources = GetGlobalResources();
            //  not thread safe!
        auto i = globalResources._refCountTable.find(object);
        if (i==globalResources._refCountTable.end()) {
            globalResources._refCountTable.insert(std::make_pair(object, 1));
            return 1;
        }
        return ++i->second;
    }

    signed IndexedGLType_Release(unsigned object) never_throws
    {
        GlobalResources& globalResources = GetGlobalResources();
            //  not thread safe!
        auto i = globalResources._refCountTable.find(object);
        if (i==globalResources._refCountTable.end()) {
            assert(0);
            return -1;
        }
        signed result = --i->second;
        if (!result) {
            globalResources._refCountTable.erase(i);
        }
        return result;
    }

    #if PLATFORMOS_ACTIVE == PLATFORMOS_WINDOWS
        static void OutputString(const char string[])
        {
            OutputDebugStringA(string);
        }
    #else
        static void OutputString(const char string[])
        {
            printf("%s", string);
        }
    #endif

    void ReportLeaks()
    {
        GlobalResources& globalResources = GetGlobalResources();
        OutputString(XlDynFormatString("[OpenGL Leaks] %i objects remain\n", globalResources._refCountTable.size()).c_str());
        auto count=0u;
        for (std::map<unsigned, signed>::const_iterator i=globalResources._refCountTable.cbegin(); i!=globalResources._refCountTable.cend(); ++i, ++count) {
            OutputString(XlDynFormatString("  [%i] Object (%i) has (%i) refs\n", count, i->first, i->second).c_str());
        }
    }

}}

