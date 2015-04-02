//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System;
using System.ComponentModel.Composition;
using System.Drawing;
using System.Windows.Forms;
using System.Threading;
using System.IO;


using Sce.Atf;
using Sce.Atf.Dom;
using Sce.Atf.Applications;
using Sce.Atf.Adaptation;


using Sce.Atf.VectorMath;

using LevelEditorCore;

using ResourceLister = LevelEditorCore.ResourceLister;
using ViewTypes = Sce.Atf.Rendering.ViewTypes;


namespace RenderingInterop
{

    /// <summary>
    /// Thumbnail resolver for image resources</summary>        
    [Export(typeof(IInitializable))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ResourcePreview : IInitializable
    {
        public ResourcePreview()
        {

            m_controlInfo = new ControlInfo(
                "Resource Preview".Localize(),
                "Preview selected 3d resource".Localize(),
                StandardControlGroup.Hidden);
        }

        #region IInitializable Members

        void IInitializable.Initialize()
        {
            if (m_resourceLister == null) return;

            m_resourceLister.SelectionChanged += resourceLister_SelectionChanged;
            m_renderSurface = new NativeViewControl();
            m_controlHostService.RegisterControl(m_renderSurface, m_controlInfo, null);
            Init();

        }

        #endregion

        private void resourceLister_SelectionChanged(object sender, EventArgs e)
        {
           // if (m_game == null) Init();

            m_game.RootGameObjectFolder.GameObjects.Clear();                        
            m_renderSurface.Invalidate();

            Uri resourceUri = m_resourceLister.LastSelected;
            if (resourceUri == null) return;

            IResource resource = m_resourceService.Load(resourceUri);
            IGameObject gob = m_resourceConverterService.Convert(resource);

            if (gob != null)
            {
                m_game.RootGameObjectFolder.GameObjects.Add(gob);
                m_renderSurface.ResetCamera = true;
                m_renderSurface.Render();
                m_controlInfo.Name = "Resource Preview: ".Localize() + Path.GetFileName(resourceUri.LocalPath);                
            }
            else
            {
                m_controlInfo.Name = "Resource Preview".Localize();
            }
        }

        private void Init()
        {
            // if (m_game != null) 
            //     return;
            // 
            // NativeObjectAdapter curLevel = GameEngine.GetGameLevel();
            // try
            // {
            //     // create new document by creating a Dom node of the root type defined by the schema                 
            //     DomNode rootNode = new DomNode(m_schemaLoader.GameType, m_schemaLoader.GameRootElement);
            //     INameable nameable = rootNode.As<INameable>();
            //     nameable.Name = "Game";
            //     NativeObjectAdapter gameLevel = rootNode.Cast<NativeObjectAdapter>();
            //     GameEngine.CreateObject(gameLevel);
            //     GameEngine.SetGameLevel(gameLevel);
            //     gameLevel.UpdateNativeOjbect();
            //     NativeDocumentAdapter gworld = rootNode.Cast<NativeDocumentAdapter>();
            //     m_game = rootNode.Cast<IGame>();
            //     IGameObjectFolder rootFolder = m_game.RootGameObjectFolder;
            //     m_renderSurface.Game = m_game;
            //     m_renderSurface.GameEngineProxy = m_gameEngine;
            // 
            // }
            // finally
            // {
            //     GameEngine.SetGameLevel(curLevel);
            // }
            // 
            // 
            // m_mainWindow.Closed += delegate
            // {
            //     GameEngine.DestroyObject(m_game.Cast<NativeObjectAdapter>());
            //     m_renderSurface.Dispose();
            // };
        }

        // [Import(AllowDefault = false)]
        // private ISchemaLoader m_schemaLoader = null;
        // 
        // [Import(AllowDefault = false)]
        // private IMainWindow m_mainWindow = null;

        [Import(AllowDefault = true)]
        private ResourceLister m_resourceLister = null;

        [Import(AllowDefault = false)]
        private IResourceService m_resourceService = null;

        [Import(AllowDefault = false)]
        private ResourceConverterService m_resourceConverterService = null;

        [Import(AllowDefault = false)]
        private ControlHostService m_controlHostService = null;

        // [Import(AllowDefault = false)]
        // private IGameEngineProxy m_gameEngine;

        private NativeViewControl m_renderSurface;
        private IGame m_game;        
        
        private readonly ControlInfo m_controlInfo;

        private class NativeViewControl : ViewControl
        {

            public NativeViewControl()
            {
                ClientSize = new Size(16, 16);
                // if (!GameEngine.IsInError)
                // {
                //     swapChainId = GameEngine.GetObjectTypeId("SwapChain");
                //     SurfaceId = GameEngine.CreateObject(0, 0, swapChainId, this.Handle, IntPtr.Size);
                //     SizePropId = GameEngine.GetObjectPropertyId(swapChainId, "Size");
                //     GameEngine.SetObjectProperty(swapChainId, 0, SurfaceId, SizePropId, ClientSize);
                //     BkgColorPropId = GameEngine.GetObjectPropertyId(swapChainId, "BkgColor");
                //     GameEngine.SetObjectProperty(swapChainId, 0, SurfaceId, BkgColorPropId, BackColor);
                // }

                m_renderState = new RenderState();
                m_renderState.RenderFlag = GlobalRenderFlags.Solid | GlobalRenderFlags.Textured | GlobalRenderFlags.Lit;
                m_renderState.WireFrameColor = Color.DarkBlue;
                m_renderState.SelectionColor = Color.FromArgb(66, 255, 161);
                BackColor = SystemColors.ControlDark;


                #region Context menu
                GlobalRenderFlags[] flags = new GlobalRenderFlags[] 
                {
                    GlobalRenderFlags.Solid,
                    GlobalRenderFlags.WireFrame,
                    GlobalRenderFlags.Textured, 
                    GlobalRenderFlags.RenderBackFace,
                    GlobalRenderFlags.Lit
                };
                ContextMenuStrip cntx = new ContextMenuStrip();
                foreach (var flag in flags)
                {
                    ToolStripMenuItem item =
                        new ToolStripMenuItem(flag.ToString());
                    item.Tag = flag;
                    item.Click += delegate(object sender, EventArgs e)
                    {
                        ToolStripMenuItem menu = (ToolStripMenuItem)sender;
                        RenderState.RenderFlag = RenderState.RenderFlag ^ (GlobalRenderFlags)menu.Tag;
                        this.Invalidate();
                    };

                    cntx.Items.Add(item);
                }



                cntx.Items.Add(new ToolStripSeparator());
                ToolStripMenuItem views = new ToolStripMenuItem("View");
                cntx.Items.Add(views);
                views.DropDownOpening += delegate
                {
                    foreach (ToolStripMenuItem item in views.DropDownItems)
                    {
                        item.Checked = (ViewType == (ViewTypes)item.Tag);
                    }
                };

                foreach (var val in Enum.GetValues(typeof(ViewTypes)))
                {
                    ViewTypes viewtype = (ViewTypes)val;
                    ToolStripMenuItem item = new ToolStripMenuItem(viewtype.ToString());
                    item.Tag = viewtype;
                    item.Click += delegate(object sender, EventArgs e)
                    {
                        ToolStripMenuItem menuItem = (ToolStripMenuItem)sender;
                        this.ViewType = (ViewTypes)menuItem.Tag;
                    };

                    views.DropDownItems.Add(item);
                }


                cntx.Opening += delegate
                {
                    GlobalRenderFlags renderflags = RenderState.RenderFlag;
                    foreach (ToolStripItem item in cntx.Items)
                    {
                        if (item is ToolStripSeparator) break;
                        ((ToolStripMenuItem)item).Checked = (renderflags & (GlobalRenderFlags)item.Tag) != 0;
                    }
                };

                ContextMenuStrip = cntx;
                #endregion
            }

            public bool ResetCamera
            {
                get;
                set;
            }

            public IGameEngineProxy GameEngineProxy
            {
                get;
                set;
            }
            public IGame Game
            {
                get;
                set;
            }
            // public ulong SurfaceId
            // {
            //     get;
            //     private set;
            // }

            public RenderState RenderState
            {
                get { return m_renderState; }
            }

            protected override void OnBackColorChanged(EventArgs e)
            {
                base.OnBackColorChanged(e);
                // if (swapChainId != 0)
                // {
                //     GameEngine.SetObjectProperty(swapChainId, 0, SurfaceId, BkgColorPropId, BackColor);
                // }
            }

            protected override void OnSizeChanged(EventArgs e)
            {
                base.OnSizeChanged(e);
                Size sz = ClientSize;
                //if (!GameEngine.IsInError && SurfaceId != 0 && sz.Width > 0 && sz.Height > 0)
                //{
                //    GameEngine.SetObjectProperty(swapChainId, 0, SurfaceId, SizePropId, sz);
                    Camera.Aspect = (float)sz.Width / (float)sz.Height;
                //}
            }
            protected override void Dispose(bool disposing)
            {
                // GameEngine.DestroyObject(0, SurfaceId, swapChainId);
                // SurfaceId = 0;
                if (disposing)
                {
                    m_renderState.Dispose();
                }
                base.Dispose(disposing);
            }

            protected override void OnPaint(PaintEventArgs e)
            {
                if (GameEngine.IsInError)
                {
                    e.Graphics.Clear(BackColor);
                    if (GameEngine.IsInError)
                        e.Graphics.DrawString(GameEngine.CriticalError, Font, Brushes.Red, 1, 1);
                    return;
                }
                Render();
            }

            // render the scene.
            public override void Render()
            {
            }

            /// <summary>
            /// Raises the <see cref="E:System.Windows.Forms.Control.MouseDown"></see> event.
            /// </summary>
            /// <param name="e">A <see cref="T:System.Windows.Forms.MouseEventArgs"></see> that contains the event data.</param>
            protected override void OnMouseDown(MouseEventArgs e)
            {
                Focus();
                CameraController.MouseDown(this, e);
                base.OnMouseDown(e);
            }

            /// <summary>
            /// Raises the <see cref="E:System.Windows.Forms.Control.MouseMove"></see> event.
            /// </summary>
            /// <param name="e">A <see cref="T:System.Windows.Forms.MouseEventArgs"></see> that contains the event data.</param>
            protected override void OnMouseMove(MouseEventArgs e)
            {
                CameraController.MouseMove(this, e);
                base.OnMouseMove(e);
            }

            /// <summary>
            /// Raises the <see cref="E:System.Windows.Forms.Control.MouseUp"></see> event.
            /// </summary>
            /// <param name="e">A <see cref="T:System.Windows.Forms.MouseEventArgs"></see> that contains the event data.</param>
            protected override void OnMouseUp(MouseEventArgs e)
            {
                CameraController.MouseUp(this, e);
                base.OnMouseUp(e);
            }

            private RenderState m_renderState;
            // private readonly uint swapChainId;
            // private readonly uint SizePropId;
            // private readonly uint BkgColorPropId;
        }
    }
}
