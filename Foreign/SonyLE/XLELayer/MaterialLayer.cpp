// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NativeManipulators.h"
#include "../../Assets/Assets.h"
#include "../../RenderCore/Assets/Material.h"
#include "../../Tools/GUILayer/MarshalString.h"

using namespace Sce::Atf;
using namespace Sce::Atf::Applications;
using namespace Sce::Atf::Dom;
using namespace System;
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

    [Export(MaterialSchemaLoader::typeid)]
    [PartCreationPolicy(CreationPolicy::Shared)]
    public ref class MaterialSchemaLoader : public XmlSchemaTypeLoader
    {
    public:
        DomNodeType^ GetMaterialType() { return GetNodeType("gap:RawMaterial"); }

        IPropertyEditingContext^ CreatePropertyContext(GUILayer::RawMaterial^ material);

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
                    for each (ComponentModel::PropertyDescriptor^ propDecr in annotationDescriptor)
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

    private:
        static array<Attribute^>^ AsArray(AttributeCollection^ collection)
        {
            if (!collection->Count) return nullptr;

            auto result = gcnew array<Attribute^>(collection->Count);
            for (int c=0; c<collection->Count; ++c)
                result[c] = collection[c];
            return result;
        }
    };

    public ref class RawMaterialShaderConstants_GetAndSet : public GUILayer::IGetAndSetProperties
    {
    public:
        virtual bool TryGetMember(String^ name, bool caseInsensitive, Type^ type, Object^% result) 
        {
                // We can choose to interact with the "BindingList" object attached the C++/CLI object
                // or the underlying C++ layer. If we change the C++ layer, then we need to manually
                // rebuild the binding list objects in the GUILayer part.
                // Using the C++ layer give us a little more flexibility. 
            if (name->Length < 2) return false;

            auto cName = name->Substring(2);
            decltype(_material->ShaderConstants) list = nullptr;
            if (name[0] == 'C' && name[1] == '-') {
                list = _material->ShaderConstants;
            } else if (name[0] == 'P' && name[1] == '-') {
                list = _material->MaterialParameterBox;
            } else if (name[0] == 'R' && name[1] == '-') {
                list = _material->ResourceBindings;
            }

            for each(auto v in list) {
                if (String::Compare(cName, v->Name) == 0) {
                    try {
                        result = Convert::ChangeType(v->Value, type);
                        return result != nullptr;
                    } catch (NotSupportedException^) {}
                    catch (FormatException^) 
                    {
                            // special case for booleans that fail the 
                            // normal conversion process.
                            //  We want booleans to generally become "0" or "1"
                            //  - but the standard .net conversion operations
                            //  don't support these forms. It's awkward to solve
                            //  this problem using TypeConverter classes... So let's
                            //  just do it with a special case here...
                        if (type == Boolean::typeid) {
                            auto parsed = ImpliedTyping::Parse<bool>(
                                clix::marshalString<clix::E_UTF8>(v->Value).c_str());
                            if (parsed.first) {
                                result = gcnew Boolean(parsed.second);
                                return true;
                            }
                        }
                    }
                }
            }

                //  If we return false, then the entry will be blanked out.
                //  that's ok for now... we just have to hit the "default" 
                //  button to make it edittable

            return false;
        }

        virtual bool TrySetMember(String^ name, bool caseInsensitive, Object^ value)
        {
            auto cName = name->Substring(2);
            decltype(_material->ShaderConstants) list = nullptr;
            if (name[0] == 'C' && name[1] == '-') {
                list = _material->ShaderConstants;
            } else if (name[0] == 'P' && name[1] == '-') {
                list = _material->MaterialParameterBox;
            } else if (name[0] == 'R' && name[1] == '-') {
                list = _material->ResourceBindings;
            }

            for each(auto v in list) {
                if (String::Compare(cName, v->Name) == 0) {
                    auto asBoolean = dynamic_cast<Boolean^>(value);
                    if (asBoolean) v->Value = (*asBoolean) ? "1" : "0";
                    else v->Value = value->ToString();
                    return true;
                }
            }

                // no pre-existing value.. create a new one
            list->Add(GUILayer::RawMaterial::MakePropertyPair(cName, value->ToString()));
            return true;
        }

        RawMaterialShaderConstants_GetAndSet(GUILayer::RawMaterial^ material) : _material(material) {}
        ~RawMaterialShaderConstants_GetAndSet() {}
    protected:
        GUILayer::RawMaterial^ _material;
    };

    IPropertyEditingContext^ MaterialSchemaLoader::CreatePropertyContext(GUILayer::RawMaterial^ material)
    {
        List<ComponentModel::PropertyDescriptor^>^ propertyDescriptors = nullptr;
        auto descriptors = GetMaterialType()->GetTag<PropertyDescriptorCollection^>();
        if (descriptors && descriptors->Count > 0) {
            propertyDescriptors = gcnew List<ComponentModel::PropertyDescriptor^>(descriptors->Count);
            for each(ComponentModel::PropertyDescriptor^ d in descriptors)
                propertyDescriptors->Add(d);
        }

        auto ps = gcnew GUILayer::BasicPropertySource(
            gcnew RawMaterialShaderConstants_GetAndSet(material),
            propertyDescriptors);
        return gcnew PropertyBridge(ps);
    }
}

