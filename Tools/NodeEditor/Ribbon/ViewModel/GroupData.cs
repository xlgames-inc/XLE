using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.ComponentModel;
using System.Collections.ObjectModel;
using System.Windows.Input;

namespace NodeEditorRibbon.ViewModel
{
    public class GroupData : ControlData
    {
        public GroupData(string header)
        {
            Label = header;
        }

        [DesignerSerializationVisibility(DesignerSerializationVisibility.Content)]
        public ObservableCollection<ControlData> ControlDataCollection
        {
            get
            {
                if (_controlDataCollection == null)
                {
                    _controlDataCollection = new ObservableCollection<ControlData>();

                    Uri smallImage = new Uri("/NodeEditorRibbon;component/Images/Paste_16x16.png", UriKind.Relative);
                    Uri largeImage = new Uri("/NodeEditorRibbon;component/Images/Paste_32x32.png", UriKind.Relative);

                    for (int i = 0; i < ViewModelData.ButtonCount; i++)
                    {
                        _controlDataCollection.Add(new ButtonData()
                        {
                            Label = "Button " + i,
                            SmallImage = smallImage,
                            LargeImage = largeImage,
                            ToolTipTitle = "ToolTip Title",
                            ToolTipDescription = "ToolTip Description",
                            ToolTipImage = smallImage,
                            Command = ViewModelData.DefaultCommand
                        });
                    }
                    for (int i = 0; i < ViewModelData.ToggleButtonCount; i++)
                    {
                        _controlDataCollection.Add(new ToggleButtonData()
                        {
                            Label = "ToggleButton " + i,
                            SmallImage = smallImage,
                            LargeImage = largeImage,
                            ToolTipTitle = "ToolTip Title",
                            ToolTipDescription = "ToolTip Description",
                            ToolTipImage = smallImage,
                            Command = ViewModelData.DefaultCommand
                        });
                    }
                    for (int i = 0; i < ViewModelData.RadioButtonCount; i++)
                    {
                        _controlDataCollection.Add(new RadioButtonData()
                        {
                            Label = "RadioButton " + i,
                            SmallImage = smallImage,
                            LargeImage = largeImage,
                            ToolTipTitle = "ToolTip Title",
                            ToolTipDescription = "ToolTip Description",
                            ToolTipImage = smallImage,
                            Command = ViewModelData.DefaultCommand
                        });
                    }
                    for (int i = 0; i < ViewModelData.CheckBoxCount; i++)
                    {
                        _controlDataCollection.Add(new CheckBoxData()
                        {
                            Label = "CheckBox " + i,
                            SmallImage = smallImage,
                            LargeImage = largeImage,
                            ToolTipTitle = "ToolTip Title",
                            ToolTipDescription = "ToolTip Description",
                            ToolTipImage = smallImage,
                            Command = ViewModelData.DefaultCommand
                        });
                    }
                    for (int i = 0; i < ViewModelData.TextBoxCount; i++)
                    {
                        _controlDataCollection.Add(new TextBoxData()
                        {
                            Label = "TextBox " + i,
                            SmallImage = smallImage,
                            LargeImage = largeImage,
                            ToolTipTitle = "ToolTip Title",
                            ToolTipDescription = "ToolTip Description",
                            ToolTipImage = smallImage,
                            Command = ViewModelData.DefaultCommand
                        });
                    }
                    for (int i = 0; i < ViewModelData.MenuButtonCount; i++)
                    {
                        _controlDataCollection.Add(new MenuButtonData()
                        {
                            Label = "MenuButton " + i,
                            SmallImage = smallImage,
                            LargeImage = largeImage,
                            ToolTipTitle = "ToolTip Title",
                            ToolTipDescription = "ToolTip Description",
                            ToolTipImage = smallImage,
                            Command = ViewModelData.DefaultCommand
                        });
                    }
                    for (int i = 0; i < ViewModelData.SplitButtonCount; i++)
                    {
                        _controlDataCollection.Add(new SplitButtonData()
                        {
                            Label = "SplitButton " + i,
                            SmallImage = smallImage,
                            LargeImage = largeImage,
                            ToolTipTitle = "ToolTip Title",
                            ToolTipDescription = "ToolTip Description",
                            ToolTipImage = smallImage,
                            Command = ViewModelData.DefaultCommand,
                            IsCheckable = true
                        });
                    }
                    for (int i = 0; i < ViewModelData.ComboBoxCount; i++)
                    {
                        _controlDataCollection.Add(new ComboBoxData()
                        {
                            Label = "ComboBox " + i,
                            SmallImage = smallImage,
                            LargeImage = largeImage,
                            ToolTipTitle = "ToolTip Title",
                            ToolTipDescription = "ToolTip Description",
                            ToolTipImage = smallImage,
                            Command = ViewModelData.DefaultCommand
                        });
                    }
                }
                return _controlDataCollection;
            }
        }
        private ObservableCollection<ControlData> _controlDataCollection;

    }
}
