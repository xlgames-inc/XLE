// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NativeManipulators.h"
#include "PropertyDescriptorUtils.h"
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
    [Export(MaterialSchemaLoader::typeid)]
    [PartCreationPolicy(CreationPolicy::Shared)]
    public ref class MaterialSchemaLoader : public DataDrivenPropertyContextHelper
    {
    public:
        IPropertyEditingContext^ CreatePropertyContext(GUILayer::RawMaterial^ material);

        MaterialSchemaLoader()
        {
            SchemaResolver = gcnew ResourceStreamResolver(Assembly::GetExecutingAssembly(), ".");
            Load("material.xsd");
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
        auto ps = gcnew GUILayer::BasicPropertySource(
            gcnew RawMaterialShaderConstants_GetAndSet(material),
            GetPropertyDescriptors("gap:RawMaterial"));
        return gcnew PropertyBridge(ps);
    }
}

