using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace LevelEditor.TerrainControls
{
    public partial class XLETerrainManipControls : UserControl
    {
        public XLETerrainManipControls()
        {
            InitializeComponent();
        }

        private void manipulatorSelection_SelectedIndexChanged(object sender, EventArgs e)
        {
            var newManipulator = manipulatorSelection.SelectedItem as string;
            if (newManipulator != null)
            {
                ActiveManipulator = newManipulator; 

                if (_manipulators != null) {
                    SetFocusProperties(
                        XLELayer.ManipulatorPropertyContext.Create(
                            _manipulators, ActiveManipulator));
                } else {
                    SetFocusProperties(null);
                }
            }
        }

        public void SetManipulators(GUILayer.IManipulatorSet manipulators)
        {
            manipulatorProperties.Bind(null); 

            _manipulators = manipulators;
            var names = manipulators.GetManipulatorNames();
            manipulatorSelection.DataSource = names;
        }

        public void SetFocusProperties(object properties)
        {
            manipulatorProperties.Bind(properties as Sce.Atf.Applications.IPropertyEditingContext);
        }

        public string ActiveManipulator { get; private set; }

        private GUILayer.IManipulatorSet _manipulators = null;
    }
}
