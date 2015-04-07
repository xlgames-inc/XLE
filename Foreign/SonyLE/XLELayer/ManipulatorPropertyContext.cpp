// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ManipulatorPropertyContext.h"
#include "../../Tools/ToolsRig/IManipulator.h"
#include "../../Tools/GUILayer/MarshalString.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/StringUtils.h"

using namespace System;
using namespace System::Collections::Generic;
using namespace System::ComponentModel;
using namespace Sce::Atf::Applications;

namespace XLELayer
{
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

    ref class ManipulatorPropertyContext::Helper : public ::System::Dynamic::DynamicObject
    {
    public:
        bool TryGetMember(System::Dynamic::GetMemberBinder^ binder, Object^% result) override
        {
            return TryGetMember(binder->Name, binder->IgnoreCase, result);
        }

        bool TrySetMember(System::Dynamic::SetMemberBinder^ binder, Object^ value) override
        {
            return TryGetMember(binder->Name, binder->IgnoreCase, value);
        }

        bool TryGetMember(System::String^ name, bool ignoreCase, Object^% result)
        {
            auto nativeName = clix::marshalString<clix::E_UTF8>(name);
            auto floatParam = FindParameter(nativeName.c_str(), _manipulator->GetFloatParameters(), ignoreCase);
            if (floatParam) {
                result = gcnew Single(*(float*)PtrAdd(_manipulator.get(), floatParam->_valueOffset));
                return true;
            }

            auto boolParam = FindParameter(nativeName.c_str(), _manipulator->GetBoolParameters(), ignoreCase);
            if (boolParam) {
                result = gcnew Boolean(*(bool*)PtrAdd(_manipulator.get(), floatParam->_valueOffset));
                return true;
            }

            result = nullptr;
            return false;
        }

        bool TrySetMember(System::String^ name, bool ignoreCase, Object^ value)
        {
            auto nativeName = clix::marshalString<clix::E_UTF8>(name);
            auto floatParam = FindParameter(nativeName.c_str(), _manipulator->GetFloatParameters(), ignoreCase);
            if (floatParam) {
                *(float*)PtrAdd(_manipulator.get(), floatParam->_valueOffset) = (float)value;
                return true;
            }
            auto boolParam = FindParameter(nativeName.c_str(), _manipulator->GetBoolParameters(), ignoreCase);
            if (boolParam) {
                *(bool*)PtrAdd(_manipulator.get(), floatParam->_valueOffset) = (bool)value;
                return true;
            }
            return false;
        }

        Helper(std::shared_ptr<ToolsRig::IManipulator> manipulator)
            : _manipulator(manipulator)
        {
        }
    protected:
        clix::shared_ptr<ToolsRig::IManipulator> _manipulator;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    ref class ManipulatorPropertyContext::DynamicPropertyDescriptor : PropertyDescriptor
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
            auto dynObject = dynamic_cast<Helper^>(component);
            if (dynObject) {
                System::Object^ result = nullptr;
                if (dynObject->TryGetMember(Name, false, result)) {
                    return result;
                }
            }
            return nullptr;
        }

        void SetValue(System::Object^ component, System::Object^ value) override
        {
            auto dynObject = dynamic_cast<Helper^>(component);
            if (dynObject) {
                dynObject->TrySetMember(Name, false, value);
            }
        }

        bool CanResetValue(System::Object^ component) override  { return true; }
        void ResetValue(System::Object^ component) override {}

        bool ShouldSerializeValue(System::Object^ component) override { return false; }
        property Type^ ComponentType    { Type^ get() override { return Helper::typeid; } } 
        property bool IsReadOnly        { bool get() override { return false; } }
        property Type^ PropertyType     { Type^ get() override { return _propertyType; } }
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    IEnumerable<Object^>^ ManipulatorPropertyContext::Items::get()
    {
        auto result = gcnew List<Object^>();
        result->Add(_helper);
        return result; 
    }

    IEnumerable<System::ComponentModel::PropertyDescriptor^>^ 
        ManipulatorPropertyContext::PropertyDescriptors::get()
    {
            // We must convert each property in the manipulator 
            // into a property descriptor that can be used with 
            // the ATF GUI elements
        using System::ComponentModel::PropertyDescriptor;
        auto result = gcnew List<PropertyDescriptor^>();

        auto fParams = _manipulator->GetFloatParameters();
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

        return result;
    }
    
            // note --  no protection on this pointer. Caller must ensure the
            //          native manipulator stays around for the life-time of 
            //          this object.
    ManipulatorPropertyContext::ManipulatorPropertyContext(std::shared_ptr<ToolsRig::IManipulator> manipulator)
        : _manipulator(manipulator)
    {
        _helper = gcnew Helper(_manipulator);
    }

    ManipulatorPropertyContext::~ManipulatorPropertyContext()
    {
        delete _helper;
    }

    ManipulatorPropertyContext^ ManipulatorPropertyContext::Create(
        GUILayer::IManipulatorSet^ mani, System::String^ name)
    {
        auto m = mani->GetManipulator(name);
        if (m) return gcnew ManipulatorPropertyContext(
            *(std::shared_ptr<ToolsRig::IManipulator>*)m.GetNativeOpaque());
        return nullptr;
    }


}


