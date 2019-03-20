// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Drawing;
using namespace System::Runtime::Serialization;
using System::Runtime::InteropServices::OutAttribute;

namespace GUILayer 
{

        ///////////////////////////////////////////////////////////////
	public enum class PreviewGeometry { Chart, Plane2D, Box, Sphere, Model };

	public ref class PreviewSettings
	{
	public:
		property PreviewGeometry    Geometry;
		property String^            OutputToVisualize;

		static String^ PreviewGeometryToString(PreviewGeometry geo);
		static PreviewGeometry PreviewGeometryFromString(String^ input);
     };

		///////////////////////////////////////////////////////////////
	[DataContract] public ref class NodeGraphMetaData
    {
    public:
        property String^ DefaultsMaterial;
        property String^ PreviewModelFile;

        // Restrictions placed on the input variables
        [DataMember] property Dictionary<String^, String^>^ Variables { Dictionary<String^, String^>^ get() { if (!_variables) _variables = gcnew Dictionary<String^, String^>(); return _variables; } }

        // Configuration settings for the output file
        [DataMember] bool HasTechniqueConfig;
        [DataMember] property Dictionary<String^, String^>^ ShaderParameters { Dictionary<String^, String^>^ get() { if (!_shaderParameters) _shaderParameters = gcnew Dictionary<String^, String^>(); return _shaderParameters; } }

		NodeGraphMetaData() { HasTechniqueConfig = false; }

    private:
        Dictionary<String^, String^>^ _variables = nullptr;
        Dictionary<String^, String^>^ _shaderParameters = nullptr;
    };

	ref class NodeGraphFile;

	public ref class NodeGraphPreviewConfiguration
	{
	public:
		NodeGraphFile^		_nodeGraph;
		String^				_subGraphName;
		UInt32				_previewNodeId; 
		PreviewSettings^	_settings;
		IEnumerable<KeyValuePair<String^, String^>>^ _variableRestrictions;
	};

	ref class TechniqueDelegateWrapper;
	ref class MessageRelayWrapper;

	public ref class ShaderGeneratorLayer
	{
	public:

		/*Tuple<String^, String^>^ 
			GeneratePreviewShader(
				NodeGraph^ graph,
				UInt32 previewNodeId, 
				NodeGraphSignature^ signature,
				NodeGraphFile^ nodeGraphFile,
				PreviewSettings^ settings,
				IEnumerable<KeyValuePair<String^, String^>>^ variableRestrictions);*/

		static Tuple<String^, String^>^ GeneratePreviewShader(
			NodeGraphFile^ nodeGraphFile,
			String^ subGraphName, UInt32 previewNodeId,
			PreviewSettings^ settings, IEnumerable<KeyValuePair<String^, String^>>^ variableRestrictions);

		static void		LoadNodeGraphFile(String^ filename, [Out] NodeGraphFile^% nodeGraph, [Out] NodeGraphMetaData^% context);
        static void		Serialize(System::IO::Stream^ stream, String^ name, NodeGraphFile^ nodeGraphFile, NodeGraphMetaData^ context);

		TechniqueDelegateWrapper^ MakeTechniqueDelegate(
			NodeGraphMetaData^ doc, 
			NodeGraphPreviewConfiguration^ nodeGraphFile);

		TechniqueDelegateWrapper^ MakeTechniqueDelegate(
			NodeGraphFile^ nodeGraph,
			String^ subGraphName,
			MessageRelayWrapper^ logMessages);
	};

}


