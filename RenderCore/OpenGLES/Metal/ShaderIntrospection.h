
#pragma once

#include "IndexedGLType.h"
#include "../../Types.h"
#include "../../../Utility/ParameterBox.h"
#include "../../../Utility/IteratorUtils.h"

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
        using HashType = uint64; // ParameterBox::ParameterNameHash;

        SetUniformCommandGroup MakeBinding(HashType uniformStructName, IteratorRange<const MiniInputElementDesc*> inputElements);

        ShaderIntrospection(const ShaderProgram& shader);
        ~ShaderIntrospection();
    private:
        class Struct
        {
        public:
            class Uniform
            {
            public:
                HashType    _bindingName;
                int         _location;
                GLenum      _type;
                int         _count;

                DEBUG_ONLY(std::string _name;)
            };

            std::vector<Uniform> _uniforms;
        };
        std::vector<std::pair<HashType, Struct>> _structs;
    };

    // SetUniformCommandGroup MakeBinding(const UnboundShaderUniformGroup& unbound, const Techniques::PredefinedCBLayout& layout);
    // SetUniformCommandGroup MakeBinding(const UnboundShaderUniformGroup& unbound, IteratorRange<const MiniInputElementDesc*> inputElements);
    void Bind(DeviceContext& context, const SetUniformCommandGroup& uniforms, IteratorRange<const void*> data);


}}

