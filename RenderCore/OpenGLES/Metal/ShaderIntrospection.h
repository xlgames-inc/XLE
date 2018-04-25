
#pragma once

#include "ObjectFactory.h"
#include "../../Types.h"
#include "../../../Utility/ParameterBox.h"
#include "../../../Utility/IteratorUtils.h"

namespace RenderCore { class ConstantBufferElementDesc; }

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
        DEBUG_ONLY(std::string _name;)
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

            std::string _name;
            DEBUG_ONLY(bool _isBound;)
        };

#if _DEBUG
        std::vector<Uniform> UnboundUniforms() const;
        void MarkBound(HashType uniformName);
#endif

        class Struct
        {
        public:
            std::vector<Uniform> _uniforms;
            std::string _name;
        };

        Uniform     FindUniform(HashType uniformName) const;
        Struct      FindStruct(HashType structName) const;

        ShaderIntrospection(const ShaderProgram& shader);
        ~ShaderIntrospection();
    private:
        std::vector<std::pair<HashType, Struct>> _structs;
    };

    void Bind(DeviceContext& context, const SetUniformCommandGroup& uniforms, IteratorRange<const void*> data);

}}
