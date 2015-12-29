// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "stdafx.h"

#include "ShaderFragmentArchive.h"
#include "../GUILayer/MarshalString.h"
#include "../../ShaderParser/InterfaceSignature.h"
#include "../../ShaderParser/ParameterSignature.h"
#include "../../Utility/Streams/FileSystemMonitor.h"
#include "../../Utility/Streams/PathUtils.h"

namespace ShaderFragmentArchive
{

    Function::Function(ShaderSourceParser::FunctionSignature& function)
    {
        InputParameters = gcnew List<Parameter^>();
        Outputs = gcnew List<Parameter^>();

        using namespace clix;
        for (auto i=function._parameters.begin(); i!=function._parameters.end(); ++i) {
            Parameter^ p = gcnew Parameter;
            p->Name = marshalString<E_UTF8>(i->_name);
            p->Type = marshalString<E_UTF8>(i->_type);
            p->Semantic = marshalString<E_UTF8>(i->_semantic);

            if (i->_direction & ShaderSourceParser::FunctionSignature::Parameter::In)
                InputParameters->Add(p);
            if (i->_direction & ShaderSourceParser::FunctionSignature::Parameter::Out)
                Outputs->Add(p);
        }

        if (!function._returnType.empty() && function._returnType != "void") {
            Parameter^ p = gcnew Parameter;
            p->Name = marshalString<E_UTF8>("result");
            p->Type = marshalString<E_UTF8>(function._returnType);
            Outputs->Add(p);
        }

        Name = marshalString<E_UTF8>(function._name);
    }

    Function::~Function() { delete InputParameters; delete Outputs; }

    String^     Function::BuildParametersString()
    {
        System::Text::StringBuilder stringBuilder;
        stringBuilder.Append("(");
        bool first = true;
        for each(Parameter^ p in InputParameters) {
            if (!first) stringBuilder.Append(", ");
            first = false;

            stringBuilder.Append(p->Type);
            stringBuilder.Append(" ");
            stringBuilder.Append(p->Name);
            if (p->Semantic != nullptr && !p->Semantic->Empty) {
                stringBuilder.Append(" : ");
                stringBuilder.Append(p->Semantic);
            }
        }
        stringBuilder.Append(")");
        return stringBuilder.ToString();
    }

    ParameterStruct::ParameterStruct(ShaderSourceParser::ParameterStructSignature& parameterStruct)
    {
        Parameters = gcnew List<Parameter^>();

        using namespace clix;
        for (auto i=parameterStruct._parameters.begin(); i!=parameterStruct._parameters.end(); ++i) {
            Parameter^ p = gcnew Parameter("");
            p->Name = marshalString<E_UTF8>(i->_name);
            p->Type = marshalString<E_UTF8>(i->_type);
            p->Semantic = marshalString<E_UTF8>(i->_semantic);
            p->Source = Parameter::SourceType::Material;
            Parameters->Add(p);
        }

        Name = marshalString<E_UTF8>(parameterStruct._name);
    }

    ParameterStruct::~ParameterStruct() { delete Parameters; }

    String^ ParameterStruct::BuildBodyString()
    {
        System::Text::StringBuilder stringBuilder;
        stringBuilder.Append("{");
        for each(Parameter^ p in Parameters) {
            stringBuilder.Append(p->Type);
            stringBuilder.Append(" ");
            stringBuilder.Append(p->Name);
            if (p->Semantic != nullptr && !p->Semantic->Empty) {
                stringBuilder.Append(" : ");
                stringBuilder.Append(p->Semantic);
            }
            stringBuilder.Append("; ");
        }
        stringBuilder.Append("}");
        return stringBuilder.ToString();
    }

    class ShaderFragmentChangeCallback : public Utility::OnChangeCallback
    {
    public:
        virtual void    OnChange();
        ShaderFragmentChangeCallback(ShaderFragment^ shaderFragment, System::Threading::SynchronizationContext^ mainThread);
        virtual ~ShaderFragmentChangeCallback();
    private:
        gcroot<ShaderFragment^> _shaderFragment;
        gcroot<System::Threading::SynchronizationContext^> _mainThread;
    };

    void    ShaderFragmentChangeCallback::OnChange() 
    {
        _mainThread->Post(gcnew System::Threading::SendOrPostCallback(_shaderFragment, &ShaderFragment::OnChange), nullptr);
    }

    ShaderFragmentChangeCallback::ShaderFragmentChangeCallback(ShaderFragment^ shaderFragment, System::Threading::SynchronizationContext^ mainThread)
        : _shaderFragment(shaderFragment)
        , _mainThread(mainThread)
    {}

    ShaderFragmentChangeCallback::~ShaderFragmentChangeCallback()
    {}

    static void RegisterFileDependency(std::shared_ptr<Utility::OnChangeCallback> validationIndex, const char filename[])
    {
        char directoryName[MaxPath], baseName[MaxPath];
        XlDirname(directoryName, dimof(directoryName), filename);
        auto len = XlStringLen(directoryName);
        if (len > 0) { directoryName[len-1] = '\0'; }
        XlBasename(baseName, dimof(baseName), filename);
        Utility::AttachFileSystemMonitor(StringSection<char>(directoryName), StringSection<char>(baseName), validationIndex);
    }

    void ShaderFragment::OnChange(Object^obj)
    {
        ++_changeMarker;
        ChangeEvent(obj);
    }

    ShaderFragment::ShaderFragment(String^ sourceFile)
    {
        _fileChangeCallback = nullptr;
        _changeMarker = 0;

            // 
            //      Load the given shader fragment, and extract the interface
            //      information.
            //
            //      The heavy lifting is done in native C++ code. So here,
            //      we're mostly just marshaling the data into a format
            //      that suits us.
            //
        Name = System::IO::Path::GetFileNameWithoutExtension(sourceFile);
        Functions = gcnew List<Function^>();
        ParameterStructs = gcnew List<ParameterStruct^>();

        using namespace clix;
        std::string nativeString;
        try
        {

            auto contents = System::IO::File::ReadAllText(sourceFile);
            nativeString = marshalString<E_UTF8>(contents);

        } catch (System::IO::IOException^) {

                //      Hit an exception! Most likely, the file just doesn't
                //      exist.
            ExceptionString = "Failed while opening file";

        }
            
        if (!nativeString.empty()) {

            try {
                auto nativeSignature = ShaderSourceParser::BuildShaderFragmentSignature(
                    nativeString.c_str(), nativeString.size());

                    //
                    //      \todo -- support compilation errors in the shader code!
                    //

                for (auto i=nativeSignature._functions.begin(); i!=nativeSignature._functions.end(); ++i) {
                    Function^ function = gcnew Function(*i);
                    Functions->Add(function);
                }

                for (auto i=nativeSignature._parameterStructs.begin(); i!=nativeSignature._parameterStructs.end(); ++i) {
                    ParameterStruct^ pstruct = gcnew ParameterStruct(*i);
                    ParameterStructs->Add(pstruct);
                }
            } catch (const ShaderSourceParser::Exceptions::ParseError& ) {
                ExceptionString = "Failed during parsing. Look for compilation errors.";
            }
            
        }

        auto changeCallback = std::shared_ptr<Utility::OnChangeCallback>(
            new ShaderFragmentChangeCallback(this, System::Threading::SynchronizationContext::Current));
        RegisterFileDependency(changeCallback, marshalString<E_UTF8>(sourceFile).c_str());
        _fileChangeCallback = new std::shared_ptr<Utility::OnChangeCallback>(changeCallback);
    }

    ShaderFragment::~ShaderFragment()
    {
        delete Functions;
        delete ParameterStructs;
        delete Name;
        delete ExceptionString;
        delete _fileChangeCallback;
    }

    Parameter::Parameter(String^ archiveName)
    {
        using namespace clix;
        ArchiveName = archiveName;

        if (archiveName != nullptr && archiveName->Length > 0) {
            using namespace clix;
            std::string nativeString;
            try
            {

                auto contents = System::IO::File::ReadAllText(archiveName);
                nativeString = marshalString<E_UTF8>(contents);

            } catch (System::IO::IOException^) {

                    //      Hit an exception! Most likely, the file just doesn't
                    //      exist.
                ExceptionString = "Failed while opening file";

            }

            try 
            {

                auto nativeParameter = ShaderSourceParser::LoadSignature(nativeString.c_str(), nativeString.size());

                Name        = marshalString<E_UTF8>(nativeParameter._name);
                Description = marshalString<E_UTF8>(nativeParameter._description);
                Min         = marshalString<E_UTF8>(nativeParameter._min);
                Max         = marshalString<E_UTF8>(nativeParameter._max);
                Type        = marshalString<E_UTF8>(nativeParameter._type);
                TypeExtra   = marshalString<E_UTF8>(nativeParameter._typeExtra);
                Semantic    = marshalString<E_UTF8>(nativeParameter._semantic);
                Default     = marshalString<E_UTF8>(nativeParameter._default);
                if (nativeParameter._source == ShaderSourceParser::ParameterSignature::Source::System) {
                    Source      = SourceType::System;
                } else {
                    Source      = SourceType::Material;
                }

            } catch (const ShaderSourceParser::Exceptions::ParseError& ) {

                ExceptionString = "Failure in parser. Check text file format";

            }
        }
    }

    void Parameter::DeepCopyFrom(Parameter^ otherParameter)
    {
        ArchiveName = gcnew String(otherParameter->ArchiveName);
        Name = gcnew String(otherParameter->Name);
        Description = gcnew String(otherParameter->Description);
        Min = gcnew String(otherParameter->Min);
        Max = gcnew String(otherParameter->Max);
        Type = gcnew String(otherParameter->Type);
        TypeExtra = gcnew String(otherParameter->TypeExtra);
        Source = otherParameter->Source;
        ExceptionString = gcnew String(otherParameter->ExceptionString);
        Semantic = gcnew String(otherParameter->Semantic);
        Default = gcnew String(otherParameter->Default);
    }

    ShaderFragment^   Archive::GetFragment(String^ name)
    {
        System::Threading::Monitor::Enter(_dictionary);
        try
        {
                // todo -- should case be insensitive for filenames?
                //          "name" is really a filename here
            if (_dictionary->ContainsKey(name)) {
                ShaderFragment^ result = _dictionary[name];
                    // if the "change marker" is set to a value greater than 
                    //  zero, we have to delete and recreate this object
                    //  (it means the file has changed since the last parse)
                if (result->GetChangeMarker()>=0) {
                    _dictionary->Remove(name);
                } else
                    return result;
            }

            auto newFragment = gcnew ShaderFragment(name);
            _dictionary->Add(name, newFragment);
            return newFragment;
        } finally {
            System::Threading::Monitor::Exit(_dictionary);
        }
        return nullptr;
    }
        
    Function^   Archive::GetFunction(String^ name)
    {
        System::Threading::Monitor::Enter(_dictionary);
        try
        {
            auto colonIndex = name->IndexOf(":");
            String ^fileName = nullptr, ^functionName = nullptr;
            if (colonIndex != -1) {
                fileName        = name->Substring(0, colonIndex);
                functionName    = name->Substring(colonIndex+1);
            } else {
                fileName        = name;
            }            

            ShaderFragment^ fragment = GetFragment(fileName);

                // look for a function with the given name (case sensitive here)
            for each (Function^ f in fragment->Functions) {
                if (f->Name == functionName) {
                    return f;
                }
            }

        } finally {
            System::Threading::Monitor::Exit(_dictionary);
        }
        return nullptr;
    }

    ParameterStruct^  Archive::GetParameterStruct(String^ name)
    {
        System::Threading::Monitor::Enter(_dictionary);
        try
        {
            auto colonIndex = name->IndexOf(":");
            String ^fileName = nullptr, ^parameterStructName = nullptr;
            if (colonIndex != -1) {
                fileName                = name->Substring(0, colonIndex);
                parameterStructName     = name->Substring(colonIndex+1);
            } else {
                fileName        = name;
            }            

            ShaderFragment^ fragment = GetFragment(fileName);

                // look for a function with the given name (case sensitive here)
            for each (ParameterStruct^ p in fragment->ParameterStructs) {
                if (p->Name == parameterStructName) {
                    return p;
                }
            }

        } finally {
            System::Threading::Monitor::Exit(_dictionary);
        }

        return nullptr;
    }

    Parameter^ Archive::GetParameter(String^ name)
    {
        System::Threading::Monitor::Enter(_dictionary);
        try
        {
                // <archiveName>:<struct/fn name>:<parameter name>
            auto colonIndex = name->IndexOf(":");
            String ^fileName = nullptr, ^parameterStructName = nullptr, ^parameterName = nullptr;
            if (colonIndex != -1) {
                fileName = name->Substring(0, colonIndex);
                    
                auto secondColonIndex = name->IndexOf(":", colonIndex+1);
                if (secondColonIndex != -1) {
                    parameterStructName = name->Substring(colonIndex+1, secondColonIndex-colonIndex-1);
                    parameterName = name->Substring(secondColonIndex+1);
                } else {
                    parameterName = name->Substring(colonIndex);
                }
            } else {
                return nullptr;
            }

            ShaderFragment^ str = GetFragment(fileName);
            for each(ParameterStruct^ ps in str->ParameterStructs) {
                if (ps->Name == parameterStructName) {
                    for each (Parameter^ p in ps->Parameters) {
                        if (p->Name == parameterName) {
                            return p;
                        }
                    }
                }
            }

                // also try function parameters...?

            for each(Function^ fn in str->Functions) {
                if (fn->Name == parameterStructName) {
                    for each(Function::Parameter^ p in fn->InputParameters) {
                        if (p->Name == parameterName) {
                            Parameter^ result = gcnew Parameter("");
                            result->Name = p->Name;
                            result->Type = p->Type;
                            result->Source = Parameter::SourceType::Material;
                            result->Semantic = p->Semantic;
                            return result;
                        }
                    }
                    for each(Function::Parameter^ p in fn->Outputs) {
                        if (p->Name == parameterName) {
                            Parameter^ result = gcnew Parameter("");
                            result->Name = p->Name;
                            result->Type = p->Type;
                            result->Source = Parameter::SourceType::Material;
                            result->Semantic = p->Semantic;
                            return result;
                        }
                    }
                }
            }

            return nullptr;

        } finally {
            System::Threading::Monitor::Exit(_dictionary);
        }

        return nullptr;
    }

    Archive::Archive()
    {
        _dictionary = gcnew Dictionary<String^, ShaderFragment^>(StringComparer::CurrentCultureIgnoreCase);
    }

}




