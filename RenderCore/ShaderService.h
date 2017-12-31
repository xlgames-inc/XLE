// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/AssetsCore.h"
#include "../Assets/DepVal.h"
#include "../Utility/StringUtils.h"
#include "../Utility/IteratorUtils.h"
#include "../Core/Prefix.h"
#include "../Core/Types.h"
#include <memory>
#include <vector>
#include <utility>
#include <assert.h>

namespace Assets
{
	class DependencyValidation; class DependentFileState; 
	class CompileFuture; class ICompileMarker; 
	class DeferredConstruction; class IArtifact;
}

namespace RenderCore
{
    enum class ShaderStage
    {
        Vertex, Pixel, Geometry, Hull, Domain, Compute,
        Null,
        Max
    };

    class ShaderService
    {
    public:
        using ResChar = ::Assets::ResChar;
        
        class ResId
        {
        public:
            ResChar     _filename[MaxPath];
            ResChar     _entryPoint[64];
            ResChar     _shaderModel[32];
            bool        _dynamicLinkageEnabled;

            ResId(StringSection<ResChar> filename, StringSection<ResChar> entryPoint, StringSection<ResChar> shaderModel);
            ResId();

            ShaderStage AsShaderStage() const;

        protected:
            ResId(StringSection<ResChar> initializer);
        };
        
        class ShaderHeader
        {
        public:
            static const auto Version = 1u;
            unsigned _version;
			char _shaderModel[8];
            unsigned _dynamicLinkageEnabled;
        };

        class IShaderSource
        {
        public:
            virtual std::shared_ptr<::Assets::CompileFuture> CompileFromFile(
                StringSection<::Assets::ResChar> resId, 
                StringSection<::Assets::ResChar> definesTable) const = 0;
            
            virtual std::shared_ptr<::Assets::CompileFuture> CompileFromMemory(
                StringSection<char> shaderInMemory, StringSection<char> entryPoint, 
				StringSection<char> shaderModel, StringSection<::Assets::ResChar> definesTable) const = 0;

            virtual ~IShaderSource();
        };

        class ILowLevelCompiler
        {
        public:
            using Payload = ::Assets::Blob;

            virtual void AdaptShaderModel(
                ResChar destination[], 
                const size_t destinationCount,
				StringSection<ResChar> source) const = 0;

            virtual bool DoLowLevelCompile(
                /*out*/ Payload& payload,
                /*out*/ Payload& errors,
                /*out*/ std::vector<::Assets::DependentFileState>& dependencies,
                const void* sourceCode, size_t sourceCodeLength,
                const ResId& shaderPath,
                StringSection<::Assets::ResChar> definesTable) const = 0;

            virtual std::string MakeShaderMetricsString(
                const void* byteCode, size_t byteCodeSize) const = 0;

            virtual ~ILowLevelCompiler();
        };

        std::shared_ptr<::Assets::CompileFuture> CompileFromFile(
            StringSection<::Assets::ResChar> resId, 
            StringSection<::Assets::ResChar> definesTable) const;

        std::shared_ptr<::Assets::CompileFuture> CompileFromMemory(
            StringSection<char> shaderInMemory, 
            StringSection<char> entryPoint, StringSection<char> shaderModel, 
            StringSection<::Assets::ResChar> definesTable) const;

        void AddShaderSource(std::shared_ptr<IShaderSource> shaderSource);

        static ResId MakeResId(
            StringSection<::Assets::ResChar> initializer, 
            const ILowLevelCompiler* compiler = nullptr);

        static ShaderService& GetInstance() { assert(s_instance); return *s_instance; }
        static void SetInstance(ShaderService*);

        ShaderService();
        ~ShaderService();

    protected:
        static ShaderService* s_instance;
        std::vector<std::shared_ptr<IShaderSource>> _shaderSources;
    };

    /// <summary>Represents a chunk of compiled shader code</summary>
    /// Typically we construct CompiledShaderByteCode with either a reference
    /// to a file or a string containing high-level shader code.
    ///
    /// When loading a shader from a file, there is a special syntax for the "initializer":
    ///  * {filename}:{entry point}:{shader model}
    ///
    /// <example>
    /// For example:
    ///     <code>\code
    ///         CompiledShaderByteCode byteCode("shaders/basic.psh:MainFunction:ps_5_0");
    ///     \endcode</code>
    ///     This will load the file <b>shaders/basic.psh</b>, and look for the entry point
    ///     <b>MainFunction</b>. The shader will be compiled with pixel shader 5.0 shader model.
    /// </example>
    ///
    /// Most clients will want to use the default shader model for a given stage. To use the default
    /// shader model, use ":ps_*". This will always use a shader model that is valid for the current
    /// hardware. Normally use of an explicit shader model is only required when pre-compiling many
    /// shaders for the final game image.
    ///
    /// The constructor will invoke background compile operations.
    /// The resulting compiled byte code can be accessed using GetByteCode()
    /// However, GetByteCode can throw exceptions (such as ::Assets::Exceptions::PendingAsset
    /// and ::Assets::Exceptions::InvalidAsset). If the background compile operation has
    /// not completed yet, a PendingAsset exception will be thrown.
    ///
    /// Alternatively, use TryGetByteCode() to return an error code instead of throwing an
    /// exception. But note that TryGetByteCode() can still throw exceptions -- but only in
    /// unusual situations (such as programming errors or hardware faults)
    class CompiledShaderByteCode
    {
    public:
        IteratorRange<const void*>  GetByteCode() const;
        
		ShaderStage		GetStage() const;
        bool            DynamicLinkingEnabled() const;

		CompiledShaderByteCode(const ::Assets::Blob&, const ::Assets::DepValPtr&);
		CompiledShaderByteCode();
        ~CompiledShaderByteCode();

		CompiledShaderByteCode(const CompiledShaderByteCode&) = delete;
        CompiledShaderByteCode& operator=(const CompiledShaderByteCode&) = delete;
        CompiledShaderByteCode(CompiledShaderByteCode&&);
        CompiledShaderByteCode& operator=(CompiledShaderByteCode&&);

        auto        GetDependencyValidation() const -> const ::Assets::DepValPtr& { return _depVal; }

        static const uint64 CompileProcessType;

    private:
		::Assets::Blob			_shader;
		::Assets::DepValPtr		_depVal;
    };
}

