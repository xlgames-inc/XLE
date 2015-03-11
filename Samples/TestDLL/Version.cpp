// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

namespace Samples
{

/////////////////////////////////////////////////////////////////////////////////////////////

    //  The "VersionString" object here will be replaced with a true version
    //  number by a build script.
    //  VersionString_Start & VersionString_End are markers used by the script
    //  to tell where the actual version number begins and ends
    //  In Visual Studio, we can use the "$X" suffix on segment assignments to
    //  control the order of the objects within the segment (otherwise the 
    //  compiler has freedom to reorder them in any way)

/////////////////////////////////////////////////////////////////////////////////////////////

#pragma section(".ver$a",read)
#pragma section(".ver$b",read)
#pragma section(".ver$c",read)

__declspec(allocate(".ver$a")) static char VersionString_Start[] = "<VERSION_NUMBER<";
__declspec(allocate(".ver$b")) char VersionString[64] = "v0.0.0";
__declspec(allocate(".ver$c")) static char VersionString_End[] = ">VERSION_NUMBER>";

/////////////////////////////////////////////////////////////////////////////////////////////

#pragma section(".ver$d",read)
#pragma section(".ver$e",read)
#pragma section(".ver$f",read)

__declspec(allocate(".ver$d")) static char BuildDateString_Start[] = "<VERSION_DATE<";
__declspec(allocate(".ver$e")) char BuildDateString[64] = "";
__declspec(allocate(".ver$f")) static char BuildDateString_End[] = ">VERSION_DATE>";

/////////////////////////////////////////////////////////////////////////////////////////////

}

