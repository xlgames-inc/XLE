//****************************************************************************
//
//  File:       CommonCOMInterfaces.cs
//
//  Contents:   Common COM Interfaces which are used with the Windows Ribbon 
//              Framework.
//
//****************************************************************************

using System;
using System.Runtime.InteropServices;

namespace RibbonLib.Interop
{
    /// <summary>
    /// HRESULT Wrapper
    /// </summary>
    public enum HRESULT : uint
    {
        S_OK = 0x00000000,
        S_FALSE = 0x00000001,
        E_FAIL = 0x80004005,
        E_NOTIMPL = 0x80004001,
    }

    public static class NativeMethods
    {
        public const uint DONT_RESOLVE_DLL_REFERENCES = 0x00000001;
        public const uint LOAD_LIBRARY_AS_DATAFILE = 0x00000002;
        public const uint LOAD_WITH_ALTERED_SEARCH_PATH = 0x00000008;
        public const uint LOAD_IGNORE_CODE_AUTHZ_LEVEL = 0x00000010;
        public const uint LOAD_LIBRARY_AS_IMAGE_RESOURCE = 0x00000020;
        public const uint LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE = 0x00000040;
        public const uint LOAD_LIBRARY_REQUIRE_SIGNED_TARGET = 0x00000080;

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern IntPtr LoadLibrary([MarshalAs(UnmanagedType.LPStr)] string lpFileName);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern IntPtr LoadLibraryEx(string lpFileName, IntPtr hFile, uint dwFlags);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool FreeLibrary([In] IntPtr hModule);

        public static bool Succeeded(HRESULT hr)
        {
            return ((int)hr >= 0);
        }

        public static bool Failed(HRESULT hr)
        {
            return ((int)hr < 0);
        }
    }

    [ComImport]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [Guid("00000100-0000-0000-C000-000000000046")]
    public interface IEnumUnknown
    {
        // Retrieves the specified number of items in the enumeration sequence
        [PreserveSig]
        HRESULT Next([In, MarshalAs(UnmanagedType.U4)] uint celt,
                 [Out, MarshalAs(UnmanagedType.LPArray, ArraySubType = UnmanagedType.IUnknown, SizeParamIndex = 0)] object[] rgelt,
                 [Out, MarshalAs(UnmanagedType.U4)] out uint pceltFetched);
       
        // Skips over the specified number of items in the enumeration sequence.
        [PreserveSig]
        HRESULT Skip([In, MarshalAs(UnmanagedType.U4)] uint celt);

        // Resets the enumeration sequence to the beginning.
        [PreserveSig]
        HRESULT Reset();

        // Creates a new enumerator that contains the same enumeration state as the current one.
        [PreserveSig]
        HRESULT Clone(out IEnumUnknown ppenum);
    }

    [ComImport]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [Guid("886D8EEB-8CF2-4446-8D02-CDBA1DBDCF99")]
    public interface IPropertyStore
    {
        // Gets the number of properties attached to the file.
        [PreserveSig]
        HRESULT GetCount([Out] out uint cProps);

        // Gets a property key from an item's array of properties.
        [PreserveSig]
        HRESULT GetAt([In] uint iProp, out PropertyKey pkey);

        // Gets data for a specific property.
        [PreserveSig]
        HRESULT GetValue([In] ref PropertyKey key, out PropVariant pv);

        // Sets a new property value, or replaces or removes an existing value.
        [PreserveSig]
        HRESULT SetValue([In] ref PropertyKey key, [In] ref PropVariant pv);

        // Saves a property change.
        [PreserveSig]
        HRESULT Commit();
    }

}
