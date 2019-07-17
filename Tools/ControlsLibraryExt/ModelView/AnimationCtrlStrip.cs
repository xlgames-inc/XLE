using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace ControlsLibraryExt
{
    public partial class AnimationCtrlStrip : UserControl
    {
        public AnimationCtrlStrip()
        {
            InitializeComponent();

            updateTimer.Interval = 200;
            updateTimer.Tick += UpdateTimeBar;
        }

        private float SelectedAnimationLength
        {
            get
            {
                var selectedAnimation = _animationState.ActiveAnimation;
                float result = 5.0f;
                foreach (var s in _animationState.AnimationList)
                {
                    if (String.Compare(s.Name, selectedAnimation) == 0)
                    {
                        result = s.EndTime - s.BeginTime;
                    }
                }
                return result;
            }
        }

        private float CurrentAnimationTime(int currentTickCount)
        {
            if (_animationState.CurrentState == GUILayer.VisAnimationState.State.Playing) {
                return (_animationState.AnimationTime + (currentTickCount - _animationState.AnchorTime) / 1000.0f) % SelectedAnimationLength;
            } else {
                return _animationState.AnimationTime;
            }
        }

        private void UpdateTimeBar(object sender, EventArgs e)
        {
            if (SelectedAnimationLength > 0)
            {
                _timeBar.Value = (int)(CurrentAnimationTime(Environment.TickCount) / SelectedAnimationLength * 1000000);
            }
            else
            {
                _timeBar.Value = 0;
            }
        }

        private void checkBox1_CheckedChanged(object sender, EventArgs e)
        {
            var tickCount = Environment.TickCount;
            _animationState.AnimationTime = CurrentAnimationTime(tickCount);
            _animationState.AnchorTime = (uint)tickCount;
            _animationState.CurrentState = _playButton.Checked ? GUILayer.VisAnimationState.State.Playing : GUILayer.VisAnimationState.State.Stopped;
            updateTimer.Enabled = _animationState.CurrentState == GUILayer.VisAnimationState.State.Playing;
            if (_animationState.CurrentState == GUILayer.VisAnimationState.State.Playing)
            {
                updateTimer.Start();
            }
            else
            {
                updateTimer.Stop();
            }
            InvokeOnInvalidateViews();
        }

        private void _animationSelection_SelectedIndexChanged(object sender, EventArgs e)
        {
            _animationState.AnimationTime = 0.0f;
            _animationState.AnchorTime = (uint)Environment.TickCount;
            if (string.Compare(_animationSelection.Text, BindPoseStr)==0)
            {
                _animationState.ActiveAnimation = string.Empty;
                _animationState.CurrentState = GUILayer.VisAnimationState.State.BindPose;
            }
            else
            {
                _animationState.ActiveAnimation = _animationSelection.Text;
                if (_animationState.CurrentState == GUILayer.VisAnimationState.State.BindPose)
                    _animationState.CurrentState = GUILayer.VisAnimationState.State.Stopped;
            }
            _playButton.Checked = _animationState.CurrentState == GUILayer.VisAnimationState.State.Playing;
            _playButton.Enabled = _animationState.CurrentState != GUILayer.VisAnimationState.State.BindPose;
            UpdateTimeBar(null, null);
            InvokeOnInvalidateViews();
        }

        readonly System.Windows.Forms.Timer updateTimer = new System.Windows.Forms.Timer();

        private void _timeBar_Scroll(object sender, EventArgs e)
        {
            _animationState.AnimationTime = (_timeBar.Value / 1000000.0f) * SelectedAnimationLength;
            _animationState.AnchorTime = (uint)Environment.TickCount;
            InvokeOnInvalidateViews();
        }

        public GUILayer.VisAnimationState AnimationState
        {
            set
            {
                _animationState = value;
                _animationState.AddOnChangedCallback(() => { UpdateFromAnimationState(); });
                UpdateFromAnimationState();
            }
        }

        private string BindPoseStr = "<<bind pose>>";

        private void UpdateFromAnimationState()
        {
            _playButton.Checked = _animationState.CurrentState == GUILayer.VisAnimationState.State.Playing;
            _playButton.Enabled = _animationState.CurrentState != GUILayer.VisAnimationState.State.BindPose;
            _animationSelection.Items.Clear();
            _animationSelection.Items.Add(BindPoseStr);
            foreach (var s in _animationState.AnimationList)
            {
                _animationSelection.Items.Add(s.Name);
            }
            _animationSelection.Text = _animationState.ActiveAnimation;
            UpdateTimeBar(null, null);
        }

        private GUILayer.VisAnimationState _animationState;

        public event EventHandler OnInvalidateViews;

        private void InvokeOnInvalidateViews()
        {
            if (OnInvalidateViews != null)
                OnInvalidateViews.Invoke(this, EventArgs.Empty);
        }
    }
}
