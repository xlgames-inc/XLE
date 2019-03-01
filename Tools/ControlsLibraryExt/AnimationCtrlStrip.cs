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

            /*AvailableAnimations = new List<string>{
                "Walk", "Run", "Dodge"
            };*/

            updateTimer.Interval = 200;
            updateTimer.Tick += UpdateTimeBar;
        }

        /*private class AnimationState
        {
            internal int AnchorTime;
            internal float Time;
            internal bool Playing;
        }
        private AnimationState CurrentState = new AnimationState { AnchorTime = 0, Time = 0.0f, Playing = false };*/

        private float SelectedAnimationLength;
        // private string SelectedAnimation;
        private IEnumerable<string> AvailableAnimations
        {
            set
            {
                _animationSelection.Items.Clear();
                foreach(string s in value)
                    _animationSelection.Items.Add(s);
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
        }

        private void _animationSelection_SelectedIndexChanged(object sender, EventArgs e)
        {
            SelectedAnimationLength = 5.0f;
            _animationState.AnimationTime = 0.0f;
            _animationState.AnchorTime = (uint)Environment.TickCount;
            _animationState.ActiveAnimation = _animationSelection.Text;
            UpdateTimeBar(null, null);
        }

        readonly System.Windows.Forms.Timer updateTimer = new System.Windows.Forms.Timer();

        private void _timeBar_Scroll(object sender, EventArgs e)
        {
            _animationState.AnimationTime = (_timeBar.Value / 1000000.0f) * SelectedAnimationLength;
            _animationState.AnchorTime = (uint)Environment.TickCount;
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

        private void UpdateFromAnimationState()
        {
            _playButton.Checked = _animationState.CurrentState == GUILayer.VisAnimationState.State.Playing;
            AvailableAnimations = _animationState.AnimationList;
            _animationSelection.Text = _animationState.ActiveAnimation;
        }

        private GUILayer.VisAnimationState _animationState;
    }
}
