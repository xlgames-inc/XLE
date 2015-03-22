using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace ModelViewer
{
    public partial class ModalModifications : Form
    {
        public ModalModifications(GUILayer.EngineDevice engine)
        {
            InitializeComponent();

            _modifiedAssets.LoadOnDemand = true;
            _modifiedAssets.Model = new GUILayer.DivergentAssetList(engine);
        }
    }
}
