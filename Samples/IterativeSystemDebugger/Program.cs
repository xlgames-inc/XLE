using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace IterativeSystemDebugger
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
            using (var device = new GUILayer.EngineDevice()) {
                device.AttachDefaultCompilers();
                Application.Run(new MainForm());
            }
        }
    }
}
