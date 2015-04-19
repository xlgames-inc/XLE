//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System;
using System.ComponentModel;
using System.Drawing;
using Sce.Atf;


namespace RenderingInterop
{
    /// <summary>
    /// class mirros RenderState in C++
    /// the memory layout and size in bytes do not have to match.
    /// class name and common property names have to match.
    /// </summary>
    public class RenderState : DisposableObject
    {
        /// <summary>
        /// Event that is raised when any properties of this RenderState change</summary>
        public event EventHandler OnChanged;
          
        private GlobalRenderFlags m_renderflags;
        
        [Browsable(false)]
        public GlobalRenderFlags RenderFlag
        {
            get { return m_renderflags; }
            set 
            { 
                if (value != m_renderflags)
                {
                    m_renderflags = value;
                    OnChanged.Raise(this, EventArgs.Empty);
                }
            }
        }


        private Color m_wireColor;

        [CategoryAttribute("Render Settings"),
        DescriptionAttribute("color used for wireframe mode")]       
        public Color WireFrameColor
        {
            get { return m_wireColor; }
            set 
            {
                if (value != m_wireColor)
                {
                    m_wireColor = value;
                    OnChanged.Raise(this, EventArgs.Empty);
                }
            }
        }

        private Color m_selectionColor;

        [CategoryAttribute("Render Settings"),
        DescriptionAttribute("Selection color")]
        public Color SelectionColor
        {
            get { return m_selectionColor; }
            set
            {
                if (value != m_selectionColor)
                {
                    m_selectionColor = value;
                    OnChanged.Raise(this, EventArgs.Empty);
                }
            }
        }

        private DisplayFlagModes m_displayCaption;

        /// <summary>
        /// Gets/Sets value to control display caption on objects.</summary>
        [CategoryAttribute("Render Settings"),
         DescriptionAttribute("Display object name")]
        public DisplayFlagModes DisplayCaption
        {
            get { return m_displayCaption; }
            set
            {
                if (m_displayCaption != value)
                {
                    m_displayCaption = value;
                    OnChanged.Raise(this, EventArgs.Empty);
                }
            }
        }

        private DisplayFlagModes m_displayBound;

        /// <summary>
        /// Gets/Sets value to control display bounding box of objects.</summary>
        [CategoryAttribute("Render Settings"),
         DescriptionAttribute("Display object bound")]
        public DisplayFlagModes DisplayBound
        {
            get { return m_displayBound; }
            set
            {
                if (m_displayBound != value)
                {
                    m_displayBound = value;
                    OnChanged.Raise(this, EventArgs.Empty);
                }
            }
        }

        private DisplayFlagModes m_displayPivot;

        /// <summary>
        /// Gets/Sets value to control dispaly pivot of objects.</summary>
        [CategoryAttribute("Render Settings"),
         DescriptionAttribute("Display object pivot")]
        public DisplayFlagModes DisplayPivot
        {
            get { return m_displayPivot; }
            set
            {
                if (m_displayPivot != value)
                {
                    m_displayPivot = value;
                    OnChanged.Raise(this, EventArgs.Empty);
                }
            }
        }

        private string m_activeEnvironmentSettings = "environment";

        [CategoryAttribute("Environment"),
         DescriptionAttribute("Active environments settings")]
        public string EnvironmentSettings
        {
            get { return m_activeEnvironmentSettings; }
            set
            {
                if (m_activeEnvironmentSettings != value)
                {
                    m_activeEnvironmentSettings = value;
                    OnChanged.Raise(this, EventArgs.Empty);
                }
            }
        }
    }

    public enum DisplayFlagModes
    {
        None,     // don't display the property
        Always,   // always display
        Selection, // display only for selected objects.
    }

   
}
