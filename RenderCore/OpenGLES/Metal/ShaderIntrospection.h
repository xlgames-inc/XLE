
#pragma once

#include "IndexedGLType.h"
#include "../../Types.h"
#include "../../../Utility/ParameterBox.h"
#include "../../../Utility/IteratorUtils.h"

namespace RenderCore { class ConstantBufferElement; }

namespace RenderCore { namespace Metal_OpenGLES
{
    class ShaderProgram;

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
    };

    class ShaderIntrospection
    {
    public:
        using HashType = uint64_t;

        SetUniformCommandGroup  MakeBinding(HashType uniformStructName, IteratorRange<const ConstantBufferElement*> inputElements);

        class Uniform
        {
        public:
            HashType    _bindingName;
            int         _location;
            GLenum      _type;
            int         _elementCount;

            DEBUG_ONLY(std::string _name;)
        };

        Uniform                FindUniform(HashType uniformName) const;

        ShaderIntrospection(const ShaderProgram& shader);
        ~ShaderIntrospection();
    private:
        class Struct
        {
        public:
            std::vector<Uniform> _uniforms;
        };
        std::vector<std::pair<HashType, Struct>> _structs;
    };

    void Bind(DeviceContext& context, const SetUniformCommandGroup& uniforms, IteratorRange<const void*> data);

}}
