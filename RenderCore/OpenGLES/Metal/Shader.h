// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../../Assets/AssetsCore.h"
#include "../../../Utility/Mixins.h"
#include "../../ShaderService.h"
#include "ObjectFactory.h"

namespace RenderCore { class CompiledShaderByteCode; class IDevice; }

namespace RenderCore { namespace Metal_OpenGLES
{
    class ShaderProgram
    {
    public:
        using UnderlyingType = OpenGL::ShaderProgram*;
        UnderlyingType GetUnderlying() const { return _underlying.get(); }
        const ::Assets::DepValPtr& GetDependencyValidation() { return _depVal; }
        uint32_t GetGUID() const { return _guid; }

        ShaderProgram(ObjectFactory& factory, const CompiledShaderByteCode& vs, const CompiledShaderByteCode& fs);
        ~ShaderProgram();
    private:
        intrusive_ptr<OpenGL::ShaderProgram>    _underlying;
        ::Assets::DepValPtr                     _depVal;
        uint32_t                                _guid;
    };

    std::shared_ptr<ShaderService::ILowLevelCompiler> CreateLowLevelShaderCompiler(IDevice& device);

    void DestroyGLESCachedShaders();
}}
