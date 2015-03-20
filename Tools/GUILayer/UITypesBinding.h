// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "CLIXAutoPtr.h"
#include "MarshalString.h"
#include "../../PlatformRig/ModelVisualisation.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/SystemUtils.h"
#include "../../Utility/ParameterBox.h"

using namespace System::ComponentModel;
using namespace System::Windows::Forms;
using namespace System::Drawing::Design;

namespace RenderCore { namespace Assets { class RawMaterial; }}

namespace GUILayer
{
    template<typename T> using AutoToShared = clix::auto_ptr<std::shared_ptr<T>>;

    private ref class FileNameEditor : UITypeEditor
    {
    public:
        UITypeEditorEditStyle GetEditStyle(ITypeDescriptorContext^ context) override
        {
            return UITypeEditorEditStyle::Modal;
        }

        Object^ EditValue(
            ITypeDescriptorContext^ context, 
            System::IServiceProvider^ provider, 
            Object^ value) override
        {
            ofd->FileName = value->ToString();

            char dirName[MaxPath];
            XlDirname(dirName, dimof(dirName), clix::marshalString<clix::E_UTF8>(ofd->FileName).c_str());

            ofd->InitialDirectory = clix::marshalString<clix::E_UTF8>(dirName);
            ofd->Filter = "Model files|*.dae|All Files|*.*";
            if (ofd->ShowDialog() == DialogResult::OK) {
                return ofd->FileName;
            }

            return __super::EditValue(context, provider, value);
        }

        FileNameEditor()
        {
            ofd = gcnew OpenFileDialog();
        }

    private:
        OpenFileDialog ^ofd;
    };

    public ref class VisCameraSettings
    {
    public:
        std::shared_ptr<PlatformRig::VisCameraSettings> GetUnderlying() { return *_object.get(); }

        VisCameraSettings(std::shared_ptr<PlatformRig::VisCameraSettings> attached)
        {
            _object.reset(new std::shared_ptr<PlatformRig::VisCameraSettings>(std::move(attached)));
        }
        ~VisCameraSettings()
        {
            _object.reset();
        }
    protected:
        AutoToShared<PlatformRig::VisCameraSettings> _object;
    };

    public ref class ModelVisSettings
    {
    public:
        [Category("Model")]
        [Description("Active model file")]
        [EditorAttribute(FileNameEditor::typeid, UITypeEditor::typeid)]
        property System::String^ ModelName
        {
            System::String^ get()
            {
                return clix::marshalString<clix::E_UTF8>((*_object)->_modelName.c_str());
            }

            void set(System::String^ value)
            {
                    //  we need to make a filename relative to the current working
                    //  directory
                auto nativeName = clix::marshalString<clix::E_UTF8>(value);
                char directory[MaxPath];
                XlGetCurrentDirectory(dimof(directory), directory);
                XlMakeRelPath(directory, dimof(directory), directory, nativeName.c_str());
                (*_object)->_modelName = directory;

                (*_object)->_pendingCameraAlignToModel = true; 
                (*_object)->_changeEvent.Trigger(); 
            }
        }

        enum class ColourByMaterialType { None, All, MouseOver };

        [Category("Visualisation")]
        [Description("Highlight material divisions")]
        property ColourByMaterialType ColourByMaterial
        {
            ColourByMaterialType get() { return (ColourByMaterialType)(*_object)->_colourByMaterial; }
            void set(ColourByMaterialType value)
            {
                (*_object)->_colourByMaterial = unsigned(value); 
                (*_object)->_changeEvent.Trigger(); 
            }
        }

        [Category("Visualisation")]
        [Description("Reset camera to match the object")]
        property bool ResetCamera
        {
            bool get() { return (*_object)->_pendingCameraAlignToModel; }
            void set(bool value)
            {
                (*_object)->_pendingCameraAlignToModel = value; 
                (*_object)->_changeEvent.Trigger(); 
            }
        }

        property VisCameraSettings^ Camera
        {
            VisCameraSettings^ get() { return gcnew VisCameraSettings((*_object)->_camera); }
        }

        std::shared_ptr<PlatformRig::ModelVisSettings> GetUnderlying() { return *_object.get(); }

        void AttachCallback(PropertyGrid^ callback);

        ModelVisSettings(std::shared_ptr<PlatformRig::ModelVisSettings> attached)
        {
            _object.reset(new std::shared_ptr<PlatformRig::ModelVisSettings>(std::move(attached)));
        }

        ~ModelVisSettings() { _object.reset(); }

        static ModelVisSettings^ CreateDefault();

    protected:
        AutoToShared<PlatformRig::ModelVisSettings> _object;
    };

    public ref class VisMouseOver
    {
    public:
        [Description("Intersection coordinate")]
        property System::String^ IntersectionPt { System::String^ get(); }

        [Description("Draw call index")]
        property unsigned DrawCallIndex { unsigned get(); }

        [Category("Material")]
        property System::String^ MaterialName { System::String^ get(); }

        void AttachCallback(PropertyGrid^ callback);

        VisMouseOver(
            std::shared_ptr<PlatformRig::VisMouseOver> attached,
            std::shared_ptr<PlatformRig::ModelVisSettings> settings,
            std::shared_ptr<PlatformRig::ModelVisCache> cache);
        ~VisMouseOver();
    protected:
        AutoToShared<PlatformRig::VisMouseOver> _object;
        AutoToShared<PlatformRig::ModelVisSettings> _modelSettings;
        AutoToShared<PlatformRig::ModelVisCache> _modelCache;
    };

    public ref class BindingUtil
    {
    public:
        template<typename NameType, typename ValueType>
            ref class PropertyPair : public INotifyPropertyChanged
        {
        public:
            property NameType Name { NameType get(); void set(NameType); }
            property ValueType Value { ValueType get(); void set(ValueType); }

            virtual event PropertyChangedEventHandler^ PropertyChanged;

            PropertyPair() {}
            PropertyPair(NameType name, ValueType value) { Name = name; Value = value; }

        protected:
            void NotifyPropertyChanged(/*[CallerMemberName]*/ System::String^ propertyName);

            NameType _name;
            ValueType _value;
        };

        typedef PropertyPair<System::String^, unsigned> StringIntPair;
        typedef PropertyPair<System::String^, System::String^> StringStringPair;
    };

    public ref class RawMaterial
    {
    public:
        using NativeConfig = RenderCore::Assets::RawMaterial;
        property BindingList<BindingUtil::StringIntPair^>^ MaterialParameterBox {
            BindingList<BindingUtil::StringIntPair^>^ get();
        }

        property BindingList<BindingUtil::StringIntPair^>^ ShaderConstants {
            BindingList<BindingUtil::StringIntPair^>^ get();
        }

        property BindingList<BindingUtil::StringStringPair^>^ ResourceBindings {
            BindingList<BindingUtil::StringStringPair^>^ get();
        }

        const NativeConfig* GetUnderlying() { return _underlying.get() ? _underlying->get() : nullptr; }

        System::Collections::Generic::List<RawMaterial^>^ BuildInheritanceList();
        property System::String^ Filename { System::String^ get(); }
        property System::String^ SettingName { System::String^ get(); }

        RawMaterial(System::String^ initialiser);
        RawMaterial(std::shared_ptr<NativeConfig> underlying);
        ~RawMaterial();
    protected:
        AutoToShared<NativeConfig> _underlying;
        System::String^ DummyFilename;
        System::String^ DummySettingName;

        BindingList<BindingUtil::StringIntPair^>^ _materialParameterBox;
        BindingList<BindingUtil::StringIntPair^>^ _shaderConstants;
        BindingList<BindingUtil::StringStringPair^>^ _resourceBindings;
        void ParameterBox_Changed(System::Object^, ListChangedEventArgs^);
        void ResourceBinding_Changed(System::Object^, ListChangedEventArgs^);
    };
}

