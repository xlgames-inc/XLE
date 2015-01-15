//****************************************************************************
//
//  File:       PropertyKey.cs
//
//  Contents:   Interop wrapper for native PropertyKey structure. Originally 
//              sourced from http://code.msdn.microsoft.com/PreviewRibbon 
//              project. My modifications:
//                1. Separated PropertyKey definiton from Ribbon related code
//                2. Exposed as public fields: FormatId and PropertyId
//
//****************************************************************************

using System;
using System.Globalization;
using System.Runtime.InteropServices;

namespace RibbonLib.Interop
{
    [StructLayout(LayoutKind.Sequential)]
    public struct PropertyKey
    {
        #region Constructors

        public PropertyKey(Guid fmtid, uint pid)
        {
            this.FormatId = fmtid;
            this.PropertyId = pid;
        }

        #endregion

        #region Public methods

        public static bool operator ==(PropertyKey left, PropertyKey right)
        {
            return ((left.FormatId == right.FormatId) && (left.PropertyId == right.PropertyId));
        }

        public static bool operator !=(PropertyKey left, PropertyKey right)
        {
            return !(left == right);
        }

        public override string ToString()
        {
            return "PKey: " + FormatId.ToString() + ":" + PropertyId.ToString(CultureInfo.InvariantCulture.NumberFormat);
        }

        // Return pinned memory to unmanaged code so that it doesn't get freed while unmanaged code still needs it.
        public IntPtr ToPointer()
        {
            if (!s_pinnedCache.ContainsKey(this))
            {
                s_pinnedCache.Add(this, GCHandle.Alloc(this, GCHandleType.Pinned));
            }

            return s_pinnedCache[this].AddrOfPinnedObject();
        }

        public override bool Equals(object obj)
        {
            if (obj == null)
            {
                return false;
            }

            if (!(obj is PropertyKey))
            {
                return false;
            }
            
            return (this == (PropertyKey)obj);
        }

        public override int GetHashCode()
        {
            return (FormatId.GetHashCode() ^ PropertyId.GetHashCode());
        }

        #endregion

        #region Private fields

        // This cache allocates pinned memory for the property key so that it doesn't get garbage-collected
        // while the unmanaged code works with it on the other side of interop.
        // Use the ToPointer() function when calling interop methods which take property keys as parameters.
        static System.Collections.Generic.Dictionary<PropertyKey, GCHandle> s_pinnedCache =
            new System.Collections.Generic.Dictionary<PropertyKey, GCHandle>(16);
         
        public Guid FormatId;

        public uint PropertyId;

        #endregion
    }

    // It is sometimes useful to represent the struct as a reference-type 
    // (eg, for methods that allow passing a null PropertyKey pointer).
    [StructLayout(LayoutKind.Sequential)]
    public class PropertyKeyRef
    {
        public PropertyKey PropertyKey;

        public static PropertyKeyRef From(PropertyKey value)
        {
            PropertyKeyRef obj = new PropertyKeyRef();
            obj.PropertyKey = value;
            return obj;
        }
    }
}
