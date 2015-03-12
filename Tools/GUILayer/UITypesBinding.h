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

using namespace System::ComponentModel;
using namespace System::Windows::Forms;
using namespace System::Drawing::Design;

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

        [Category("Visualisation")]
        [Description("Highlight material divisions")]
        property bool ColourByMaterial
        {
            bool get() { return (*_object)->_colourByMaterial; }
            void set(bool value)
            {
                (*_object)->_colourByMaterial = value; 
                (*_object)->_changeEvent.Trigger(); 
            }
        }

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

        std::shared_ptr<PlatformRig::ModelVisSettings> GetUnderlying() { return *_object.get(); }

        ModelVisSettings(std::shared_ptr<PlatformRig::ModelVisSettings> attached)
        {
            _object.reset(new std::shared_ptr<PlatformRig::ModelVisSettings>(std::move(attached)));
        }

        ~ModelVisSettings() { delete _object; }

        static ModelVisSettings^ CreateDefault()
        {
            auto attached = std::make_shared<PlatformRig::ModelVisSettings>();
            return gcnew ModelVisSettings(std::move(attached));
        }

    protected:
        AutoToShared<PlatformRig::ModelVisSettings> _object;
    };
}

