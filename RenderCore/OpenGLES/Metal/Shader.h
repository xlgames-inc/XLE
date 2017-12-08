// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../../Assets/AssetsCore.h"
#include "../../../Utility/Mixins.h"
#include "../../ShaderService.h"
#include "IndexedGLType.h"

namespace RenderCore { class CompiledShaderByteCode; class IDevice; }

namespace RenderCore { namespace Metal_OpenGLES
{
        ////////////////////////////////////////////////////////////////////////////////////////////////

    class ShaderProgram : noncopyable
    {
    public:
        typedef OpenGL::ShaderProgram*  UnderlyingType;
        UnderlyingType                  GetUnderlying() const { return _underlying.get(); }

        const ::Assets::DepValPtr&      GetDependencyValidation() { return _depVal; }

        ShaderProgram(  const CompiledShaderByteCode& vs,
                        const CompiledShaderByteCode& fs);
        ~ShaderProgram();
    private:
        intrusive_ptr<OpenGL::ShaderProgram>    _underlying;
        ::Assets::DepValPtr                     _depVal;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

    std::shared_ptr<ShaderService::ILowLevelCompiler> CreateLowLevelShaderCompiler(IDevice& device);

//     class VertexShader : noncopyable
//     {
//     public:
//             //
//             //          Resource interface
//             //
//         VertexShader(const ResChar initializer[]);
// 
//         typedef ID3D::VertexShader  UnderlyingType;
//         UnderlyingType*             GetUnderlying() const { return _underlying.get(); }
//         
//     private:
//         intrusive_ptr<UnderlyingType>      _underlying;
//     };
// 
//         ////////////////////////////////////////////////////////////////////////////////////////////////
// 
//     class PixelShader : noncopyable
//     {
//     public:
//             //
//             //          Resource interface
//             //
//         PixelShader(const ResChar initializer[]);
// 
//         typedef ID3D::PixelShader   UnderlyingType;
//         UnderlyingType*             GetUnderlying() const { return _underlying.get(); }
//         
//     private:
//         intrusive_ptr<UnderlyingType>      _underlying;
//     };
}}
