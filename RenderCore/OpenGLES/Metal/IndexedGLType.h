// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../../Core/Exceptions.h"
#include "../../../Utility/IntrusivePtr.h"
#include <map>

namespace RenderCore { namespace Metal_OpenGLES
{
        ////////////////////////////////////////////////////////////////////////////////////////////////

    typedef unsigned        RawGLHandle;
    static const unsigned   RawGLHandle_Invalid = 0;

    namespace GlObject_Type { enum Enum { Shader, ShaderProgram, Texture, RenderBuffer, FrameBuffer, Buffer, Resource }; }
    namespace ShaderType    { enum Enum { VertexShader, FragmentShader }; }

    void                    ReportLeaks();

        ////////////////////////////////////////////////////////////////////////////////////////////////

    template <int Type> class GlObject
    {
    public:
        signed      AddRef() const never_throws;
        signed      Release() const never_throws;

        template <int OtherType> GlObject<OtherType>*       As() never_throws;
        template <int OtherType> const GlObject<OtherType>* As() const never_throws;

        friend intrusive_ptr<GlObject<GlObject_Type::Shader> >         CreateShader(ShaderType::Enum type);
        friend intrusive_ptr<GlObject<GlObject_Type::ShaderProgram> >  CreateShaderProgram();
        friend intrusive_ptr<GlObject<GlObject_Type::Texture> >        CreateTexture();
        friend intrusive_ptr<GlObject<GlObject_Type::RenderBuffer> >   CreateRenderBuffer();
        friend intrusive_ptr<GlObject<GlObject_Type::FrameBuffer> >    CreateFrameBuffer();
        friend intrusive_ptr<GlObject<GlObject_Type::Buffer> >         CreateBuffer();

    private:
        GlObject();
        ~GlObject();

        friend class GlObject<GlObject_Type::Resource>;
        template <int OtherType> GlObject<OtherType>* ResourceCast() never_throws;
        template <int OtherType> const GlObject<OtherType>* ResourceCast() const never_throws;
    };
}}

namespace OpenGL
{
    typedef RenderCore::Metal_OpenGLES::GlObject<RenderCore::Metal_OpenGLES::GlObject_Type::Shader>             Shader;
    typedef RenderCore::Metal_OpenGLES::GlObject<RenderCore::Metal_OpenGLES::GlObject_Type::ShaderProgram>      ShaderProgram;
    typedef RenderCore::Metal_OpenGLES::GlObject<RenderCore::Metal_OpenGLES::GlObject_Type::Buffer>             Buffer;
    typedef RenderCore::Metal_OpenGLES::GlObject<RenderCore::Metal_OpenGLES::GlObject_Type::Texture>            Texture;
    typedef RenderCore::Metal_OpenGLES::GlObject<RenderCore::Metal_OpenGLES::GlObject_Type::RenderBuffer>       RenderBuffer;
    typedef RenderCore::Metal_OpenGLES::GlObject<RenderCore::Metal_OpenGLES::GlObject_Type::FrameBuffer>        FrameBuffer;
    typedef RenderCore::Metal_OpenGLES::GlObject<RenderCore::Metal_OpenGLES::GlObject_Type::Resource>           Resource;
}
    
namespace RenderCore { namespace Metal_OpenGLES
{
    intrusive_ptr<OpenGL::Shader>              CreateShader(ShaderType::Enum type);
    intrusive_ptr<OpenGL::ShaderProgram>       CreateShaderProgram();
    intrusive_ptr<OpenGL::Buffer>              CreateBuffer();
    intrusive_ptr<OpenGL::Texture>             CreateTexture();
    intrusive_ptr<OpenGL::RenderBuffer>        CreateRenderBuffer();
    intrusive_ptr<OpenGL::FrameBuffer>         CreateFrameBuffer();

    #pragma warning(push)
    #pragma warning(disable: 4127)      // conditional expression is constant

        template <int Type>
            template <int OtherType>
                GlObject<OtherType>* GlObject<Type>::As() never_throws
        {
            if (Type == OtherType || OtherType == GlObject_Type::Resource) {
                return (GlObject<OtherType>*)this;
            } 
            return (GlObject<OtherType>*)RawGLHandle_Invalid;
        }

        template <>
            template <int OtherType>
                GlObject<OtherType>* GlObject<GlObject_Type::Resource>::As() never_throws
        {
            if (OtherType == GlObject_Type::Resource) {
                return (GlObject<OtherType>*)this;
            } 
            return ResourceCast<OtherType>();
        }

        template <int Type>
            template <int OtherType>
                const GlObject<OtherType>* GlObject<Type>::As() const never_throws
        {
            if (Type == OtherType || OtherType == GlObject_Type::Resource) {
                return (GlObject<OtherType>*)this;
            } 
            return (GlObject<OtherType>*)RawGLHandle_Invalid;
        }

        template <>
            template <int OtherType>
                const GlObject<OtherType>* GlObject<GlObject_Type::Resource>::As() const never_throws
        {
            if (OtherType == GlObject_Type::Resource) {
                return (GlObject<OtherType>*)this;
            } 
            return ResourceCast<OtherType>();
        }

    #pragma warning(pop)

    class GlobalResources
    {
    public:
        std::map<unsigned, signed> _refCountTable;
    };
}}
