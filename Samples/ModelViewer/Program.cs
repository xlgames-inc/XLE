// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows.Forms;

namespace ModelViewer
{
    static class Program
    {
        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            GUILayer.EngineDevice.SetDefaultWorkingDirectory();
            using(var device = new GUILayer.EngineDevice()) {
                device.AttachColladaCompilers();
                Application.Run(new MainForm());
            }
        }
    }
}
