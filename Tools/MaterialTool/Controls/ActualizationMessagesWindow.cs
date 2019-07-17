using System;
using System.ComponentModel.Composition;
using System.Windows.Forms;

using System.Collections.Generic;
using System.Linq;
using System.Text;

using Sce.Atf;
using Sce.Atf.Controls.Adaptable;

namespace MaterialTool.Controls
{
    [Export(typeof(ActualizationMessagesWindow))]
    [Export(typeof(IInitializable))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public partial class ActualizationMessagesWindow : AdaptableControl, IInitializable
    {
        public ActualizationMessagesWindow()
        {
            _readout = new TextBox();
            _readout.Dock = DockStyle.Fill;
            _readout.WordWrap = true;
            _readout.Multiline = true;
            _readout.ScrollBars = ScrollBars.Vertical;
            _readout.ReadOnly = true;
            Controls.Add(_readout);
        }

        private TextBox _readout;

        public void SetContext(GUILayer.MessageRelayWrapper context)
        {
            var existingContext = ContextAs<GUILayer.MessageRelayWrapper>();
            if (existingContext != null)
            {
                existingContext.OnChangeEvent -= OnChangeEvent;
            }
            Context = context;
            if (context != null)
            {
                context.OnChangeEvent += OnChangeEvent;
                UpdateText();
            }
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                SetContext(null);
                _readout.Dispose();
            }
            base.Dispose(disposing);
        }

        private void OnChangeEvent(object sender, EventArgs args)
        {
            if (InvokeRequired)
            {
                BeginInvoke(new Action<object, EventArgs>(OnChangeEvent), new[] { sender, args });
            }
            else
            {
                UpdateText();
            }
        }

        private void UpdateText()
        {
            var context = ContextAs<GUILayer.MessageRelayWrapper>();
            if (context == null)
            {
                _readout.Text = string.Empty;
                return;
            }

            _readout.Text = System.Text.RegularExpressions.Regex.Replace(context.Messages, @"\r\n|\n\r|\n|\r", System.Environment.NewLine);        // (make sure we to convert the line endings into windows form);
        }

        #region IInitializable Members

        void IInitializable.Initialize()
        {
            m_controlHostService.RegisterControl(
                new ActualizationMessagesWindow(), 
                new Sce.Atf.Applications.ControlInfo(
                    "Actualization log messages".Localize(),
                    "Displays errors and other messages from actualizing shaders for preview".Localize(),
                    Sce.Atf.Applications.StandardControlGroup.Right), 
                null);
        }

        #endregion

        [Import(AllowDefault = false)]
        private Sce.Atf.Applications.IControlHostService m_controlHostService = null;
    }
}
