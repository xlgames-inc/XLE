// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4512)

#include "UITypesBinding.h"
#include "../../RenderCore/Assets/Material.h"
#include "../../Utility/StringFormat.h"
#include <msclr\auto_gcroot.h>

namespace GUILayer
{

///////////////////////////////////////////////////////////////////////////////////////////////////

    class InvalidatePropertyGrid : public OnChangeCallback
    {
    public:
        void    OnChange();

        InvalidatePropertyGrid(PropertyGrid^ linked);
        ~InvalidatePropertyGrid();
    protected:
        msclr::auto_gcroot<PropertyGrid^> _linked;
    };

    void    InvalidatePropertyGrid::OnChange()
    {
        if (_linked.get()) {
            _linked->Invalidate();
        }
    }

    InvalidatePropertyGrid::InvalidatePropertyGrid(PropertyGrid^ linked) : _linked(linked) {}
    InvalidatePropertyGrid::~InvalidatePropertyGrid() {}

    void ModelVisSettings::AttachCallback(PropertyGrid^ callback)
    {
        (*_object)->_changeEvent._callbacks.push_back(
            std::shared_ptr<OnChangeCallback>(new InvalidatePropertyGrid(callback)));
    }

    ModelVisSettings^ ModelVisSettings::CreateDefault()
    {
        auto attached = std::make_shared<PlatformRig::ModelVisSettings>();
        return gcnew ModelVisSettings(std::move(attached));
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename NameType, typename ValueType>
        NameType BindingUtil::PropertyPair<NameType, ValueType>::Name::get() { return _name; }

    template<typename NameType, typename ValueType>
        void BindingUtil::PropertyPair<NameType, ValueType>::Name::set(NameType newValue)
    {
        _name = newValue;
        NotifyPropertyChanged("Name");
    }

    template<typename NameType, typename ValueType>
        ValueType BindingUtil::PropertyPair<NameType, ValueType>::Value::get() { return _value; } 

    template<typename NameType, typename ValueType>
        void BindingUtil::PropertyPair<NameType, ValueType>::Value::set(ValueType newValue)
    {
        _value = newValue;
        NotifyPropertyChanged("Value");
    }

    template<typename NameType, typename ValueType>
        void BindingUtil::PropertyPair<NameType, ValueType>::NotifyPropertyChanged(System::String^ propertyName)
    {
        PropertyChanged(this, gcnew PropertyChangedEventArgs(propertyName));
    }

    public ref class BindingConv
    {
    public:
        static BindingList<BindingUtil::StringIntPair^>^ AsBindingList(const ParameterBox& paramBox);
        static ParameterBox AsParameterBox(BindingList<BindingUtil::StringIntPair^>^);

        static BindingList<BindingUtil::StringStringPair^>^ AsBindingList(const RenderCore::Assets::ResourceBindingSet& bindingSet);
        static RenderCore::Assets::ResourceBindingSet AsResourceBindingList(BindingList<BindingUtil::StringStringPair^>^);
    };

    BindingList<BindingUtil::StringIntPair^>^ BindingConv::AsBindingList(const ParameterBox& paramBox)
    {
        auto result = gcnew BindingList<BindingUtil::StringIntPair^>();
        std::vector<std::pair<std::string, std::string>> stringTable;
        paramBox.BuildStringTable(stringTable);

        for (auto i=stringTable.cbegin(); i!=stringTable.cend(); ++i) {
            result->Add(
                gcnew BindingUtil::StringIntPair(
                    clix::marshalString<clix::E_UTF8>(i->first),
                    XlAtoI32(i->second.c_str())));
        }

        return result;
    }

    ParameterBox BindingConv::AsParameterBox(BindingList<BindingUtil::StringIntPair^>^ input)
    {
        ParameterBox result;
        for each(auto i in input) {
                //  We get items with null names when they are being added, but
                //  not quite finished yet. We have to ignore in this case.
            if (i->Name && i->Name->Length > 0) {
                result.SetParameter(
                    clix::marshalString<clix::E_UTF8>(i->Name),
                    i->Value);
            }
        }
        return result;
    }

    BindingList<BindingUtil::StringStringPair^>^ BindingConv::AsBindingList(
        const RenderCore::Assets::ResourceBindingSet& bindingSet)
    {
        auto result = gcnew BindingList<BindingUtil::StringStringPair^>();

        for (auto i=bindingSet.cbegin(); i!=bindingSet.cend(); ++i) {
            StringMeld<64> nameTemp;
            nameTemp << std::hex << i->_bindHash;

            result->Add(
                gcnew BindingUtil::StringStringPair(
                    clix::marshalString<clix::E_UTF8>((const char*)nameTemp),
                    clix::marshalString<clix::E_UTF8>(i->_resourceName)));
        }

        return result;
    }

    RenderCore::Assets::ResourceBindingSet BindingConv::AsResourceBindingList(BindingList<BindingUtil::StringStringPair^>^ input)
    {
        using namespace RenderCore::Assets;
        ResourceBindingSet result;

        for each(auto i in input) {
            if (i->Name && i->Name->Length > 0) {
                MaterialGuid guid = XlAtoI64(clix::marshalString<clix::E_UTF8>(i->Name).c_str(), nullptr, 16);
                auto ins = std::lower_bound(
                    result.begin(), result.end(), 
                    guid, ResourceBinding::Compare());
                if (ins==result.end() || ins->_bindHash != guid) {
                    std::string value;
                    if (i->Value) { value = clix::marshalString<clix::E_UTF8>(i->Value); }
                    result.insert(ins, 
                        ResourceBinding(guid, value));
                }
            }
        }

        return result;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    BindingList<BindingUtil::StringIntPair^>^ 
        RawMaterial::MaterialParameterBox::get()
    {
        if (!_underlying.get()) { return nullptr; }
        if (!_materialParameterBox) {
            _materialParameterBox = BindingConv::AsBindingList((*_underlying)->_matParamBox);
            _materialParameterBox->ListChanged += 
                gcnew ListChangedEventHandler(
                    this, &RawMaterial::ParameterBox_Changed);
            _materialParameterBox->AllowNew = true;
            _materialParameterBox->AllowEdit = true;
        }
        return _materialParameterBox;
    }

    BindingList<BindingUtil::StringIntPair^>^ 
        RawMaterial::ShaderConstants::get()
    {
        if (!_underlying.get()) { return nullptr; }
        if (!_shaderConstants) {
            _shaderConstants = BindingConv::AsBindingList((*_underlying)->_constants);
            _shaderConstants->ListChanged += 
                gcnew ListChangedEventHandler(
                    this, &RawMaterial::ParameterBox_Changed);
            _shaderConstants->AllowNew = true;
            _shaderConstants->AllowEdit = true;
        }
        return _shaderConstants;
    }

    BindingList<BindingUtil::StringStringPair^>^ 
        RawMaterial::ResourceBindings::get()
    {
        if (!_underlying.get()) { return nullptr; }
        if (!_resourceBindings) {
            _resourceBindings = BindingConv::AsBindingList((*_underlying)->_resourceBindings);
            _resourceBindings->ListChanged += 
                gcnew ListChangedEventHandler(
                    this, &RawMaterial::ResourceBinding_Changed);
            _resourceBindings->AllowNew = true;
            _resourceBindings->AllowEdit = true;
        }
        return _resourceBindings;
    }

    void RawMaterial::ParameterBox_Changed(System::Object^ obj, ListChangedEventArgs^e)
    {
            //  Commit these changes back to the native object by re-creating the parameter box
            //  Ignore a couple of cases... 
            //      - moving an item is unimportant
            //      - added a new item with a null name (this occurs when the new item
            //          hasn't been fully filled in yet)
            //   Similarly, don't we really need to process a removal of an item with 
            //   an empty name.. but there's no way to detect this case
        if (e->ListChangedType == ListChangedType::ItemMoved) {
            return;
        }

        using Box = BindingList<BindingUtil::StringIntPair^>;

        if (e->ListChangedType == ListChangedType::ItemAdded) {
            assert(e->NewIndex < ((Box^)obj)->Count);
            if (!((Box^)obj)[e->NewIndex]->Name || ((Box^)obj)[e->NewIndex]->Name->Length > 0) {
                return;
            }
        }

        if (_underlying.get()) {
            if (obj == _materialParameterBox) {
                (*_underlying)->_matParamBox = BindingConv::AsParameterBox(_materialParameterBox);
            } else if (obj == _shaderConstants) {
                (*_underlying)->_constants = BindingConv::AsParameterBox(_shaderConstants);
            }
        }
    }

    void RawMaterial::ResourceBinding_Changed(System::Object^ obj, ListChangedEventArgs^ e)
    {
        if (e->ListChangedType == ListChangedType::ItemMoved) {
            return;
        }

        using Box = BindingList<BindingUtil::StringStringPair^>;

        if (e->ListChangedType == ListChangedType::ItemAdded) {
            assert(e->NewIndex < ((Box^)obj)->Count);
            if (!((Box^)obj)[e->NewIndex]->Name || ((Box^)obj)[e->NewIndex]->Name->Length > 0) {
                return;
            }
        }

        if (_underlying.get()) {
            assert(obj == _resourceBindings);
            (*_underlying)->_resourceBindings = BindingConv::AsResourceBindingList(_resourceBindings);
        }
    }

    System::Collections::Generic::List<RawMaterial^>^ RawMaterial::BuildInheritanceList()
    {
        // create a RawMaterial wrapper object for all of the inheritted objects
        
        if (_underlying.get()) {
            auto result = gcnew System::Collections::Generic::List<RawMaterial^>();

            for (   auto i=(*_underlying)->_inherit.cbegin(); 
                    i!=(*_underlying)->_inherit.cend(); ++i) {
                result->Add(gcnew RawMaterial(
                    clix::marshalString<clix::E_UTF8>(*i)));
            }
            return result;
        }
        return nullptr;
    }

    System::String^ RawMaterial::Filename::get()
    {
        if (!_underlying.get()) { return DummyFilename; }
        return clix::marshalString<clix::E_UTF8>((*_underlying)->GetInitializerFilename());
    }

    System::String^ RawMaterial::SettingName::get()
    {
        if (!_underlying.get()) { return DummySettingName; }
        return clix::marshalString<clix::E_UTF8>((*_underlying)->GetSettingName());
    }

    RawMaterial::RawMaterial(System::String^ initialiser)
    {
        TRY {
            auto nativeInit = clix::marshalString<clix::E_UTF8>(initialiser);
            auto& source = ::Assets::GetAssetDep<NativeConfig>(nativeInit.c_str());
            auto copy = std::make_shared<NativeConfig>(source);
            _underlying.reset(
                new std::shared_ptr<NativeConfig>(std::move(copy)));
        } CATCH (const Assets::Exceptions::InvalidResource&) {
            auto colon = initialiser->IndexOf(':');
            if (colon > 1) {
                DummyFilename = initialiser->Substring(0, colon);
                DummySettingName = initialiser->Substring(colon+1);
            } else {
                DummyFilename = initialiser;
            }
        } CATCH_END
    }

    RawMaterial::RawMaterial(
        std::shared_ptr<NativeConfig> underlying)
    {
        _underlying.reset(
            new std::shared_ptr<NativeConfig>(std::move(underlying)));
    }

    RawMaterial::~RawMaterial()
    {}


    template BindingUtil::PropertyPair<System::String^, unsigned>;
    template BindingUtil::PropertyPair<System::String^, System::String^>;
}

