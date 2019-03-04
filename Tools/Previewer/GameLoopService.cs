using System;
using System.ComponentModel.Composition;
using System.ComponentModel.Composition.Hosting;
using System.Windows.Forms;
using System.Runtime.InteropServices;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Controls;
using Sce.Atf.Controls.Adaptable;

namespace Previewer
{
    [Export(typeof(IInitializable))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    class GameLoopService : IInitializable
    {
        void IInitializable.Initialize()
        {
            Application.Idle += Application_Idle;
        }

        private void Application_Idle(object sender, EventArgs e)
        {
            while (GUILayer.EngineControl.HasRegularAnimationControls() && IsIdle())
            {
                if (!ApplicationIsActive())
                {
                    System.Threading.Thread.Sleep(16);
                }

                GUILayer.EngineControl.TickRegularAnimation();
            }
        }

        private bool IsIdle()
        {
            return PeekMessage(out m_msg, IntPtr.Zero, 0, 0, 0) == 0;
        }

        [System.Security.SuppressUnmanagedCodeSecurity]
        [DllImport("User32.dll", CharSet = CharSet.Unicode)]
        private static extern int PeekMessage(out Message msg, IntPtr hWnd, uint messageFilterMin, uint messageFilterMax, uint flags);
        private Message m_msg;

        [DllImport("user32.dll", CharSet = CharSet.Auto, ExactSpelling = true)]
        private static extern IntPtr GetForegroundWindow();

        [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        private static extern int GetWindowThreadProcessId(IntPtr handle, out int processId);

        private bool ApplicationIsActive()
        {
            // Check to see if this application is still in the foreground
            // If we drop into the background, we should suppress updates.
            var foregroundWindow = GetForegroundWindow();
            if (foregroundWindow == IntPtr.Zero) return false;

            int foreWindowProcess;
            GetWindowThreadProcessId(foregroundWindow, out foreWindowProcess);
            return foreWindowProcess == System.Diagnostics.Process.GetCurrentProcess().Id;
        }
    }
}
