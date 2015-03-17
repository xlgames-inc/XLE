// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4512)

#include "UITypesBinding.h"
#include "../../RenderCore/Assets/MaterialSettingsFile.h"
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

///////////////////////////////////////////////////////////////////////////////////////////////////

    System::String^ BindingUtil::StringIntPair::Name::get() { return _name; }

    void BindingUtil::StringIntPair::Name::set(System::String^ newValue)
    {
        _name = newValue;
        NotifyPropertyChanged("Name");
    }

    unsigned BindingUtil::StringIntPair::Value::get() { return _value; } 
    void BindingUtil::StringIntPair::Value::set(unsigned newValue)
    {
        _value = newValue;
        NotifyPropertyChanged("Value");
    }

    void BindingUtil::StringIntPair::NotifyPropertyChanged(System::String^ propertyName)
    {
        PropertyChanged(this, gcnew PropertyChangedEventArgs(propertyName));
    }

    BindingList<BindingUtil::StringIntPair^>^ BindingUtil::AsBindingList(const ParameterBox& paramBox)
    {
        auto result = gcnew BindingList<StringIntPair^>();
        std::vector<std::pair<std::string, std::string>> stringTable;
        paramBox.BuildStringTable(stringTable);

        for (auto i=stringTable.cbegin(); i!=stringTable.cend(); ++i) {
            result->Add(
                gcnew StringIntPair(
                    clix::marshalString<clix::E_UTF8>(i->first),
                    XlAtoI32(i->second.c_str())));
        }

        return result;
    }

    ParameterBox BindingUtil::AsParameterBox(BindingList<StringIntPair^>^ input)
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

///////////////////////////////////////////////////////////////////////////////////////////////////

    BindingList<BindingUtil::StringIntPair^>^ 
        RawMaterial::MaterialParameterBox::get()
    {
        if (!_materialParameterBox) {
            _materialParameterBox = BindingUtil::AsBindingList((*_underlying)->_matParamBox);
            _materialParameterBox->ListChanged += 
                gcnew ListChangedEventHandler(
                    this, &RawMaterial::ParameterBox_Changed);
        }
        return _materialParameterBox;
    }

    BindingList<BindingUtil::StringIntPair^>^ 
        RawMaterial::ShaderConstants::get()
    {
        if (!_shaderConstants) {
            _shaderConstants = BindingUtil::AsBindingList((*_underlying)->_constants);
            _shaderConstants->ListChanged += 
                gcnew ListChangedEventHandler(
                    this, &RawMaterial::ParameterBox_Changed);
        }
        return _shaderConstants;
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
                (*_underlying)->_matParamBox = BindingUtil::AsParameterBox(_materialParameterBox);
            } else if (obj == _shaderConstants) {
                (*_underlying)->_constants = BindingUtil::AsParameterBox(_shaderConstants);
            }
        }
    }

    RawMaterial::RawMaterial(System::String^ initialiser)
    {
        auto nativeInit = clix::marshalString<clix::E_UTF8>(initialiser);
        auto& source = ::Assets::GetAssetDep<NativeConfig>(nativeInit.c_str());
        auto copy = std::make_shared<NativeConfig>(source);
        _underlying.reset(
            new std::shared_ptr<NativeConfig>(std::move(copy)));
    }

    RawMaterial::RawMaterial(
        std::shared_ptr<NativeConfig> underlying)
    {
        _underlying.reset(
            new std::shared_ptr<NativeConfig>(std::move(underlying)));
    }

    RawMaterial::~RawMaterial()
    {}

}

