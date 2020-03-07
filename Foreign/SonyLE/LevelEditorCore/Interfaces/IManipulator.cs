//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System.Drawing;
using System.Windows.Forms;

namespace LevelEditorCore
{
    public enum ManipulatorPickResult { Miss, DeferredBeginDrag, ImmediateBeginDrag };

    /// <summary>    
    /// current IManipulator is coupled with opengl related classes like scene-node
    /// talk to Ron about how to refactor 
    /// </summary>
    public interface IManipulator
    {
        /// <summary>
        /// Pick manipulator</summary>
        /// <param name="vc"> The view control that the 
        /// manipulator is rendering on</param>
        /// <param name="scrPt"> 2d point in screen space.
        /// where (0,0) is topleft and  (width,height) is bottom right</param>
        ManipulatorPickResult Pick(ViewControl vc, Point scrPt);

        /// <summary>
        /// Render manipulator.</summary>
        void Render(object context, ViewControl vc);
        void RenderPostProcessing(object context, ViewControl vc);

        /// <summary>
        /// Begin drag
        /// called one time before dragging
        /// A good place to compute some values that are useful during drag operation.
        /// </summary>
        void OnBeginDrag(ViewControl vc, Point scrPt);

        /// <summary>
        /// called per mouse move. Perform drag.</summary>  
        /// <param name="vc"> The view control that the 
        /// manipulator is rendering on</param>
        /// <param name="scrPt"> 2d point in screen space.
        /// where (0,0) is topleft and  (width,height) is bottom right</param>
        void OnDragging(ViewControl vc, Point scrPt);

        /// <summary>
        /// called once after dragging ends 
        /// A good place to commit the changes or do some validation
        /// <param name="vc"> The view control that the 
        /// manipulator is rendering on</param>
        /// <param name="scrPt"> 2d point in screen space.
        /// where (0,0) is topleft and  (width,height) is bottom right</param>
        /// </summary>
        void OnEndDrag(ViewControl vc, Point scrPt);

        void OnMouseWheel(ViewControl vc, Point scrPt, int delta);

        Control GetHoveringControl();
        event System.EventHandler OnHoveringControlChanged;

        /// <summary>
        /// Gets ManipulatorInfo.
        /// ManipulatorInfo is used by ManipulatorCommands to create menue
        /// for this manipulator. 
        /// </summary>
        ManipulatorInfo ManipulatorInfo { get; }
    }
}
