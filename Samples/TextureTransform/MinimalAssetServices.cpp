// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MinimalAssetServices.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../RenderCore/ShaderService.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../Assets/AssetUtils.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../ConsoleRig/AttachableInternal.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/MemoryUtils.h"

namespace Samples
{
    using namespace RenderCore;

    class MinimalShaderSource : public ShaderService::IShaderSource
    {
    public:
        class PendingMarker : public ShaderService::IPendingMarker
        {
        public:
            const Payload& Resolve(const char initializer[], const ::Assets::DepValPtr& depVal) const; 
            ::Assets::AssetState TryResolve(Payload& result, const ::Assets::DepValPtr& depVal) const;
            Payload GetErrors() const;

            ::Assets::AssetState StallWhilePending() const;
            ShaderStage::Enum GetStage() const;

            PendingMarker(
                Payload payload, 
                std::vector<::Assets::DependentFileState> deps, ShaderStage::Enum stage);
            PendingMarker(Payload errors);
            ~PendingMarker();
            PendingMarker(const PendingMarker&) = delete;
            const PendingMarker& operator=(const PendingMarker&) = delete;
        private:
            Payload _payload;
            std::vector<::Assets::DependentFileState> _deps;
            ShaderStage::Enum _stage;
            Payload _errors;
        };

        using IPendingMarker = ShaderService::IPendingMarker;
        std::shared_ptr<IPendingMarker> CompileFromFile(
            const ::Assets::ResChar resId[], 
            const ::Assets::ResChar definesTable[]) const;
            
        std::shared_ptr<IPendingMarker> CompileFromMemory(
            const char shaderInMemory[], const char entryPoint[], 
            const char shaderModel[], const ::Assets::ResChar definesTable[]) const;

        MinimalShaderSource(std::shared_ptr<ShaderService::ILowLevelCompiler> compiler);
        ~MinimalShaderSource();

    protected:
        std::shared_ptr<ShaderService::ILowLevelCompiler> _compiler;

        std::shared_ptr<IPendingMarker> Compile(
            const void* shaderInMemory, size_t size,
            const ShaderService::ResId& resId,
            const ::Assets::ResChar definesTable[]) const;
    };


    auto MinimalShaderSource::PendingMarker::Resolve(
        const char initializer[],
        const std::shared_ptr<::Assets::DependencyValidation>& depVal) const -> const Payload&
    {
        if (!_payload) {
            StringSection<char> errorsString;
            if (_errors)
                errorsString = StringSection<char>(
                    (const char*)AsPointer(_errors->cbegin()),
                    (const char*)AsPointer(_errors->cend()));

            Throw(::Assets::Exceptions::InvalidAsset(initializer, errorsString.AsString().c_str()));
        }

        if (depVal)
            for (const auto& i:_deps)
                ::Assets::RegisterFileDependency(depVal, MakeStringSection(i._filename));
        return _payload;
    }

    ::Assets::AssetState MinimalShaderSource::PendingMarker::TryResolve(
        Payload& result,
        const std::shared_ptr<::Assets::DependencyValidation>& depVal) const
    {
        if (!_payload) return ::Assets::AssetState::Invalid;
        result = _payload;
        if (depVal)
            for (const auto& i:_deps)
                ::Assets::RegisterFileDependency(depVal, MakeStringSection(i._filename));
        return ::Assets::AssetState::Ready;
    }

    auto MinimalShaderSource::PendingMarker::GetErrors() const -> Payload { return _errors; }

    ::Assets::AssetState MinimalShaderSource::PendingMarker::StallWhilePending() const 
    {
        return _payload ? ::Assets::AssetState::Ready : ::Assets::AssetState::Invalid;
    }

    ShaderStage::Enum MinimalShaderSource::PendingMarker::GetStage() const { return _stage; }

    MinimalShaderSource::PendingMarker::PendingMarker(
        Payload payload, 
        std::vector<::Assets::DependentFileState> deps, ShaderStage::Enum stage)
    : _payload(std::move(payload)), _deps(std::move(deps)), _stage(stage)
    {}
    MinimalShaderSource::PendingMarker::PendingMarker(Payload errors)
    : _errors(std::move(errors)), _stage(ShaderStage::Null) {}
    MinimalShaderSource::PendingMarker::~PendingMarker() {}

    auto MinimalShaderSource::Compile(
        const void* shaderInMemory, size_t size,
        const ShaderService::ResId& resId,
        const ::Assets::ResChar definesTable[]) const -> std::shared_ptr<IPendingMarker>
    {
        using Payload = PendingMarker::Payload;
        Payload payload, errors;
        std::vector<::Assets::DependentFileState> deps;

        bool success = _compiler->DoLowLevelCompile(
            payload, errors, deps,
            shaderInMemory, size, resId, definesTable);

        if (!success)
            return std::make_shared<PendingMarker>(errors);
        return std::make_shared<PendingMarker>(
            std::move(payload), std::move(deps), resId.AsShaderStage());
    }

    auto MinimalShaderSource::CompileFromFile(
        const ::Assets::ResChar resource[], 
        const ::Assets::ResChar definesTable[]) const
        -> std::shared_ptr<IPendingMarker>
    {
        auto resId = ShaderService::MakeResId(resource, *_compiler);

        size_t fileSize = 0;
        auto fileData = LoadFileAsMemoryBlock(resId._filename, &fileSize);
        return Compile(fileData.get(), fileSize, resId, definesTable);
    }
            
    auto MinimalShaderSource::CompileFromMemory(
        const char shaderInMemory[], const char entryPoint[], 
        const char shaderModel[], const ::Assets::ResChar definesTable[]) const
        -> std::shared_ptr<IPendingMarker>
    {
        auto shaderLen = XlStringLen(shaderInMemory);
        return Compile(
            shaderInMemory, shaderLen,
            ShaderService::ResId(
                StringMeld<64>() << "ShaderInMemory_" << Hash64(shaderInMemory, &shaderInMemory[shaderLen]), 
                entryPoint, shaderModel),
            definesTable);
    }

    MinimalShaderSource::MinimalShaderSource(std::shared_ptr<ShaderService::ILowLevelCompiler> compiler)
    : _compiler(compiler) {}
    MinimalShaderSource::~MinimalShaderSource() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    MinimalAssetServices* MinimalAssetServices::s_instance = nullptr;

    MinimalAssetServices::MinimalAssetServices(RenderCore::IDevice* device)
    {
        _shaderService = std::make_unique<ShaderService>();
        auto shaderSource = std::make_shared<MinimalShaderSource>(Metal::CreateLowLevelShaderCompiler());
        _shaderService->AddShaderSource(shaderSource);

        if (device) {
            BufferUploads::AttachLibrary(ConsoleRig::GlobalServices::GetInstance());
            _bufferUploads = BufferUploads::CreateManager(device);
        }

        ConsoleRig::GlobalServices::GetCrossModule().Publish(*this);
    }

    MinimalAssetServices::~MinimalAssetServices()
    {
        if (_bufferUploads) {
            _bufferUploads.reset();
            BufferUploads::DetachLibrary();
        }

        ConsoleRig::GlobalServices::GetCrossModule().Withhold(*this);
    }

    void MinimalAssetServices::AttachCurrentModule()
    {
        assert(s_instance==nullptr);
        s_instance = this;
        ShaderService::SetInstance(_shaderService.get());
    }

    void MinimalAssetServices::DetachCurrentModule()
    {
        assert(s_instance==this);
        ShaderService::SetInstance(nullptr);
        s_instance = nullptr;
    }
}

