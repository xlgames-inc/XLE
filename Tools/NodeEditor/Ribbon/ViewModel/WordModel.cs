using System;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using RibbonSamples.Common;
using System.ComponentModel;
using System.Collections.ObjectModel;

namespace NodeEditorRibbon.ViewModel
{
    public static class WordModel
    {
        #region Application Menu

        public static ControlData New
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_New";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/NewDocument_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Open
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Open";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "Open (Ctrl+O)";

                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Open_16x16.png", UriKind.Relative),
                            ToolTipTitle = TooTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Save
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Save";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "Save (Ctrl+S)";

                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Save_16x16.png", UriKind.Relative),
                            ToolTipTitle = TooTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData SaveAs
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Save_As";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "SaveAs (F12)";

                        ApplicationSplitMenuItemData splitMenuItemData = new ApplicationSplitMenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Save_16x16.png", UriKind.Relative),
                            ToolTipTitle = TooTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                        _dataCollection[Str] = splitMenuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData SaveAsWordDocument
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Word Document";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "Save the file as a Word Document.";

                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Document_16x16.png", UriKind.Relative),
                            ToolTipTitle = TooTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "W",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData SaveAsWordTemplate
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Word _Template";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "Save the docuemt as a template that can be used to format future documents.";

                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Document_16x16.png", UriKind.Relative),
                            ToolTipTitle = TooTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "T",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData SaveAsWord97To2003Document
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Word _97-2003 Document";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "Save a copy of the document that is fully compatible with Word 97-2003.";

                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Document_16x16.png", UriKind.Relative),
                            ToolTipTitle = TooTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "9",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData SaveAsOpenDocumentText
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Open_Document Text";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "Save the document in the Open Document Format.";

                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Document_16x16.png", UriKind.Relative),
                            ToolTipTitle = TooTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "D",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData SaveAsPdfOrXps
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_PDF or XPS";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "Publish a copy of the document as a PDF or XPS file.";

                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Document_16x16.png", UriKind.Relative),
                            ToolTipTitle = TooTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "P",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData SaveAsOtherFormat
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Other Formats";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "Open the Save As dialog box to select from all possible file types.";

                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Save_16x16.png", UriKind.Relative),
                            ToolTipTitle = TooTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "O",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Print
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Print";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "Print (Ctrl+P)";

                        ApplicationSplitMenuItemData splitMenuItemData = new ApplicationSplitMenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Print_16x16.png", UriKind.Relative),
                            ToolTipTitle = TooTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                        _dataCollection[Str] = splitMenuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData PrintOptions
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Print Options";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "Select a printer, number of copies, and other printing options before printing.";

                        ApplicationSplitMenuItemData splitMenuItemData = new ApplicationSplitMenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Print_16x16.png", UriKind.Relative),
                            ToolTipTitle = TooTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "P",
                        };
                        _dataCollection[Str] = splitMenuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData QuickPrint
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Quick Print";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Send the document directly to the default printer without any changes.";

                        ApplicationMenuItemData menuItemData = new ApplicationMenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Print_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "Q",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData PrintPreview
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Print Pre_view";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Preview and make changes to the pages before printing.";

                        ApplicationMenuItemData menuItemData = new ApplicationMenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/PrintPreview_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "V",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Prepare
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Pr_epare";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        ApplicationMenuItemData menuItemData = new ApplicationMenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/NewDocument_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Properties
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Properties";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "View and edit document properties, such as Title, Author and Keywords.";

                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/NewDocument_16x16.png", UriKind.Relative),
                            ToolTipTitle = TooTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "P",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData InspectDocument
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Inspect Document";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "Check the document for hidden metadata or personal information.";

                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/NewDocument_16x16.png", UriKind.Relative),
                            ToolTipTitle = TooTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "I",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData EncryptDocument
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Encrypt Document";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "Increase the security of the document by adding encryption.";

                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/NewDocument_16x16.png", UriKind.Relative),
                            ToolTipTitle = TooTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "E",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData RestrictPermission
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Restrict Permission";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "Grant people access while restricting their ability to edit, copy and print.";

                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/NewPermission_32x32.png", UriKind.Relative),
                            ToolTipTitle = TooTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "R",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData UnrestrictedAccess
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Unrestricted Access";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            IsCheckable = true,
                            IsChecked = true,
                            KeyTip = "U",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData RestrictedAccess
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Restricted Access";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            IsCheckable = true,
                            KeyTip = "R",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData DoNotReplyAll
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Do Not Reply All";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            IsCheckable = true,
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData MicrosoftAllAllRights
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Microsoft All - All Rights";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            IsCheckable = true,
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData MicrosoftAllAllRightsExceptCopyAndPrint
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Microsoft All - All Rights Except Copy and Print";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            IsCheckable = true,
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData MicrosoftAllReadOnly
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Microsoft All - Read Only";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            IsCheckable = true,
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData MicrosoftFteAllRightsExceptCopyAndPrint
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Microsoft FTE - All Rights Except Copy and Print";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            IsCheckable = true,
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData MicrosoftFteReadOnly
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Microsoft FTE - Read Only";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            IsCheckable = true,
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData ManageCredentials
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Manage Credentials";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            IsCheckable = true,
                            KeyTip = "M",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData AddADigitalSignature
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Add a Digital _Signature";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "Ensure the integrity of the document by adding an invisible signature.";

                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/NewDocument_16x16.png", UriKind.Relative),
                            ToolTipTitle = TooTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "S",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData MarkAsFinal
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Mark as _Final";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "Let the readers know the document is final and make it read-only.";

                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/NewDocument_16x16.png", UriKind.Relative),
                            ToolTipTitle = TooTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "F",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData RunCompatibilityChecker
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Run _Compatibility Checker";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "Check for features not supported by earlier versions of Word.";

                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/NewDocument_16x16.png", UriKind.Relative),
                            ToolTipTitle = TooTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "C",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Send
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Sen_d";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        ApplicationMenuItemData menuItemData = new ApplicationMenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/SendLinkByEmail_32x32.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Email
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_E-mail";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Send a copy of the document in an email as an attachment.";

                        ApplicationMenuItemData menuItemData = new ApplicationMenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/SendLinkByEmail_32x32.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "E",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData EmailAsPdfAttachment
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "E-mail as _PDF Attachment";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Send a copy of the document in a message as a PDF attachment.";

                        ApplicationMenuItemData menuItemData = new ApplicationMenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/SendLinkByEmail_32x32.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "P",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData EmailAsXpsAttachment
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "E-mail as XP_S Attachment";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Send a copy of the document in a message as a XPS attachment.";

                        ApplicationMenuItemData menuItemData = new ApplicationMenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/SendLinkByEmail_32x32.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "S",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData InternetFax
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Internet Fa_x";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Use an Internet fax service to fax the document.";

                        ApplicationMenuItemData menuItemData = new ApplicationMenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Printer_48x48.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "X",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Publish
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "P_ublish";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        ApplicationMenuItemData menuItemData = new ApplicationMenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/PublishPlan_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Blog
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Blog";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Create a new blog post with the content of the document.";

                        ApplicationMenuItemData menuItemData = new ApplicationMenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/ConnectionManager_48x48.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "B",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData DocumentManagementServer
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Document Management Server";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Share the document by saving it to a document management server.";

                        ApplicationMenuItemData menuItemData = new ApplicationMenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/ConnectionManager_48x48.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "D",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData CreateDocumentWorkspace
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Create Document Workspace";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Create a new site for the document and keep the local copy synchronized.";

                        ApplicationMenuItemData menuItemData = new ApplicationMenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/ConnectionManager_48x48.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "C",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Workflows
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Wor_kflows";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        ApplicationMenuItemData menuItemData = new ApplicationMenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/NewDocument_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Close
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Close";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        ApplicationMenuItemData menuItemData = new ApplicationMenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/FolderClose_48x48.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData RecentDocuments
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Recent Documents";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        GalleryCategoryData<RecentDocumentData> galleryCategoryData = new GalleryCategoryData<RecentDocumentData>();

                        for (int i = 0; i < 6; i++)
                        {
                            RecentDocumentData recentDocumentData = new RecentDocumentData()
                            {
                                Index = i+1,
                                Label = "Recent Doc " + i,
                            };
                            galleryCategoryData.GalleryItemDataCollection.Add(recentDocumentData);
                        }

                        _dataCollection[Str] = galleryCategoryData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData WordOptions
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Word Options";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        ButtonData buttonData = new ButtonData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Options_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "I",
                        };
                        _dataCollection[Str] = buttonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData ExitWord
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Exit Word";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        ButtonData buttonData = new ButtonData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Close_16x16.png", UriKind.Relative),
                            Command = ApplicationCommands.Close,
                            KeyTip = "X",
                        };
                        _dataCollection[Str] = buttonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        #endregion Application Menu


        #region Help Button

        public static ControlData Help
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Help";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        ButtonData Data = new ButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Help_16x16.png", UriKind.Relative),
                            ToolTipTitle = "Help (F1)",
                            ToolTipDescription = "Microsoft Ribbon for WPF",
                        };
                        _dataCollection[Str] = Data;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        #endregion
        public static ControlData Clipboard
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Clipboard";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        GroupData Data = new GroupData(Str)
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Paste_16x16.png", UriKind.Relative),
                            LargeImage = new Uri("/NodeEditorRibbon;component/Images/Paste_32x32.png", UriKind.Relative),
                            KeyTip = "ZC",
                        };
                        _dataCollection[Str] = Data;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        #region Font Group Model

        public static ControlData Font
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Font";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        GroupData Data = new GroupData(Str)
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Font_16x16.png", UriKind.Relative),
                            LargeImage = new Uri("/NodeEditorRibbon;component/Images/Font_32x32.png", UriKind.Relative),
                            KeyTip = "ZF",
                        };
                        _dataCollection[Str] = Data;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Paste
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Paste";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "Paste (Ctrl+V)";
                        string ToolTipDescription = "Paste the contents of the Clipboard.";
                        string DropDownToolTipDescription = "Click here for more options such as pasting only the values or formatting.";

                        SplitButtonData splitButtonData = new SplitButtonData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Paste_16x16.png", UriKind.Relative),
                            LargeImage = new Uri("/NodeEditorRibbon;component/Images/Paste_32x32.png", UriKind.Relative),
                            ToolTipTitle = TooTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = ApplicationCommands.Paste,
                            KeyTip = "V",
                        };
                        splitButtonData.DropDownButtonData.ToolTipTitle = TooTipTitle;
                        splitButtonData.DropDownButtonData.ToolTipDescription = DropDownToolTipDescription;
                        splitButtonData.DropDownButtonData.Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute);
                        splitButtonData.DropDownButtonData.KeyTip = "V";
                        _dataCollection[Str] = splitButtonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData PasteSpecial
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Paste _Special";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "Paste Special (Alt+Ctrl+V)";

                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Paste_16x16.png", UriKind.Relative),
                            ToolTipTitle = TooTipTitle,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData PasteAsHyperlink
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Paste As _Hyperlink";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Paste_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Cut
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Cut";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string CutToolTipTitle = "Cut (Ctrl+X)";
                        string CutToolTipDescription = "Cut the selection and put it on the Clipboard.";

                        ButtonData buttonData = new ButtonData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Cut_16x16.png", UriKind.Relative),
                            ToolTipTitle = CutToolTipTitle,
                            ToolTipDescription = CutToolTipDescription,
                            Command = ApplicationCommands.Cut,
                            KeyTip = "X",
                        };
                        _dataCollection[Str] = buttonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Copy
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Copy";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Copy (Ctrl+C)";
                        string ToolTipDescription = "Copy selection and put it on the Clipboard.";

                        ButtonData buttonData = new ButtonData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Copy_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = ApplicationCommands.Copy,
                            KeyTip = "C",
                        };
                        _dataCollection[Str] = buttonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData FormatPainter
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Format Painter";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Format Painter (Ctrl+Shift+C)";
                        string ToolTipDescription = "Copy the formatting from one place and apply it to another. \n\n Double click this button to apply the same formatting to multiple places in the document.";

                        ButtonData buttonData = new ButtonData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/FormatPainter_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            ToolTipFooterTitle = HelpFooterTitle,
                            ToolTipFooterImage = new Uri("/NodeEditorRibbon;component/Images/Help_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "FP",
                        };
                        _dataCollection[Str] = buttonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData FontFace
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Font Face";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Font (Ctrl+Shift+F)";
                        string ToolTipDescription = "Change the font face.";

                        ComboBoxData comboBoxData = new ComboBoxData()
                        {
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            KeyTip = "FF",
                        };

                        _dataCollection[Str] = comboBoxData;
                    }
                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData FontFaceGallery
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Font Face Gallery";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        GalleryData<FontFamily> galleryData = new GalleryData<FontFamily>()
                        {
                            SelectedItem = SystemFonts.MessageFontFamily,
                        };

                        GalleryCategoryData<FontFamily> recentlyUsedCategoryData = new GalleryCategoryData<FontFamily>()
                        {
                            Label = "Recently Used Fonts"
                        };

                        galleryData.CategoryDataCollection.Add(recentlyUsedCategoryData);

                        GalleryCategoryData<FontFamily> allFontsCategoryData = new GalleryCategoryData<FontFamily>()
                        {
                            Label = "All Fonts"
                        };

                        foreach (FontFamily fontFamily in System.Windows.Media.Fonts.SystemFontFamilies)
                        {
                            allFontsCategoryData.GalleryItemDataCollection.Add(fontFamily);
                        }

                        galleryData.CategoryDataCollection.Add(allFontsCategoryData);

                        Action<FontFamily> ChangeFontFace = delegate(FontFamily parameter)
                        {
                            UserControlWord wordControl = WordControl;
                            if (wordControl != null)
                            {
                                wordControl.ChangeFontFace(parameter);

                                if (!recentlyUsedCategoryData.GalleryItemDataCollection.Contains(parameter))
                                {
                                    recentlyUsedCategoryData.GalleryItemDataCollection.Add(parameter);
                                }
                            }
                        };

                        Func<FontFamily, bool> CanChangeFontFace = delegate(FontFamily parameter)
                        {
                            UserControlWord wordControl = WordControl;
                            if (wordControl != null)
                            {
                                return wordControl.CanChangeFontFace(parameter);
                            }

                            return false;
                        };

                        Action<FontFamily> PreviewFontFace = delegate(FontFamily parameter)
                        {
                            UserControlWord wordControl = WordControl;
                            if (wordControl != null)
                            {
                                wordControl.PreviewFontFace(parameter);
                            }
                        };

                        Action CancelPreviewFontFace = delegate()
                        {
                            UserControlWord wordControl = WordControl;
                            if (wordControl != null)
                            {
                                wordControl.CancelPreviewFontFace();
                            }
                        };

                        galleryData.Command = new PreviewDelegateCommand<FontFamily>(ChangeFontFace, CanChangeFontFace, PreviewFontFace, CancelPreviewFontFace);

                        _dataCollection[Str] = galleryData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData FontSize
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Font Size";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Font Size (Ctrl+Shift+P)";
                        string ToolTipDescription = "Change the font size.";

                        ComboBoxData comboBoxData = new ComboBoxData()
                        {
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            KeyTip = "FS",
                        };

                        _dataCollection[Str] = comboBoxData;
                    }
                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData FontSizeGallery
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Font Size Gallery";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        Action<double?> ChangeFontSize = delegate(double? parameter)
                        {
                            UserControlWord wordControl = WordControl;
                            if (wordControl != null)
                            {
                                wordControl.ChangeFontSize(parameter);
                            }
                        };

                        Func<double?, bool> CanChangeFontSize = delegate(double? parameter)
                        {
                            UserControlWord wordControl = WordControl;
                            if (wordControl != null)
                            {
                                return wordControl.CanChangeFontSize(parameter);
                            }

                            return false;
                        };

                        Action<double?> PreviewFontSize = delegate(double? parameter)
                        {
                            UserControlWord wordControl = WordControl;
                            if (wordControl != null)
                            {
                                wordControl.PreviewFontSize(parameter);
                            }
                        };

                        Action CancelPreviewFontSize = delegate()
                        {
                            UserControlWord wordControl = WordControl;
                            if (wordControl != null)
                            {
                                wordControl.CancelPreviewFontSize();
                            }
                        };

                        GalleryData<double?> galleryData = new GalleryData<double?>()
                        {
                            Command = new PreviewDelegateCommand<double?>(ChangeFontSize, CanChangeFontSize, PreviewFontSize, CancelPreviewFontSize),
                            SelectedItem = 11,
                        };

                        GalleryCategoryData<double?> galleryCategoryData = new GalleryCategoryData<double?>();
                        galleryCategoryData.GalleryItemDataCollection.Add(8);
                        galleryCategoryData.GalleryItemDataCollection.Add(9);
                        galleryCategoryData.GalleryItemDataCollection.Add(10);
                        galleryCategoryData.GalleryItemDataCollection.Add(11);
                        galleryCategoryData.GalleryItemDataCollection.Add(12);
                        galleryCategoryData.GalleryItemDataCollection.Add(14);
                        galleryCategoryData.GalleryItemDataCollection.Add(16);
                        galleryCategoryData.GalleryItemDataCollection.Add(18);
                        galleryCategoryData.GalleryItemDataCollection.Add(20);
                        galleryCategoryData.GalleryItemDataCollection.Add(22);
                        galleryCategoryData.GalleryItemDataCollection.Add(24);
                        galleryCategoryData.GalleryItemDataCollection.Add(28);
                        galleryCategoryData.GalleryItemDataCollection.Add(36);
                        galleryCategoryData.GalleryItemDataCollection.Add(48);
                        galleryCategoryData.GalleryItemDataCollection.Add(72);

                        galleryData.CategoryDataCollection.Add(galleryCategoryData);
                        _dataCollection[Str] = galleryData;

                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData GrowFont
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Grow Font";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Grow Font (Ctrl+>)";
                        string ToolTipDescription = "Increase the font size.";

                        ButtonData buttonData = new ButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Font_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = EditingCommands.IncreaseFontSize,
                            KeyTip = "FG",
                        };
                        _dataCollection[Str] = buttonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData ShrinkFont
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Shrink Font";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Shrink Font (Ctrl+<)";
                        string ToolTipDescription = "Decrease the font size.";

                        ButtonData buttonData = new ButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Font_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = EditingCommands.DecreaseFontSize,
                            KeyTip = "FK",
                        };
                        _dataCollection[Str] = buttonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData ClearFormatting
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Clear Formatting";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Clear Formatting";
                        string ToolTipDescription = "Clear all the formatting from the selection, leaving only the plain text.";

                        ButtonData buttonData = new ButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/ClearFormatting_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            ToolTipFooterTitle = HelpFooterTitle,
                            ToolTipFooterImage = new Uri("/NodeEditorRibbon;component/Images/Help_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "E",
                        };
                        _dataCollection[Str] = buttonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Bold
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Bold";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Bold (Ctrl+B)";
                        string ToolTipDescription = "Make the selected text bold.";

                        ToggleButtonData toggleButtonData = new ToggleButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Bold_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = EditingCommands.ToggleBold,
                            KeyTip = "1",
                        };
                        _dataCollection[Str] = toggleButtonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Italic
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Italic";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Italic (Ctrl+I)";
                        string ToolTipDescription = "Italicize the selected text.";

                        ToggleButtonData toggleButtonData = new ToggleButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Italic_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = EditingCommands.ToggleItalic,
                            KeyTip = "2",
                        };
                        _dataCollection[Str] = toggleButtonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Underline
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Underline";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Underline (Ctrl+U)";
                        string ToolTipDescription = "Underline the selected text.";

                        SplitButtonData splitButtonData = new SplitButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/LineColor_16x16.png", UriKind.Relative),
                            Command = EditingCommands.ToggleUnderline,
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            KeyTip = "3",
                        };

                        _dataCollection[Str] = splitButtonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData UnderlineGallery
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Underline Gallery";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        GalleryData<string> galleryData = new GalleryData<string>()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/LineColor_16x16.png", UriKind.Relative),
                            Command = EditingCommands.ToggleUnderline,
                        };

                        GalleryCategoryData<string> galleryCategoryData = new GalleryCategoryData<string>();
                        galleryCategoryData.GalleryItemDataCollection.Add("Underline");
                        galleryCategoryData.GalleryItemDataCollection.Add("Double underline");
                        galleryCategoryData.GalleryItemDataCollection.Add("Thick underline");
                        galleryCategoryData.GalleryItemDataCollection.Add("Dotted underline");
                        galleryCategoryData.GalleryItemDataCollection.Add("Dashed underline");
                        galleryCategoryData.GalleryItemDataCollection.Add("Dot-dash underline");
                        galleryCategoryData.GalleryItemDataCollection.Add("Dot-dot-dash underline");
                        galleryCategoryData.GalleryItemDataCollection.Add("Wave underline");
                        galleryData.CategoryDataCollection.Add(galleryCategoryData);

                        _dataCollection[Str] = galleryData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData MoreUnderlines
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_More Underlines";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Color_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Strikethrough
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Strikethrough";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Strikethrough";
                        string ToolTipDescription = "Draw a line through the middle of the selected text.";

                        ToggleButtonData toggleButtonData = new ToggleButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Erase_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "4",
                        };
                        _dataCollection[Str] = toggleButtonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Subscript
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Subscript";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Subscript (Ctrl+=)";
                        string ToolTipDescription = "Create small letters below the test baseline.";

                        ToggleButtonData toggleButtonData = new ToggleButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/FontScript_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = EditingCommands.ToggleSubscript,
                            KeyTip = "5",
                        };
                        _dataCollection[Str] = toggleButtonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Superscript
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Superscript";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Superscript (Ctrl+Shift++)";
                        string ToolTipDescription = "Create small letters above the line of text. \n\n To create a footnote, click Insert Footnote on References tab.";

                        ToggleButtonData toggleButtonData = new ToggleButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/FontScript_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = EditingCommands.ToggleSuperscript,
                            KeyTip = "6",
                        };
                        _dataCollection[Str] = toggleButtonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData ChangeCase
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Change Case";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Change Case";
                        string ToolTipDescription = "Change all the selected text to UPPERCASE, lowercase or other common capitalizations.";

                        _dataCollection[Str] = new MenuButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/FontScript_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            ToolTipFooterTitle = HelpFooterTitle,
                            ToolTipFooterImage = new Uri("/NodeEditorRibbon;component/Images/Help_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "7",
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData SentenceCase
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Sentence case.";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Default_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Uppercase
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_UPPERCASE";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Default_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Lowercase
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_lowercase";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Default_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData CapitalizeEachWord
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Capitalize Each Word";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Default_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData ToggleCase
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_tOGGLE cASE";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Default_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData TextHighlightColor
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Text Highlight Color";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Text Highlight Color";
                        string ToolTipDescription = "Make the text look like it was marked with a highlighter pen.";

                        SplitMenuItemData splitMenuItemData = new SplitMenuItemData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Highlight_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = new PreviewDelegateCommand<Brush>(ChangeTextHighlightColor, CanChangeTextHighlightColor, PreviewTextHighlightColor, CancelPreviewTextHighlightColor),
                            KeyTip = "I",
                        };

                        _dataCollection[Str] = splitMenuItemData;
                    }

                    return _dataCollection[Str];

                }
            }
        }

        public static ControlData TextHighlightColorGallery
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Text Highlight Color Gallery";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        GalleryData<Brush> galleryData = new GalleryData<Brush>()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Highlight_16x16.png", UriKind.Relative),
                            Command = new PreviewDelegateCommand<Brush>(ChangeTextHighlightColor, CanChangeTextHighlightColor, PreviewTextHighlightColor, CancelPreviewTextHighlightColor),
                            SelectedItem = SystemColors.ControlBrush,
                        };

                        GalleryCategoryData<Brush> galleryCategoryData = new GalleryCategoryData<Brush>();
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Yellow);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Green);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Turquoise);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Pink);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Blue);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Red);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.DarkBlue);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Teal);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Green);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Violet);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.DarkRed);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.DarkOrange);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.DarkSeaGreen);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Aqua);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Gray);

                        galleryData.CategoryDataCollection.Add(galleryCategoryData);

                        galleryCategoryData = new GalleryCategoryData<Brush>()
                        {
                            Label = "No Color"
                        };
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Transparent);
                        galleryData.CategoryDataCollection.Add(galleryCategoryData);

                        _dataCollection[Str] = galleryData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        private static void ChangeTextHighlightColor(Brush parameter)
        {
            UserControlWord wordControl = WordControl;
            if (wordControl != null)
            {
                wordControl.ChangeTextHighlightColor(parameter);
            }
        }

        private static bool CanChangeTextHighlightColor(Brush parameter)
        {
            UserControlWord wordControl = WordControl;
            if (wordControl != null)
            {
                return wordControl.CanChangeTextHighlightColor(parameter);
            }

            return false;
        }

        private static void PreviewTextHighlightColor(Brush parameter)
        {
            UserControlWord wordControl = WordControl;
            if (wordControl != null)
            {
                wordControl.PreviewTextHighlightColor(parameter);
            }
        }

        private static void CancelPreviewTextHighlightColor()
        {
            UserControlWord wordControl = WordControl;
            if (wordControl != null)
            {
                wordControl.CancelPreviewTextHighlightColor();
            }
        }

        public static ControlData StopHighlighting
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Stop Highlighting";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Default_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData FontColor
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Font Color";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Font Color";
                        string ToolTipDescription = "Change the text color.";

                        SplitMenuItemData splitMenuItemData = new SplitMenuItemData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/FontColor_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            ToolTipFooterTitle = HelpFooterTitle,
                            ToolTipFooterImage = new Uri("/NodeEditorRibbon;component/Images/Help_16x16.png", UriKind.Relative),
                            Command = new PreviewDelegateCommand<Brush>(ChangeFontColor, CanChangeFontColor, PreviewFontColor, CancelPreviewFontColor),
                            KeyTip = "FC",
                        };

                        _dataCollection[Str] = splitMenuItemData;
                    }

                    return _dataCollection[Str];

                }
            }
        }

        public static ControlData FontColorGallery
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Font Color Gallery";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        GalleryData<Brush> galleryData = new GalleryData<Brush>()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/FontColor_16x16.png", UriKind.Relative),
                            Command = new PreviewDelegateCommand<Brush>(ChangeFontColor, CanChangeFontColor, PreviewFontColor, CancelPreviewFontColor),
                            SelectedItem = SystemColors.ControlBrush,
                        };

                        GalleryCategoryData<Brush> galleryCategoryData = new GalleryCategoryData<Brush>()
                        {
                            Label = "Automatic Color"
                        };
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Black);
                        galleryData.CategoryDataCollection.Add(galleryCategoryData);

                        galleryCategoryData = new GalleryCategoryData<Brush>()
                        {
                            Label = "Theme Colors"
                        };
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.White);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Black);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Tan);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.DarkBlue);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Blue);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Red);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Olive);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Purple);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Aqua);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Orange);

                        float percent10 = 0.90f;
                        float percent25 = 0.75f;
                        float percent40 = 0.60f;
                        float percent55 = 0.45f;
                        float percent70 = 0.30f;

                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.White.Color * percent10));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Black.Color * percent10));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Tan.Color * percent10));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.DarkBlue.Color * percent10));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Blue.Color * percent10));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Red.Color * percent10));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Olive.Color * percent10));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Purple.Color * percent10));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Aqua.Color * percent10));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Orange.Color * percent10));

                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.White.Color * percent25));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Black.Color * percent25));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Tan.Color * percent25));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.DarkBlue.Color * percent25));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Blue.Color * percent25));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Red.Color * percent25));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Olive.Color * percent25));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Purple.Color * percent25));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Aqua.Color * percent25));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Orange.Color * percent25));

                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.White.Color * percent40));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Black.Color * percent40));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Tan.Color * percent40));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.DarkBlue.Color * percent40));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Blue.Color * percent40));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Red.Color * percent40));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Olive.Color * percent40));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Purple.Color * percent40));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Aqua.Color * percent40));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Orange.Color * percent40));

                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.White.Color * percent55));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Black.Color * percent55));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Tan.Color * percent55));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.DarkBlue.Color * percent55));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Blue.Color * percent55));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Red.Color * percent55));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Olive.Color * percent55));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Purple.Color * percent55));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Aqua.Color * percent55));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Orange.Color * percent55));

                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.White.Color * percent70));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Black.Color * percent70));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Tan.Color * percent70));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.DarkBlue.Color * percent70));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Blue.Color * percent70));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Red.Color * percent70));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Olive.Color * percent70));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Purple.Color * percent70));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Aqua.Color * percent70));
                        galleryCategoryData.GalleryItemDataCollection.Add(new SolidColorBrush(Brushes.Orange.Color * percent70));

                        galleryData.CategoryDataCollection.Add(galleryCategoryData);

                        galleryCategoryData = new GalleryCategoryData<Brush>()
                        {
                            Label = "Standard Colors"
                        };
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.DarkRed);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Red);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Orange);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Yellow);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.LightGreen);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Green);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.LightBlue);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Blue);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.DarkBlue);
                        galleryCategoryData.GalleryItemDataCollection.Add(Brushes.Purple);

                        galleryData.CategoryDataCollection.Add(galleryCategoryData);

                        _dataCollection[Str] = galleryData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        private static void ChangeFontColor(Brush parameter)
        {
            UserControlWord wordControl = WordControl;
            if (wordControl != null)
            {
                wordControl.ChangeFontColor(parameter);
            }
        }

        private static bool CanChangeFontColor(Brush parameter)
        {
            UserControlWord wordControl = WordControl;
            if (wordControl != null)
            {
                return wordControl.CanChangeFontColor(parameter);
            }

            return false;
        }

        private static void PreviewFontColor(Brush parameter)
        {
            UserControlWord wordControl = WordControl;
            if (wordControl != null)
            {
                wordControl.PreviewFontColor(parameter);
            }
        }

        private static void CancelPreviewFontColor()
        {
            UserControlWord wordControl = WordControl;
            if (wordControl != null)
            {
                wordControl.CancelPreviewFontColor();
            }
        }

        public static ControlData MoreColors
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_More Colors";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Color_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        #endregion Font Group Model

        #region Paragraph Group Model

        public static ControlData Paragraph
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Paragraph";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        GroupData Data = new GroupData(Str)
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Paragraph_16x16.png", UriKind.Relative),
                            LargeImage = new Uri("/NodeEditorRibbon;component/Images/Paragraph_32x32.png", UriKind.Relative),
                            KeyTip = "ZP",
                        };
                        _dataCollection[Str] = Data;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Bullets
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Bullets";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Bullets";
                        string ToolTipDescription = "Start a bulleted list.\n\nClick the arrow to choose different bullet styles.";

                        SplitButtonData splitButtonData = new SplitButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Bullets_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            IsCheckable = true,
                            Command = EditingCommands.ToggleBullets,
                            IsVerticallyResizable = true,
                            IsHorizontallyResizable = true,
                            KeyTip = "U",
                        };
                        _dataCollection[Str] = splitButtonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData BulletsGallery
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Bullets Gallery";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        GalleryData<Uri> galleryData = new GalleryData<Uri>()
                        {
                        };

                        GalleryCategoryData<Uri> recentlyUsedCategoryData = new GalleryCategoryData<Uri>()
                        {
                            Label = "Recently Used Bullets"
                        };

                        galleryData.CategoryDataCollection.Add(recentlyUsedCategoryData);

                        GalleryCategoryData<Uri> bulletLibraryCategoryData = new GalleryCategoryData<Uri>()
                        {
                            Label = "Bullet Library"
                        };
                        bulletLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/DownArrow_32x32.png", UriKind.Relative));
                        bulletLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/LeftArrow_32x32.png", UriKind.Relative));
                        bulletLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/Minus_32x32.png", UriKind.Relative));
                        bulletLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/Plus_32x32.png", UriKind.Relative));
                        bulletLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/RefreshArrow_32x32.png", UriKind.Relative));
                        bulletLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/RightArrow_32x32.png", UriKind.Relative));
                        bulletLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/Tick_32x32.png", UriKind.Relative));
                        bulletLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/UpArrow_32x32.png", UriKind.Relative));

                        galleryData.CategoryDataCollection.Add(bulletLibraryCategoryData);

                        GalleryCategoryData<Uri> documentBulletsCategoryData = new GalleryCategoryData<Uri>()
                        {
                            Label = "Document Bullets"
                        };

                        documentBulletsCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/Tick_32x32.png", UriKind.Relative));

                        galleryData.CategoryDataCollection.Add(documentBulletsCategoryData);

                        Action<Uri> galleryCommandExecuted = delegate(Uri parameter)
                        {
                            if (!recentlyUsedCategoryData.GalleryItemDataCollection.Contains(parameter))
                            {
                                recentlyUsedCategoryData.GalleryItemDataCollection.Add(parameter);
                            }
                        };

                        Func<Uri, bool> galleryCommandCanExecute = delegate(Uri parameter)
                        {
                            return true;
                        };

                        galleryData.Command = new DelegateCommand<Uri>(galleryCommandExecuted, galleryCommandCanExecute);
                        _dataCollection[Str] = galleryData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData ChangeListLevel
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Change List Level";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/MultiLevelList_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData ChangeListLevelGallery
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "ChangeListLevelGallery";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        GalleryData<string> galleryData = new GalleryData<string>()
                        {
                        };

                        GalleryCategoryData<string> categoryData = new GalleryCategoryData<string>()
                        {
                        };

                        categoryData.GalleryItemDataCollection.Add("  > First");
                        categoryData.GalleryItemDataCollection.Add("    > Second");
                        categoryData.GalleryItemDataCollection.Add("      > Third");
                        categoryData.GalleryItemDataCollection.Add("        > Fourth");
                        categoryData.GalleryItemDataCollection.Add("          > Fifth");
                        categoryData.GalleryItemDataCollection.Add("            > Sixth");
                        categoryData.GalleryItemDataCollection.Add("              > Seventh");
                        categoryData.GalleryItemDataCollection.Add("                > Eighth");
                        categoryData.GalleryItemDataCollection.Add("                  > Ninth");

                        galleryData.CategoryDataCollection.Add(categoryData);

                        galleryData.Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute);
                        _dataCollection[Str] = galleryData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData DefaultNewBullet
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Default New Bullet...";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Default_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData DefaultNewNumberFormat
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Default New Number Format...";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Default_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData DefaultNewMultilevelList
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Default New Multilevel List...";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Default_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData DefaultNewListStyle
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Default New _List Style...";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Default_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Numbering
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Numbering";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Numbering";
                        string ToolTipDescription = "Start a numbered list.\n\nClick the arrow to choose different numbering formats.";

                        SplitButtonData splitButtonData = new SplitButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Numbering_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            IsCheckable = true,
                            Command = EditingCommands.ToggleNumbering,
                            IsVerticallyResizable = true,
                            IsHorizontallyResizable = true,
                            KeyTip = "N",
                        };
                        _dataCollection[Str] = splitButtonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData NumberingGallery
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Numbering Gallery";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        GalleryData<Uri> galleryData = new GalleryData<Uri>()
                        {
                        };

                        GalleryCategoryData<Uri> recentlyUsedCategoryData = new GalleryCategoryData<Uri>()
                        {
                            Label = "Recently Used Number Formats"
                        };

                        galleryData.CategoryDataCollection.Add(recentlyUsedCategoryData);

                        GalleryCategoryData<Uri> numberingLibraryCategoryData = new GalleryCategoryData<Uri>()
                        {
                            Label = "Numbering Library"
                        };
                        numberingLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/DownArrow_32x32.png", UriKind.Relative));
                        numberingLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/LeftArrow_32x32.png", UriKind.Relative));
                        numberingLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/Minus_32x32.png", UriKind.Relative));
                        numberingLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/Plus_32x32.png", UriKind.Relative));
                        numberingLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/RefreshArrow_32x32.png", UriKind.Relative));
                        numberingLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/RightArrow_32x32.png", UriKind.Relative));
                        numberingLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/Tick_32x32.png", UriKind.Relative));
                        numberingLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/UpArrow_32x32.png", UriKind.Relative));

                        galleryData.CategoryDataCollection.Add(numberingLibraryCategoryData);

                        GalleryCategoryData<Uri> documentNumberFormatsCategoryData = new GalleryCategoryData<Uri>()
                        {
                            Label = "Document Number Formats"
                        };

                        documentNumberFormatsCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/Tick_32x32.png", UriKind.Relative));

                        galleryData.CategoryDataCollection.Add(documentNumberFormatsCategoryData);

                        Action<Uri> galleryCommandExecuted = delegate(Uri parameter)
                        {
                            if (!recentlyUsedCategoryData.GalleryItemDataCollection.Contains(parameter))
                            {
                                recentlyUsedCategoryData.GalleryItemDataCollection.Add(parameter);
                            }
                        };

                        Func<Uri, bool> galleryCommandCanExecute = delegate(Uri parameter)
                        {
                            return true;
                        };

                        galleryData.Command = new DelegateCommand<Uri>(galleryCommandExecuted, galleryCommandCanExecute);
                        _dataCollection[Str] = galleryData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData MultilevelList
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "MultilevelList";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Multilevel List";
                        string ToolTipDescription = "Start a multilevel list.\n\nClick the arrow to choose different multilevel list styles.";

                        MenuButtonData menuButtonData = new MenuButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/MultiLevelList_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            IsVerticallyResizable = true,
                            IsHorizontallyResizable = true,
                            KeyTip = "M",
                        };
                        _dataCollection[Str] = menuButtonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData MultilevelListGallery
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "MultilevelList Gallery";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        GalleryData<Uri> galleryData = new GalleryData<Uri>()
                        {
                            CanUserFilter = true,
                        };

                        GalleryCategoryData<Uri> currentListCategoryData = new GalleryCategoryData<Uri>()
                        {
                            Label = "Current List"
                        };

                        galleryData.CategoryDataCollection.Add(currentListCategoryData);

                        GalleryCategoryData<Uri> listLibraryCategoryData = new GalleryCategoryData<Uri>()
                        {
                            Label = "List Library"
                        };
                        listLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/DownArrow_32x32.png", UriKind.Relative));
                        listLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/LeftArrow_32x32.png", UriKind.Relative));
                        listLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/Minus_32x32.png", UriKind.Relative));
                        listLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/Plus_32x32.png", UriKind.Relative));
                        listLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/RefreshArrow_32x32.png", UriKind.Relative));
                        listLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/RightArrow_32x32.png", UriKind.Relative));
                        listLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/Tick_32x32.png", UriKind.Relative));
                        listLibraryCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/UpArrow_32x32.png", UriKind.Relative));

                        galleryData.CategoryDataCollection.Add(listLibraryCategoryData);

                        GalleryCategoryData<Uri> documentListsCategoryData = new GalleryCategoryData<Uri>()
                        {
                            Label = "Lists in Current Documents"
                        };

                        documentListsCategoryData.GalleryItemDataCollection.Add(new Uri("/NodeEditorRibbon;component/Images/Tick_32x32.png", UriKind.Relative));

                        galleryData.CategoryDataCollection.Add(documentListsCategoryData);

                        Action<Uri> galleryCommandExecuted = delegate(Uri parameter)
                        {
                            if (!currentListCategoryData.GalleryItemDataCollection.Contains(parameter))
                            {
                                currentListCategoryData.GalleryItemDataCollection.Clear();
                                currentListCategoryData.GalleryItemDataCollection.Add(parameter);
                            }
                        };

                        Func<Uri, bool> galleryCommandCanExecute = delegate(Uri parameter)
                        {
                            return true;
                        };

                        galleryData.Command = new DelegateCommand<Uri>(galleryCommandExecuted, galleryCommandCanExecute);
                        _dataCollection[Str] = galleryData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData DecreaseIndent
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "DecreaseIndent";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Decrease Indent";
                        string ToolTipDescription = "Decreases the indent level of the paragraph.";

                        ButtonData buttonData = new ButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/DecreaseIndent_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = EditingCommands.DecreaseIndentation,
                            KeyTip = "AO",
                        };
                        _dataCollection[Str] = buttonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData IncreaseIndent
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "IncreaseIndent";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Increase Indent";
                        string ToolTipDescription = "Increases the indent level of the paragraph.";

                        ButtonData buttonData = new ButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/IncreaseIndent_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = EditingCommands.IncreaseIndentation,
                            KeyTip = "AI",
                        };
                        _dataCollection[Str] = buttonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Sort
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Sort";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Sort";
                        string ToolTipDescription = "Alphabetize the selected text or sort numerical data.";
                        string ToolTipFooterTitle = "Press F1 for more help.";

                        ButtonData buttonData = new ButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Sort_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            ToolTipFooterTitle = ToolTipFooterTitle,
                            ToolTipFooterImage = new Uri("/NodeEditorRibbon;component/Images/Help_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "SO",
                        };
                        _dataCollection[Str] = buttonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData ShowHide
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "ShowHide";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Show/Hide (Ctrl + *)";
                        string ToolTipDescription = "Show paragraph marks and other hidden formatting symbols.";
                        string ToolTipFooterTitle = "Press F1 for more help.";

                        ToggleButtonData toggleButtonData = new ToggleButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/ShowHide_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            ToolTipFooterTitle = ToolTipFooterTitle,
                            ToolTipFooterImage = new Uri("/NodeEditorRibbon;component/Images/Help_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "8",
                        };
                        _dataCollection[Str] = toggleButtonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData AlignTextLeft
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "AlignTextLeft";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Align Text Left (Ctrl + L)";
                        string ToolTipDescription = "Align text to the left.";

                        RadioButtonData radioButtonData = new RadioButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/AlignLeft_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = EditingCommands.AlignLeft,
                            KeyTip = "AL",
                        };
                        _dataCollection[Str] = radioButtonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData AlignTextCenter
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "AlignTextCenter";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Center (Ctrl + E)";
                        string ToolTipDescription = "Center text.";

                        RadioButtonData radioButtonData = new RadioButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/AlignCenter_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = EditingCommands.AlignCenter,
                            KeyTip = "AC",
                        };
                        _dataCollection[Str] = radioButtonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData AlignTextRight
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "AlignTextRight";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Align Text Left (Ctrl + L)";
                        string ToolTipDescription = "Align text to the right.";

                        RadioButtonData radioButtonData = new RadioButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/AlignRight_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = EditingCommands.AlignRight,
                            KeyTip = "AR",
                        };
                        _dataCollection[Str] = radioButtonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Justify
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Justify";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Justify (Ctrl + J)";
                        string ToolTipDescription = "Aligns text to both left and right margins, adding extra space between words as necessary.\n\n";
                        ToolTipDescription += "This creates a clean along left and right side of the page.";

                        RadioButtonData radioButtonData = new RadioButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Justify_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = EditingCommands.AlignJustify,
                            KeyTip = "AJ",
                        };
                        _dataCollection[Str] = radioButtonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData LineSpacing
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "LineSpacing";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Line Spacing";
                        string ToolTipDescription = "Change the spacing between line of text.\n\n";
                        ToolTipDescription += "You can also customize the amount of space added before and after the paragraphs.";

                        MenuButtonData menuButtonData = new MenuButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/LineSpacing_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "K",
                        };
                        _dataCollection[Str] = menuButtonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        #region LineSpacing Model

        private static void SetIsCheckedOfLineSpacingMenuItem(MenuItemData menuItemData, object parameter)
        {
            menuItemData.IsChecked = (menuItemData == parameter);
        }

        private static void LineSpacingMenuItemDefaultExecute(MenuItemData menuItemData)
        {
            SetIsCheckedOfLineSpacingMenuItem((MenuItemData)LineSpacingFirstValue, menuItemData);
            SetIsCheckedOfLineSpacingMenuItem((MenuItemData)LineSpacingSecondValue, menuItemData);
            SetIsCheckedOfLineSpacingMenuItem((MenuItemData)LineSpacingThirdValue, menuItemData);
            SetIsCheckedOfLineSpacingMenuItem((MenuItemData)LineSpacingFourthValue, menuItemData);
            SetIsCheckedOfLineSpacingMenuItem((MenuItemData)LineSpacingFifthValue, menuItemData);
            SetIsCheckedOfLineSpacingMenuItem((MenuItemData)LineSpacingSixthValue, menuItemData);
        }

        private static bool LineSpacingMenuItemDefaultCanExecute(MenuItemData menuItemData)
        {
            return true;
        }

        public static ControlData LineSpacingFirstValue
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "LineSpacingFirstValue";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = "1.0",
                            IsCheckable = true,
                            Command = new DelegateCommand<MenuItemData>(LineSpacingMenuItemDefaultExecute, LineSpacingMenuItemDefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData LineSpacingSecondValue
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "LineSpacingSecondValue";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = "1.15",
                            IsCheckable = true,
                            Command = new DelegateCommand<MenuItemData>(LineSpacingMenuItemDefaultExecute, LineSpacingMenuItemDefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData LineSpacingThirdValue
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "LineSpacingThirdValue";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = "1.5",
                            IsCheckable = true,
                            Command = new DelegateCommand<MenuItemData>(LineSpacingMenuItemDefaultExecute, LineSpacingMenuItemDefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData LineSpacingFourthValue
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "LineSpacingFourthValue";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = "2.0",
                            IsCheckable = true,
                            Command = new DelegateCommand<MenuItemData>(LineSpacingMenuItemDefaultExecute, LineSpacingMenuItemDefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData LineSpacingFifthValue
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "LineSpacingFifthValue";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = "2.5",
                            IsCheckable = true,
                            Command = new DelegateCommand<MenuItemData>(LineSpacingMenuItemDefaultExecute, LineSpacingMenuItemDefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData LineSpacingSixthValue
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "LineSpacingSixthValue";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = "3.0",
                            IsCheckable = true,
                            Command = new DelegateCommand<MenuItemData>(LineSpacingMenuItemDefaultExecute, LineSpacingMenuItemDefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData LineSpacingOptions
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Line Spacing Options...";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData AddSpaceBeforeParagraph
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Add Space _Before Paragraph";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/UpArrow_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData RemoveSpaceAfterParagraph
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Remove Space _After Paragraph";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/DownArrow_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        #endregion

        public static ControlData Shading
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Shading";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Shading";
                        string ToolTipDescription = "Color the background behind selected text or paragraph";
                        string ToolTipFooterTitle = "Press F1 for more help.";

                        SplitButtonData splitButtonData = new SplitButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Shading_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            ToolTipFooterTitle = ToolTipFooterTitle,
                            ToolTipFooterImage = new Uri("/NodeEditorRibbon;component/Images/Help_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "H",
                        };
                        _dataCollection[Str] = splitButtonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Borders
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Borders";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Borders";

                        SplitButtonData splitButtonData = new SplitButtonData()
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Borders_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            IsCheckable = true,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "B",
                        };
                        _dataCollection[Str] = splitButtonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        #region Borders Model

        private static void BorderMenuItemDefaultExecute(MenuItemData menuItemData)
        {
            MenuItemData bottomBorder = (MenuItemData)BottomBorder;
            MenuItemData topBorder = (MenuItemData)TopBorder;
            MenuItemData leftBorder = (MenuItemData)LeftBorder;
            MenuItemData rightBorder = (MenuItemData)RightBorder;
            MenuItemData noBorder = (MenuItemData)NoBorder;
            MenuItemData allBorders = (MenuItemData)AllBorders;
            MenuItemData outsideBorders = (MenuItemData)OutsideBorders;
            MenuItemData insideBorders = (MenuItemData)InsideBorders;
            MenuItemData insideHorizontalBorder = (MenuItemData)InsideHorizontalBorder;
            MenuItemData insideVerticalBorder = (MenuItemData)InsideVerticalBorder;

            if (menuItemData == bottomBorder ||
                menuItemData == topBorder ||
                menuItemData == leftBorder ||
                menuItemData == rightBorder)
            {
                outsideBorders.IsChecked = (bottomBorder.IsChecked &&
                    topBorder.IsChecked &&
                    leftBorder.IsChecked &&
                    rightBorder.IsChecked);
            }

            if (menuItemData == outsideBorders)
            {
                bottomBorder.IsChecked = topBorder.IsChecked = leftBorder.IsChecked = rightBorder.IsChecked = outsideBorders.IsChecked;
            }

            if (menuItemData == insideHorizontalBorder ||
                menuItemData == insideVerticalBorder)
            {
                insideBorders.IsChecked = (insideHorizontalBorder.IsChecked &&
                    insideVerticalBorder.IsChecked);
            }

            if (menuItemData == insideBorders)
            {
                insideHorizontalBorder.IsChecked = insideVerticalBorder.IsChecked = insideBorders.IsChecked;
            }

            if (menuItemData == noBorder)
            {
                bottomBorder.IsChecked = false;
                topBorder.IsChecked = false;
                leftBorder.IsChecked = false;
                rightBorder.IsChecked = false;
                outsideBorders.IsChecked = false;
                insideBorders.IsChecked = false;
                insideHorizontalBorder.IsChecked = false;
                insideVerticalBorder.IsChecked = false;
            }

            if (menuItemData == allBorders)
            {
                bottomBorder.IsChecked = true;
                topBorder.IsChecked = true;
                leftBorder.IsChecked = true;
                rightBorder.IsChecked = true;
                outsideBorders.IsChecked = true;
                insideBorders.IsChecked = true;
                insideHorizontalBorder.IsChecked = true;
                insideVerticalBorder.IsChecked = true;
            }
        }

        private static bool BordersMenuItemDefaultCanExecute(MenuItemData menuItemData)
        {
            return true;
        }

        public static ControlData BottomBorder
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Bottom Border";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/BottomBorder_16x16.png", UriKind.Relative),
                            IsCheckable = true,
                            Command = new DelegateCommand<MenuItemData>(BorderMenuItemDefaultExecute, BordersMenuItemDefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData TopBorder
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "To_p Border";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/TopBorder_16x16.png", UriKind.Relative),
                            IsCheckable = true,
                            Command = new DelegateCommand<MenuItemData>(BorderMenuItemDefaultExecute, BordersMenuItemDefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData LeftBorder
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Left Border";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/LeftBorder_16x16.png", UriKind.Relative),
                            IsCheckable = true,
                            Command = new DelegateCommand<MenuItemData>(BorderMenuItemDefaultExecute, BordersMenuItemDefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData RightBorder
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Right Border";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/RightBorder_16x16.png", UriKind.Relative),
                            IsCheckable = true,
                            Command = new DelegateCommand<MenuItemData>(BorderMenuItemDefaultExecute, BordersMenuItemDefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData NoBorder
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_No Border";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/NoBorder_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand<MenuItemData>(BorderMenuItemDefaultExecute, BordersMenuItemDefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData AllBorders
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_All Borders";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Borders_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand<MenuItemData>(BorderMenuItemDefaultExecute, BordersMenuItemDefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData InsideBorders
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Inside Borders";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/InsideBorders_16x16.png", UriKind.Relative),
                            IsCheckable = true,
                            Command = new DelegateCommand<MenuItemData>(BorderMenuItemDefaultExecute, BordersMenuItemDefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData OutsideBorders
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Out_side Borders";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/OuterBorders_16x16.png", UriKind.Relative),
                            IsCheckable = true,
                            Command = new DelegateCommand<MenuItemData>(BorderMenuItemDefaultExecute, BordersMenuItemDefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData InsideHorizontalBorder
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Inside _Horizontal Border";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/InsideHorizontalBorder_16x16.png", UriKind.Relative),
                            IsCheckable = true,
                            Command = new DelegateCommand<MenuItemData>(BorderMenuItemDefaultExecute, BordersMenuItemDefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData InsideVerticalBorder
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Inside _Vertical Border";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/InsideVerticalBorder_16x16.png", UriKind.Relative),
                            IsCheckable = true,
                            Command = new DelegateCommand<MenuItemData>(BorderMenuItemDefaultExecute, BordersMenuItemDefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData DiagonalDownBorder
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Diagonal Do_wn Border";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/DiagonalDownBorder_16x16.png", UriKind.Relative),
                            IsCheckable = true,
                            Command = new DelegateCommand<MenuItemData>(BorderMenuItemDefaultExecute, BordersMenuItemDefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData DiagonalUpBorder
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Diagonal _Up Border";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/DiagonalUpBorder_16x16.png", UriKind.Relative),
                            IsCheckable = true,
                            Command = new DelegateCommand<MenuItemData>(BorderMenuItemDefaultExecute, BordersMenuItemDefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData HorizontalLine
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Hori_zontal Line";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/InsideHorizontalBorder_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData DrawTable
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Draw Table";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/DrawTable_16x16.png", UriKind.Relative),
                            IsCheckable = true,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData ViewGridLines
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "View _Gridlines";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/ShowGridlines_16x16.png", UriKind.Relative),
                            IsCheckable = true,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData BordersAndShading
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "B_orders And Shading";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        _dataCollection[Str] = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/BordersAndShading_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        #endregion

        #endregion Paragraph Group Model

        #region Styles Group Model 

        public static ControlData Styles
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Styles";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        GroupData Data = new GroupData(Str)
                        {
                            LargeImage = new Uri("/NodeEditorRibbon;component/Images/StylesGroup.png", UriKind.Relative),
                            KeyTip = "ZS",
                        };
                        _dataCollection[Str] = Data;
                    }

                    return _dataCollection[Str];
                }
            }
        }


        public static ControlData ChangeStyles
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Change Styles";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Change Styles";
                        string ToolTipDescription = "Change the set of styles, colors, fonts, and paragraph spacing used in this document.";

                        MenuButtonData menuButtonData = new MenuButtonData()
                        {
                            Label = Str, 
                            LargeImage = new Uri("/NodeEditorRibbon;component/Images/Styles_32x32.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            KeyTip = "G",
                        };
                        _dataCollection[Str] = menuButtonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData StylesSet
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Style Set";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Forecolor_16x16.png", UriKind.Relative),
                            IsVerticallyResizable = true,
                            KeyTip = "Y",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Colors
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Colors";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/ChooseColor_16x16.png", UriKind.Relative),
                            IsVerticallyResizable = true,
                            KeyTip = "C",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Fonts
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Fonts";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Font_16x16.png", UriKind.Relative),
                            IsVerticallyResizable = true,
                            KeyTip = "F",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData ParagraphSpacing
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Paragraph Spacing";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/ParagraphSpacing_16x16.png", UriKind.Relative),
                            KeyTip = "P",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData SetAsDefault
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Set as Default";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = Str;
                        string ToolTipDescription = "Set the cuurent style set and theme as the default when you create a new document.";

                        MenuItemData menuItemData = new MenuItemData()
                        {
                            Label = Str,
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "S",
                        };
                        _dataCollection[Str] = menuItemData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static GalleryData<StylesSet> StylesSetGalleryData
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "StylesSetGalleryData";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        // TODO: replace string with an object (IsChecked, StyleName). Define DataTemplate
                        GalleryData<StylesSet> stylesData = new GalleryData<StylesSet>();
                        GalleryCategoryData<StylesSet> singleCategory = new GalleryCategoryData<StylesSet>();

                        singleCategory.GalleryItemDataCollection.Add(new StylesSet() { Label = "Default (Black and White)", IsSelected = true } );
                        singleCategory.GalleryItemDataCollection.Add(new StylesSet() { Label = "Distinctive" });
                        singleCategory.GalleryItemDataCollection.Add(new StylesSet() { Label = "Elegant"});
                        singleCategory.GalleryItemDataCollection.Add(new StylesSet() { Label = "Fancy"});
                        singleCategory.GalleryItemDataCollection.Add(new StylesSet() { Label = "Formal"});
                        singleCategory.GalleryItemDataCollection.Add(new StylesSet() { Label = "Manuscript"});
                        singleCategory.GalleryItemDataCollection.Add(new StylesSet() { Label = "Modern"});
                        singleCategory.GalleryItemDataCollection.Add(new StylesSet() { Label = "Newsprint"});
                        singleCategory.GalleryItemDataCollection.Add(new StylesSet() { Label = "Perspective"});
                        singleCategory.GalleryItemDataCollection.Add(new StylesSet() { Label = "Simple"});
                        singleCategory.GalleryItemDataCollection.Add(new StylesSet() { Label = "Thatch"});
                        singleCategory.GalleryItemDataCollection.Add(new StylesSet() { Label = "Traditional"});
                        singleCategory.GalleryItemDataCollection.Add(new StylesSet() { Label = "Word 2003"});
                        singleCategory.GalleryItemDataCollection.Add(new StylesSet() { Label = "Word 2010"});

                        stylesData.CategoryDataCollection.Clear();
                        stylesData.CategoryDataCollection.Add(singleCategory);
                        _dataCollection[Str] = stylesData;
                    }

                    return _dataCollection[Str] as GalleryData<StylesSet>;
                }
            }
        }

        public static GalleryData<string> StylesColorsGalleryData
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "StylesColorsGalleryData";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        // TODO: replace string with an object (Color Palette, StyleName). Define DataTemplate
                        GalleryData<string> stylesData = new GalleryData<string>();
                        GalleryCategoryData<string> singleCategory = new GalleryCategoryData<string>();

                        singleCategory.Label = "Built-In";
                        singleCategory.GalleryItemDataCollection.Add("Office");
                        singleCategory.GalleryItemDataCollection.Add("Grayscale");
                        singleCategory.GalleryItemDataCollection.Add("Adjacency");
                        singleCategory.GalleryItemDataCollection.Add("Angles");
                        singleCategory.GalleryItemDataCollection.Add("Apex");
                        singleCategory.GalleryItemDataCollection.Add("Apothecary");
                        singleCategory.GalleryItemDataCollection.Add("Aspect");
                        singleCategory.GalleryItemDataCollection.Add("Austin");
                        singleCategory.GalleryItemDataCollection.Add("Black Tie");
                        singleCategory.GalleryItemDataCollection.Add("Civic");
                        singleCategory.GalleryItemDataCollection.Add("Clarity");
                        singleCategory.GalleryItemDataCollection.Add("Composite");
                        singleCategory.GalleryItemDataCollection.Add("Concourse");
                        singleCategory.GalleryItemDataCollection.Add("Couture");
                        singleCategory.GalleryItemDataCollection.Add("Elemental");
                        singleCategory.GalleryItemDataCollection.Add("Equity");
                        singleCategory.GalleryItemDataCollection.Add("Essential");
                        singleCategory.GalleryItemDataCollection.Add("Executive");
                        singleCategory.GalleryItemDataCollection.Add("Flow");
                        singleCategory.GalleryItemDataCollection.Add("Foundry");
                        singleCategory.GalleryItemDataCollection.Add("Grid");
                        singleCategory.GalleryItemDataCollection.Add("Horizon");
                        singleCategory.GalleryItemDataCollection.Add("Median");
                        singleCategory.GalleryItemDataCollection.Add("Newsprint");
                        singleCategory.GalleryItemDataCollection.Add("Perspective");
                        singleCategory.GalleryItemDataCollection.Add("Solstice");
                        singleCategory.GalleryItemDataCollection.Add("Technic");
                        singleCategory.GalleryItemDataCollection.Add("Urban");
                        singleCategory.GalleryItemDataCollection.Add("Verve");
                        singleCategory.GalleryItemDataCollection.Add("Waveform");

                        stylesData.CategoryDataCollection.Add(singleCategory);
                        _dataCollection[Str] = stylesData;
                    }

                    return _dataCollection[Str] as GalleryData<string>;
                }
            }
        }

        public static GalleryData<ThemeFonts> StylesFontsGalleryData
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "StylesFontsGalleryData";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        GalleryData<ThemeFonts> stylesData = new GalleryData<ThemeFonts>();
                        GalleryCategoryData<ThemeFonts> singleCategory = new GalleryCategoryData<ThemeFonts>();

                        singleCategory.Label = "Built-In";
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Office", Field2 = "Cambria", Field3 = "Calibri", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Office 2", Field2 = "Calibri", Field3 = "Cambria", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Office Classic", Field2 = "Arial", Field3 = "Times New Roman", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Office Classic 2", Field2 = "Arial", Field3 = "Arial", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Adjacency", Field2 = "Franklin Gothic", Field3 = "Franklin Gothic Book", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Angles", Field2 = "Cambria", Field3 = "Calibri", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Apex", Field2 = "Lucida Sans", Field3 = "Book Antiqua", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Apothecary", Field2 = "Book Antiqua", Field3 = "Century Gothic", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Aspect", Field2 = "Verdana", Field3 = "Verdana", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Austin", Field2 = "Century Gothic", Field3 = "Century Gothic", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Black Tie", Field2 = "Garamond", Field3 = "Garamond", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Civic", Field2 = "Georgia", Field3 = "Georgia", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Office", Field2 = "Cambria", Field3 = "Calibri", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Office", Field2 = "Cambria", Field3 = "Calibri", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Office", Field2 = "Cambria", Field3 = "Calibri", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Office", Field2 = "Cambria", Field3 = "Calibri", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Office", Field2 = "Cambria", Field3 = "Calibri", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Office", Field2 = "Cambria", Field3 = "Calibri", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Office", Field2 = "Cambria", Field3 = "Calibri", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Office", Field2 = "Cambria", Field3 = "Calibri", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Office", Field2 = "Cambria", Field3 = "Calibri", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Office", Field2 = "Cambria", Field3 = "Calibri", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Office", Field2 = "Cambria", Field3 = "Calibri", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });
                        singleCategory.GalleryItemDataCollection.Add(new ThemeFonts() { Label = "Office", Field2 = "Cambria", Field3 = "Calibri", Field1 = "/NodeEditorRibbon;component/Images/ThemeFonts.png" });

                        stylesData.CategoryDataCollection.Add(singleCategory);
                        _dataCollection[Str] = stylesData;
                    }

                    return _dataCollection[Str] as GalleryData<ThemeFonts>;
                }
            }
        }

        public static GalleryData<string> StylesParagraphGalleryData
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "StylesParagraphGalleryData";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        GalleryData<string> stylesData = new GalleryData<string>();
                        GalleryCategoryData<string> firstCategory = new GalleryCategoryData<string>();
                        firstCategory.Label = "Style Set";
                        firstCategory.GalleryItemDataCollection.Add("Traditional");

                        GalleryCategoryData<string> secondCategory = new GalleryCategoryData<string>();
                        secondCategory.Label = "Built-In";
                        secondCategory.GalleryItemDataCollection.Add("No Paragraph Space");
                        secondCategory.GalleryItemDataCollection.Add("Compact");
                        secondCategory.GalleryItemDataCollection.Add("Tight");
                        secondCategory.GalleryItemDataCollection.Add("Open");
                        secondCategory.GalleryItemDataCollection.Add("Relaxed");
                        secondCategory.GalleryItemDataCollection.Add("Double");
                        
                        stylesData.CategoryDataCollection.Add(firstCategory);
                        stylesData.CategoryDataCollection.Add(secondCategory);
                        _dataCollection[Str] = stylesData;
                    }

                    return _dataCollection[Str] as GalleryData<string>;
                }
            }
        }

        #endregion

        #region Editing model

        public static ControlData Editing
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Editing";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        GroupData Data = new GroupData(Str)
                        {
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Find_16x16.png", UriKind.Relative),
                            LargeImage = new Uri("/NodeEditorRibbon;component/Images/Find_32x32.png", UriKind.Relative),
                            KeyTip="ZN",
                        };
                        _dataCollection[Str] = Data;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Find
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Find";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "Find (Ctrl+F)";
                        string ToolTipDescription = "Find text or other content in the document.";
                        string DropDownToolTipDescription = "Find and select specific text, formatting or other type of information within the document.";
                        string DropDownToolTipFooter = "You can also replace the information with new text or formatting.";

                        SplitButtonData FindSplitButtonData = new SplitButtonData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Find_16x16.png", UriKind.Relative),
                            ToolTipTitle = TooTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "FD",
                        };
                        FindSplitButtonData.DropDownButtonData.ToolTipTitle = TooTipTitle;
                        FindSplitButtonData.DropDownButtonData.ToolTipDescription = DropDownToolTipDescription;
                        FindSplitButtonData.DropDownButtonData.ToolTipFooterDescription = DropDownToolTipFooter;
                        FindSplitButtonData.DropDownButtonData.Command = new DelegateCommand(delegate { });
                        _dataCollection[Str] = FindSplitButtonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData FindMenuItem
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Find";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "Find (Ctrl+F)";
                        string ToolTipDescription = "Find text or other content in the document.";

                        MenuItemData FindMenuItemData = new MenuItemData()
                        {
                            Label = Str,
                            ToolTipTitle = TooTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Find_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "F",
                        };
                        _dataCollection[Str] = FindMenuItemData;
                    }

                    return _dataCollection[Str];
                }
            }

        }

        public static ControlData AdvancedFind
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Advanced Find...";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "Advanced Find";
                        string ToolTipDescription = "Find text in the document.";

                        MenuItemData FindMenuItemData = new MenuItemData()
                        {
                            Label = Str,
                            ToolTipTitle = TooTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Find_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "A",
                        };
                        _dataCollection[Str] = FindMenuItemData;
                    }

                    return _dataCollection[Str];
                }
            }

        }

        public static ControlData GoTo
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Go To...";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string TooTipTitle = "Go To (Ctrl+G)";
                        string ToolTipDescription = "Navigate to a specific place in the document.\n\r\n\r" +
                        "Depending on the type of the document, you can navigate to a specific page number, line number, footnote, table, comment, or other object.";
                        string ToolTipFooterTitle = "Press F1 for more help.";
                        MenuItemData MenuItemData = new MenuItemData()
                        {
                            Label = Str,
                            ToolTipTitle = TooTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            ToolTipFooterTitle = ToolTipFooterTitle,
                            ToolTipFooterImage = new Uri("/NodeEditorRibbon;component/Images/Help_16x16.png", UriKind.Relative),
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/GoTo_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "G",
                        };
                        _dataCollection[Str] = MenuItemData;
                    }

                    return _dataCollection[Str];
                }
            }

        }

        public static ControlData Replace
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Replace";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ReplaceToolTipTitle = "Replace (Ctrl+H)";
                        string ReplaceToolTipDescription = "Replace text in the document.";

                        ButtonData ReplaceButtonData = new ButtonData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Replace_16x16.png", UriKind.Relative),
                            ToolTipTitle = ReplaceToolTipTitle,
                            ToolTipDescription = ReplaceToolTipDescription,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "R",
                        };
                        _dataCollection[Str] = ReplaceButtonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData Select
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Select";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Select";
                        string ToolTipDescription = "Select text or objects in the document.\n\r\n\r" +
                        "Use Select Object to allow you to select objects that had been positioned behind the text.";

                        MenuButtonData SelectMenuButtonData = new MenuButtonData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Select_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip="SL",
                        };
                        _dataCollection[Str] = SelectMenuButtonData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData SelectAll
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Select _All";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Select All (Ctrl+A)";
                        string ToolTipDescription = "Select all items";

                        MenuItemData Data = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Select_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "A",
                        };
                        _dataCollection[Str] = Data;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData SelectObjects
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Select _Objects";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Select Objects";
                        string ToolTipDescription = "Select rectangular regions of ink strokes, shapes and text";

                        MenuItemData Data = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/Select_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "O",
                        };
                        _dataCollection[Str] = Data;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData SelectAllText
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "_Select All Text With Similar Formatting (No Data)";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        MenuItemData Data = new MenuItemData()
                        {
                            Label = Str,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "S",
                        };
                        _dataCollection[Str] = Data;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData SelectionPane
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Selection _Pane...";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = "Selection Pane";
                        string ToolTipDescription = "Show the Selection Pane to help select individual objects and to change their order and visibility.";
 
                        MenuItemData Data = new MenuItemData()
                        {
                            Label = Str,
                            SmallImage = new Uri("/NodeEditorRibbon;component/Images/SelectionPane_16x16.png", UriKind.Relative),
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "P",
                            IsCheckable = true,
                        };
                        _dataCollection[Str] = Data;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        #endregion

        #region Insert Table Group Model 

        public static ControlData Table
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Table";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        string ToolTipTitle = Str;
                        string ToolTipDescription = "Insert or draw a table into the document.";

                        _dataCollection[Str] = new MenuButtonData()
                        {
                            LargeImage = new Uri("/NodeEditorRibbon;component/Images/Table_32x32.png", UriKind.Relative),
                            Label = Str,
                            ToolTipTitle = ToolTipTitle,
                            ToolTipDescription = ToolTipDescription,
                            ToolTipFooterTitle = HelpFooterTitle,
                            ToolTipFooterImage = new Uri("/NodeEditorRibbon;component/Images/Help_16x16.png", UriKind.Relative),
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            KeyTip = "T",
                        };
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData TableGallery
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Table Gallery";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        GalleryCategoryData<RowColumnCount> galleryCategoryData = new GalleryCategoryData<RowColumnCount>();
                        for (int i = 1; i <= 8; i++)
                        {
                            for (int j = 1; j <= 10; j++)
                            {
                                galleryCategoryData.GalleryItemDataCollection.Add(new RowColumnCount() { RowCount = i, ColumnCount = j });
                            }
                        }

                        GalleryData<RowColumnCount> galleryData = new GalleryData<RowColumnCount>()
                        {
                            Command = new PreviewDelegateCommand<RowColumnCount>(InsertTable, CanInsertTable, PreviewInsertTable, CancelPreviewInsertTable),
                            SelectedItem = galleryCategoryData.GalleryItemDataCollection[0],
                        };

                        galleryData.CategoryDataCollection.Add(galleryCategoryData);
                        _dataCollection[Str] = galleryData;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        private static void InsertTable(RowColumnCount parameter)
        {
            UserControlWord wordControl = WordControl;
            if (wordControl != null)
            {
                wordControl.InsertTable();
            }
        }

        private static bool CanInsertTable(RowColumnCount parameter)
        {
            UserControlWord wordControl = WordControl;
            if (wordControl != null)
            {
                return wordControl.CanInsertTable();
            }

            return false;
        }

        private static void PreviewInsertTable(RowColumnCount parameter)
        {
            UserControlWord wordControl = WordControl;
            if (wordControl != null)
            {
                wordControl.PreviewInsertTable(parameter);
            }
        }

        private static void CancelPreviewInsertTable()
        {
            UserControlWord wordControl = WordControl;
            if (wordControl != null)
            {
                wordControl.CancelPreviewInsertTable();
            }
        }

        public static TabData DesignTab
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Design";

                    if (!_miscData.ContainsKey(Str))
                    {
                        TabData designTabData = new TabData() { Header = Str, ContextualTabGroupHeader = "Table Tools" }; 
                        _miscData[Str] = designTabData;
                    }

                    return _miscData[Str] as TabData;
                }
            }
        }

        public static TabData LayoutTab
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Layout";

                    if (!_miscData.ContainsKey(Str))
                    {
                        TabData layoutTabData = new TabData() { Header = Str, ContextualTabGroupHeader = "Table Tools" };
                        _miscData[Str] = layoutTabData;
                    }

                    return _miscData[Str] as TabData;
                }
            }
        }

        public static ContextualTabGroupData TableTabGroup
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Table Tools";

                    if (!_miscData.ContainsKey(Str))
                    {
                        ContextualTabGroupData tableData = new ContextualTabGroupData() { Header = Str };
                        tableData.TabDataCollection.Add(DesignTab);
                        tableData.TabDataCollection.Add(LayoutTab);

                        _miscData[Str] = tableData;
                    }

                    return _miscData[Str] as ContextualTabGroupData;
                }
            }
        }


        public static ControlData HeaderRow
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Header Row";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        CheckBoxData Data = new CheckBoxData()
                        {
                            Label = Str,
                            ToolTipTitle = Str,
                            ToolTipDescription = "Display special formatting for the first row of the table.",
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            IsChecked = true,
                        };
                        _dataCollection[Str] = Data;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData FirstColumn
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "First Column";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        CheckBoxData Data = new CheckBoxData()
                        {
                            Label = Str,
                            ToolTipTitle = Str,
                            ToolTipDescription = "Display special formatting for the first column of the table.",
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            IsChecked = true,
                        };
                        _dataCollection[Str] = Data;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData TotalRow
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Total Row";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        CheckBoxData Data = new CheckBoxData()
                        {
                            Label = Str,
                            ToolTipTitle = Str,
                            ToolTipDescription = "Display special formatting for the last row of the table.",
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                        _dataCollection[Str] = Data;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData LastColumn
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Last Column";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        CheckBoxData Data = new CheckBoxData()
                        {
                            Label = Str,
                            ToolTipTitle = Str,
                            ToolTipDescription = "Display special formatting for the last column of the table.",
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                        _dataCollection[Str] = Data;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData BandedRows
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Banded Rows";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        CheckBoxData Data = new CheckBoxData()
                        {
                            Label = Str,
                            ToolTipTitle = Str,
                            ToolTipDescription = "Display banded rows.",
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                            IsChecked = true,
                        };
                        _dataCollection[Str] = Data;
                    }

                    return _dataCollection[Str];
                }
            }
        }

        public static ControlData BandedColumns
        {
            get
            {
                lock (_lockObject)
                {
                    string Str = "Banded Columns";

                    if (!_dataCollection.ContainsKey(Str))
                    {
                        CheckBoxData Data = new CheckBoxData()
                        {
                            Label = Str,
                            ToolTipTitle = Str,
                            ToolTipDescription = "Display banded columns.",
                            Command = new DelegateCommand(DefaultExecuted, DefaultCanExecute),
                        };
                        _dataCollection[Str] = Data;
                    }

                    return _dataCollection[Str];
                }
            }
        }
        #endregion

        private static void DefaultExecuted()
        {
        }

        private static bool DefaultCanExecute()
        {
            return true;
        }

        private static UserControlWord WordControl
        {
            get
            {
                        // DavidJ -- Application.Current is null here... Why..?
                if (Application.Current != null && Application.Current.Properties.Contains("WordControlRef"))
                {
                    WeakReference wordControlRef = Application.Current.Properties["WordControlRef"] as WeakReference;
                    if (wordControlRef != null)
                    {
                        return wordControlRef.Target as UserControlWord;
                    }
                }
                return null;
            }

        }

        #region Data

        private const string HelpFooterTitle = "Press F1 for more help.";
        private static object _lockObject = new object();
        private static Dictionary<string, ControlData> _dataCollection = new Dictionary<string,ControlData>();

        // Store any data that doesnt inherit from ControlData
        private static Dictionary<string, object> _miscData = new Dictionary<string, object>();

        #endregion Data

    }
}
