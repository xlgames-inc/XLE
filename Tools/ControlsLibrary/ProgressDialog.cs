// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Windows.Forms;
using System.Threading;

namespace ControlsLibrary
{
    public partial class ProgressDialog : Form
    {
        public ProgressDialog()
        {
            InitializeComponent();
            _syncContext = SynchronizationContext.Current;
        }

        public bool IsReady() { return _syncContext != null; }

        internal void Execute(SendOrPostCallback del)
        {
            // note -- if the progress dialog hasn't been shown yet, the message will be lost!
            if (_syncContext != null)
                _syncContext.Post(del, null);
        }

        private class StepInterface : GUILayer.IStep
        {
            public virtual void SetProgress(uint progress) 
            {
                _dlg.Execute((o) => { _dlg._bar.Value = (int)progress; });
            }

            public virtual void Advance() 
            {
                _dlg.Execute((o) => { _dlg._bar.PerformStep(); });
            }

            public virtual bool IsCancelled() 
            { 
                return _dlg.Cancelled; 
            }

            public virtual void EndStep() 
            {
                _dlg.Execute((o) => { _dlg._stepLabel.Text = ""; });
            }

            internal StepInterface(ProgressDialog dlg, string name, uint progressMax, bool cancellable)
            {
                _dlg = dlg;
                _dlg.Execute(
                    (o) =>
                    {
                        _dlg._stepLabel.Text = name;
                        _dlg._bar.Maximum = (int)progressMax;
                        _dlg._bar.Step = 1;
                        _dlg.Cancelled = false;
                        _dlg._cancelBtn.Enabled = cancellable;
                    });
            }

            private ProgressDialog _dlg;
        }

        public class ProgressInterface : GUILayer.IProgress, IDisposable
        {
            public virtual GUILayer.IStep BeginStep(string name, uint progressMax, bool cancellable)
            {
                return new StepInterface(_dlg, name, progressMax, cancellable);
            }

            public ProgressInterface()
            {
                _dlg = null;

                // create a background thread, and initialize the dlg within in
                _dlgThread = new System.Threading.Thread(
                    () =>
                    {
                        _dlg = new ControlsLibrary.ProgressDialog();
                        System.Windows.Forms.Application.Run(_dlg);
                    });
                _dlgThread.Start();

                // stall here waiting for _dlg to become something other than null
                while (_dlg == null) { System.Threading.Thread.Yield(); }
            }
            
            private ProgressDialog _dlg;
            private System.Threading.Thread _dlgThread;

            public void Dispose()
            {
                Dispose(true);
                GC.SuppressFinalize(this);
            }
            protected virtual void Dispose(bool disposing)
            {
                if (disposing)
                {
                    _dlg.Execute((o) => { _dlg.Close(); });
                    _dlgThread.Join();
                    _dlg.Dispose();
                }
            }
        }

        private bool _cancelled = false;
        internal bool Cancelled
        {
            get { return _cancelled; }
            set 
            {
                _cancelled = value;
                if (_cancelled) _cancelBtn.Text = "* Cancel";
                else _cancelBtn.Text = "Cancel";
            }
        }
        internal SynchronizationContext _syncContext;

        private void _cancelBtn_Click(object sender, EventArgs e) { Cancelled = true; }
    }
}
