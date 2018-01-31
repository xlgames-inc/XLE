// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../../Assets/AssetsCore.h"
#include "../../../Utility/Mixins.h"
#include "../../ShaderService.h"

namespace RenderCore { class CompiledShaderByteCode; class IDevice; }

namespace RenderCore { namespace Metal_AppleMetal
{
    class ShaderProgram
    {
    public:
        const ::Assets::DepValPtr& GetDependencyValidation() { return _depVal; }
        uint32_t GetGUID() const { return _guid; }

        ShaderProgram(const CompiledShaderByteCode& vs, const CompiledShaderByteCode& fs);
        ~ShaderProgram();
    private:
        ::Assets::DepValPtr                     _depVal;
        uint32_t                                _guid;
    };

    std::shared_ptr<ShaderService::ILowLevelCompiler> CreateLowLevelShaderCompiler(IDevice& device);
}}
