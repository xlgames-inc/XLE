// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Format.h"     // (for FeatureSet)
#include "../../../Core/Exceptions.h"
#include "../../../Utility/IntrusivePtr.h"
#include "../../../Utility/Threading/Mutex.h"
#include <unordered_map>

typedef uint32_t GLenum;

namespace RenderCore { class IDevice; class IResource; }

namespace RenderCore { namespace Metal_OpenGLES
{
    #if defined(_DEBUG)
        void CheckGLError(const char context[]);
    #else
        inline void CheckGLError(const char context[]) {}
    #endif
    
        ////////////////////////////////////////////////////////////////////////////////////////////////

    using RawGLHandle = uint32_t;
    static const RawGLHandle   RawGLHandle_Invalid = 0;

    namespace GlObject_Type { enum Enum { Shader, ShaderProgram, Texture, RenderBuffer, FrameBuffer, Buffer, Resource, Sampler, VAO }; }
    namespace ShaderType    { enum Enum { VertexShader, FragmentShader }; }

        ////////////////////////////////////////////////////////////////////////////////////////////////

    template <int Type> class GlObject
    {
    public:
        signed      AddRef() const never_throws;
        signed      Release() const never_throws;

        template <int OtherType> GlObject<OtherType>*       As() never_throws;
        template <int OtherType> const GlObject<OtherType>* As() const never_throws;

        RawGLHandle AsRawGLHandle() const never_throws;

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
    using Shader        = RenderCore::Metal_OpenGLES::GlObject<RenderCore::Metal_OpenGLES::GlObject_Type::Shader>;
    using ShaderProgram = RenderCore::Metal_OpenGLES::GlObject<RenderCore::Metal_OpenGLES::GlObject_Type::ShaderProgram>;
    using Buffer        = RenderCore::Metal_OpenGLES::GlObject<RenderCore::Metal_OpenGLES::GlObject_Type::Buffer>;
    using Texture       = RenderCore::Metal_OpenGLES::GlObject<RenderCore::Metal_OpenGLES::GlObject_Type::Texture>;
    using RenderBuffer  = RenderCore::Metal_OpenGLES::GlObject<RenderCore::Metal_OpenGLES::GlObject_Type::RenderBuffer>;
    using FrameBuffer   = RenderCore::Metal_OpenGLES::GlObject<RenderCore::Metal_OpenGLES::GlObject_Type::FrameBuffer>;
    using Resource      = RenderCore::Metal_OpenGLES::GlObject<RenderCore::Metal_OpenGLES::GlObject_Type::Resource>;
    using Sampler       = RenderCore::Metal_OpenGLES::GlObject<RenderCore::Metal_OpenGLES::GlObject_Type::Sampler>;
    using VAO           = RenderCore::Metal_OpenGLES::GlObject<RenderCore::Metal_OpenGLES::GlObject_Type::VAO>;
}
    
namespace RenderCore { namespace Metal_OpenGLES
{
    class ObjectFactory
    {
    public:
        intrusive_ptr<GlObject<GlObject_Type::Shader> >         CreateShader(ShaderType::Enum type);
        intrusive_ptr<GlObject<GlObject_Type::ShaderProgram> >  CreateShaderProgram();
        intrusive_ptr<GlObject<GlObject_Type::Texture> >        CreateTexture();
        intrusive_ptr<GlObject<GlObject_Type::RenderBuffer> >   CreateRenderBuffer();
        intrusive_ptr<GlObject<GlObject_Type::FrameBuffer> >    CreateFrameBuffer();
        intrusive_ptr<GlObject<GlObject_Type::Buffer> >         CreateBuffer();
        intrusive_ptr<GlObject<GlObject_Type::Sampler> >        CreateSampler();
        intrusive_ptr<GlObject<GlObject_Type::VAO> >            CreateVAO();

        signed  IndexedGLType_AddRef(unsigned type, RawGLHandle object) never_throws;
        signed  IndexedGLType_Release(unsigned type, RawGLHandle object) never_throws;
        void    ReportLeaks();

        FeatureSet::BitField GetFeatureSet() const { return _featureSet; }

        ObjectFactory(FeatureSet::BitField featureSet);
        ~ObjectFactory();

        ObjectFactory& operator=(const ObjectFactory&) = delete;
        ObjectFactory(const ObjectFactory&) = delete;

        static bool WriteObjectLabels();
    private:
        std::vector<std::pair<uint64_t, signed>> _refCountTable;
        Threading::Mutex _refCountTableLock;
        FeatureSet::BitField _featureSet;
    };

    class DeviceContext;

    ObjectFactory& GetObjectFactory(IDevice& device);
    ObjectFactory& GetObjectFactory(IResource& resource);
    ObjectFactory& GetObjectFactory(DeviceContext&);
    ObjectFactory& GetObjectFactory();

    template<typename Type>
        intrusive_ptr<Type> MakeTrackingPointer(unsigned inputHandle)
        {
            return {(Type*)(size_t)inputHandle};
        }

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

    template <int Type>
        inline RawGLHandle GlObject<Type>::AsRawGLHandle() const never_throws
    {
        return (RawGLHandle)(size_t)this;
    }

    inline bool ObjectFactory::WriteObjectLabels()
    {
        #if defined(_DEBUG)
            return true;
        #else
            return false;
        #endif
    }

}}
