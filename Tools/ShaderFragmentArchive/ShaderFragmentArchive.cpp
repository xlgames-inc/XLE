// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderFragmentArchive.h"
#include "../GUILayer/MarshalString.h"
#include "../../ShaderParser/InterfaceSignature.h"
#include "../../ShaderParser/ParameterSignature.h"
#include "../../ShaderParser/GraphSyntax.h"
#include "../../ShaderParser/Exceptions.h"
#include "../../Assets/IFileSystem.h"
#include "../../Utility/Streams/FileSystemMonitor.h"
#include "../../Utility/Streams/PathUtils.h"

namespace ShaderFragmentArchive
{

    Function::Function(StringSection<> name, const ShaderPatcher::NodeGraphSignature& function)
    {
		ShaderPatcherLayer::ConversionContext convContext;
		Signature = ShaderPatcherLayer::NodeGraphSignature::ConvertFromNative(function, convContext);
        Name = clix::marshalString<clix::E_UTF8>(name);
    }

    Function::~Function() {}

    System::String^     Function::BuildParametersString()
    {
        System::Text::StringBuilder stringBuilder;
        stringBuilder.Append("(");
        bool first = true;
        for each(auto p in Signature->Parameters) {
			if (p->Direction != ShaderPatcherLayer::NodeGraphSignature::ParameterDirection::In) continue;

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

    ParameterStruct::ParameterStruct(const ShaderPatcher::UniformBufferSignature& parameterStruct)
    {
        Parameters = gcnew List<Parameter^>();

        using namespace clix;
        for (auto i=parameterStruct._parameters.begin(); i!=parameterStruct._parameters.end(); ++i) {
            Parameter^ p = gcnew Parameter();
            p->Name = marshalString<E_UTF8>(i->_name);
            p->Type = marshalString<E_UTF8>(i->_type);
            p->Semantic = marshalString<E_UTF8>(i->_semantic);
            p->Source = Parameter::SourceType::Material;
            Parameters->Add(p);
        }

        Name = marshalString<E_UTF8>(parameterStruct._name);
    }

    ParameterStruct::~ParameterStruct() { delete Parameters; }

    System::String^ ParameterStruct::BuildBodyString()
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
        msclr::gcroot<System::WeakReference^> _shaderFragment;
        msclr::gcroot<System::Threading::SynchronizationContext^> _mainThread;
    };

    void    ShaderFragmentChangeCallback::OnChange() 
    {
        auto frag = dynamic_cast<ShaderFragment^>(_shaderFragment->Target);
        if (frag)
            _mainThread->Post(gcnew System::Threading::SendOrPostCallback(frag, &ShaderFragment::OnChange), nullptr);
    }

    ShaderFragmentChangeCallback::ShaderFragmentChangeCallback(ShaderFragment^ shaderFragment, System::Threading::SynchronizationContext^ mainThread)
        : _shaderFragment(gcnew System::WeakReference(shaderFragment))
        , _mainThread(mainThread)
    {}

    ShaderFragmentChangeCallback::~ShaderFragmentChangeCallback()
    {}

    void ShaderFragment::OnChange(Object^obj)
    {
        ++_changeMarker;
        ChangeEvent(obj, System::EventArgs::Empty);
    }

    ShaderFragment::ShaderFragment(System::String^ sourceFile)
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
        Functions = gcnew List<Function^>();
        ParameterStructs = gcnew List<ParameterStruct^>();

		std::string nativeFilename;

		size_t size = 0;
        std::unique_ptr<uint8[]> file;
        try
        {
			Name = System::IO::Path::GetFileNameWithoutExtension(sourceFile);
			nativeFilename = clix::marshalString<clix::E_UTF8>(sourceFile);
            file = ::Assets::TryLoadFileAsMemoryBlock(nativeFilename, &size);
        } catch (System::IO::IOException^) {

                //      Hit an exception! Most likely, the file just doesn't
                //      exist.
            ExceptionString = "Failed while opening file";

        } catch (System::ArgumentException^) {
			ExceptionString = "Failed while opening file (probably bad filename)";
		}
            
        if (file && size != 0) {

			auto srcCode = MakeStringSection((const char*)file.get(), (const char*)PtrAdd(file.get(), size));
            try {
				if (XlEqStringI(MakeFileNameSplitter(nativeFilename).Extension(), "graph")) {

					auto graphSyntax = ShaderPatcher::ParseGraphSyntax(srcCode);
					for (const auto& subGraph:graphSyntax._subGraphs) {
						Function^ function = gcnew Function(MakeStringSection(subGraph.first), subGraph.second._signature);
						Functions->Add(function);
					}

				} else {

					auto nativeSignature = ShaderSourceParser::ParseHLSL(srcCode);

					for (const auto& fn:nativeSignature._functions) {
						Function^ function = gcnew Function(MakeStringSection(fn.first), fn.second);
						Functions->Add(function);
					}

					for (const auto& ps:nativeSignature._uniformBuffers) {
						ParameterStruct^ pstruct = gcnew ParameterStruct(ps);
						ParameterStructs->Add(pstruct);
					}
				}
            } catch (const ShaderSourceParser::Exceptions::ParsingFailure& ) {
                ExceptionString = "Failed during parsing. Look for compilation errors.";
            }
            
        }

        std::shared_ptr<ShaderFragmentChangeCallback> changeCallback(
            new ShaderFragmentChangeCallback(this, System::Threading::SynchronizationContext::Current));
		::Assets::MainFileSystem::TryMonitor(MakeStringSection(clix::marshalString<clix::E_UTF8>(sourceFile)), changeCallback);
        _fileChangeCallback = changeCallback;
    }

    ShaderFragment::~ShaderFragment()
    {
        delete Functions;
        delete ParameterStructs;
        delete Name;
        delete ExceptionString;
        _fileChangeCallback.reset();
    }

    ShaderFragment^   Archive::GetFragment(System::String^ name, GUILayer::DirectorySearchRules^ searchRules)
    {
		if (searchRules)
			name = searchRules->ResolveFile(name);
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
                if (result->GetChangeMarker()>0) {
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
        
    Function^   Archive::GetFunction(System::String^ name, GUILayer::DirectorySearchRules^ searchRules)
    {
        System::Threading::Monitor::Enter(_dictionary);
        try
        {
            auto colonIndex = name->IndexOf(":");
            System::String ^fileName = nullptr, ^functionName = nullptr;
            if (colonIndex != -1) {
                fileName        = name->Substring(0, colonIndex);
                functionName    = name->Substring(colonIndex+1);
            } else {
                fileName        = name;
            }            

            ShaderFragment^ fragment = GetFragment(fileName, searchRules);

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

    ParameterStruct^  Archive::GetUniformBuffer(System::String^ name, GUILayer::DirectorySearchRules^ searchRules)
    {
        System::Threading::Monitor::Enter(_dictionary);
        try
        {
            auto colonIndex = name->IndexOf(":");
            System::String ^fileName = nullptr, ^parameterStructName = nullptr;
            if (colonIndex != -1) {
                fileName                = name->Substring(0, colonIndex);
                parameterStructName     = name->Substring(colonIndex+1);
            } else {
                fileName        = name;
            }            

            ShaderFragment^ fragment = GetFragment(fileName, searchRules);

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

    Archive::Archive()
    {
        _dictionary = gcnew Dictionary<System::String^, ShaderFragment^>(System::StringComparer::CurrentCultureIgnoreCase);
    }

}




