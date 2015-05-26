using System;
using System.Collections.Generic;
using Sce.Atf.Adaptation;

namespace LevelEditor.NewItemPages
{
    interface NewItemPage
    {
        System.Windows.Forms.Control Control { get; }
        void Execute(IAdaptable parent);
        bool CanOperateOn(IAdaptable parent);
    }
}
