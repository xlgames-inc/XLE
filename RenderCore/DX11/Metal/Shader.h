// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DX11.h"
#include "../../ShaderService.h"
#include "../../RenderCore/IDevice_Forward.h"
#include "../../../Assets/AssetsCore.h"
#include "../../../Utility/IntrusivePtr.h"
#include "../../../Core/Types.h"
#include <memory>
#include <vector>

namespace Assets { class DependencyValidation; }
namespace RenderCore { class InputElementDesc; }

namespace RenderCore { namespace Metal_DX11
{

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class VertexShader
    {
    public:
            //
            //          Resource interface
            //
        explicit VertexShader(const ::Assets::ResChar initializer[]);
        explicit VertexShader(const CompiledShaderByteCode& byteCode);
        VertexShader();
        ~VertexShader();

        typedef ID3D::VertexShader* UnderlyingType;
        UnderlyingType  GetUnderlying() const { return _underlying.get(); }
        ID3D::ClassLinkage* GetClassLinkage() const { return _classLinkage.get(); }
        
    private:
        intrusive_ptr<ID3D::VertexShader>   _underlying;
        intrusive_ptr<ID3D::ClassLinkage>   _classLinkage;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class PixelShader
    {
    public:
            //
            //          Resource interface
            //
        explicit PixelShader(const ::Assets::ResChar initializer[]);
        explicit PixelShader(const CompiledShaderByteCode& byteCode);
        PixelShader();
        ~PixelShader();

        typedef ID3D::PixelShader*  UnderlyingType;
        UnderlyingType      GetUnderlying() const { return _underlying.get(); }
        ID3D::ClassLinkage* GetClassLinkage() const { return _classLinkage.get(); }
        
    private:
        intrusive_ptr<ID3D::PixelShader>    _underlying;
        intrusive_ptr<ID3D::ClassLinkage>   _classLinkage;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class GeometryShader
    {
    public:
        class StreamOutputInitializers
        {
        public:
            const InputElementDesc* _outputElements;
            unsigned _outputElementCount;
            const unsigned* _outputBufferStrides;
            unsigned _outputBufferCount;

            StreamOutputInitializers(
                const InputElementDesc outputElements[], unsigned outputElementCount,
                const unsigned outputBufferStrides[], unsigned outputBufferCount)
                : _outputElements(outputElements), _outputElementCount(outputElementCount)
                , _outputBufferStrides(outputBufferStrides), _outputBufferCount(outputBufferCount)
            {}
            StreamOutputInitializers()
                : _outputElements(nullptr), _outputElementCount(0)
                , _outputBufferStrides(nullptr), _outputBufferCount(0)
            {}
        };

            //
            //          Resource interface
            //
        GeometryShader(const ::Assets::ResChar initializer[], const StreamOutputInitializers& soInitializers = GetDefaultStreamOutputInitializers());
        explicit GeometryShader(const CompiledShaderByteCode& byteCode, const StreamOutputInitializers& soInitializers = GetDefaultStreamOutputInitializers());
        GeometryShader();
        ~GeometryShader();
        GeometryShader(GeometryShader&& moveFrom);
        GeometryShader& operator=(GeometryShader&& moveFrom);

        static void SetDefaultStreamOutputInitializers(const StreamOutputInitializers&);
        static const StreamOutputInitializers& GetDefaultStreamOutputInitializers();

        typedef ID3D::GeometryShader*       UnderlyingType;
        UnderlyingType                      GetUnderlying() const { return _underlying.get(); }
        
    private:
        intrusive_ptr<ID3D::GeometryShader>     _underlying;
        static StreamOutputInitializers         _hackInitializers;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class HullShader
    {
    public:
        explicit HullShader(const ::Assets::ResChar initializer[], const ::Assets::ResChar definesTable[]=nullptr);
        explicit HullShader(const CompiledShaderByteCode& byteCode);
        ~HullShader();

        typedef ID3D::HullShader*       UnderlyingType;
        UnderlyingType                  GetUnderlying() const { return _underlying.get(); }

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const     { return _validationCallback; }
        
    private:
        intrusive_ptr<ID3D::HullShader>            _underlying;
        std::shared_ptr<::Assets::DependencyValidation>   _validationCallback;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class DomainShader
    {
    public:
        explicit DomainShader(const ::Assets::ResChar initializer[], const ::Assets::ResChar definesTable[]=nullptr);
        explicit DomainShader(const CompiledShaderByteCode& byteCode);
        ~DomainShader();

        typedef ID3D::DomainShader*     UnderlyingType;
        UnderlyingType                  GetUnderlying() const { return _underlying.get(); }

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const     { return _validationCallback; }
        
    private:
        intrusive_ptr<ID3D::DomainShader>          _underlying;
        std::shared_ptr<::Assets::DependencyValidation>   _validationCallback;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class ComputeShader
    {
    public:
        explicit ComputeShader(const ::Assets::ResChar initializer[], const ::Assets::ResChar definesTable[]=nullptr);
        explicit ComputeShader(const CompiledShaderByteCode& byteCode);
        ComputeShader();
        ~ComputeShader();

        typedef ID3D::ComputeShader*    UnderlyingType;
        UnderlyingType                  GetUnderlying() const { return _underlying.get(); }

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const     { return _validationCallback; }
        
    private:
        intrusive_ptr<ID3D::ComputeShader>         _underlying;
        std::shared_ptr<::Assets::DependencyValidation>   _validationCallback;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    ///
    /// <summary>A set of shaders for each stage of the shader pipeline</summary>
    ///
    /// A ShaderProgram contains a set of shaders, each of which will be bound to the pipeline
    /// at the same time. Most of the time, it will contain just a vertex shader and a pixel shader.
    /// 
    /// In DirectX, it's easy to manage shaders for different stages of the pipeline separately. But
    /// OpenGL prefers to combine all of the shaders together into one shader program object.
    ///
    ///     The DirectX11 shader pipeline looks like this:
    ///         <list>
    ///             <item>Vertex Shader</item>
    ///             <item>Hull Shader</item>
    ///             <item>(fixed function tessellator)</item>
    ///             <item>Domain shader</item>
    ///             <item>Geometry shader</item>
    ///             <item>Pixel Shader</item>
    ///         </list>
    ///
    /// Normally ShaderProgram objects are created and managed using the "Assets::GetAsset" or "Assets::GetAssetDep"
    /// functions.
    /// For example:
    /// <code>\code
    ///     auto& shader = Assets::GetAssetDep<ShaderProgram>("shaders/basic.vsh:main:vs_*", "shaders/basic.psh:main:ps_*");
    /// \endcode</code>
    ///
    /// But it's also possible to construct a ShaderProgram as a basic object. 
    /// <code>\code
    ///     ShaderProgram myShaderProgram("shaders/basic.vsh:main:vs_*", "shaders/basic.psh:main:ps_*");
    /// \endcode</code>
    ///
    /// Note that the parameters ":main:vs_*" specify the entry point and shader compilation model.
    ///     <list>
    ///         <item>":main" -- this is the entry point function. That is, there is a function in
    ///                 the shader file called "main." It will take the shader input parameters and 
    ///                 return the shader output values.</item>
    ///         <item>":vs_*" -- this is the compilation model. "vs" means vertex shader. The "_*"
    ///                 means to use the default compilation model for the current device. But it can 
    ///                 be replaced with an appropriate compilation model 
    ///                     (eg, vs_5_0, vs_4_0_level_9_1, ps_3_0, etc)
    ///                 Normally it's best to use the default compilation model.</item>
    ///     </list>
    ///
    class ShaderProgram
    {
    public:
        ShaderProgram(  const ::Assets::ResChar vertexShaderInitializer[], 
                        const ::Assets::ResChar fragmentShaderInitializer[]);
        
        ShaderProgram(  const ::Assets::ResChar vertexShaderInitializer[], 
                        const ::Assets::ResChar fragmentShaderInitializer[],
                        const ::Assets::ResChar definesTable[]);

        ShaderProgram(  const ::Assets::ResChar vertexShaderInitializer[], 
                        const ::Assets::ResChar geometryShaderInitializer[],
                        const ::Assets::ResChar fragmentShaderInitializer[],
                        const ::Assets::ResChar definesTable[]);

        ShaderProgram(  const CompiledShaderByteCode& compiledVertexShader, 
                        const CompiledShaderByteCode& compiledFragmentShader);

		ShaderProgram();
        ~ShaderProgram();

        const VertexShader&                 GetVertexShader() const             { return _vertexShader; }
        const GeometryShader&               GetGeometryShader() const           { return _geometryShader; }
        const PixelShader&                  GetPixelShader() const              { return _pixelShader; }
        const CompiledShaderByteCode&       GetCompiledVertexShader() const     { return *_compiledVertexShader; }
        const CompiledShaderByteCode&       GetCompiledPixelShader() const      { return *_compiledPixelShader; }
        const CompiledShaderByteCode*       GetCompiledGeometryShader() const   { return _compiledGeometryShader; }
        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const     { return _validationCallback; }

        bool DynamicLinkingEnabled() const;

		ShaderProgram(ShaderProgram&&) never_throws;
        ShaderProgram& operator=(ShaderProgram&&) never_throws;

    protected:
        const CompiledShaderByteCode*           _compiledVertexShader;
        const CompiledShaderByteCode*           _compiledPixelShader;
        const CompiledShaderByteCode*           _compiledGeometryShader;
        VertexShader                            _vertexShader;
        PixelShader                             _pixelShader;
        GeometryShader                          _geometryShader;
        std::shared_ptr<::Assets::DependencyValidation>   _validationCallback;

        ShaderProgram(const ShaderProgram&) = delete;
        ShaderProgram& operator=(const ShaderProgram&) = delete;
    };

    ///
    /// <summary>A shader program with tessellation support</summary>
    ///
    /// A DeepShaderProgram is the same as ShaderProgram, but with tessellation support. Most
    /// ShaderPrograms don't need tessellation shaders, so for simplicity they are excluded
    /// from the default ShaderProgram object.
    ///
    class DeepShaderProgram : public ShaderProgram
    {
    public:
        DeepShaderProgram(  const ::Assets::ResChar vertexShaderInitializer[], 
                            const ::Assets::ResChar geometryShaderInitializer[],
                            const ::Assets::ResChar fragmentShaderInitializer[],
                            const ::Assets::ResChar hullShaderInitializer[],
                            const ::Assets::ResChar domainShaderInitializer[],
                            const ::Assets::ResChar definesTable[]);
        ~DeepShaderProgram();

        const HullShader&                   GetHullShader() const               { return _hullShader; }
        const DomainShader&                 GetDomainShader() const             { return _domainShader; }
        const CompiledShaderByteCode&       GetCompiledHullShader() const       { return _compiledHullShader; }
        const CompiledShaderByteCode&       GetCompiledDomainShader() const     { return _compiledDomainShader; }

    private:
        const CompiledShaderByteCode&   _compiledHullShader;
        const CompiledShaderByteCode&   _compiledDomainShader;
        HullShader                      _hullShader;
        DomainShader                    _domainShader;

        DeepShaderProgram(const DeepShaderProgram&);
        DeepShaderProgram& operator=(const DeepShaderProgram&);
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    std::shared_ptr<ShaderService::ILowLevelCompiler> CreateLowLevelShaderCompiler(IDevice& device);

    intrusive_ptr<ID3D::ShaderReflection>  CreateReflection(const CompiledShaderByteCode& shaderCode);

    extern template intrusive_ptr<ID3D::ShaderReflection>;

}}