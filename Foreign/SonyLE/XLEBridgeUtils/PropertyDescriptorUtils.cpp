// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PropertyDescriptorUtils.h"

using namespace System::Xml;
using namespace Sce::Atf;
using namespace Sce::Atf::Dom;

namespace XLEBridgeUtils
{
    static IEnumerable<ComponentModel::PropertyDescriptor^>^ 
         AsEnumerable(PropertyDescriptorCollection^ input)
     {
        List<ComponentModel::PropertyDescriptor^>^ propertyDescriptors = nullptr;
        if (input && input->Count > 0) {
            propertyDescriptors = gcnew List<ComponentModel::PropertyDescriptor^>(input->Count);
            for each(ComponentModel::PropertyDescriptor^ d in input)
                propertyDescriptors->Add(d);
        }
        return propertyDescriptors;
     }

    PropertyDescriptorCollection^
        DataDrivenPropertyContextHelper::GetPropertyDescriptors(String^ type)
    {
        return GetNodeType(type)->GetTag<PropertyDescriptorCollection^>();
    }

    void DataDrivenPropertyContextHelper::ParseAnnotations(
        XmlSchemaSet^ schemaSet,
        IDictionary<NamedMetadata^, IList<XmlNode^>^>^ annotations) 
    {
        __super::ParseAnnotations(schemaSet, annotations);

        for each(auto kv in annotations) {
            if (kv.Value->Count == 0) continue;

            auto nodeType = dynamic_cast<DomNodeType^>(kv.Key);
            if (!nodeType) continue;

                //  The annotations define property descriptors for this type
                //  This is designed to mirror the same schema layout as the main 
                //  dom files used by the level editor.
            auto localDescriptor = nodeType->GetTagLocal<PropertyDescriptorCollection^>();
            auto annotationDescriptor = ParseXml(nodeType, kv.Value);
                
            if (localDescriptor != nullptr) {
                for each (ComponentModel::PropertyDescriptor^ propDecr in annotationDescriptor)
                    localDescriptor->Add(propDecr);
            } else localDescriptor = annotationDescriptor;
               
            if (localDescriptor->Count > 0)
                nodeType->SetTag<PropertyDescriptorCollection^>(localDescriptor);
        }
    }

    PropertyDescriptorCollection^ DataDrivenPropertyContextHelper::ParseXml(DomNodeType^ type, IEnumerable<XmlNode^>^ annotations)
    {
        auto rawDescriptors = Sce::Atf::Dom::PropertyDescriptor::ParseXml(type, annotations);

            //  Convert these descriptors into our adapter types
            //  The ATF adapters are only made to be used with Dom Node types... But we
            //  want to do something very similar with a non-dom-node type.
            //  This is sort of awkward... But actually it reuses all of the
            //  code in ATF really well. It's a quite effective solution, even
            //  though it's a bit wierd.

        auto descriptors = gcnew PropertyDescriptorCollection(
            EmptyArray<Sce::Atf::Dom::PropertyDescriptor^>::Instance);

        for each(auto descriptor in rawDescriptors) {
            auto attrib = dynamic_cast<AttributePropertyDescriptor^>(descriptor);
            if (!attrib) continue;

            auto newDescriptor = gcnew DynamicPropertyDescriptor(
                attrib->Name,
                attrib->AttributeInfo,
                attrib->Category,
                attrib->Description,
                attrib->IsReadOnly,
                attrib->GetEditor(Object::typeid),
                attrib->Converter,
                AsArray(attrib->Attributes));
            descriptors->Add(newDescriptor);
        }

        delete rawDescriptors;
        return descriptors;
    }

    array<Attribute^>^ DataDrivenPropertyContextHelper::AsArray(AttributeCollection^ collection)
    {
        if (!collection->Count) return nullptr;

        auto result = gcnew array<Attribute^>(collection->Count);
        for (int c=0; c<collection->Count; ++c)
            result[c] = collection[c];
        return result;
    }


    IEnumerable<Object^>^ BasicPropertyEditingContext::Items::get() 
    {
        return _objects;
    }

    IEnumerable<System::ComponentModel::PropertyDescriptor^>^ 
        BasicPropertyEditingContext::PropertyDescriptors::get()
    {
        return _properties;
    }

    BasicPropertyEditingContext::BasicPropertyEditingContext(
        Object^ object, 
        PropertyDescriptorCollection^ properties)
    {
        _objects = gcnew List<Object^>();
        _objects->Add(object);
        _properties = AsEnumerable(properties);
    }

    BasicPropertyEditingContext::~BasicPropertyEditingContext() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    IEnumerable<Object^>^ PropertyBridge::Items::get() { return _source->Items; }
    IEnumerable<System::ComponentModel::PropertyDescriptor^>^ PropertyBridge::PropertyDescriptors::get()
    {
        return _propDescs;
    }

    PropertyBridge::PropertyBridge(GUILayer::IPropertySource^ source) : _source(source) 
    {
            // expensive conversion from the old PropertyDescriptorCollection to 
            // a generic enumerable type
        _propDescs = AsEnumerable(_source->PropertyDescriptors);
    }
    PropertyBridge::~PropertyBridge() { delete _source; }
}
