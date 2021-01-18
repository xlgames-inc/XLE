
#include "ShaderIntrospection.h"
#include "Shader.h"
#include "Format.h"
#include "../../UniformsStream.h"        // (for ConstantBufferElementDesc)
#include "../../../OSServices/Log.h"
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
        if (s == _structs.end() || s->first != uniformStructName) {
            #if _DEBUG
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
                auto arrayElementsInShader = std::max(1u, (unsigned)i._elementCount);
                auto arrayElementsInInputBinding = std::max(1u, b->_arrayElementCount);
                
                // On DesktopGL, the arrays in the shader can be truncated if some elements are not used
                // Let's verify that the input data has at least enough data to fill up the array
                // elements in the shader.
                assert(arrayElementsInShader <= arrayElementsInInputBinding);
                
                auto basicType = GLUniformTypeAsTypeDesc(i._type);
                auto inputBasicType = AsImpliedType(b->_nativeFormat);
                if (basicType == inputBasicType) {
                    assert(i._location != -1);
                    result._commands.push_back({i._location, i._type, std::min(arrayElementsInShader, arrayElementsInInputBinding), b->_offset });
                } else {
                    #if defined(EXTRA_INPUT_LAYOUT_PROPERTIES)
                        Log(Warning) << "In MakeBinding, binding shader struct to predefined layout failed because of type mismatch on uniform (" << i._name << ") in struct (" << s->second._name << ")" << std::endl;
                    #endif
                }
            } else {
                #if defined(EXTRA_INPUT_LAYOUT_PROPERTIES) && defined(EXTRA_INPUT_LAYOUT_LOGGING)
                    Log(Warning) << "In MakeBinding, uniform in shader (" << i._name << ") in struct (" << s->second._name << ") was not matched to binding" << std::endl;
                #endif
            }
        }

        #if defined(EXTRA_INPUT_LAYOUT_PROPERTIES)
            result._name = s->second._name;
        #endif

        return result;
    }

    auto ShaderIntrospection::FindUniform(HashType uniformName) const -> Uniform
    {
        // This only finds global uniforms
        auto globals = LowerBound(_structs, (uint64_t)0ull);
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
        if (str != _structs.end() && str->first == structName)
            return str->second;
        return {};
    }

    auto ShaderIntrospection::FindUniformBlock(HashType structName) const -> UniformBlock
    {
        auto i = LowerBound(_blockIdx, structName);
        if (i != _blockIdx.end() && i->first == structName)
            return i->second;
        return {~0u};
    }

    std::string ShaderIntrospection::GetName(const ShaderProgram& program, const Uniform& uniform)
    {
        #if defined(EXTRA_INPUT_LAYOUT_PROPERTIES)
            return uniform._name;
        #else
            auto glProgram = program.GetUnderlying()->AsRawGLHandle();

            GLint uniformMaxNameLength;
            glGetProgramiv(glProgram, GL_ACTIVE_UNIFORM_MAX_LENGTH, &uniformMaxNameLength);

            char nameBuffer[uniformMaxNameLength];
            std::memset(nameBuffer, 0, uniformMaxNameLength);

            GLint size = 0;
            GLenum type = 0;
            glGetActiveUniform(glProgram, uniform._activeUniformIndex, uniformMaxNameLength, nullptr, &size, &type, nameBuffer);
            return nameBuffer;
        #endif
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

    std::pair<StringSection<>, StringSection<>> FindStructSeparator(StringSection<> input)
    {
        for (auto i=input.begin(); i!=input.end(); i++) {
            if (*i == '.') return {{input.begin(), i}, {i+1, input.end()}};
            // An underscore also works like a fake struct separator.
            // However, ignore the underscore if it's the first, second or third character
            // so that prefixes like "u_" aren't considered structs.
            if (i > (input.begin()+2) && *i == '_') {
                // If we get 'xx' immediately after the underscore, we're copying to skip over
                // those as well. This is to avoid double underscore scenarios. Ie, if the
                // variable name starts with an underscore, we'll end up with a double. These are
                // illegal In GLSL, so let's work around it
                if ((input.end() - i) >= 3 && i[1] == 'x' && i[2] == 'x') {
                    return {{input.begin(), i}, {i+3, input.end()}};
                }

                return {{input.begin(), i}, {i+1, input.end()}};
            }
        }
        return {input, {}};
    }

    ShaderIntrospection::ShaderIntrospection(const ShaderProgram& program)
    {
        // Iterate through the shader interface and pull out the information that's interesting
        // to us.

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
            if (location == -1) {
                continue;       // uniforms at location -1 are not useful -- they can be values inside of a uniform buffers (which we'll get to in the uniform buffer reflection below)
            }

            ////////////////////////////////////////////////
            auto fullName = MakeStringSection(nameBuffer);
            auto separatedNames = FindStructSeparator(fullName);
            if (!separatedNames.second.IsEmpty()) {
                assert(std::find(separatedNames.second.begin(), separatedNames.second.end(), '.') == separatedNames.second.end());     // can't support nested structures

                auto structName = separatedNames.first;
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
                        HashVariableName(separatedNames.second),
                        location, type, size, c

                        #if defined(EXTRA_INPUT_LAYOUT_PROPERTIES)
                            , fullName.AsString()
                        #endif
                    });
            } else {
                // for global uniform, add into dummy struct at "0"
                auto i = LowerBound(_structs, HashType(0));
                if (i==_structs.end() || i->first != 0)
                    i = _structs.insert(i, std::make_pair(0, Struct{{}
                        #if defined(EXTRA_INPUT_LAYOUT_PROPERTIES)
                            , "global"
                        #endif
                        }));

                i->second._uniforms.emplace_back(
                    Uniform {
                        HashVariableName(fullName),
                        location, type, size, c

                        #if defined(_DEBUG)
                            , fullName.AsString()
                        #endif
                    });
            }
            ////////////////////////////////////////////////
        }

#if defined(GL_ES_VERSION_3_0)
        bool uniformBlockSupport = GetObjectFactory().GetFeatureSet() & FeatureSet::GLES300;
        if (uniformBlockSupport) {
            GLint uniformBlockCount = 0;
            glGetProgramiv(glProgram, GL_ACTIVE_UNIFORM_BLOCKS, &uniformBlockCount);

            std::vector<GLchar> name;
            std::vector<GLint> uniformIndicies;
            std::vector<GLint> uniformOffsets;
            for (unsigned c=0; c<uniformBlockCount; ++c) {
                GLint nameLen;
                glGetActiveUniformBlockiv(glProgram, c, GL_UNIFORM_BLOCK_NAME_LENGTH, &nameLen);
                if (!nameLen) continue;

                name.resize(nameLen);
                glGetActiveUniformBlockName(glProgram, c, nameLen, NULL, &name[0]);

                GLint blockSize = 0;
                glGetActiveUniformBlockiv(glProgram, c, GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize);

                // we can query:
                // GL_UNIFORM_BLOCK_BINDING
                // GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS
                // GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES
                // GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER
                // GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER

                GLint uniformCount = 0;
                glGetActiveUniformBlockiv(glProgram, c, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &uniformCount);

                uniformIndicies.resize(uniformCount);
                uniformOffsets.resize(uniformCount);
                glGetActiveUniformBlockiv(glProgram, c, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, uniformIndicies.data());
                glGetActiveUniformsiv(glProgram, GLsizei(uniformCount), (const GLuint*)uniformIndicies.data(), GL_UNIFORM_OFFSET, uniformOffsets.data());

                std::string uniformBufferName;

                std::vector<Uniform> uniforms;
                for (unsigned u=0; u<uniformCount; ++u) {
                    GLint size = 0;
                    GLenum type = 0;
                    glGetActiveUniform(glProgram, uniformIndicies[u], uniformMaxNameLength, nullptr, &size, &type, nameBuffer);

                    auto fullName = MakeStringSection(nameBuffer);
                    auto separatedNames = FindStructSeparator(fullName);
                    if (!separatedNames.second.IsEmpty()) {
                        assert(std::find(separatedNames.second.begin(), separatedNames.second.end(), '.') == separatedNames.second.end());     // can't support nested structures
                        #if defined(_DEBUG)
                            if (!uniformBufferName.empty() && !XlEqString(separatedNames.first, uniformBufferName)) {
                                Log(Warning) << "Multiple scoped names detected within uniform block. (" << uniformBufferName << ") and (" << separatedNames.first.AsString() << ") detected" << std::endl;
                            }
                        #endif
                        if (uniformBufferName.empty())
                            uniformBufferName = separatedNames.first.AsString();
                        uniforms.emplace_back(
                            Uniform {
                                HashVariableName(separatedNames.second),
                                (int)uniformOffsets[u], type, size, (unsigned)uniformIndicies[u]

                                #if defined(EXTRA_INPUT_LAYOUT_PROPERTIES)
                                    , fullName.AsString()
                                #endif
                            });
                    } else {
                        #if defined(_DEBUG)
                            if (!uniformBufferName.empty() && uniformBufferName != "global") {
                                Log(Warning) << "Multiple scoped names detected within uniform block. (" << uniformBufferName << ") and (global) detected" << std::endl;
                            }
                        #endif
                        if (uniformBufferName.empty())
                            uniformBufferName = "global";
                        uniforms.emplace_back(
                            Uniform {
                                HashVariableName(fullName),
                                (int)uniformOffsets[u], type, size, (unsigned)uniformIndicies[u]

                                #if defined(EXTRA_INPUT_LAYOUT_PROPERTIES)
                                    , fullName.AsString()
                                #endif
                            });
                    }
                }

                //
                // There are 2 names we have access to
                //  1) the name of the actual block. In C++, this is equivalent to the
                //      name of the type/struct/class
                //  2) the name of the uniform. This is like the actual variable name
                //
                // The uniform name is used to refer to the members by the shader code.
                // For example:
                //  uniform MaterialType
                //  {
                //      vec4 Diffuse;
                //  } MaterialUniform;
                //
                //  Here, in the shader code we write:
                //      MaterialUniform.Diffuse
                //  to refer to the value (not MaterialType.Diffuse)
                //
                //  The name we ideally want to bind to is also the uniform name -- since this is
                //  more consistant with other shader languages. However, there's no obvious way
                //  to access it via this introspection interface. We can only get the type name.
                //  It's odd because we can only access the type name from the CPU side, but we
                //  only really use the uniform name from the shader side.
                //  The best way to deal with this is just to always have the type name and uniform
                //  name be the same string, which should prevent any confusion.
                //

                auto n = std::string(&name[0], &name[nameLen-1]);       // -1 to remove the null terminator
                auto h = Hash64(n);
                auto i = LowerBound(_blockIdx, h);
                if (i==_blockIdx.end() || i->first != h) {
                    i = _blockIdx.insert(i, std::make_pair(h,
                        UniformBlock {
                            c, (unsigned)blockSize, std::move(uniforms)

                            #if defined(EXTRA_INPUT_LAYOUT_PROPERTIES)
                                , n
                            #endif
                        }));
                }
            }
        }
#endif

        CheckGLError("Construct ShaderIntrospection");
    }

    ShaderIntrospection::~ShaderIntrospection()
    {
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    unsigned Bind(DeviceContext& context, const SetUniformCommandGroup& uniforms, IteratorRange<const void*> data)
    {
        assert(data.begin() && data.size());

        for (auto& cmd:uniforms._commands) {
            // (note, only glUniform1iv supported at the moment! -- used by texture uniform binding)
            unsigned bytesRemaining = unsigned(data.size() - cmd._dataOffset);
            switch (cmd._type) {

            case GL_FLOAT:
                glUniform1fv(cmd._location, std::min(bytesRemaining / (unsigned)sizeof(float), cmd._count), (GLfloat*)PtrAdd(AsPointer(data.begin()), cmd._dataOffset));
                break;
            case GL_FLOAT_VEC2:
                glUniform2fv(cmd._location, std::min(bytesRemaining / (unsigned)(2*sizeof(float)), cmd._count), (GLfloat*)PtrAdd(AsPointer(data.begin()), cmd._dataOffset));
                break;
            case GL_FLOAT_VEC3:
                glUniform3fv(cmd._location, std::min(bytesRemaining / (unsigned)(3*sizeof(float)), cmd._count), (GLfloat*)PtrAdd(AsPointer(data.begin()), cmd._dataOffset));
                break;
            case GL_FLOAT_VEC4:
                glUniform4fv(cmd._location, std::min(bytesRemaining / (unsigned)(4*sizeof(float)), cmd._count), (GLfloat*)PtrAdd(AsPointer(data.begin()), cmd._dataOffset));
                break;

            case GL_FLOAT_MAT2:
                glUniformMatrix2fv(cmd._location, std::min(bytesRemaining / (unsigned)(4*sizeof(float)), cmd._count), GL_FALSE, (GLfloat*)PtrAdd(AsPointer(data.begin()), cmd._dataOffset));
                break;
            case GL_FLOAT_MAT3:
                glUniformMatrix3fv(cmd._location, std::min(bytesRemaining / (unsigned)(9*sizeof(float)), cmd._count), GL_FALSE, (GLfloat*)PtrAdd(AsPointer(data.begin()), cmd._dataOffset));
                break;
            case GL_FLOAT_MAT4:
                glUniformMatrix4fv(cmd._location, std::min(bytesRemaining / (unsigned)(16*sizeof(float)), cmd._count), GL_FALSE, (GLfloat*)PtrAdd(AsPointer(data.begin()), cmd._dataOffset));
                break;

            case GL_INT:
            case GL_SAMPLER_2D:
            case GL_SAMPLER_CUBE:
            case GL_SAMPLER_2D_SHADOW:
            case GL_INT_SAMPLER_2D:
            case GL_UNSIGNED_INT_SAMPLER_2D:
            case GL_BOOL:
                glUniform1iv(cmd._location, std::min(bytesRemaining / (unsigned)sizeof(unsigned), cmd._count), (GLint*)PtrAdd(AsPointer(data.begin()), cmd._dataOffset));
                break;

            case GL_INT_VEC2:
            case GL_BOOL_VEC2:
                glUniform2iv(cmd._location, std::min(bytesRemaining / (unsigned)(2*sizeof(unsigned)), cmd._count), (GLint*)PtrAdd(AsPointer(data.begin()), cmd._dataOffset));
                break;
            case GL_INT_VEC3:
            case GL_BOOL_VEC3:
                glUniform3iv(cmd._location, std::min(bytesRemaining / (unsigned)(3*sizeof(unsigned)), cmd._count), (GLint*)PtrAdd(AsPointer(data.begin()), cmd._dataOffset));
                break;
            case GL_INT_VEC4:
            case GL_BOOL_VEC4:
                glUniform4iv(cmd._location, std::min(bytesRemaining / (unsigned)(4*sizeof(unsigned)), cmd._count), (GLint*)PtrAdd(AsPointer(data.begin()), cmd._dataOffset));
                break;

            default:
                assert(false); // "Uniform type not supported in Bind ShaderUniformGroup (%i)", cmd._type
                break;
            }
        }

        CheckGLError("Bind SetUniformCommandGroup");
        return (unsigned)uniforms._commands.size();
    }

    std::ostream& operator<<(std::ostream&str, const SetUniformCommandGroup& cmdGroup)
    {
        for (const auto&cmd:cmdGroup._commands) {
            str << "\tSet " << GLenumAsString(cmd._type);
            if (cmd._count > 1)
                str << "[" << cmd._count << "]";
            str << " location: " << cmd._location;
            str << " from data offset: 0x" << std::hex << cmd._dataOffset << std::dec << std::endl;
        }
        return str;
    }
}}
