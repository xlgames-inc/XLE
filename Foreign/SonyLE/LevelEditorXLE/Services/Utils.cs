// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;

namespace LevelEditorXLE
{
    internal static class Utils
    {
        internal static Uri CurrentDirectoryAsUri()
        {
            return new Uri(System.IO.Directory.GetCurrentDirectory().TrimEnd('\\') + "\\");
        }
    }
}
