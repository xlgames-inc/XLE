// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ShaderGenerator.h"
#include "../GUILayer/CLIXAutoPtr.h"
#include "../../Utility/StringUtils.h"
#include <memory>

using namespace System::Collections::Generic;
using namespace System::ComponentModel;
using namespace System::ComponentModel::Composition;

namespace ShaderPatcher { class NodeGraphSignature; class UniformBufferSignature; }
namespace Utility { class OnChangeCallback; }

namespace ShaderFragmentArchive 
{

        ///////////////////////////////////////////////////////////////
    public ref class Function
    {
    public:
        property System::String^							Name;
		property ShaderPatcherLayer::NodeGraphSignature^	Signature;

        Function(StringSection<> name, const ShaderPatcher::NodeGraphSignature& function);
        ~Function();
        System::String^ BuildParametersString();
    };

        ///////////////////////////////////////////////////////////////
    public ref class Parameter
    {
    public:
        enum class SourceType { Material, System, Output, Constant };
        
        property System::String^        Name;

        property SourceType				Source;
        property System::String^        Default;

        property System::String^        Type;
        property System::String^        Semantic;
    };

        ///////////////////////////////////////////////////////////////
    public ref class ParameterStruct
    {
    public:
        property System::String^		Name;
        property List<Parameter^>^      Parameters;

        ParameterStruct(const ShaderPatcher::UniformBufferSignature& parameterStruct);
        ~ParameterStruct();
        System::String^ BuildBodyString();
    };

        ///////////////////////////////////////////////////////////////
    public ref class ShaderFragment
	{
    public:
        property List<Function^>^           Functions;
        property List<ParameterStruct^>^    ParameterStructs;
        property System::String^			Name;
        property System::String^			ExceptionString;

        ShaderFragment(System::String^ sourceFile);
        ~ShaderFragment();

        event       System::EventHandler^ ChangeEvent;
        void        OnChange(Object^);
        unsigned    GetChangeMarker() { return _changeMarker; }
    private:
        clix::shared_ptr<Utility::OnChangeCallback> _fileChangeCallback;
        unsigned _changeMarker;
	};

        ///////////////////////////////////////////////////////////////
    [Export(Archive::typeid)]
    [PartCreationPolicy(CreationPolicy::Shared)]
    public ref class Archive
    {
    public: 
        ShaderFragment^      GetFragment(System::String^ name, GUILayer::DirectorySearchRules^ searchRules);
        Function^            GetFunction(System::String^ name, GUILayer::DirectorySearchRules^ searchRules);
        ParameterStruct^     GetUniformBuffer(System::String^ name, GUILayer::DirectorySearchRules^ searchRules);

        Archive();
    private:
        Dictionary<System::String^, ShaderFragment^>^    _dictionary;
    };
    
}
