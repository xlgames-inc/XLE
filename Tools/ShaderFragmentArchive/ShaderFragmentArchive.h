// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>

using namespace System;
using namespace System::Collections::Generic;
using namespace System::ComponentModel;

namespace ShaderSourceParser { class FunctionSignature; class ParameterStructSignature; }
namespace Utility { class OnChangeCallback; }

namespace ShaderFragmentArchive {

        ///////////////////////////////////////////////////////////////
    public ref class Function
    {
    public:
        property String^    Name;

        ref class Parameter
        {
        public:
            property String^    Type;
            property String^    Semantic;
            property String^    Name;
        };

        property List<Parameter^>^      InputParameters;
        property List<Parameter^>^      Outputs;

        Function(ShaderSourceParser::FunctionSignature& function);
        ~Function();
        String^ BuildParametersString();
    };

        ///////////////////////////////////////////////////////////////
    public ref class Parameter
    {
    public:
        enum class SourceType { Material, InterpolatorIntoVertex, InterpolatorIntoPixel, System, Output, Constant };
        
        [CategoryAttribute("Name")]     property String^        Name;
        [CategoryAttribute("Name")]     property String^        Description;

        [CategoryAttribute("System")]   property SourceType     Source;
        [CategoryAttribute("System")]   property String^        Default;
        [CategoryAttribute("System"),
         ReadOnlyAttribute(true)]       property String^        ArchiveName;

        [CategoryAttribute("Type")]     property String^        Type;
        [CategoryAttribute("Type")]     property String^        TypeExtra;
        [CategoryAttribute("Type")]     property String^        Semantic;
        [CategoryAttribute("Type")]     property String^        Min;
        [CategoryAttribute("Type")]     property String^        Max;
        
        [ReadOnlyAttribute(true)]       property String^        ExceptionString;
        
        Parameter(String^ archiveName);
        void DeepCopyFrom(Parameter^ otherParameter);
    };

        ///////////////////////////////////////////////////////////////
    public ref class ParameterStruct
    {
    public:
        property String^                Name;
        property List<Parameter^>^      Parameters;

        ParameterStruct(ShaderSourceParser::ParameterStructSignature& parameterStruct);
        ~ParameterStruct();
        String^ BuildBodyString();
    };

        ///////////////////////////////////////////////////////////////
    public delegate void ChangeEventHandler(Object^ sender);
    public ref class ShaderFragment
	{
    public:
        property List<Function^>^           Functions;
        property List<ParameterStruct^>^    ParameterStructs;
        property String^                    Name;
        property String^                    ExceptionString;

        ShaderFragment(String^ sourceFile);
        ~ShaderFragment();

        event ChangeEventHandler^ ChangeEvent;
        void    OnChange(Object^);
        unsigned    GetChangeMarker() { return _changeMarker; }
    private:
        std::shared_ptr<Utility::OnChangeCallback>*  _fileChangeCallback;
        unsigned _changeMarker;
	};

        ///////////////////////////////////////////////////////////////
    public ref class Archive
    {
    public: 
        static ShaderFragment^      GetFragment(String^ name);
        static Function^            GetFunction(String^ name);
        static ParameterStruct^     GetParameterStruct(String^ name);
        static Parameter^           GetParameter(String^ name);
    private:
        static System::Collections::Generic::Dictionary<String^, ShaderFragment^>^    _dictionary;
        static Archive();
    };
    
}
