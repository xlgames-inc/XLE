// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../../Assets/AssetsCore.h"
#include "../../../Utility/Mixins.h"
#include "IndexedGLType.h"

namespace RenderCore { namespace Metal_OpenGLES
{
        ////////////////////////////////////////////////////////////////////////////////////////////////

    class ShaderProgram : noncopyable
    {
    public:
        ShaderProgram(  const ::Assets::ResChar vertexShaderInitializer[], 
                        const ::Assets::ResChar fragmentShaderInitializer[]);
        ~ShaderProgram();
        
        typedef OpenGL::ShaderProgram*    UnderlyingType;
        UnderlyingType                          GetUnderlying() const { return _underlying.get(); }

    private:
        intrusive_ptr<OpenGL::ShaderProgram>     _underlying;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

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
