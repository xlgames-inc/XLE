// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "CLIXAutoPtr.h"
#include "MarshalString.h"
#include "../ToolsRig/ModelVisualisation.h"
#include "../ToolsRig/DivergentAsset.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/SystemUtils.h"
#include "../../Utility/ParameterBox.h"

using namespace System::ComponentModel;
using namespace System::Drawing::Design;
using namespace System::Collections::Generic;

namespace RenderCore { namespace Assets { class RawMaterial; } }
namespace RenderCore { namespace Techniques { class Material; } }
namespace ToolsRig { class VisOverlaySettings; class VisMouseOver; }

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
            if (ofd->ShowDialog() == System::Windows::Forms::DialogResult::OK) {
                return ofd->FileName;
            }

            return __super::EditValue(context, provider, value);
        }

        FileNameEditor()
        {
            ofd = gcnew System::Windows::Forms::OpenFileDialog();
        }

    private:
        System::Windows::Forms::OpenFileDialog ^ofd;
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
        property System::String^ ModelName
        {
            System::String^ get()
            {
                return clix::marshalString<clix::E_UTF8>(_object->_modelName);
            }

            void set(System::String^ value);
        }

        [Category("Model")]
        [Description("Active material file")]
        [EditorAttribute(FileNameEditor::typeid, UITypeEditor::typeid)]
        property System::String^ MaterialName
        {
            System::String^ get()
            {
                return clix::marshalString<clix::E_UTF8>(_object->_materialName);
            }

            void set(System::String^ value);
        }

        [Category("Model")]
        [Description("Supplements")]
        property System::String^ Supplements
        {
            System::String^ get()
            {
                return clix::marshalString<clix::E_UTF8>(_object->_supplements);
            }

            void set(System::String^ value);
        }

        [Category("Model")]
        [Description("Level Of Detail")]
        property unsigned LevelOfDetail
        {
            unsigned get() { return _object->_levelOfDetail; }
            void set(unsigned value);
        }

		[Category("Animation")]
        [Description("Active animation file")]
        [EditorAttribute(FileNameEditor::typeid, UITypeEditor::typeid)]
        property System::String^ AnimationFileName
        {
            System::String^ get()
            {
                return clix::marshalString<clix::E_UTF8>(_object->_animationFileName);
            }

            void set(System::String^ value);
        }

		[Category("Animation")]
        [Description("Active skeleton file")]
        [EditorAttribute(FileNameEditor::typeid, UITypeEditor::typeid)]
        property System::String^ SkeletonFileName
        {
            System::String^ get()
            {
                return clix::marshalString<clix::E_UTF8>(_object->_skeletonFileName);
            }

            void set(System::String^ value);
        }

		ModelVisSettings(const std::shared_ptr<ToolsRig::ModelVisSettings>& attached)
        {
            _object = attached;
        }

        ModelVisSettings() 
        {
            _object = std::make_shared<ToolsRig::ModelVisSettings>();
        }

        ~ModelVisSettings() { _object.reset(); }

		static ModelVisSettings^ CreateDefault();

		virtual event PropertyChangedEventHandler^ PropertyChanged;
		const std::shared_ptr<ToolsRig::ModelVisSettings>& GetUnderlying() { return _object.GetNativePtr(); }

	protected:
		clix::shared_ptr<ToolsRig::ModelVisSettings> _object;

		void NotifyPropertyChanged(System::String^ propertyName);
	};

	public ref class VisOverlaySettings
	{
	public:
        // [Category("Environment")]
        // [Description("Environment settings name")]
        // property System::String^ EnvSettingsFile;

        enum class ColourByMaterialType { None, All, MouseOver };
		enum class SkeletonModes { None, Render, BoneNames };

        [Category("Visualisation")]
        [Description("Highlight material divisions")]
        property ColourByMaterialType ColourByMaterial;

		[Category("Visualisation")]
        [Description("Mode for skeleton visualization")]
        property SkeletonModes SkeletonMode;

        [Category("Visualisation")]
        [Description("Draw Normals, tangents and bitangents")]
        property bool DrawNormals;

        [Category("Visualisation")]
        [Description("Draw wireframe")]
        property bool DrawWireframe;

		std::shared_ptr<ToolsRig::VisOverlaySettings> ConvertToNative();
		static VisOverlaySettings^ ConvertFromNative(const ToolsRig::VisOverlaySettings& input);
    };

    public ref class VisMouseOver
    {
    public:
        [Description("Intersection coordinate")]
        property System::String^ IntersectionPt { System::String^ get(); }

        [Description("Draw call index")]
        property unsigned DrawCallIndex { unsigned get(); }

        [Description("Model file name")]
        property System::String^ ModelName { System::String^ get(); }

        [Category("Material")]
        property System::String^ MaterialName { System::String^ get(); }

        [Browsable(false)] property bool HasMouseOver { bool get(); }
        [Browsable(false)] property System::String^ FullMaterialName { System::String^ get(); }
        [Browsable(false)] property uint64 MaterialBindingGuid { uint64 get(); }

        void AttachCallback(System::Windows::Forms::PropertyGrid^ callback);
        const std::shared_ptr<ToolsRig::VisMouseOver>& GetUnderlying() { return _object.GetNativePtr(); }

        static System::String^ BuildFullMaterialName(
            const ToolsRig::ModelVisSettings& modelSettings,
            uint64 materialGuid);
        static System::String^ DescriptiveMaterialName(System::String^ fullName);

        VisMouseOver(
            std::shared_ptr<ToolsRig::VisMouseOver> attached,
            std::shared_ptr<ToolsRig::ModelVisSettings> settings);
        VisMouseOver();
        ~VisMouseOver();

    protected:
        clix::shared_ptr<ToolsRig::VisMouseOver> _object;
        clix::shared_ptr<ToolsRig::ModelVisSettings> _modelSettings;
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
        void NotifyPropertyChanged(/*[CallerMemberName]*/ System::String^ propertyName);
        System::Threading::SynchronizationContext^ _propertyChangedContext;

        NameType _name;
        ValueType _value;
    };

    using StringIntPair = PropertyPair<System::String^, unsigned> ;
    using StringStringPair = PropertyPair<System::String^, System::String^>;

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

        virtual event PropertyChangedEventHandler^ PropertyChanged;

        using NativeConfig = ToolsRig::DivergentAsset<RenderCore::Assets::RawMaterial>;
        RenderStateSet(std::shared_ptr<NativeConfig> underlying);
        ~RenderStateSet();
    protected:
        clix::shared_ptr<NativeConfig> _underlying;

        void NotifyPropertyChanged(/*[CallerMemberName]*/ System::String^ propertyName);
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
        
        static StringStringPair^ MakePropertyPair(System::String^ name, System::String^ value) { return gcnew StringStringPair(name, value); }

        property RenderStateSet^ StateSet { RenderStateSet^ get() { return _renderStateSet; } }

        property System::String^ TechniqueConfig { System::String^ get(); void set(System::String^); }

        const RenderCore::Assets::RawMaterial* GetUnderlying();

        System::String^ BuildInheritanceList();
        void Resolve(RenderCore::Techniques::Material& destination);

        void AddInheritted(System::String^);
        void RemoveInheritted(System::String^);

        property System::String^ Filename { System::String^ get(); }
        property System::String^ Initializer { System::String^ get(); }

        static RawMaterial^ Get(System::String^ initializer);
        static RawMaterial^ CreateUntitled();

        ~RawMaterial();
    private:
        using NativeConfig = ToolsRig::DivergentAsset<RenderCore::Assets::RawMaterial>;
        clix::shared_ptr<NativeConfig> _underlying;

        RenderStateSet^ _renderStateSet;

        BindingList<StringStringPair^>^ _materialParameterBox;
        BindingList<StringStringPair^>^ _shaderConstants;
        BindingList<StringStringPair^>^ _resourceBindings;
        System::String^ _initializer;
        void ParameterBox_Changed(System::Object^, ListChangedEventArgs^);
        void ResourceBinding_Changed(System::Object^, ListChangedEventArgs^);

        RawMaterial(System::String^ initialiser);

		uint32 _transId;
		void CheckBindingInvalidation();

        static RawMaterial();
        static Dictionary<System::String^, System::WeakReference^>^ s_table;
    };

    public ref class InvalidAssetList
    {
    public:
        property IEnumerable<System::Tuple<System::String^, System::String^>^>^ AssetList 
        {
            IEnumerable<System::Tuple<System::String^, System::String^>^>^ get();
        }

        static bool			HasInvalidAssets();
        delegate void		OnChange();
        event OnChange^		_onChange;
        void				RaiseChangeEvent();

        InvalidAssetList();
        ~InvalidAssetList();
    };
}
