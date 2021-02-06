// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../../Assets/AssetsCore.h"
#include "../../ShaderService.h"
#include "../../../Foreign/OCPtr/OCPtr.hpp"

namespace RenderCore { class CompiledShaderByteCode; class IDevice; }

namespace RenderCore { namespace Metal_AppleMetal
{
    class ObjectFactory;

    class ShaderProgram
    {
    public:
        const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }
        uint32_t GetGUID() const { return _guid; }
        #if defined(_DEBUG)
            std::string SourceIdentifiers() const { return _sourceIdentifiers; };
        #endif

        ShaderProgram(ObjectFactory& factory, const CompiledShaderByteCode& vs, const CompiledShaderByteCode& fs);
        ~ShaderProgram();

        /* KenD -- Metal TODO -- shader construction will need to account for shader variants and conditional compilation, possibly with function constants */
        ShaderProgram(const std::string& vertexFunctionName, const std::string& fragmentFunctionName);
        TBC::OCPtr<id> _vf; // MTLFunction
        TBC::OCPtr<id> _ff; // MTLFunction

    private:
        ::Assets::DepValPtr                     _depVal;
        uint32_t                                _guid;

        #if defined(_DEBUG)
            std::string _sourceIdentifiers;
        #endif
    };

    std::shared_ptr<ILowLevelCompiler> CreateLowLevelShaderCompiler(IDevice& device);
}}
