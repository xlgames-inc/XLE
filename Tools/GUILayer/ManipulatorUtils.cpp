// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ManipulatorUtils.h"
#include "ExportedNativeTypes.h"
#include "../ToolsRig/IManipulator.h"
#include "../GUILayer/MarshalString.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/StringUtils.h"

using namespace System;
using namespace System::Collections::Generic;
using namespace System::ComponentModel;

namespace GUILayer
{
    IPropertySource::~IPropertySource() {}

    template<typename ParamType>
        static const ParamType* FindParameter(
            const char name[], std::pair<ParamType*, size_t> params, bool caseInsensitive)
    {
        for (unsigned c=0; c<params.second; ++c) {
            bool match = false;
            if (caseInsensitive) {
                match = !XlCompareStringI(params.first[c]._name, name);
            } else {
                match = !XlCompareString(params.first[c]._name, name);
            }
            if (match) 
                return &params.first[c];
        }
        return nullptr;
    }

    public interface class IGetAndSetProperties
    {
    public:
        virtual bool TryGetMember(System::String^ name, bool caseInsensitive, System::Type^ type, Object^% result);
        virtual bool TrySetMember(System::String^ name, bool caseInsensitive, Object^ value);
    };

    public ref class BasicPropertySource : public IPropertySource
    {
    public:
        property PropertyDescriptorsType^ PropertyDescriptors { virtual PropertyDescriptorsType^ get() override; }
        property IEnumerable<Object^>^ Items { virtual IEnumerable<Object^>^ get() override; }

        BasicPropertySource(IGetAndSetProperties^ getAndSet, PropertyDescriptorsType^ propertyDescriptors);
        ~BasicPropertySource();
    protected:
        IGetAndSetProperties^ _helper;
        PropertyDescriptorsType^ _propertyDescriptors;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    ref class DynamicPropertyDescriptor : public PropertyDescriptor
    {
    public:
        Type^ _propertyType;

        DynamicPropertyDescriptor(
            System::String^ propertyName, Type^ propertyType, array<Attribute^,1>^ propertyAttributes)
            : PropertyDescriptor(propertyName, propertyAttributes)
        {
            _propertyType = propertyType;
        }

        System::Object^ GetValue(System::Object^ component) override
        {
            auto dynObject = dynamic_cast<IGetAndSetProperties^>(component);
            if (dynObject) {
                System::Object^ result = nullptr;
                if (dynObject->TryGetMember(Name, false, _propertyType, result)) {
                    return result;
                }
            }
            return nullptr;
        }

        void SetValue(System::Object^ component, System::Object^ value) override
        {
            auto dynObject = dynamic_cast<IGetAndSetProperties^>(component);
            if (dynObject) {
                dynObject->TrySetMember(Name, false, value);
            }
        }

        bool CanResetValue(System::Object^ component) override  { return true; }
        void ResetValue(System::Object^ component) override {}

        bool ShouldSerializeValue(System::Object^ component) override { return false; }
        property Type^ ComponentType    { Type^ get() override { return IGetAndSetProperties::typeid; } } 
        property bool IsReadOnly        { bool get() override { return false; } }
        property Type^ PropertyType     { Type^ get() override { return _propertyType; } }
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    IEnumerable<Object^>^ BasicPropertySource::Items::get()
    {
        auto result = gcnew List<Object^>();
        result->Add(_helper);
        return result; 
    }

    auto BasicPropertySource::PropertyDescriptors::get() -> PropertyDescriptorsType^
    {
        return _propertyDescriptors;
    }
    
            // note --  no protection on this pointer. Caller must ensure the
            //          native manipulator stays around for the life-time of 
            //          this object.
    BasicPropertySource::BasicPropertySource(
        IGetAndSetProperties^ getAndSet, PropertyDescriptorsType^ propertyDescriptors)
    : _propertyDescriptors(propertyDescriptors)
    {
        _helper = getAndSet;
    }

    BasicPropertySource::~BasicPropertySource()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    IManipulatorSet::~IManipulatorSet() {}

    public ref class Manipulator_GetAndSet : public IGetAndSetProperties
    {
    public:
        static System::ComponentModel::PropertyDescriptorCollection^ 
            CreatePropertyDescriptors(ToolsRig::IManipulator& manipulators);

        virtual bool TryGetMember(System::String^ name, bool caseInsensitive, System::Type^ type, Object^% result) 
        {
            auto nativeName = clix::marshalString<clix::E_UTF8>(name);
            auto floatParam = FindParameter(nativeName.c_str(), _manipulator->GetFloatParameters(), caseInsensitive);
            if (floatParam) {
                if (type == Single::typeid) {
                    result = gcnew Single(*(float*)PtrAdd(_manipulator.get(), floatParam->_valueOffset));
                    return true;
                }
            }

            auto intParam = FindParameter(nativeName.c_str(), _manipulator->GetIntParameters(), caseInsensitive);
            if (intParam) {
                if (type == Int32::typeid) {
                    result = gcnew Int32(*(int*)PtrAdd(_manipulator.get(), intParam->_valueOffset));
                    return true;
                }
            }

            auto boolParam = FindParameter(nativeName.c_str(), _manipulator->GetBoolParameters(), caseInsensitive);
            if (boolParam) {
                if (type == Boolean::typeid) {
                    unsigned* i = (unsigned*)PtrAdd(_manipulator.get(), boolParam->_valueOffset);
                    result = gcnew Boolean(!!((*i) & (1<<boolParam->_bitIndex)));
                    return true;
                }
            }

            result = nullptr;
            return false;
        }

        virtual bool TrySetMember(System::String^ name, bool caseInsensitive, Object^ value)
        {
            auto nativeName = clix::marshalString<clix::E_UTF8>(name);
            auto floatParam = FindParameter(nativeName.c_str(), _manipulator->GetFloatParameters(), caseInsensitive);
            if (floatParam) {
                if (dynamic_cast<Single^>(value)) {
                    *(float*)PtrAdd(_manipulator.get(), floatParam->_valueOffset) = *dynamic_cast<Single^>(value);
                    return true;
                }
            }

            auto intParam = FindParameter(nativeName.c_str(), _manipulator->GetIntParameters(), caseInsensitive);
            if (intParam) {
                if (dynamic_cast<Int32^>(value)) {
                    *(int*)PtrAdd(_manipulator.get(), intParam->_valueOffset) = *dynamic_cast<Int32^>(value);
                    return true;
                }
            }

            auto boolParam = FindParameter(nativeName.c_str(), _manipulator->GetBoolParameters(), caseInsensitive);
            if (boolParam) {
                if (dynamic_cast<Boolean^>(value)) {
                    unsigned* i = (unsigned*)PtrAdd(_manipulator.get(), boolParam->_valueOffset);
                    if (*dynamic_cast<Boolean^>(value)) {
                        *i |= (1<<boolParam->_bitIndex);
                    } else {
                        *i &= ~(1<<boolParam->_bitIndex);
                    }
                    return true;
                }
            }
            return false;
        }

        Manipulator_GetAndSet(std::shared_ptr<ToolsRig::IManipulator> manipulator)
            : _manipulator(std::move(manipulator)) {}
        ~Manipulator_GetAndSet() {}
    protected:
        clix::shared_ptr<ToolsRig::IManipulator> _manipulator;
    };

    System::ComponentModel::PropertyDescriptorCollection^ 
        Manipulator_GetAndSet::CreatePropertyDescriptors(ToolsRig::IManipulator& manipulators)
    {
            // We must convert each property in the manipulator 
            // into a property descriptor that can be used with 
            // the ATF GUI elements
        using System::ComponentModel::PropertyDescriptor;
        auto result = gcnew List<PropertyDescriptor^>();

        auto fParams = manipulators.GetFloatParameters();
        for (size_t c=0; c<fParams.second; ++c) {
            const auto& param = fParams.first[c];
            auto descriptor = 
                gcnew DynamicPropertyDescriptor(
                    clix::marshalString<clix::E_UTF8>(param._name),
                    System::Single::typeid, 
                    gcnew array<Attribute^, 1> {
                        gcnew DescriptionAttribute(
                            String::Format("{0} to {1} ({2})",
                                param._min, param._max, 
                                (param._scaleType == ToolsRig::IManipulator::FloatParameter::Linear)?"Linear":"Logarithmic"))
                    });
            result->Add(descriptor);
        }

        auto iParams = manipulators.GetIntParameters();
        for (size_t c=0; c<iParams.second; ++c) {
            const auto& param = iParams.first[c];
            auto descriptor = 
                gcnew DynamicPropertyDescriptor(
                    clix::marshalString<clix::E_UTF8>(param._name),
                    System::Int32::typeid, 
                    gcnew array<Attribute^, 1> {
                        gcnew DescriptionAttribute(
                            String::Format("{0} to {1} ({2})",
                                param._min, param._max, 
                                (param._scaleType == ToolsRig::IManipulator::IntParameter::Linear)?"Linear":"Logarithmic"))
                    });
            result->Add(descriptor);
        }

        auto bParams = manipulators.GetBoolParameters();
        for (size_t c=0; c<bParams.second; ++c) {
            const auto& param = bParams.first[c];
            auto descriptor = 
                gcnew DynamicPropertyDescriptor(
                    clix::marshalString<clix::E_UTF8>(param._name),
                    System::Boolean::typeid, nullptr);
            result->Add(descriptor);
        }

            // The conversion here is expensive, because PropertyDescriptorCollection
            // goes back to pre-generic C#.
            // But PropertyDescriptorCollection is also more compatible than just a
            // IEnumerable<PropertyDescriptor^>
        return gcnew System::ComponentModel::PropertyDescriptorCollection(result->ToArray());
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    IPropertySource^ IManipulatorSet::GetProperties(System::String^ name)
    {
        auto m = GetManipulator(name);
        if (m)
            return gcnew BasicPropertySource(
                gcnew Manipulator_GetAndSet(m.GetNativePtr()),
                Manipulator_GetAndSet::CreatePropertyDescriptors(*m.GetNativePtr()));
        return nullptr;
    }

    template clix::shared_ptr<ToolsRig::IManipulator>;
}


