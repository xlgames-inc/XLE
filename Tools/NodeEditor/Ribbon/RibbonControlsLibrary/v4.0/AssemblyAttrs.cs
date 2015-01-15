//---------------------------------------------------------------------------
//
// Copyright (C) Microsoft Corporation.  All rights reserved.
//
// This file specifies various assembly level attributes.
//
//---------------------------------------------------------------------------

using System.Reflection;
using System.Windows;
using System.Windows.Markup;

// Version number is built like this:
// Major and minor version = the target .NET Framework version, such as 3.5 
// Build = YMMDD where MM and DD are today and Y is the number of years since 
//       the last major release aligned to the calendar year. For instance, 
//       Y = 2010-2006 (the year of the target Major release, 3.0 in our case)  
[assembly: AssemblyVersion("4.0.0.11019")]
// this Version number should match the one defined in %SDXROOT%\Tools\ndp20.versions.xml
// which gets picked up by the asmmeta file 

[assembly:ThemeInfo(
    // Specifies the location of theme specific resources
    ResourceDictionaryLocation.SourceAssembly,
    // Specifies the location of non-theme specific resources:
    ResourceDictionaryLocation.SourceAssembly)]

[assembly: XmlnsDefinition("http://schemas.microsoft.com/winfx/2006/xaml/presentation/ribbon", "Microsoft.Windows.Controls.Ribbon")]
[assembly: XmlnsDefinition("http://schemas.microsoft.com/winfx/2006/xaml/presentation", "Microsoft.Windows.Controls")]
