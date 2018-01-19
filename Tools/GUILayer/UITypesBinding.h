// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DelayedDeleteQueue.h"
#include "CLIXAutoPtr.h"
#include "MarshalString.h"
#include "../ToolsRig/ModelVisualisation.h"
#include "../ToolsRig/DivergentAsset.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/SystemUtils.h"
#include "../../Utility/ParameterBox.h"

using namespace System;
using namespace System::ComponentModel;
using namespace System::Windows::Forms;
using namespace System::Drawing::Design;
using namespace System::Collections::Generic;

namespace RenderCore { namespace Assets { class RawMaterial; } }
namespace RenderCore { namespace Techniques { class Material; } }

namespace GUILayer
{
    public ref class FileNameEditor : UITypeEditor
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
            ofd->FileName = value ? value->ToString() : "";

            char dirName[MaxPath];
            XlDirname(dirName, dimof(dirName), clix::marshalString<clix::E_UTF8>(ofd->FileName).c_str());

            ofd->InitialDirectory = clix::marshalString<clix::E_UTF8>(dirName);
            ofd->Filter = "Model files|*.dae|Material files|*.material|All Files|*.*";
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
    protected:
        clix::shared_ptr<ToolsRig::VisCameraSettings> _object;
    };

    public ref class ModelVisSettings
    {
    public:
        [Category("Model")]
        [Description("Active model file")]
        [EditorAttribute(FileNameEditor::typeid, UITypeEditor::typeid)]
        property String^ ModelName
        {
            String^ get()
            {
                return clix::marshalString<clix::E_UTF8>(_object->_modelName);
            }

            void set(String^ value);
        }

        [Category("Model")]
        [Description("Active material file")]
        [EditorAttribute(FileNameEditor::typeid, UITypeEditor::typeid)]
        property String^ MaterialName
        {
            String^ get()
            {
                return clix::marshalString<clix::E_UTF8>(_object->_materialName);
            }

            void set(String^ value);
        }

        [Category("Model")]
        [Description("Supplements")]
        property String^ Supplements
        {
            String^ get()
            {
                return clix::marshalString<clix::E_UTF8>(_object->_supplements);
            }

            void set(String^ value);
        }

        [Category("Model")]
        [Description("Level Of Detail")]
        property unsigned LevelOfDetail
        {
            unsigned get() { return _object->_levelOfDetail; }
            void set(unsigned value);
        }

        [Category("Environment")]
        [Description("Environment settings name")]
        property String^ EnvSettingsFile
        {
            String^ get()
            {
                return clix::marshalString<clix::E_UTF8>(_object->_envSettingsFile);
            }

            void set(String^ value);
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

        [Category("Visualisation")]
        [Description("Draw Normals, tangents and bitangents")]
        property bool DrawNormals
        {
            bool get() { return _object->_drawNormals; }
            void set(bool value)
            {
                _object->_drawNormals = value; 
                _object->_changeEvent.Trigger(); 
            }
        }

        [Category("Visualisation")]
        [Description("Draw wireframe")]
        property bool DrawWireframe
        {
            bool get() { return _object->_drawWireframe; }
            void set(bool value)
            {
                _object->_drawWireframe = value; 
                _object->_changeEvent.Trigger(); 
            }
        }

        [Browsable(false)]
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

        static ModelVisSettings^ CreateDefault();

    protected:
        clix::shared_ptr<ToolsRig::ModelVisSettings> _object;
        VisCameraSettings^ _camSettings;
    };

    public ref class VisMouseOver
    {
    public:
        [Description("Intersection coordinate")]
        property String^ IntersectionPt { String^ get(); }

        [Description("Draw call index")]
        property unsigned DrawCallIndex { unsigned get(); }

        [Description("Model file name")]
        property String^ ModelName { String^ get(); }

        [Category("Material")]
        property String^ MaterialName { String^ get(); }

        [Browsable(false)] property bool HasMouseOver { bool get(); }
        [Browsable(false)] property String^ FullMaterialName { String^ get(); }
        [Browsable(false)] property uint64 MaterialBindingGuid { uint64 get(); }

        void AttachCallback(PropertyGrid^ callback);
        std::shared_ptr<ToolsRig::VisMouseOver> GetUnderlying() { return _object.GetNativePtr(); }

        static String^ BuildFullMaterialName(
            const ToolsRig::ModelVisSettings& modelSettings,
            RenderCore::Assets::ModelCache& modelCache,
            uint64 materialGuid);
        static String^ DescriptiveMaterialName(String^ fullName);

        VisMouseOver(
            std::shared_ptr<ToolsRig::VisMouseOver> attached,
            std::shared_ptr<ToolsRig::ModelVisSettings> settings,
            std::shared_ptr<RenderCore::Assets::ModelCache> cache);
        VisMouseOver();
        ~VisMouseOver();

    protected:
        clix::shared_ptr<ToolsRig::VisMouseOver> _object;
        clix::shared_ptr<ToolsRig::ModelVisSettings> _modelSettings;
        clix::shared_ptr<RenderCore::Assets::ModelCache> _modelCache;
    };

    template<typename NameType, typename ValueType>
        public ref class PropertyPair : public INotifyPropertyChanged
    {
    public:
        property NameType Name { NameType get(); void set(NameType); }
        property ValueType Value { ValueType get(); void set(ValueType); }

        PropertyPair() { _propertyChangedContext = System::Threading::SynchronizationContext::Current; }
        PropertyPair(NameType name, ValueType value) : PropertyPair() { Name = name; Value = value; }

        virtual event PropertyChangedEventHandler^ PropertyChanged;

    protected:
        void NotifyPropertyChanged(/*[CallerMemberName]*/ String^ propertyName);
        System::Threading::SynchronizationContext^ _propertyChangedContext;

        NameType _name;
        ValueType _value;
    };

    using StringIntPair = PropertyPair<String^, unsigned> ;
    using StringStringPair = PropertyPair<String^, String^>;

    public enum class StandardBlendModes
    {
        Inherit,
        NoBlending,
        Decal,
        Transparent,
        TransparentPremultiplied,
        Add, 
        AddAlpha,
        Subtract,
        SubtractAlpha,
        Min, 
        Max,
        OrderedTransparent,
        OrderedTransparentPremultiplied,
        Complex     // some mode other than one of the standard modes
    };

    public ref class RenderStateSet : System::ComponentModel::INotifyPropertyChanged
    {
    public:
        using CheckState = System::Windows::Forms::CheckState;
        property CheckState DoubleSided { CheckState get(); void set(CheckState); }
        property CheckState Wireframe { CheckState get(); void set(CheckState); }

        enum class DeferredBlendState { Opaque, Decal, Unset };
        property DeferredBlendState DeferredBlend { DeferredBlendState get(); void set(DeferredBlendState); }

        property StandardBlendModes StandardBlendMode { StandardBlendModes get(); void set(StandardBlendModes); }

        virtual event System::ComponentModel::PropertyChangedEventHandler^ PropertyChanged;

        using NativeConfig = ToolsRig::DivergentAsset<RenderCore::Assets::RawMaterial>;
        RenderStateSet(std::shared_ptr<NativeConfig> underlying);
        ~RenderStateSet();
    protected:
        clix::shared_ptr<NativeConfig> _underlying;

        void NotifyPropertyChanged(/*[CallerMemberName]*/ String^ propertyName);
        System::Threading::SynchronizationContext^ _propertyChangedContext;
    };

    public ref class RawMaterial
    {
    public:
        property BindingList<StringStringPair^>^ MaterialParameterBox {
            BindingList<StringStringPair^>^ get();
        }

        property BindingList<StringStringPair^>^ ShaderConstants {
            BindingList<StringStringPair^>^ get();
        }

        property BindingList<StringStringPair^>^ ResourceBindings {
            BindingList<StringStringPair^>^ get();
        }
        
        static StringStringPair^ MakePropertyPair(String^ name, String^ value) { return gcnew StringStringPair(name, value); }

        property RenderStateSet^ StateSet { RenderStateSet^ get() { return _renderStateSet; } }

        property String^ TechniqueConfig { String^ get(); void set(String^); }

        const RenderCore::Assets::RawMaterial* GetUnderlying();

        String^ BuildInheritanceList();
        void Resolve(RenderCore::Techniques::Material& destination);

        void AddInheritted(String^);
        void RemoveInheritted(String^);

        property String^ Filename { String^ get(); }
        property String^ Initializer { String^ get(); }

        static RawMaterial^ Get(String^ initializer);
        static RawMaterial^ CreateUntitled();

        ~RawMaterial();
    private:
        using NativeConfig = ToolsRig::DivergentAsset<RenderCore::Assets::RawMaterial>;
        clix::shared_ptr<NativeConfig> _underlying;

        RenderStateSet^ _renderStateSet;

        BindingList<StringStringPair^>^ _materialParameterBox;
        BindingList<StringStringPair^>^ _shaderConstants;
        BindingList<StringStringPair^>^ _resourceBindings;
        String^ _initializer;
        void ParameterBox_Changed(System::Object^, ListChangedEventArgs^);
        void ResourceBinding_Changed(System::Object^, ListChangedEventArgs^);

        RawMaterial(String^ initialiser);

		uint32 _transId;
		void CheckBindingInvalidation();

        static RawMaterial();
        static Dictionary<String^, WeakReference^>^ s_table;
    };

    public ref class InvalidAssetList
    {
    public:
        property IEnumerable<Tuple<String^, String^>^>^ AssetList 
        {
            IEnumerable<Tuple<String^, String^>^>^ get();
        }

        static bool HasInvalidAssets();

        delegate void OnChange();
        event OnChange^ _onChange;
        void RaiseChangeEvent();

        InvalidAssetList();
        ~InvalidAssetList();
    protected:
        unsigned _eventId;
    };
}
