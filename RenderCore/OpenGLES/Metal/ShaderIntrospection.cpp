
#include "ShaderIntrospection.h"
#include "Shader.h"
#include "Format.h"
#include "../../UniformsStream.h"        // (for ConstantBufferElementDesc)
#include "../../../Utility/IteratorUtils.h"
#include "../../../Utility/MemoryUtils.h"

#include "IncludeGLES.h"

namespace RenderCore { namespace Metal_OpenGLES
{

    SetUniformCommandGroup ShaderIntrospection::MakeBinding(
        HashType uniformStructName,
        IteratorRange<const ConstantBufferElementDesc*> inputElements)
    {
        SetUniformCommandGroup result;
        auto s = LowerBound(_structs, uniformStructName);
        if (s == _structs.end()) {
#if DEBUG
            // Try internals instead in a debug build, and throw an exception if found
            for (const auto &ele : inputElements) {
                const auto &found = FindUniform(ele._semanticHash);
                if (found._bindingName == 0) {
                    continue;
                }
                auto basicType = GLUniformTypeAsTypeDesc(found._type);
                auto inputBasicType = AsImpliedType(ele._nativeFormat);
                if (basicType == inputBasicType) {
                    Throw(Exceptions::BasicLabel("Found a uniform outside of a struct. All uniforms must be located within structs. Problem uniform: %s", found._name.c_str()));
                }
            }
#endif
            return result;
        }

        for (const auto& i:s->second._uniforms) {
            auto bindingName = i._bindingName;
            auto b = std::find_if(inputElements.begin(), inputElements.end(),
                                  [bindingName](const ConstantBufferElementDesc& e) { return e._semanticHash == bindingName; });
            if (b != inputElements.end()) {
                // Check for compatibility of types.
                assert(i._elementCount == b->_arrayElementCount); // "Array uniforms within structs not currently supported");
                auto basicType = GLUniformTypeAsTypeDesc(i._type);
                auto inputBasicType = AsImpliedType(b->_nativeFormat);
                if (basicType == inputBasicType) {
                    result._commands.push_back({i._location, i._type, (unsigned)i._elementCount, b->_offset });
                } else {
                    assert(0);
                    /*#if DEBUG
                        if (limitFrequency(ObjCHashCombine((size_t)i._bindingName, (size_t)"UniformTypeMsg"), 1)) {
                            NSLog(@"[limited] warning -- binding shader struct to predefined layout failed because of type mismatch");
                        }
                    #endif*/
                }

                #if defined(_DEBUG)
                const_cast<Uniform&>(i)._isBound = true;
                #endif
            }
        }

        #if defined(_DEBUG)
            result._name = s->second._name;
        #endif

        return result;
    }

    auto ShaderIntrospection::FindUniform(HashType uniformName) const -> Uniform
    {
        // This only finds global uniforms
        auto globals = LowerBound(_structs, 0ull);
        if (globals == _structs.end() || globals->first != 0) return {0,0,0,0};

        auto i = std::find_if(
            globals->second._uniforms.begin(), globals->second._uniforms.end(),
            [uniformName](const Uniform& u) { return (uniformName >= u._bindingName) && (uniformName < u._bindingName+u._elementCount); });
        if (i == globals->second._uniforms.end()) return {0,0,0,0};

        return *i;
    }

    auto ShaderIntrospection::FindStruct(HashType structName) const -> Struct
    {
        auto str = LowerBound(_structs, structName);
        if (str != _structs.end() || str->first == structName)
            return str->second;
        return {};
    }

    static uint64_t HashVariableName(StringSection<> name)
    {
        // If the variable name has array indexor syntax in there, we must strip it off
        // the end of the name.
        if (name.size() >= 2 && name[name.size()-1] == ']') {
            auto i = &name[name.size()-2];
            while (i > name.begin() && *i >= '0' && *i <= '9') --i;
            if (*i == '[')
                return HashVariableName(MakeStringSection(name.begin(), i));        // (this can strip off another one for mutli-dimensional arrays!)
        }
        return Hash64(name);
    }

    ShaderIntrospection::ShaderIntrospection(const ShaderProgram& program)
    {
        // Iterate through the shader interface and pull out the information that's interesting
        // to us.
        // glUseProgram();

        auto glProgram = program.GetUnderlying()->AsRawGLHandle();

        GLint uniformCount, uniformMaxNameLength;
        glGetProgramiv(glProgram, GL_ACTIVE_UNIFORMS, &uniformCount);
        glGetProgramiv(glProgram, GL_ACTIVE_UNIFORM_MAX_LENGTH, &uniformMaxNameLength);

        char nameBuffer[uniformMaxNameLength];
        std::memset(nameBuffer, 0, uniformMaxNameLength);

        for (unsigned c=0; c<uniformCount; ++c) {
            GLint size = 0;
            GLenum type = 0;
            glGetActiveUniform(glProgram, c, uniformMaxNameLength, nullptr, &size, &type, nameBuffer);

            if (XlComparePrefix(nameBuffer, "gl_", 3) == 0) {
                // Variables starting with gl_ are system variables, we can just ignore them
                continue;
            }

            auto location = glGetUniformLocation(glProgram, nameBuffer);

            ////////////////////////////////////////////////
            auto fullName = MakeStringSection(nameBuffer);
            auto firstDot = std::find(fullName.begin(), fullName.end(), '.');
            if (firstDot != fullName.end()) {
                assert(std::find(firstDot+1, fullName.end(), '.'));     // can't support multiple layers of indirection

                auto structName = MakeStringSection(fullName.begin(), firstDot);
                auto structNameHash = Hash64(structName);

                auto i = LowerBound(_structs, structNameHash);
                if (i==_structs.end() || i->first != structNameHash)
                    i = _structs.insert(i, std::make_pair(structNameHash, Struct{{}
                    #if defined(_DEBUG)
                        , structName.AsString()
                    #endif
                        }));

                i->second._uniforms.emplace_back(
                    Uniform {
                        HashVariableName(MakeStringSection(firstDot+1, fullName.end())),
                        location, type, size

                        #if defined(_DEBUG)
                            , fullName.AsString()
                            , false
                        #endif
                    });
            } else {
                // for global uniform, add into dummy struct at "0"
                auto i = LowerBound(_structs, HashType(0));
                if (i==_structs.end() || i->first != 0)
                    i = _structs.insert(i, std::make_pair(0, Struct{{}
                    #if defined(_DEBUG)
                        , "global"
                    #endif
                        }));

                i->second._uniforms.emplace_back(
                    Uniform {
                        HashVariableName(fullName),
                        location, type, size

                        #if defined(_DEBUG)
                            , fullName.AsString()
                            , false
                        #endif
                    });
            }
        }

        CheckGLError("Construct ShaderIntrospection");
    }

    ShaderIntrospection::~ShaderIntrospection()
    {
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    void Bind(DeviceContext& context, const SetUniformCommandGroup& uniforms, IteratorRange<const void*> data)
    {
        assert(data.begin() && data.size());

        for (auto& cmd:uniforms._commands) {
            // (note, only glUniform1iv supported at the moment! -- used by texture uniform binding)
            assert((cmd._dataOffset+4*cmd._count) <= data.size()); // "ShaderUniformGroup contains corrupt dataOffset value"
            switch (cmd._type) {

            case GL_FLOAT:
                glUniform1fv(cmd._location, cmd._count, (GLfloat*)PtrAdd(AsPointer(data.begin()), cmd._dataOffset));
                break;
            case GL_FLOAT_VEC2:
                glUniform2fv(cmd._location, cmd._count, (GLfloat*)PtrAdd(AsPointer(data.begin()), cmd._dataOffset));
                break;
            case GL_FLOAT_VEC3:
                glUniform3fv(cmd._location, cmd._count, (GLfloat*)PtrAdd(AsPointer(data.begin()), cmd._dataOffset));
                break;
            case GL_FLOAT_VEC4:
                glUniform4fv(cmd._location, cmd._count, (GLfloat*)PtrAdd(AsPointer(data.begin()), cmd._dataOffset));
                break;

            case GL_FLOAT_MAT2:
                glUniformMatrix2fv(cmd._location, cmd._count, GL_FALSE, (GLfloat*)PtrAdd(AsPointer(data.begin()), cmd._dataOffset));
                break;
            case GL_FLOAT_MAT3:
                glUniformMatrix3fv(cmd._location, cmd._count, GL_FALSE, (GLfloat*)PtrAdd(AsPointer(data.begin()), cmd._dataOffset));
                break;
            case GL_FLOAT_MAT4:
                glUniformMatrix4fv(cmd._location, cmd._count, GL_FALSE, (GLfloat*)PtrAdd(AsPointer(data.begin()), cmd._dataOffset));
                break;

            case GL_INT:
            case GL_SAMPLER_2D:
            case GL_SAMPLER_CUBE:
            case GL_SAMPLER_2D_SHADOW:
            case GL_INT_SAMPLER_2D:
            case GL_UNSIGNED_INT_SAMPLER_2D:
            case GL_BOOL:
                glUniform1iv(cmd._location, cmd._count, (GLint*)PtrAdd(AsPointer(data.begin()), cmd._dataOffset));
                break;

            case GL_INT_VEC2:
            case GL_BOOL_VEC2:
                glUniform2iv(cmd._location, cmd._count, (GLint*)PtrAdd(AsPointer(data.begin()), cmd._dataOffset));
                break;
            case GL_INT_VEC3:
            case GL_BOOL_VEC3:
                glUniform3iv(cmd._location, cmd._count, (GLint*)PtrAdd(AsPointer(data.begin()), cmd._dataOffset));
                break;
            case GL_INT_VEC4:
            case GL_BOOL_VEC4:
                glUniform4iv(cmd._location, cmd._count, (GLint*)PtrAdd(AsPointer(data.begin()), cmd._dataOffset));
                break;

            default:
                assert(false); // "Uniform type not supported in Bind ShaderUniformGroup (%i)", cmd._type
                break;
            }
        }

        CheckGLError("Bind SetUniformCommandGroup");
    }

    #if defined(_DEBUG)
    auto ShaderIntrospection::UnboundUniforms() const -> std::vector<Uniform>
    {
        std::vector<Uniform> un;
        for (auto& hs : _structs) {
            for (auto& u : hs.second._uniforms) {
                if (!u._isBound) {
                    un.push_back(u);
                }
            }
        }
        return un;
    }

    void ShaderIntrospection::MarkBound(HashType uniformName)
    {
        // This only applies to global uniforms
        auto globals = LowerBound(_structs, 0ull);
        if (globals == _structs.end() || globals->first != 0) {
            return;
        }

        auto i = std::find_if(
            globals->second._uniforms.begin(), globals->second._uniforms.end(),
            [uniformName](const Uniform& u) { return (uniformName >= u._bindingName) && (uniformName < u._bindingName+u._elementCount); });
        if (i == globals->second._uniforms.end()) {
            return;
        }

        i->_isBound = true;
    }
    #endif
}}
