using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace NodeEditorRibbon.ViewModel
{
    public static class NodesModel
    {

        public static ControlData NodesInsertGroup
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Insert";
                    if (!_dataCollection.ContainsKey(Str))
                    {
                        GroupData Data = new GroupData(Str)
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/InsideBorders_16x16.png", UriKind.Relative),
                            LargeImage = new Uri("/NodeEditorRibbon;component/Images/InsideBorders_16x16.png", UriKind.Relative),
                            KeyTip = "I",
                        };
                        _dataCollection[Str] = Data;
                    }

                    return _dataCollection[Str];
                }
            }
        }


        private static object _lockObject = new object();
        private static Dictionary<string, ControlData> _dataCollection = new Dictionary<string, ControlData>();
    }
}
