
#include "../../Assets/Assets.h"
#include "../../RenderCore/Assets/Material.h"
#include "../../Tools/GUILayer/MarshalString.h"

using namespace Sce::Atf::Applications;
using namespace Sce::Atf::Dom;
using namespace System::Collections::Generic;

namespace XLELayer
{
    public ref class MaterialLayer : IPropertyEditingContext
    {
    public:
        property IEnumerable<System::Object^>^ Items
        {
            virtual IEnumerable<System::Object^>^ get()
            {
                return nullptr;
            }
        }

        /// <summary>
        /// Gets an enumeration of the property descriptors for the items</summary>
        property IEnumerable<System::ComponentModel::PropertyDescriptor^>^ PropertyDescriptors
        {
            virtual IEnumerable<System::ComponentModel::PropertyDescriptor^>^ get() 
            {
                if (_propertyDescriptor == nullptr) {
                    using Sce::Atf::Controls::PropertyEditing::UnboundPropertyDescriptor;
                    using System::String;

                    auto category = gcnew String("General");
                    _propertyDescriptor = gcnew List<System::ComponentModel::PropertyDescriptor^>();
                    _propertyDescriptor->Add(
                        gcnew UnboundPropertyDescriptor(
                            GetType(), 
                            gcnew String("MaterialDiffuse"), 
                            gcnew String("Material Diffuse Color"), category, 
                            gcnew String("Modulate texture diffuse by this color")));
                }
                 
                return _propertyDescriptor;
            }
        }

        MaterialLayer(System::String^ name)
        {
            _rawMaterial = &::Assets::GetAssetDep<RenderCore::Assets::RawMaterial>(
                clix::marshalString<clix::E_UTF8>(name).c_str());
        }

    protected:
        const RenderCore::Assets::RawMaterial* _rawMaterial;
        List<System::ComponentModel::PropertyDescriptor^>^ _propertyDescriptor;
    };
}

