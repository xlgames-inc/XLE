using System.Collections;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Documents;
using System.Windows.Markup;
using System.Windows.Media;
using System.Xml;
using Microsoft.Windows.Controls.Ribbon;
using Microsoft.Windows.Controls.Ribbon.Primitives;
using NodeEditorRibbon.ViewModel;

namespace NodeEditorRibbon
{
    /// <summary>
    /// Interaction logic for UserControlWord.xaml
    /// </summary>
    public partial class UserControlWord : UserControl
    {
        public UserControlWord()
        {
            InitializeComponent();
        }

        #region QAT Serialization

        private void UserControl_Loaded(object sender, RoutedEventArgs e)
        {
            if (ribbon.QuickAccessToolBar == null)
            {
                ribbon.QuickAccessToolBar = new RibbonQuickAccessToolBar();
            }

            LoadQatItems(ribbon.QuickAccessToolBar);
        }

        internal void UserControl_Unloaded(object sender, RoutedEventArgs e)
        {
            SaveQatItems(ribbon.QuickAccessToolBar);
        }

        private void LoadQatItems(RibbonQuickAccessToolBar qat)
        {
            if (qat != null)
            {
                if (File.Exists(_qatFileName))
                {
                    XmlReader xmlReader = XmlReader.Create(_qatFileName);
                    QatItemCollection qatItems = (QatItemCollection)XamlReader.Load(xmlReader);
                    xmlReader.Close();
                    if (qatItems != null)
                    {
                        int remainingItemsCount = qatItems.Count;
                        QatItemCollection matchedItems = new QatItemCollection();

                        if (qatItems.Count > 0)
                        {
                            for (int tabIndex = 0; tabIndex < ribbon.Items.Count && remainingItemsCount > 0; tabIndex++)
                            {
                                matchedItems.Clear();
                                for (int qatIndex = 0; qatIndex < qatItems.Count; qatIndex++)
                                {
                                    QatItem qatItem = qatItems[qatIndex];
                                    if (qatItem.ControlIndices[0] == tabIndex)
                                    {
                                        matchedItems.Add(qatItem);
                                    }
                                }

                                RibbonTab tab = ribbon.Items[tabIndex] as RibbonTab;
                                if (tab != null)
                                {
                                    LoadQatItemsAmongChildren(matchedItems, 0, tabIndex, tab, ref remainingItemsCount);
                                }
                            }

                            for (int qatIndex = 0; qatIndex < qatItems.Count; qatIndex++)
                            {
                                QatItem qatItem = qatItems[qatIndex];
                                Control control = qatItem.Instance as Control;
                                if (control != null)
                                {
                                    if (RibbonCommands.AddToQuickAccessToolBarCommand.CanExecute(null, control))
                                    {
                                        RibbonCommands.AddToQuickAccessToolBarCommand.Execute(null, control);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        private void LoadQatItemsAmongChildren(
                    QatItemCollection previouslyMatchedItems,
                    int matchLevel,
                    int controlIndex,
                    object parent,
                    ref int remainingItemsCount)
        {
            if (previouslyMatchedItems.Count == 0)
            {
                return;
            }
            if (IsLeaf(parent))
            {
                return;
            }

            int childIndex = 0;
            DependencyObject dependencyObject = parent as DependencyObject;
            if (dependencyObject != null)
            {
                IEnumerable children = LogicalTreeHelper.GetChildren(dependencyObject);
                foreach (object child in children)
                {
                    if (remainingItemsCount == 0)
                    {
                        break;
                    }

                    QatItemCollection matchedItems = new QatItemCollection();
                    LoadQatItemIfMatchesControl(previouslyMatchedItems, matchedItems, matchLevel + 1, childIndex, child, ref remainingItemsCount);
                    LoadQatItemsAmongChildren(matchedItems, matchLevel + 1, childIndex, child, ref remainingItemsCount);
                    childIndex++;
                }
            }
            if (childIndex != 0)
            {
                return;
            }

            // if we failed to get any logical children, enumerate the visual ones
            Visual visual = parent as Visual;
            if (visual == null)
            {
                return;
            }
            for (childIndex = 0; childIndex < VisualTreeHelper.GetChildrenCount(visual); childIndex++)
            {
                if (remainingItemsCount == 0)
                {
                    break;
                }

                Visual child = VisualTreeHelper.GetChild(visual, childIndex) as Visual;
                QatItemCollection matchedItems = new QatItemCollection();
                LoadQatItemIfMatchesControl(previouslyMatchedItems, matchedItems, matchLevel + 1, childIndex, child, ref remainingItemsCount);
                LoadQatItemsAmongChildren(matchedItems, matchLevel + 1, childIndex, child, ref remainingItemsCount);
            }
        }

        private void LoadQatItemIfMatchesControl(
                    QatItemCollection previouslyMatchedItems,
                    QatItemCollection matchedItems,
                    int matchLevel,
                    int controlIndex,
                    object control,
                    ref int remainingItemsCount)
        {
            for (int qatIndex = 0; qatIndex < previouslyMatchedItems.Count; qatIndex++)
            {
                QatItem qatItem = previouslyMatchedItems[qatIndex];
                if (qatItem.ControlIndices[matchLevel] == controlIndex)
                {
                    if (qatItem.ControlIndices.Count == matchLevel + 1)
                    {
                        qatItem.Instance = control;
                        remainingItemsCount--;
                    }
                    else if (qatItem.ControlIndices.Count == matchLevel + 2 && qatItem.ControlIndices[matchLevel + 1] == -1)
                    {
                        qatItem.IsSplitHeader = true;
                        Control element = control as Control;
                        if (element != null)
                        {
                            object splitControl = element.Template.FindName("PART_HeaderButton", element);
                            if (splitControl == null)
                            {
                                element.ApplyTemplate();
                                splitControl = element.Template.FindName("PART_HeaderButton", element);
                            }
                            if (splitControl != null)
                            {
                                qatItem.Instance = splitControl;
                            }
                        }
                        remainingItemsCount--;
                    }
                    else
                    {
                        matchedItems.Add(qatItem);
                    }
                }
            }
        }

        private void SaveQatItems(RibbonQuickAccessToolBar qat)
        {
            if (qat != null)
            {
                QatItemCollection qatItems = new QatItemCollection();
                QatItemCollection remainingItems = new QatItemCollection();

                if (qat.Items.Count > 0)
                {
                    for (int qatIndex = 0; qatIndex < qat.Items.Count; qatIndex++)
                    {
                        object instance = qat.Items[qatIndex];
                        bool isSplitHeader = false;

                        if (instance is FrameworkElement)
                        {
                            FrameworkElement element = (FrameworkElement)instance;

                            if (((FrameworkElement)instance).DataContext != null)
                            {
                                instance = ((FrameworkElement)instance).DataContext;
                                isSplitHeader =
                                    (instance is SplitMenuItemData && element is ButtonBase) ||
                                    (instance is SplitButtonData && element is ButtonBase);
                            }
                        }

                        QatItem qatItem = new QatItem(instance, isSplitHeader);
                        qatItems.Add(qatItem);
                        remainingItems.Add(qatItem);
                    }

                    for (int tabIndex = 0; tabIndex < ribbon.Items.Count && remainingItems.Count > 0; tabIndex++)
                    {
                        RibbonTab tab = ribbon.Items[tabIndex] as RibbonTab;
                        SaveQatItemsAmongChildren(remainingItems, tab, tabIndex);
                    }
                }

                XmlWriter xmlWriter = XmlWriter.Create(_qatFileName);
                XamlWriter.Save(qatItems, xmlWriter);
                xmlWriter.Close();
            }
        }

        private bool IsLeaf(object element)
        {
            if ((element is RibbonButton) ||
                (element is RibbonToggleButton) ||
                (element is RibbonRadioButton) ||
                (element is RibbonCheckBox) ||
                (element is RibbonTextBox) ||
                (element is RibbonSeparator))
            {
                return true;
            }

            RibbonMenuItem menuItem = element as RibbonMenuItem;
            if (menuItem != null &&
                menuItem.Items.Count == 0)
            {
                return true;
            }

            return false;
        }

        private void SaveQatItemsAmongChildrenInner(QatItemCollection remainingItems, object parent)
        {
            SaveQatItemsIfMatchesControl(remainingItems, parent);
            if (IsLeaf(parent))
            {
                return;
            }

            int childIndex = 0;
            DependencyObject dependencyObject = parent as DependencyObject;
            if (dependencyObject != null)
            {
                IEnumerable children = LogicalTreeHelper.GetChildren(dependencyObject);
                foreach (object child in children)
                {
                    SaveQatItemsAmongChildren(remainingItems, child, childIndex);
                    childIndex++;
                }
            }
            if (childIndex != 0)
            {
                return;
            }

            // if we failed to get any logical children, enumerate the visual ones
            Visual visual = parent as Visual;
            if (visual == null)
            {
                return;
            }
            for (childIndex = 0; childIndex < VisualTreeHelper.GetChildrenCount(visual); childIndex++)
            {
                Visual child = VisualTreeHelper.GetChild(visual, childIndex) as Visual;
                SaveQatItemsAmongChildren(remainingItems, child, childIndex);
            }
        }

        private void SaveQatItemsAmongChildren(QatItemCollection remainingItems, object control, int controlIndex)
        {
            if (control == null)
            {
                return;
            }

            for (int qatIndex = 0; qatIndex < remainingItems.Count; qatIndex++)
            {
                QatItem qatItem = remainingItems[qatIndex];
                qatItem.ControlIndices.Add(controlIndex);
            }

            SaveQatItemsAmongChildrenInner(remainingItems, control);

            for (int qatIndex = 0; qatIndex < remainingItems.Count; qatIndex++)
            {
                QatItem qatItem = remainingItems[qatIndex];
                int tail = qatItem.ControlIndices.Count - 1;
                qatItem.ControlIndices.RemoveAt(tail);
            }
        }

        private bool SaveQatItemsIfMatchesControl(QatItemCollection remainingItems, object control)
        {
            bool matched = false;
            FrameworkElement element = control as FrameworkElement;
            if (element != null)
            {
                object data = element.DataContext;
                if (data != null)
                {
                    for (int qatIndex = 0; qatIndex < remainingItems.Count; qatIndex++)
                    {
                        QatItem qatItem = remainingItems[qatIndex];
                        if (qatItem.Instance == data)
                        {
                            if (qatItem.IsSplitHeader)
                            {
                                // This is the case of the Header of a SplitButton 
                                // or a SplitMenuItem added to the QAT. Note -1 is 
                                // the sentinel being used to indicate this case.

                                qatItem.ControlIndices.Add(-1);
                            }

                            remainingItems.Remove(qatItem);
                            qatIndex--;
                            matched = true;
                        }
                    }
                }
            }
            return matched;
        }

        #endregion

        #region Commands

        internal void ChangeFontSize(double? parameter)
        {
            _previousFontSize = 0;
            if (parameter.HasValue)
            {
                // RTB.FontSize = parameter.Value;
            }
        }

        internal bool CanChangeFontSize(double? parameter)
        {
            return true;
        }

        internal void PreviewFontSize(double? parameter)
        {
            // _previousFontSize = RTB.FontSize;
            // if (parameter.HasValue)
            // {
            //     RTB.FontSize = parameter.Value;
            // }
        }

        internal void CancelPreviewFontSize()
        {
            if (_previousFontSize > 0)
            {
                // RTB.FontSize = _previousFontSize;
                _previousFontSize = 0;
            }
        }

        internal void ChangeFontFace(FontFamily parameter)
        {
            _previousFontFamily = null;
            if (parameter != null)
            {
                // RTB.FontFamily = parameter;
            }
        }

        internal bool CanChangeFontFace(FontFamily parameter)
        {
            return true;
        }

        internal void PreviewFontFace(FontFamily parameter)
        {
            // _previousFontFamily = RTB.FontFamily;
            // if (parameter != null)
            // {
            //     RTB.FontFamily = parameter;
            // }
        }

        internal void CancelPreviewFontFace()
        {
            if (_previousFontFamily != null)
            {
                // RTB.FontFamily = _previousFontFamily;
                _previousFontFamily = null;
            }
        }

        internal void ChangeTextHighlightColor(Brush parameter)
        {
            _previousTextHighlightBrush = null;
            if (parameter != null)
            {
                // RTB.Selection.ApplyPropertyValue(TextElement.BackgroundProperty, parameter);
            }
        }

        internal bool CanChangeTextHighlightColor(Brush parameter)
        {
            return true;
        }

        internal void PreviewTextHighlightColor(Brush parameter)
        {
            // _previousTextHighlightBrush = (Brush)RTB.Selection.GetPropertyValue(TextElement.BackgroundProperty);
            // if (parameter != null)
            // {
            //     RTB.Selection.ApplyPropertyValue(TextElement.BackgroundProperty, parameter);
            // }
        }

        internal void CancelPreviewTextHighlightColor()
        {
            if (_previousTextHighlightBrush != null)
            {
                // RTB.Selection.ApplyPropertyValue(TextElement.BackgroundProperty, _previousTextHighlightBrush);
                _previousTextHighlightBrush = null;
            }
        }

        internal void ChangeFontColor(Brush parameter)
        {
            _previousFontBrush = null;
            if (parameter != null)
            {
                // RTB.Selection.ApplyPropertyValue(TextElement.ForegroundProperty, parameter);
            }
        }

        internal bool CanChangeFontColor(Brush parameter)
        {
            return true;
        }

        internal void PreviewFontColor(Brush parameter)
        {
            // _previousFontBrush = (Brush)RTB.Selection.GetPropertyValue(TextElement.ForegroundProperty);
            // if (parameter != null)
            // {
            //     RTB.Selection.ApplyPropertyValue(TextElement.ForegroundProperty, parameter);
            // }
        }

        internal void CancelPreviewFontColor()
        {
            if (_previousFontBrush != null)
            {
                // RTB.Selection.ApplyPropertyValue(TextElement.ForegroundProperty, _previousFontBrush);
                _previousFontBrush = null;
            }
        }

        internal void InsertTable()
        {
            _previousTable = null;

            // Activate the Table Tools contextual tab.
            WordModel.TableTabGroup.IsVisible = true;
            
            // Select first tab of contextual tab group.
            WordModel.TableTabGroup.TabDataCollection[0].IsSelected = true;
        }

        internal bool CanInsertTable()
        {
            return true;
        }

        internal void PreviewInsertTable(RowColumnCount parameter)
        {
            if (parameter != null)
            {
                _previousTable = CreateTable(parameter.RowCount, parameter.ColumnCount);
                // flowDoc.Blocks.Add(_previousTable);
            }
        }

        internal void CancelPreviewInsertTable()
        {
            if (_previousTable != null)
            {
                // flowDoc.Blocks.Remove(_previousTable);
                _previousTable = null;
            }
        }

        private void RTB_SelectionChanged(object sender, RoutedEventArgs e)
        {
            // TextPointer start = RTB.Selection.Start;
            // if (start != null)
            // {
            //     // Activate/Deactivate the Table Tools contextual tab.
            //     // A quick way to know if caret is in a Table, 
            //     // since we know that Tables are added directly to the FlowDocument and wont be in a Paragraph
            //     // Note: We dont want to change Tab selection here.
            //     WordModel.TableTabGroup.IsVisible = start.Paragraph == null; 
            // }
        }

        Table CreateTable(int rows, int columns)
        {
            Table _table = new Table();
            _table.CellSpacing = 0;
            _table.BorderBrush = Brushes.Black;
            _table.BorderThickness = new Thickness(1,1,0,0);

            _table.RowGroups.Add(new TableRowGroup());
            TableRow currentRow = null;
            for (int i = 0; i < rows; i++)
            {
                currentRow = new TableRow();
                _table.RowGroups[0].Rows.Add(currentRow);
                for (int j = 0; j < columns; j++)
                {
                    TableCell cell = new TableCell() { BorderThickness = new Thickness(0, 0, 1, 1), BorderBrush = Brushes.Black };
                    currentRow.Cells.Add(cell);
                }
            }
            return _table;
        }

        #endregion Commands

        #region Data

        private FontFamily _previousFontFamily = null;
        private double _previousFontSize = 0;
        private Brush _previousTextHighlightBrush = null;
        private Brush _previousFontBrush = null;
        private Table _previousTable = null;
        private const string _qatFileName = "UserControlWord_QAT.xml";

        #endregion Data
    }
}
