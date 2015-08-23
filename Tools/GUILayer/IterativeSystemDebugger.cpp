// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "IterativeSystemDebugger.h"
#include "IOverlaySystem.h"
#include "ManipulatorUtils.h"       // for IGetAndSetProperties
#include "MarshalString.h"
#include "../../SceneEngine/Erosion.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../Utility/Meta/ClassAccessors.h"
#include "../../Utility/MemoryUtils.h"
#include <memory>

namespace GUILayer
{
    using ErosionSettings = SceneEngine::ErosionSimulation::Settings;

    ref class ErosionOverlay : public IOverlaySystem
    {
    public:
        virtual void RenderToScene(
            RenderCore::IThreadContext* device, 
            SceneEngine::LightingParserContext& parserContext) override;
        virtual void RenderWidgets(
            RenderCore::IThreadContext* device, 
            const RenderCore::Techniques::ProjectionDesc& projectionDesc) override;
        virtual void SetActivationState(bool newState) override {}

        ErosionOverlay(
            std::shared_ptr<SceneEngine::ErosionSimulation> sim,
            ErosionIterativeSystem::Settings^ previewSettings);
        !ErosionOverlay();
        ~ErosionOverlay();
    private:
        clix::shared_ptr<SceneEngine::ErosionSimulation> _sim;
        ErosionIterativeSystem::Settings^ _previewSettings;
    };

    void ErosionOverlay::RenderToScene(
        RenderCore::IThreadContext* device,
        SceneEngine::LightingParserContext& parserContext)
    {
        auto metalContext = RenderCore::Metal::DeviceContext::Get(*device);
        _sim->RenderDebugging(*metalContext, parserContext);
    }

    void ErosionOverlay::RenderWidgets(
        RenderCore::IThreadContext* device,
        const RenderCore::Techniques::ProjectionDesc& projectionDesc)
    {

    }

    ErosionOverlay::ErosionOverlay(
        std::shared_ptr<SceneEngine::ErosionSimulation> sim,
        ErosionIterativeSystem::Settings^ previewSettings)
    : _sim(sim), _previewSettings(previewSettings)
    {}

    ErosionOverlay::!ErosionOverlay() { _sim.reset(); }
    ErosionOverlay::~ErosionOverlay() { _sim.reset(); }

////////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename Type>
        public ref class ClassAccessors_GetAndSet : public IGetAndSetProperties
    {
    public:
        virtual bool TryGetMember(System::String^ name, bool caseInsensitive, System::Type^ type, Object^% result);
        virtual bool TrySetMember(System::String^ name, bool caseInsensitive, Object^ value);

        explicit ClassAccessors_GetAndSet(std::shared_ptr<Type> type);
        !ClassAccessors_GetAndSet();
        ~ClassAccessors_GetAndSet();

    protected:
        clix::shared_ptr<Type> _type;
    };

    template<typename Type>
        bool ClassAccessors_GetAndSet<Type>::TryGetMember(System::String^ name, bool caseInsensitive, System::Type^ type, Object^% result)
    {
        auto& accessors = GetAccessors<Type>();
        auto nativeString = clix::marshalString<clix::E_UTF8>(name);
        if (type == System::Single::typeid) {
            float f = 0.f;
            bool success = accessors.TryGet(
                f, *_type.get(), Hash64(nativeString.c_str()));
            if (success)
                result = gcnew System::Single(f);
            return success;
        } 
        else if (type == System::UInt32::typeid) {
            uint32 f = 0;
            bool success = accessors.TryGet(
                f, *_type.get(), Hash64(nativeString.c_str()));
            if (success)
                result = gcnew System::UInt32(f);
            return success;
        }
        return false;
    }

    template<typename Type>
        bool ClassAccessors_GetAndSet<Type>::TrySetMember(System::String^ name, bool caseInsensitive, Object^ value)
    {
        auto& accessors = GetAccessors<Type>();
        auto nativeString = clix::marshalString<clix::E_UTF8>(name);
        if (value->GetType() == System::Single::typeid) {
            auto v = *dynamic_cast<System::Single^>(value);
            return accessors.TrySet(v, *_type.get(), Hash64(nativeString.c_str()));
        } else if (value->GetType() == System::UInt32::typeid) {
            auto v = *dynamic_cast<System::UInt32^>(value);
            return accessors.TryGet(v, *_type.get(), Hash64(nativeString.c_str()));
        }
        return false;
    }

    template<typename Type>
        ClassAccessors_GetAndSet<Type>::ClassAccessors_GetAndSet(std::shared_ptr<Type> type)
        : _type(type)
    {}

    template<typename Type>
        ClassAccessors_GetAndSet<Type>::!ClassAccessors_GetAndSet() { _type.reset(); }

    template<typename Type>
        ClassAccessors_GetAndSet<Type>::~ClassAccessors_GetAndSet() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    class ErosionIterativeSystemPimpl
    {
    public:
        std::shared_ptr<SceneEngine::ErosionSimulation> _sim;
        std::shared_ptr<ErosionSettings> _settings;
    };

    void ErosionIterativeSystem::Tick()
    {}

    ErosionIterativeSystem::ErosionIterativeSystem(String^ sourceHeights)
    {
        _pimpl.reset(new ErosionIterativeSystemPimpl);
        _pimpl->_sim = std::make_shared<SceneEngine::ErosionSimulation>(UInt2(512, 512), 1.f);
        _pimpl->_settings = std::make_shared<ErosionSettings>();
        _settings = gcnew Settings;

        _getAndSetProperties = gcnew ClassAccessors_GetAndSet<ErosionSettings>(_pimpl->_settings);
        _overlay = gcnew ErosionOverlay(_pimpl->_sim, _settings);
    }

    ErosionIterativeSystem::!ErosionIterativeSystem()
    {
        _pimpl.reset();
    }

    ErosionIterativeSystem::~ErosionIterativeSystem()
    {
        _pimpl.reset();
    }
}

