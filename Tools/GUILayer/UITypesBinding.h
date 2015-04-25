// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "CLIXAutoPtr.h"
#include "MarshalString.h"
#include "../ToolsRig/ModelVisualisation.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/SystemUtils.h"
#include "../../Utility/ParameterBox.h"

using namespace System::ComponentModel;
using namespace System::Windows::Forms;
using namespace System::Drawing::Design;
using namespace System::Collections::Generic;

namespace RenderCore { namespace Assets { class RawMaterial; class RenderStateSet; }}

namespace GUILayer
{
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
        const std::shared_ptr<ToolsRig::VisCameraSettings>& GetUnderlying() { return _object.GetNativePtr(); }
        ToolsRig::VisCameraSettings* GetUnderlyingRaw() { return _object.get(); }

        VisCameraSettings(std::shared_ptr<ToolsRig::VisCameraSettings> attached)
        {
            _object = std::move(attached);
        }
        VisCameraSettings()     { _object = std::make_shared<ToolsRig::VisCameraSettings>(); }
        ~VisCameraSettings()    { _object.reset(); }

        !VisCameraSettings()
        {
            System::Diagnostics::Debug::Assert(false, "Non deterministic delete of VisCameraSettings");
        }
    protected:
        clix::shared_ptr<ToolsRig::VisCameraSettings> _object;
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
                return clix::marshalString<clix::E_UTF8>(_object->_modelName.c_str());
            }

            void set(System::String^ value)
            {
                    //  we need to make a filename relative to the current working
                    //  directory
                auto nativeName = clix::marshalString<clix::E_UTF8>(value);
                char directory[MaxPath];
                XlGetCurrentDirectory(dimof(directory), directory);
                XlMakeRelPath(directory, dimof(directory), directory, nativeName.c_str());
                _object->_modelName = directory;

                _object->_pendingCameraAlignToModel = true; 
                _object->_changeEvent.Trigger(); 
            }
        }

        enum class ColourByMaterialType { None, All, MouseOver };

        [Category("Visualisation")]
        [Description("Highlight material divisions")]
        property ColourByMaterialType ColourByMaterial
        {
            ColourByMaterialType get() { return (ColourByMaterialType)_object->_colourByMaterial; }
            void set(ColourByMaterialType value)
            {
                _object->_colourByMaterial = unsigned(value); 
                _object->_changeEvent.Trigger(); 
            }
        }

        [Category("Visualisation")]
        [Description("Reset camera to match the object")]
        property bool ResetCamera
        {
            bool get() { return _object->_pendingCameraAlignToModel; }
            void set(bool value)
            {
                _object->_pendingCameraAlignToModel = value; 
                _object->_changeEvent.Trigger(); 
            }
        }

        property VisCameraSettings^ Camera
        {
            VisCameraSettings^ get() { return _camSettings; }
        }

        void AttachCallback(PropertyGrid^ callback);
        std::shared_ptr<ToolsRig::ModelVisSettings> GetUnderlying() { return _object.GetNativePtr(); }

        ModelVisSettings(std::shared_ptr<ToolsRig::ModelVisSettings> attached)
        {
            _object = std::move(attached);
            _camSettings = gcnew VisCameraSettings(_object->_camera);
        }

        ModelVisSettings() 
        {
            _object = std::make_shared<ToolsRig::ModelVisSettings>();
            _camSettings = gcnew VisCameraSettings(_object->_camera);
        }

        ~ModelVisSettings() { delete _camSettings; _object.reset(); }

        !ModelVisSettings()
        {
            System::Diagnostics::Debug::Assert(false, "Non deterministic delete of ModelVisSettings");
        }

        static ModelVisSettings^ CreateDefault();

    protected:
        clix::shared_ptr<ToolsRig::ModelVisSettings> _object;
        VisCameraSettings^ _camSettings;
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

        [Browsable(false)] property bool HasMouseOver { bool get(); }
        [Browsable(false)] property System::String^ FullMaterialName { System::String^ get(); }

        void AttachCallback(PropertyGrid^ callback);
        std::shared_ptr<ToolsRig::VisMouseOver> GetUnderlying() { return _object.GetNativePtr(); }

        VisMouseOver(
            std::shared_ptr<ToolsRig::VisMouseOver> attached,
            std::shared_ptr<ToolsRig::ModelVisSettings> settings,
            std::shared_ptr<ToolsRig::ModelVisCache> cache);
        VisMouseOver();
        ~VisMouseOver();

        !VisMouseOver()
        {
            System::Diagnostics::Debug::Assert(false, "Non deterministic delete of ModelVisSettings");
        }

    protected:
        clix::shared_ptr<ToolsRig::VisMouseOver> _object;
        clix::shared_ptr<ToolsRig::ModelVisSettings> _modelSettings;
        clix::shared_ptr<ToolsRig::ModelVisCache> _modelCache;
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

            PropertyPair() { _propertyChangedContext = System::Threading::SynchronizationContext::Current; }
            PropertyPair(NameType name, ValueType value) : PropertyPair() { Name = name; Value = value; }

            virtual event PropertyChangedEventHandler^ PropertyChanged;

        protected:
            void NotifyPropertyChanged(/*[CallerMemberName]*/ System::String^ propertyName);
            System::Threading::SynchronizationContext^ _propertyChangedContext;

            NameType _name;
            ValueType _value;
        };

        typedef PropertyPair<System::String^, unsigned> StringIntPair;
        typedef PropertyPair<System::String^, System::String^> StringStringPair;
    };

    public ref class RenderStateSet : System::ComponentModel::INotifyPropertyChanged
    {
    public:
        using CheckState = System::Windows::Forms::CheckState;
        property CheckState DoubleSided { CheckState get(); void set(CheckState); }
        property CheckState Wireframe { CheckState get(); void set(CheckState); }

        enum class DeferredBlendState { Opaque, Decal, Unset };
        property DeferredBlendState DeferredBlend { DeferredBlendState get(); void set(DeferredBlendState); }

        virtual event System::ComponentModel::PropertyChangedEventHandler^ PropertyChanged;

        RenderStateSet(std::shared_ptr<::Assets::DivergentAsset<RenderCore::Assets::RawMaterial>> underlying);
        ~RenderStateSet();

        !RenderStateSet()
        {
            System::Diagnostics::Debug::Assert(false, "Non deterministic delete of RenderStateSet");
        }
    protected:
        clix::shared_ptr<::Assets::DivergentAsset<RenderCore::Assets::RawMaterial>> _underlying;

        void NotifyPropertyChanged(/*[CallerMemberName]*/ System::String^ propertyName);
        System::Threading::SynchronizationContext^ _propertyChangedContext;
    };

    public ref class RawMaterial
    {
    public:
        using NativeConfig = ::Assets::DivergentAsset<RenderCore::Assets::RawMaterial>;
        property BindingList<BindingUtil::StringStringPair^>^ MaterialParameterBox {
            BindingList<BindingUtil::StringStringPair^>^ get();
        }

        property BindingList<BindingUtil::StringStringPair^>^ ShaderConstants {
            BindingList<BindingUtil::StringStringPair^>^ get();
        }

        property BindingList<BindingUtil::StringStringPair^>^ ResourceBindings {
            BindingList<BindingUtil::StringStringPair^>^ get();
        }

        property RenderStateSet^ StateSet { RenderStateSet^ get() { return _renderStateSet; } }

        const RenderCore::Assets::RawMaterial* GetUnderlying() { return (!!_underlying) ? &_underlying->GetAsset() : nullptr; }

        List<System::String^>^ BuildInheritanceList();
        static List<System::String^>^ BuildInheritanceList(System::String^ topMost);

        property System::String^ Filename { System::String^ get(); }
        property System::String^ SettingName { System::String^ get(); }

        RawMaterial(System::String^ initialiser);
        RawMaterial(std::shared_ptr<NativeConfig> underlying);
        RawMaterial(RawMaterial^ cloneFrom);
        ~RawMaterial();

        !RawMaterial()
        {
            System::Diagnostics::Debug::Assert(false, "Non deterministic delete of RawMaterial");
        }
    protected:
        clix::shared_ptr<NativeConfig> _underlying;
        RenderStateSet^ _renderStateSet;

        BindingList<BindingUtil::StringStringPair^>^ _materialParameterBox;
        BindingList<BindingUtil::StringStringPair^>^ _shaderConstants;
        BindingList<BindingUtil::StringStringPair^>^ _resourceBindings;
        void ParameterBox_Changed(System::Object^, ListChangedEventArgs^);
        void ResourceBinding_Changed(System::Object^, ListChangedEventArgs^);
    };
}
