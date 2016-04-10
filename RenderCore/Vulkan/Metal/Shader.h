// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../../Assets/AssetsCore.h"
#include "../../ShaderService.h"
#include "VulkanCore.h"
#include "IncludeVulkan.h"

namespace RenderCore { class InputElementDesc; }

namespace RenderCore { namespace Metal_Vulkan
{
    class Shader
    {
    public:
        typedef VkShaderModule UnderlyingType;
        UnderlyingType  GetUnderlying() const { return _underlying.get(); }
        bool            IsGood() const { return _underlying != nullptr; }

        Shader();
        Shader(const CompiledShaderByteCode& byteCode);
        ~Shader();
    protected:
        VulkanSharedPtr<VkShaderModule> _underlying;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class VertexShader : public Shader
    {
    public:
            //
            //          Resource interface
            //
		explicit VertexShader(const ::Assets::ResChar initializer[]);
        explicit VertexShader(const CompiledShaderByteCode& byteCode);
        VertexShader();
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class PixelShader : public Shader
    {
    public:
            //
            //          Resource interface
            //
        explicit PixelShader(const ::Assets::ResChar initializer[]);
        explicit PixelShader(const CompiledShaderByteCode& byteCode);
        PixelShader();
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class GeometryShader : public Shader
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

        static void SetDefaultStreamOutputInitializers(const StreamOutputInitializers&);
        static const StreamOutputInitializers& GetDefaultStreamOutputInitializers();
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class HullShader : public Shader
    {
    public:
        explicit HullShader(const ::Assets::ResChar initializer[], const ::Assets::ResChar definesTable[]=nullptr);
        explicit HullShader(const CompiledShaderByteCode& byteCode);
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class DomainShader : public Shader
    {
    public:
        explicit DomainShader(const ::Assets::ResChar initializer[], const ::Assets::ResChar definesTable[]=nullptr);
        explicit DomainShader(const CompiledShaderByteCode& byteCode);
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class ComputeShader : public Shader
    {
    public:
        explicit ComputeShader(const ::Assets::ResChar initializer[], const ::Assets::ResChar definesTable[]=nullptr);
        explicit ComputeShader(const CompiledShaderByteCode& byteCode);
        ~ComputeShader();

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const     { return _validationCallback; }
    private:
        std::shared_ptr<::Assets::DependencyValidation>   _validationCallback;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

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

        ShaderProgram(const ShaderProgram&) = delete;
        ShaderProgram& operator=(const ShaderProgram&) = delete;

    protected:
        const CompiledShaderByteCode*           _compiledVertexShader;
        const CompiledShaderByteCode*           _compiledPixelShader;
        const CompiledShaderByteCode*           _compiledGeometryShader;
        VertexShader                            _vertexShader;
        PixelShader                             _pixelShader;
        GeometryShader                          _geometryShader;
        std::shared_ptr<::Assets::DependencyValidation>   _validationCallback;
    };

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

    std::shared_ptr<ShaderService::ILowLevelCompiler> CreateLowLevelShaderCompiler();
}}