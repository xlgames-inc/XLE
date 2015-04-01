//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System;
using System.ComponentModel.Composition;
using System.Collections.Generic;

using Sce.Atf;
using Sce.Atf.Dom;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;

using LevelEditorCore;


using ViewTypes = Sce.Atf.Rendering.ViewTypes;

namespace RenderingInterop
{
    [Export(typeof(ISnapSettings))]
    [Export(typeof(IDesignView))]    
    [Export(typeof(DesignView))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class NativeDesignView : DesignView
    {        
        public NativeDesignView()
        {
            NativeDesignControl[] views = new NativeDesignControl[]
            {
                new NativeDesignControl(this, GameEngine.GetEditorSceneManager()) { ViewType = ViewTypes.Perspective },
                new NativeDesignControl(this, GameEngine.GetEditorSceneManager()) { ViewType = ViewTypes.Right },
                new NativeDesignControl(this, GameEngine.GetEditorSceneManager()) { ViewType = ViewTypes.Top },
                new NativeDesignControl(this, GameEngine.GetEditorSceneManager()) { ViewType = ViewTypes.Front }
            };
            foreach (var v in views)
            {
                v.AddRenderCallback(RenderCallback);
            }
            QuadView.TopLeft = views[0];
            QuadView.TopRight = views[1];
            QuadView.BottomLeft = views[2];
            QuadView.BottomRight = views[3];

            // set control names.            
            QuadView.TopLeft.Name = "TopLeft";
            QuadView.TopRight.Name = "TopRight";
            QuadView.BottomLeft.Name = "BottomLeft";
            QuadView.BottomRight.Name = "BottomRight";



            ViewMode = ViewModes.Single;
            ContextChanged += NativeDesignView_ContextChanged;
        }

        void NativeDesignView_ContextChanged(object sender, EventArgs e)
        {
            if (m_selectionContext != null)
            {
                m_selectionContext.SelectionChanged -= new EventHandler(m_selectionContext_SelectionChanged);
            }

            m_selectionContext = Context.As<ISelectionContext>();

            if (m_selectionContext != null)
            {
                m_selectionContext.SelectionChanged += new EventHandler(m_selectionContext_SelectionChanged);
            }
        }

        void m_selectionContext_SelectionChanged(object sender, EventArgs e)
        {
            IEnumerable<DomNode> domNodes = m_selectionContext.Selection.AsIEnumerable<DomNode>();
            IEnumerable<DomNode> roots = DomNode.GetRoots(domNodes);
            IEnumerable<NativeObjectAdapter> nativeObjects = roots.AsIEnumerable<NativeObjectAdapter>();
            GameEngine.SetSelection(nativeObjects);
            InvalidateViews();
        }

        private ISelectionContext m_selectionContext;

        private void RenderCallback(DesignView designView, Sce.Atf.Rendering.Camera camera)
        {
            var game = designView.Context.As<IGame>();
            GridRenderer gridRender = game.Grid.Cast<GridRenderer>();
            gridRender.Render(camera);
        }
        
    }

}
