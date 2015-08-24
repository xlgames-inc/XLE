// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once 

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Xml::Schema;
using namespace System::ComponentModel;

using System::Xml::XmlNode;
using Sce::Atf::Dom::NamedMetadata;
using Sce::Atf::Dom::DomNode;
using Sce::Atf::Dom::DomNodeType;

namespace XLEBridgeUtils
{
    public ref class BasicPropertyEditingContext : Sce::Atf::Applications::IPropertyEditingContext
    {
    public:
        property IEnumerable<Object^>^ Items { virtual IEnumerable<Object^>^ get(); }
        property IEnumerable<ComponentModel::PropertyDescriptor^>^ PropertyDescriptors
            { virtual IEnumerable<ComponentModel::PropertyDescriptor^>^ get(); }

        BasicPropertyEditingContext(
            Object^ object, 
            PropertyDescriptorCollection^ properties);
        ~BasicPropertyEditingContext();
    private:
        IEnumerable<ComponentModel::PropertyDescriptor^>^ _properties;
        List<Object^>^ _objects;
    };

    public ref class PropertyBridge : Sce::Atf::Applications::IPropertyEditingContext
    {
    public:
        property IEnumerable<Object^>^ Items { virtual IEnumerable<Object^>^ get(); }
        property IEnumerable<System::ComponentModel::PropertyDescriptor^>^ PropertyDescriptors
            { virtual IEnumerable<System::ComponentModel::PropertyDescriptor^>^ get(); }

        PropertyBridge(GUILayer::IPropertySource^ source);
        ~PropertyBridge();
    private:
        GUILayer::IPropertySource^ _source;
        IEnumerable<System::ComponentModel::PropertyDescriptor^>^ _propDescs;
    };

    public ref class DataDrivenPropertyContextHelper : public Sce::Atf::Dom::XmlSchemaTypeLoader
    {
    public:
        PropertyDescriptorCollection^ GetPropertyDescriptors(String^ type);

    protected:
        void ParseAnnotations(
            XmlSchemaSet^ schemaSet,
            IDictionary<NamedMetadata^, IList<XmlNode^>^>^ annotations) override;

        static PropertyDescriptorCollection^ ParseXml(DomNodeType^ type, IEnumerable<XmlNode^>^ annotations);

    private:
        static array<Attribute^>^ AsArray(AttributeCollection^ collection);
    };

    ref class DynamicPropertyDescriptor : public Sce::Atf::Dom::AttributePropertyDescriptor
    {
    public:
        DynamicPropertyDescriptor(
            String^ name,
            Sce::Atf::Dom::AttributeInfo^ attribute,
            String^ category,
            String^ description,
            bool isReadOnly,
            Object^ editor,
            TypeConverter^ typeConverter,
            array<Attribute^>^ attributes)
        : Sce::Atf::Dom::AttributePropertyDescriptor(
            name, attribute, category, description, 
            isReadOnly, editor, typeConverter, attributes)
        {}

        bool CanResetValue(Object^ component) override
        {
            if (IsReadOnly) return false;

            auto value = GetValue(component);
            return (value != nullptr && !value->Equals(AttributeInfo->DefaultValue))
                || (value == nullptr && AttributeInfo->DefaultValue != nullptr);
        }

        property Type^ ComponentType
        {
            Type^ get() override { return GUILayer::IGetAndSetProperties::typeid; }
        }

        Object^ GetValue(Object^ component) override
        {
            auto dynObject = dynamic_cast<GUILayer::IGetAndSetProperties^>(component);
            if (dynObject) {
                Object^ result = nullptr;
                if (dynObject->TryGetMember(AttributeInfo->Name, false, AttributeInfo->Type->ClrType, result)) {
                    return result;
                }
            }

            return nullptr;
        }

        void SetValue(Object^ component, Object^ value) override
        {
            auto dynObject = dynamic_cast<GUILayer::IGetAndSetProperties^>(component);
            if (dynObject) {
                dynObject->TrySetMember(AttributeInfo->Name, false, value);
            }
        }

        DomNode^ GetNode(Object^ component) override 
        {
            throw gcnew InvalidOperationException("Attempting to call GetNode() on non-DOM based property descriptor");
        }
    };
}

