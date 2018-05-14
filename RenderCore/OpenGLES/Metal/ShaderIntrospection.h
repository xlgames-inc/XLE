
#pragma once

#include "ObjectFactory.h"
#include "../../Types.h"
#include "../../../Utility/ParameterBox.h"
#include "../../../Utility/IteratorUtils.h"
#include <ostream>

namespace RenderCore { class ConstantBufferElementDesc; }

namespace RenderCore { namespace Metal_OpenGLES
{
    class ShaderProgram;

    #if defined(_DEBUG)
        #define STORE_UNIFORM_NAMES
    #endif

    /// <summary>A simple abstraction for multiple uniform "set" operations.</summary>
    /// This can hold data for multiple uniforms, plus the parameters for the OpenGL "glUniform..."
    /// call that is required to set them.
    /// It's not exactly the same as a "constant buffer", because it supports sparse data, and some
    /// conversion during the "set" operation... But it's a similar abstraction that works with OpenGL
    /// concepts.
    class SetUniformCommandGroup
    {
    public:
        struct SetCommand { int _location; GLenum _type; unsigned _count; size_t _dataOffset; };
        std::vector<SetCommand> _commands;
        #if defined(STORE_UNIFORM_NAMES)
            std::string _name;
        #endif

        friend std::ostream& operator<<(std::ostream&, const SetUniformCommandGroup&);
    };

    class ShaderIntrospection
    {
    public:
        using HashType = uint64_t;

        SetUniformCommandGroup  MakeBinding(HashType uniformStructName, IteratorRange<const ConstantBufferElementDesc*> inputElements);

        class Uniform
        {
        public:
            HashType    _bindingName;
            int         _location;
            GLenum      _type;
            int         _elementCount;

            #if defined(STORE_UNIFORM_NAMES)
                std::string _name;
            #endif
        };

        class Struct
        {
        public:
            std::vector<Uniform> _uniforms;

            #if defined(STORE_UNIFORM_NAMES)
                std::string _name;
            #endif
        };

        Uniform     FindUniform(HashType uniformName) const;
        Struct      FindStruct(HashType structName) const;

        IteratorRange<const std::pair<HashType, Struct>*> GetStructs() const { return MakeIteratorRange(_structs); };

        ShaderIntrospection(const ShaderProgram& shader);
        ~ShaderIntrospection();
    private:
        std::vector<std::pair<HashType, Struct>> _structs;
    };

    void Bind(DeviceContext& context, const SetUniformCommandGroup& uniforms, IteratorRange<const void*> data);

}}
