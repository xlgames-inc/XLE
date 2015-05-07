// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../Assets/Assets.h"
#include "../../RenderCore/Assets/Material.h"
#include "../../Tools/GUILayer/MarshalString.h"

using namespace Sce::Atf;
using namespace Sce::Atf::Applications;
using namespace Sce::Atf::Dom;
using namespace System::Collections::Generic;
using namespace System::Reflection;
using namespace System::ComponentModel;
using namespace System::ComponentModel::Composition;
using namespace System::Xml;
using namespace System::Xml::Schema;

namespace XLELayer
{
    ref class DynamicPropertyDescriptor : public Sce::Atf::Dom::AttributePropertyDescriptor
    {
    public:
        DynamicPropertyDescriptor(
            System::String^ name,
            Sce::Atf::Dom::AttributeInfo^ attribute,
            System::String^ category,
            System::String^ description,
            bool isReadOnly,
            System::Object^ editor,
            TypeConverter^ typeConverter,
            array<System::Attribute^>^ attributes)
        : Sce::Atf::Dom::AttributePropertyDescriptor(
            name, attribute, category, description, 
            isReadOnly, editor, typeConverter, attributes)
        {}

        bool CanResetValue(System::Object^ component) override
        {
            if (IsReadOnly) return false;

            auto value = GetValue(component);
            return (value != nullptr && !value->Equals(AttributeInfo->DefaultValue))
                || (value == nullptr && AttributeInfo->DefaultValue != nullptr);
        }

        System::Object^ GetValue(System::Object^ component) override
        {
            auto dynObject = dynamic_cast<GUILayer::IGetAndSetProperties^>(component);
            if (dynObject) {
                System::Object^ result = nullptr;
                if (dynObject->TryGetMember(AttributeInfo->Name, false, AttributeInfo->Type->ClrType, result)) {
                    return result;
                }
            }

            return nullptr;
        }

        void SetValue(System::Object^ component, System::Object^ value) override
        {
            auto dynObject = dynamic_cast<GUILayer::IGetAndSetProperties^>(component);
            if (dynObject) {
                dynObject->TrySetMember(AttributeInfo->Name, false, value);
            }
        }

        DomNode^ GetNode(System::Object^ component) override 
        {
            throw gcnew System::InvalidOperationException("Attempting to call GetNode() on non-DOM based property descriptor");
        }

    };

    [Export(MaterialSchemaLoader::typeid)]
    [PartCreationPolicy(CreationPolicy::Shared)]
    public ref class MaterialSchemaLoader : public XmlSchemaTypeLoader
    {
    public:
        MaterialSchemaLoader()
        {
            auto resourceNames = Assembly::GetExecutingAssembly()->GetManifestResourceNames();
            for each(auto resourceName in resourceNames)
            {
                System::Diagnostics::Trace::WriteLine(resourceName);
            }

            SchemaResolver = gcnew ResourceStreamResolver(Assembly::GetExecutingAssembly(), ".");
            Load("material.xsd");
        }

        DomNodeType^ GetMaterialType() { return GetNodeType("gap:RawMaterial"); }

    protected:
        void ParseAnnotations(
            XmlSchemaSet^ schemaSet,
            IDictionary<NamedMetadata^, IList<XmlNode^>^>^ annotations) override 
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
                    for each (System::ComponentModel::PropertyDescriptor^ propDecr in annotationDescriptor)
                        localDescriptor->Add(propDecr);
                } else localDescriptor = annotationDescriptor;
               
                if (localDescriptor->Count > 0)
                    nodeType->SetTag<PropertyDescriptorCollection^>(localDescriptor);
            }
        }

        static PropertyDescriptorCollection^ ParseXml(DomNodeType^ type, IEnumerable<XmlNode^>^ annotations)
        {
            auto rawDescriptors = Sce::Atf::Dom::PropertyDescriptor::ParseXml(type, annotations);

                //  Convert these descriptors into our adapter types
                //  

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
                    attrib->GetEditor(System::Object::typeid),
                    attrib->Converter,
                    AsArray(attrib->Attributes));
                descriptors->Add(newDescriptor);
            }

            delete rawDescriptors;
            return descriptors;
        }

    private:
        static array<System::Attribute^>^ AsArray(AttributeCollection^ collection)
        {
            if (!collection->Count) return nullptr;

            auto result = gcnew array<System::Attribute^>(collection->Count);
            for (int c=0; c<collection->Count; ++c)
                result[c] = collection[c];
            return result;
        }

#if 0
        static PropertyDescriptorCollection^ ParseXml(DomNodeType^ type, IEnumerable<XmlNode^>^ annotations)
        {
                // based on Sce.Atf.Dom.PropertyDescriptor
                //      --  but we can't use the default one, because that create
                //          property description objects that only work with dom nodes
                //          We want to create a special case "PropertyDescriptor" objects
                //
                //      Unfortunately it requires copying a lot of code!
            auto descriptors = gcnew PropertyDescriptorCollection(
                EmptyArray<Sce::Atf::Dom::PropertyDescriptor^>::Instance);

            for each (XmlNode^ annotation in annotations) {
                try {
                    // Get name, try to parse it as a path
                    auto nameAttr = annotation->Attributes["name"];
                    System::String^ name = nullptr;
                    array<System::String^>^ segments = nullptr;
                    if (nameAttr != nullptr) {
                        name = nameAttr->Value;
                        segments = name->Split(
                            gcnew array<System::Char>(3) {'/', '\\', '.'}, 
                            System::StringSplitOptions::RemoveEmptyEntries);
                    }

                    auto descriptor = GetDescriptor(type, annotation, name, segments);
                    if (descriptor != nullptr)
                        descriptors->Add(descriptor);
                }
                catch (AnnotationException^ ex)
                {
                    Outputs::WriteLine(OutputMessageType::Warning, ex->Message);
                }
            }

            return descriptors;
        }

        static System::ComponentModel::PropertyDescriptor^ GetDescriptor(
            DomNodeType^ type, XmlNode^ annotation, System::String^ name,
            array<System::String^>^ segments)
        {
            System::ComponentModel::PropertyDescriptor^ desc = nullptr;

                // Get mandatory display name
            auto displayNameAttr = annotation->Attributes["displayName"];
            if (displayNameAttr != nullptr) {
                if (System::String::IsNullOrEmpty(name))
                    throw gcnew AnnotationException(System::String::Format(
                        "Required name attribute is null or empty.\r\nType: {0}\r\nElement: {1}",
                        type->Name, annotation->Name));
                
                auto displayName = displayNameAttr->Value;
                if (System::String::IsNullOrEmpty(displayName))
                    displayName = name;

                // Get optional annotations
                auto category = GetAnnotation(annotation, "category");
                auto description = GetAnnotation(annotation, "description");
                bool readOnly = GetAnnotation(annotation, "readOnly") == "true";
                auto editor = CreateObject<System::Object^>(type, annotation, "editor");
                auto typeConverter = CreateObject<TypeConverter^>(type, annotation, "converter");

                if (annotation->Name == "scea.dom.editors.attribute")
                {
                    // Attribute annotation
                    if (segments == nullptr)
                        throw gcnew AnnotationException("Unnamed attribute");

                    if (segments->Length == 1) // local attribute
                    {
                        auto metaAttr = type->GetAttributeInfo(name);
                        if (metaAttr == nullptr)
                            throw gcnew AnnotationException("Type doesn't have this attribute");

                        desc = gcnew AttributePropertyDescriptor(
                            displayName, metaAttr,
                            category, description, readOnly, editor, typeConverter);
                    }
                    else // descendant attribute
                    {
                        auto metaElements = GetPath(type, segments, segments->Length - 1);
                        DomNodeType^ childType = metaElements[segments->Length - 2]->Type;
                        AttributeInfo^ metaAttr = childType->GetAttributeInfo(segments[segments->Length - 1]);
                        if (metaAttr == nullptr)
                            throw gcnew AnnotationException("Descendant type doesn't have this attribute");

                        desc = gcnew ChildAttributePropertyDescriptor(
                            displayName, metaAttr, metaElements,
                            category, description, readOnly, editor, typeConverter);
                    }
                }
                else if (annotation->Name == "scea.dom.editors.child")
                {
                    // Child value annotation
                    auto element = type->GetChildInfo(name);
                    if (element == nullptr)
                        throw gcnew AnnotationException("Type doesn't have this element");

                    desc = gcnew ChildPropertyDescriptor(
                        displayName, element,
                        category, description, readOnly, editor, typeConverter);
                }
            }
            return desc;
        }

        template<typename T>
            static T CreateObject(DomNodeType^ domNodeType, XmlNode^ annotation, System::String^ attribute)
        {
            auto typeName = GetAnnotation(annotation, attribute);
            auto paramString = System::String::Empty;
            if (typeName != nullptr) {
                // check for params
                int colonIndex = typeName->IndexOf(':');
                if (colonIndex >= 0) {
                    int paramsIndex = colonIndex + 1;
                    paramString = typeName->Substring(paramsIndex, typeName->Length - paramsIndex);
                    typeName = typeName->Substring(0, colonIndex);
                }

                // create object from type name
                auto objectType = System::Type::GetType(typeName);
                if (objectType == nullptr)
                    throw gcnew AnnotationException("Couldn't find type " + typeName);

                // initialize with params
                auto obj = Activator.CreateInstance(objectType);
                auto annotatedObj = dynamic_cast<IAnnotatedParams^>(obj);
                if (annotatedObj != null) {
                    array<System::String^>^ parameters;

                    if (!System::String::IsNullOrEmpty(paramString))
                        parameters = paramString->Split(',');
                    else
                        parameters = TryGetEnumeration(domNodeType, annotation);

                    if (parameters != null)
                        annotatedObj->Initialize(parameters);
                }

                auto result = dynamic_cast<T>(obj);
                if (!result)
                    throw gcnew AnnotationException("Object must be " + T::typeid);
                return result;
            }
            else
            {
                return nullptr;
            }
        }

        static System::String^ AnnotationsNameAttribute = "name";
        static System::String^ AnnotationsDisplayNameAttribute = "displayName";
        static System::String^ AnnotationsLegacyEnumeration = "scea.dom.editors.enumeration";

        static array<System::String^>^ TryGetEnumeration(DomNodeType^ domNodeType, XmlNode^ annotation)
        {
            array<System::String^>^ enumeration = nullptr;
            auto targetDomAttribute = annotation->Attributes[AnnotationsNameAttribute];
            if (targetDomAttribute != nullptr)
            {
                auto domObjectAttribute = domNodeType->GetAttributeInfo(targetDomAttribute->Value);
                if (domObjectAttribute != nullptr)
                {
                    auto attributeType = domObjectAttribute->Type;
                    auto xmlAnnotation = attributeType->GetTag<IEnumerable<XmlNode^>^>();
                    if (xmlAnnotation != nullptr) {
                        auto enumerationList = gcnew List<System::String^>();
                        for each (auto enumAnnotation in xmlAnnotation) {
                            if (enumAnnotation->Name == AnnotationsLegacyEnumeration) {
                                auto name = enumAnnotation->Attributes[AnnotationsNameAttribute]->Value;
                                auto displayNode =
                                    enumAnnotation->Attributes->GetNamedItem(AnnotationsDisplayNameAttribute);
                                if (displayNode != nullptr)
                                    enumerationList->Add(name + "==" + displayNode->Value);
                                else
                                    enumerationList->Add(name);
                            }
                            enumeration = enumerationList->ToArray();
                        }
                    }
                }
            }
           
            return enumeration;
        }

        static System::String^ GetAnnotation(XmlNode^ annotation, System::String^ attributeName)
        {
            System::String^ result = nullptr;
            if (annotation != nullptr) {
                auto attribute = annotation->Attributes[attributeName];
                if (attribute != nullptr)
                    result = attribute->Value;
            }
            return result;
        }

        static array<ChildInfo^>^ GetPath(DomNodeType^ type, array<System::String^>^ segments, int length)
        {
            auto result = gcnew array<ChildInfo^>(length);
            for (int i = 0; i < length; i++) {
                auto metaElement = type->GetChildInfo(segments[i]);
                if (metaElement == nullptr)
                    throw gcnew AnnotationException("Invalid path");

                result[i] = metaElement;
                type = metaElement->Type;
            }

            return result;
        }
#endif
    };

    public ref class RawMaterialShaderConstants_GetAndSet : public GUILayer::IGetAndSetProperties
    {
    public:
        virtual bool TryGetMember(System::String^ name, bool caseInsensitive, System::Type^ type, Object^% result) 
        {
                // We can choose to interact with the "BindingList" object attached the C++/CLI object
                // or the underlying C++ layer. If we change the C++ layer, then we need to manually
                // rebuild the binding list objects in the GUILayer part.
                // Using the C++ layer give us a little more flexibility. 
            auto list = _material->ShaderConstants;
            for each(auto v in list) {
                if (System::String::Compare(name, v->Name) == 0) {
                    try {
                        // result = TypeDescriptor::GetConverter(System::String::typeid)->ConvertTo(v->Value, type);
                        // return result != nullptr;
                        result = System::Convert::ChangeType(v->Value, type);
                        return result != nullptr;
                    } catch (System::NotSupportedException^) {}
                }
            }
            return false;
        }

        virtual bool TrySetMember(System::String^ name, bool caseInsensitive, Object^ value)
        {
            auto list = _material->ShaderConstants;
            for each(auto v in list) {
                if (System::String::Compare(name, v->Name) == 0) {
                    v->Value = value->ToString();
                    return true;
                }
            }

                // no pre-existing value.. create a new one
            list->Add(GUILayer::RawMaterial::MakePropertyPair(name, value->ToString()));
            return true;
        }

        RawMaterialShaderConstants_GetAndSet(GUILayer::RawMaterial^ material) : _material(material) {}
        ~RawMaterialShaderConstants_GetAndSet() {}
    protected:
        GUILayer::RawMaterial^ _material;
    };

    public ref class MaterialPropertyContext : IPropertyEditingContext
    {
    public:
        property IEnumerable<System::Object^>^ Items
        {
            virtual IEnumerable<System::Object^>^ get()
            {
                auto result = gcnew List<Object^>();
                result->Add(_helper);
                return result; 
            }
        }

        /// <summary>
        /// Gets an enumeration of the property descriptors for the items</summary>
        property IEnumerable<System::ComponentModel::PropertyDescriptor^>^ PropertyDescriptors
        {
            virtual IEnumerable<System::ComponentModel::PropertyDescriptor^>^ get() 
            {
                return _propertyDescriptors;
            }
        }

        MaterialPropertyContext(GUILayer::RawMaterial^ material, MaterialSchemaLoader^ schema)
        {
            _helper = gcnew RawMaterialShaderConstants_GetAndSet(material);
            
            _propertyDescriptors = nullptr;
            auto descriptors = schema->GetMaterialType()->GetTag<PropertyDescriptorCollection^>();
            if (descriptors && descriptors->Count > 0) {
                _propertyDescriptors = gcnew List<System::ComponentModel::PropertyDescriptor^>(descriptors->Count);
                for each(System::ComponentModel::PropertyDescriptor^ d in descriptors)
                    _propertyDescriptors->Add(d);
            }
        }

    protected:
        List<System::ComponentModel::PropertyDescriptor^>^ _propertyDescriptors;
        GUILayer::IGetAndSetProperties^ _helper;
    };
}

