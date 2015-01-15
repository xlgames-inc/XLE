//****************************************************************************
//
//  File:       RibbonCOMInterfaces.cs
//
//  Contents:   Interfaces of the Windows Ribbon Framework, based on 
//              UIRibbon.idl from windows 7 SDK
//
//****************************************************************************

using System;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;

namespace RibbonLib.Interop
{
    // Windows Ribbon interfaces implemented by the framework

    public enum ContextAvailability
    {
        NotAvailable = 0,
        Available = 1,
        Active = 2,
    }

    public enum FontProperties
    {
        NotAvailable = 0,
        NotSet = 1,
        Set = 2,
    }

    public enum FontVerticalPosition
    {
        NotAvailable = 0,
        NotSet = 1,
        SuperScript = 2,
        SubScript = 3,
    }

    public enum FontUnderline
    {
        NotAvailable = 0,
        NotSet = 1,
        Set = 2,
    }

    public enum FontDeltaSize
    {
        Grow = 0,
        Shrink = 1,
    }

    public enum ControlDock
    {
        Top = 1,
        Bottom = 3,
    }


    // Types for the color picker

    // Determines whether a swatch has a color, is nocolor or automatic.
    public enum SwatchColorType
    {
        NoColor = 0,    // Inactive swatch
        Automatic = 1,  // Automatic swatch
        RGB = 2,        // Solid color swatch
    }


    // If the mode is set to MONOCHROME, the swatch's RGB color value will be interpreted as a 1 bit-per-pixel
    // pattern.
    public enum SwatchColorMode
    {
        Normal = 0,
        Monochrome = 1,
    }


    // Simple property bag
    [ComImport]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [Guid(RibbonIIDGuid.IUISimplePropertySet)]
    public interface IUISimplePropertySet
    {
        // Retrieves the stored value of a given property
        [PreserveSig]
        HRESULT GetValue([In] ref PropertyKey key, [Out] out PropVariant value);
    }


    // Ribbon view interface
    [ComImport]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [Guid(RibbonIIDGuid.IUIRibbon)]
    public interface IUIRibbon
    {
        // Returns the Ribbon height
        [PreserveSig]
        HRESULT GetHeight([Out] out UInt32 cy);

        // Load Ribbon parameters (e.g. QuickAccessToolbar) from a stream
        [PreserveSig]
        HRESULT LoadSettingsFromStream([In, MarshalAs(UnmanagedType.Interface)] IStream pStream);

        // Save Ribbon parameters (e.g. QuickAccessToolbar) to a stream
        [PreserveSig]
        HRESULT SaveSettingsToStream([In, MarshalAs(UnmanagedType.Interface)] IStream pStream);
    }

    [Flags]
    public enum Invalidations
    {
        State = 0X00000001,             // Enabled
        Value = 0X00000002,             // Value property
        Property = 0X00000004,          // Any property
        AllProperties = 0X00000008,     // All properties
    }
    

    // Windows Ribbon Application interface
    [ComImport]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [Guid(RibbonIIDGuid.IUIFramework)]
    public interface IUIFramework
    {
        // Connects the framework and the application
        [PreserveSig]
        HRESULT Initialize(IntPtr frameWnd, IUIApplication application);

        // Releases all framework objects
        [PreserveSig]
        HRESULT Destroy();

        // Loads and instantiates the views and commands specified in markup
        [PreserveSig]
        HRESULT LoadUI(IntPtr instance, [MarshalAs(UnmanagedType.LPWStr)] string resourceName);

        // Retrieves a pointer to a view object
        [PreserveSig]
        HRESULT GetView(UInt32 viewID, [In] ref Guid riid, [Out, MarshalAs(UnmanagedType.Interface, IidParameterIndex = 1)] out object ppv);

        // Retrieves the current value of a property
        [PreserveSig]
        HRESULT GetUICommandProperty(UInt32 commandID, [In] ref PropertyKey key, [Out] out PropVariant value);

        // Immediately sets the value of a property
        [PreserveSig]
        HRESULT SetUICommandProperty(UInt32 commandID, [In] ref PropertyKey key, [In] ref PropVariant value);
        
        // Asks the framework to retrieve the new value of a property at the next update cycle
        [PreserveSig]
        HRESULT InvalidateUICommand(UInt32 commandID, Invalidations flags, [In, Optional] PropertyKeyRef key);

        // Flush all the pending UI command updates
        [PreserveSig]
        HRESULT FlushPendingInvalidations();

        // Asks the framework to switch to the list of modes specified and adjust visibility of controls accordingly
        [PreserveSig]
        HRESULT SetModes(Int32 iModes);
    }


    // Windows Ribbon ContextualUI interface
    [ComImport]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [Guid(RibbonIIDGuid.IUIContextualUI)]
    public interface IUIContextualUI
    {
        // Sets the desired anchor point where ContextualUI should be displayed.
        // Typically this is the mouse location at the time of right click.
        // x and y are in virtual screen coordinates.
        [PreserveSig]
        HRESULT ShowAtLocation(Int32 x, Int32 y);
    }


    // Windows Ribbon Collection interface
    [ComImport]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [Guid(RibbonIIDGuid.IUICollection)]
    public interface IUICollection
    {
        // Retrieves the count of the collection
        [PreserveSig]
        HRESULT GetCount([Out] out UInt32 count);

        // Retrieves an item
        [PreserveSig]
        HRESULT GetItem(UInt32 index, [Out, MarshalAs(UnmanagedType.IUnknown)] out object item);

        // Adds an item to the end
        [PreserveSig]
        HRESULT Add([In, MarshalAs(UnmanagedType.IUnknown)] object item);

        // Inserts an item
        [PreserveSig]
        HRESULT Insert(UInt32 index, [In, MarshalAs(UnmanagedType.IUnknown)] object item);

        // Removes an item at the specified position
        [PreserveSig]
        HRESULT RemoveAt(UInt32 index);

        // Replaces an item at the specified position
        [PreserveSig]
        HRESULT Replace(UInt32 indexReplaced, [In, MarshalAs(UnmanagedType.IUnknown)] object itemReplaceWith);

        // Clear the collection
        [PreserveSig]
        HRESULT Clear();
    }


    public enum CollectionChange
    {
        Insert = 0,
        Remove = 1,
        Replace = 2,
        Reset = 3,
    }

    public static class Constants
    {
        public const UInt32 UI_Collection_InvalidIndex = 0xffffffff;
        public const UInt32 UI_All_Commands = 0;
    }
    

    // Connection Sink for listening to collection changes
    [ComImport]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [Guid(RibbonIIDGuid.IUICollectionChangedEvent)]
    public interface IUICollectionChangedEvent
    {
        [PreserveSig]
        HRESULT OnChanged(CollectionChange action,
                          UInt32 oldIndex, 
                          [In, Optional, MarshalAs(UnmanagedType.IUnknown)] object oldItem,
                          UInt32 newIndex, 
                          [In, Optional, MarshalAs(UnmanagedType.IUnknown)] object newItem);
    }


    // Windows Ribbon interfaces implemented by the application

    public enum ExecutionVerb
    {
        Execute = 0,
        Preview = 1,
        CancelPreview = 2,
    }


    // Command handler interface
    [ComImport]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [Guid(RibbonIIDGuid.IUICommandHandler)]
    public interface IUICommandHandler
    {
        // User action callback, with transient execution parameters
        [PreserveSig]
        HRESULT Execute(UInt32 commandID,                      // the command that has been executed
                        ExecutionVerb verb,                 // the mode of execution
                        [In, Optional] PropertyKeyRef key,              // the property that has changed
                        [In, Optional] PropVariantRef currentValue,     // the new value of the property that has changed
                        [In, Optional] IUISimplePropertySet commandExecutionProperties); // additional data for this execution

        // Informs of the current value of a property, and queries for the new one
        [PreserveSig]
        HRESULT UpdateProperty(UInt32 commandID,                    // The ID for the Command, which is specified in the Markup resource file
                               [In] ref PropertyKey key,            // The Property Key to update
                               [In, Optional] PropVariantRef currentValue,   // A pointer to the current value for key. This parameter can be NULL
                               [In, Out] ref PropVariant newValue); // When this method returns, contains a pointer to the new value for key
    }


    // Types of UI commands
    public enum CommandType
    {
        Unknown = 0,
        Group = 1,
        Action = 2,
        Anchor = 3,
        Context = 4,
        Collection = 5,
        Commandcollection = 6,
        Decimal = 7,
        Boolean = 8,
        Font = 9,
        RecentItems = 10,
        ColorAnchor = 11,
        ColorCollection = 12,
    }


    // Types of UI Views
    public enum ViewType
    {
        Ribbon = 1,
    }


    public enum ViewVerb
    {
        Create = 0,
        Destroy = 1,
        Size = 2,
        Error = 3,
    }


    // Application callback interface
    [ComImport]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [Guid(RibbonIIDGuid.IUIApplication)]
    public interface IUIApplication
    {
        // A view has changed
        [PreserveSig]
        HRESULT OnViewChanged(UInt32 viewID,
                              ViewType typeID,
                              [In, MarshalAs(UnmanagedType.IUnknown)] object view,
                              ViewVerb verb,
                              Int32 uReasonCode);

        // Command creation callback
        [PreserveSig]
        HRESULT OnCreateUICommand(UInt32 commandID,
                                  CommandType typeID,
                                  [Out] out IUICommandHandler commandHandler);

        // Command destroy callback
        [PreserveSig]
        HRESULT OnDestroyUICommand(UInt32 commandID,
                                   CommandType typeID,
                                   [In, Optional] IUICommandHandler commandHandler);
    }


    // Container for bitmap image
    [ComImport]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [Guid(RibbonIIDGuid.IUIImage)]
    public interface IUIImage
    {
        // Retrieves a bitmap to display as an icon in the ribbon and context popup UI of the Windows Ribbon (Ribbon) framework.
        [PreserveSig]
        HRESULT GetBitmap([Out] out IntPtr bitmap);
    }

    public enum Ownership
    {
        Transfer = 0,   // IUIImage now owns HBITMAP.
        Copy = 1,       // IUIImage creates a copy of HBITMAP. Caller still owns HBITMAP.
    }

    // Produces containers for bitmap images
    [ComImport]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [Guid(RibbonIIDGuid.IUIImageFromBitmap)]
    public interface IUIImageFromBitmap
    {
        // Creates an IUIImage object from a bitmap image.
        [PreserveSig]
        HRESULT CreateImage(IntPtr bitmap, Ownership options, [Out, MarshalAs(UnmanagedType.Interface)] out IUIImage image);
    }

}
