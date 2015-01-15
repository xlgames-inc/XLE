//****************************************************************************
//
//  File:       PropVariant.cs
//
//  Contents:   Interop wrapper for the OLE PropVariant structure. 
//              Based on source from: 
//                1. http://blogs.msdn.com/adamroot/pages/interop-with-propvariants-in-net.aspx
//                2. http://code.msdn.microsoft.com/WindowsAPICodePack
//                3. http://code.msdn.microsoft.com/PreviewRibbon
//
//****************************************************************************

using System;
using System.Globalization;
using System.Runtime.InteropServices;

namespace RibbonLib.Interop
{
    /// <summary>
    /// Represents the OLE struct PROPVARIANT.
    /// </summary>
    /// <remarks>
    /// Must call Clear when finished to avoid memory leaks. If you get the value of
    /// a VT_UNKNOWN prop, an implicit AddRef is called, thus your reference will
    /// be active even after the PropVariant struct is cleared.
    /// Correct usage:
    /// 
    ///     PropVariant propVar;
    ///     GetProp(out propVar);
    ///     try
    ///     {
    ///         object value = propVar.Value;
    ///     }
    ///     finally { propVar.Clear(); }
    ///     
    /// Originally sourced from http://blogs.msdn.com/adamroot/pages/interop-with-propvariants-in-net.aspx
    /// and modified to support R/W, and SafeArray vectors, decimal values, and other fixes.
    /// </remarks>
    [StructLayout(LayoutKind.Sequential)]
    public struct PropVariant
    {
        #region Interop declarations

        [StructLayout(LayoutKind.Explicit)]
        private struct PVDecimalOuterUnion
        {
            [FieldOffset(0)]
            public decimal decVal;

            [FieldOffset(0)]
            public PropVariant propVar;
        }

        [StructLayout(LayoutKind.Explicit)]
        private struct PVVectorOuterUnion
        {
            [FieldOffset(0)]
            public PropVariant propVar;

            [FieldOffset(8)]
            public uint cElems;

            [FieldOffset(12)]
            public IntPtr pElems;
        }

        private static class UnsafeNativeMethods
        {
            [DllImport("Ole32.dll", PreserveSig = false)] // returns hresult
            internal extern static void PropVariantClear([In, Out] ref PropVariant pvar);

            [DllImport("Ole32.dll", PreserveSig = false)] // returns hresult
            internal extern static void PropVariantCopy([Out] out PropVariant pDst, [In] ref PropVariant pSrc);

            [DllImport("OleAut32.dll", PreserveSig = true)] // psa is actually returned, not hresult
            internal extern static IntPtr SafeArrayCreateVector(ushort vt, int lowerBound, uint cElems);

            [DllImport("OleAut32.dll", PreserveSig = false)] // returns hresult
            internal extern static IntPtr SafeArrayAccessData(IntPtr psa);

            [DllImport("OleAut32.dll", PreserveSig = false)] // returns hresult
            internal extern static void SafeArrayUnaccessData(IntPtr psa);

            [DllImport("OleAut32.dll", PreserveSig = true)] // retuns uint32
            internal extern static uint SafeArrayGetDim(IntPtr psa);

            [DllImport("OleAut32.dll", PreserveSig = false)] // returns hresult
            internal extern static int SafeArrayGetLBound(IntPtr psa, uint nDim);

            [DllImport("OleAut32.dll", PreserveSig = false)] // returns hresult
            internal extern static int SafeArrayGetUBound(IntPtr psa, uint nDim);

            // This decl for SafeArrayGetElement is only valid for cDims==1!
            [DllImport("OleAut32.dll", PreserveSig = false)] // returns hresult
            [return: MarshalAs(UnmanagedType.IUnknown)]
            internal extern static object SafeArrayGetElement(IntPtr psa, ref int rgIndices);

            [DllImport("OleAut32.dll", PreserveSig = false)]
            internal extern static void SafeArrayDestroy(IntPtr psa);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            internal static extern int InitPropVariantFromPropVariantVectorElem([In] ref PropVariant propvarIn, uint iElem, [Out] out PropVariant ppropvar);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            internal static extern uint InitPropVariantFromFileTime([In] ref System.Runtime.InteropServices.ComTypes.FILETIME pftIn, out PropVariant ppropvar);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            [return: MarshalAs(UnmanagedType.I4)]
            internal static extern int PropVariantGetElementCount([In] ref PropVariant propVar);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            internal static extern void PropVariantGetBooleanElem([In] ref PropVariant propVar, [In]uint iElem, [Out, MarshalAs(UnmanagedType.Bool)] out bool pfVal);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            internal static extern void PropVariantGetInt16Elem([In] ref PropVariant propVar, [In] uint iElem, [Out] out short pnVal);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            internal static extern void PropVariantGetUInt16Elem([In] ref PropVariant propVar, [In] uint iElem, [Out] out ushort pnVal);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            internal static extern void PropVariantGetInt32Elem([In] ref PropVariant propVar, [In] uint iElem, [Out] out int pnVal);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            internal static extern void PropVariantGetUInt32Elem([In] ref PropVariant propVar, [In] uint iElem, [Out] out uint pnVal);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            internal static extern void PropVariantGetInt64Elem([In] ref PropVariant propVar, [In] uint iElem, [Out] out Int64 pnVal);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            internal static extern void PropVariantGetUInt64Elem([In] ref PropVariant propVar, [In] uint iElem, [Out] out UInt64 pnVal);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            internal static extern void PropVariantGetDoubleElem([In] ref PropVariant propVar, [In] uint iElem, [Out] out double pnVal);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            internal static extern void PropVariantGetFileTimeElem([In] ref PropVariant propVar, [In] uint iElem, [Out, MarshalAs(UnmanagedType.Struct)] out System.Runtime.InteropServices.ComTypes.FILETIME pftVal);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            internal static extern void PropVariantGetStringElem([In] ref PropVariant propVar, [In]  uint iElem, [Out, MarshalAs(UnmanagedType.LPWStr)] out string ppszVal);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            internal static extern void InitPropVariantFromBooleanVector([In, Out] bool[] prgf, uint cElems, out PropVariant ppropvar);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            internal static extern void InitPropVariantFromInt16Vector([In, Out] Int16[] prgn, uint cElems, out PropVariant ppropvar);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            internal static extern void InitPropVariantFromUInt16Vector([In, Out] UInt16[] prgn, uint cElems, out PropVariant ppropvar);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            internal static extern void InitPropVariantFromInt32Vector([In, Out] Int32[] prgn, uint cElems, out PropVariant ppropvar);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            internal static extern void InitPropVariantFromUInt32Vector([In, Out] UInt32[] prgn, uint cElems, out PropVariant ppropvar);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            internal static extern void InitPropVariantFromInt64Vector([In, Out] Int64[] prgn, uint cElems, out PropVariant ppropvar);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            internal static extern void InitPropVariantFromUInt64Vector([In, Out] UInt64[] prgn, uint cElems, out PropVariant ppropvar);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            internal static extern void InitPropVariantFromDoubleVector([In, Out] double[] prgn, uint cElems, out PropVariant ppropvar);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            internal static extern void InitPropVariantFromFileTimeVector([In, Out] System.Runtime.InteropServices.ComTypes.FILETIME[] prgft, uint cElems, out PropVariant ppropvar);

            [DllImport("propsys.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            internal static extern void InitPropVariantFromStringVector([In, Out] string[] prgsz, uint cElems, out PropVariant ppropvar);

        }

        #endregion

        #region Public methods

        public static PropVariant Empty
        {
            get
            {
                PropVariant empty = new PropVariant();
                empty.valueType = (ushort)VarEnum.VT_EMPTY;
                empty.wReserved1 = empty.wReserved2 = empty.wReserved3 = 0;
                empty.valueData = IntPtr.Zero;
                empty.valueDataExt = 0;
                return empty;
            }
        }

        /// <summary>
        /// Creates a PropVariant from an object
        /// </summary>
        /// <param name="value">The object containing the data.</param>
        /// <returns>An initialized PropVariant</returns>
        public static PropVariant FromObject(object value)
        {
            PropVariant propVar = new PropVariant();

            if (value == null)
            {
                propVar.Clear();
                return propVar;
            }

            if (value.GetType() == typeof(string))
            {
                //Strings require special consideration, because they cannot be empty as well
                if (String.IsNullOrEmpty(value as string) || String.IsNullOrEmpty((value as string).Trim()))
                    throw new ArgumentException("String argument cannot be null or empty.");
                propVar.SetString(value as string);
            }
            else if (value.GetType() == typeof(bool?))
            {
                propVar.SetBool((value as bool?).Value);
            }
            else if (value.GetType() == typeof(bool))
            {
                propVar.SetBool((bool)value);
            }
            else if (value.GetType() == typeof(byte?))
            {
                propVar.SetByte((value as byte?).Value);
            }
            else if (value.GetType() == typeof(byte))
            {
                propVar.SetByte((byte)value);
            }
            else if (value.GetType() == typeof(sbyte?))
            {
                propVar.SetSByte((value as sbyte?).Value);
            }
            else if (value.GetType() == typeof(sbyte))
            {
                propVar.SetSByte((sbyte)value);
            }
            else if (value.GetType() == typeof(short?))
            {
                propVar.SetShort((value as short?).Value);
            }
            else if (value.GetType() == typeof(short))
            {
                propVar.SetShort((short)value);
            }
            else if (value.GetType() == typeof(ushort?))
            {
                propVar.SetUShort((value as ushort?).Value);
            }
            else if (value.GetType() == typeof(ushort))
            {
                propVar.SetUShort((ushort)value);
            }
            else if (value.GetType() == typeof(int?))
            {
                propVar.SetInt((value as int?).Value);
            }
            else if (value.GetType() == typeof(int))
            {
                propVar.SetInt((int)value);
            }
            else if (value.GetType() == typeof(uint?))
            {
                propVar.SetUInt((value as uint?).Value);
            }
            else if (value.GetType() == typeof(uint))
            {
                propVar.SetUInt((uint)value);
            }
            else if (value.GetType() == typeof(long?))
            {
                propVar.SetLong((value as long?).Value);
            }
            else if (value.GetType() == typeof(long))
            {
                propVar.SetLong((long)value);
            }
            else if (value.GetType() == typeof(ulong?))
            {
                propVar.SetULong((value as ulong?).Value);
            }
            else if (value.GetType() == typeof(ulong))
            {
                propVar.SetULong((ulong)value);
            }
            else if (value.GetType() == typeof(double?))
            {
                propVar.SetDouble((value as double?).Value);
            }
            else if (value.GetType() == typeof(double))
            {
                propVar.SetDouble((double)value);
            }
            else if (value.GetType() == typeof(decimal?))
            {
                propVar.SetDecimal((value as decimal?).Value);
            }
            else if (value.GetType() == typeof(decimal))
            {
                propVar.SetDecimal((decimal)value);
            }
            else if (value.GetType() == typeof(DateTime?))
            {
                propVar.SetDateTime((value as DateTime?).Value);
            }
            else if (value.GetType() == typeof(DateTime))
            {
                propVar.SetDateTime((DateTime)value);
            }
            else if (value.GetType() == typeof(string[]))
            {
                propVar.SetStringVector((value as string[]));
            }
            else if (value.GetType() == typeof(short[]))
            {
                propVar.SetShortVector((value as short[]));
            }
            else if (value.GetType() == typeof(ushort[]))
            {
                propVar.SetUShortVector((value as ushort[]));
            }
            else if (value.GetType() == typeof(int[]))
            {
                propVar.SetIntVector((value as int[]));
            }
            else if (value.GetType() == typeof(uint[]))
            {
                propVar.SetUIntVector((value as uint[]));
            }
            else if (value.GetType() == typeof(long[]))
            {
                propVar.SetLongVector((value as long[]));
            }
            else if (value.GetType() == typeof(ulong[]))
            {
                propVar.SetULongVector((value as ulong[]));
            }
            else if (value.GetType() == typeof(DateTime[]))
            {
                propVar.SetDateTimeVector((value as DateTime[]));
            }
            else if (value.GetType() == typeof(bool[]))
            {
                propVar.SetBoolVector((value as bool[]));
            }
            else
            {
                throw new ArgumentException("This Value type is not supported.");
            }

            return propVar;
        }

        public bool IsNull()
        {
            return (valueType == (ushort)VarEnum.VT_EMPTY || valueType == (ushort)VarEnum.VT_NULL);
        }

        /// <summary>
        /// Called to clear the PropVariant's referenced and local memory.
        /// </summary>
        /// <remarks>
        /// You must call Clear to avoid memory leaks.
        /// </remarks>
        public void Clear()
        {
            // Can't pass "this" by ref, so make a copy to call PropVariantClear with
            PropVariant var = this;
            UnsafeNativeMethods.PropVariantClear(ref var);

            // Since we couldn't pass "this" by ref, we need to clear the member fields manually
            // NOTE: PropVariantClear already freed heap data for us, so we are just setting
            //       our references to null.
            valueType = (ushort)VarEnum.VT_EMPTY;
            wReserved1 = wReserved2 = wReserved3 = 0;
            valueData = IntPtr.Zero;
            valueDataExt = 0;
        }

        public PropVariant Clone()
        {
            // Can't pass "this" by ref, so make a bitwise copy on the stack, to call API with
            PropVariant var = this;

            PropVariant clone;
            UnsafeNativeMethods.PropVariantCopy(out clone, ref var);

            return clone;
        }

        /// <summary>
        /// Set an unsigned int value
        /// </summary>
        /// <param name="value">The new value to set.</param>
        public void SetUInt(UInt32 value)
        {
            if (!this.IsNull()) this.Clear();

            valueType = (ushort)VarEnum.VT_UI4;
            valueData = (IntPtr)((int)value);
        }

        /// <summary>
        /// Set a bool value
        /// </summary>
        /// <param name="value">The new value to set.</param>
        public void SetBool(bool value)
        {
            if (!this.IsNull()) this.Clear();

            valueType = (ushort)VarEnum.VT_BOOL;
            valueData = ((value == true) ? (IntPtr)65535 : (IntPtr)0);
        }

        /// <summary>
        /// Set a DateTime value
        /// </summary>
        /// <param name="value">The new value to set.</param>
        public void SetDateTime(DateTime value)
        {
            if (!this.IsNull()) this.Clear();
            
            valueType = (ushort)VarEnum.VT_FILETIME;

            PropVariant propVar;
            System.Runtime.InteropServices.ComTypes.FILETIME ft = DateTimeTotFileTime(value);
            UnsafeNativeMethods.InitPropVariantFromFileTime(ref ft, out propVar);
            CopyData(propVar);
        }

        /// <summary>
        /// Set a string value
        /// </summary>
        /// <param name="value">The new value to set.</param>
        public void SetString(string value)
        {
            if (!this.IsNull()) this.Clear();

            valueType = (ushort)VarEnum.VT_LPWSTR;
            valueData = Marshal.StringToCoTaskMemUni(value);
        }

        /// <summary>
        /// Set an IUnknown value
        /// </summary>
        /// <param name="value">The new value to set.</param>
        public void SetIUnknown(object value)
        {
            if (!this.IsNull()) this.Clear();

            valueType = (ushort)VarEnum.VT_UNKNOWN;
            valueData = Marshal.GetIUnknownForObject(value);
        }

        /// <summary>
        /// Set a safe array value
        /// </summary>
        /// <param name="array">The new value to set.</param>
        public void SetSafeArray(Array array)
        {
            if (!this.IsNull()) this.Clear();

            if (array == null) return;

            const ushort vtUnknown = 13;
            IntPtr psa = UnsafeNativeMethods.SafeArrayCreateVector(vtUnknown, 0, (uint)array.Length);

            IntPtr pvData = UnsafeNativeMethods.SafeArrayAccessData(psa);
            try // to remember to release lock on data
            {
                for (int i = 0; i < array.Length; ++i)
                {
                    object obj = array.GetValue(i);
                    IntPtr punk = (obj != null) ? Marshal.GetIUnknownForObject(obj) : IntPtr.Zero;
                    Marshal.WriteIntPtr(pvData, i * IntPtr.Size, punk);
                }
            }
            finally
            {
                UnsafeNativeMethods.SafeArrayUnaccessData(psa);
            }

            this.valueType = (ushort)VarEnum.VT_ARRAY | (ushort)VarEnum.VT_UNKNOWN;
            this.valueData = psa;
        }

        /// <summary>
        /// Set a byte value
        /// </summary>
        /// <param name="value">The new value to set.</param>
        public void SetByte(byte value)
        {
            if (!this.IsNull()) this.Clear();
            
            valueType = (ushort)VarEnum.VT_UI1;
            valueData = (IntPtr)value;
        }

        /// <summary>
        /// Set a sbyte value
        /// </summary>
        /// <param name="value">The new value to set.</param>
        public void SetSByte(sbyte value)
        {
            if (!this.IsNull()) this.Clear();

            valueType = (ushort)VarEnum.VT_I1;
            valueData = (IntPtr)value;
        }

        /// <summary>
        /// Set a short value
        /// </summary>
        /// <param name="value">The new value to set.</param>
        public void SetShort(short value)
        {
            if (!this.IsNull()) this.Clear();

            valueType = (ushort)VarEnum.VT_I2;
            valueData = (IntPtr)value;
        }

        /// <summary>
        /// Set an unsigned short value
        /// </summary>
        /// <param name="value">The new value to set.</param>
        public void SetUShort(ushort value)
        {
            if (!this.IsNull()) this.Clear();
            
            valueType = (ushort)VarEnum.VT_UI2;
            valueData = (IntPtr)value;
        }

        /// <summary>
        /// Set an int value
        /// </summary>
        /// <param name="value">The new value to set.</param>
        public void SetInt(int value)
        {
            if (!this.IsNull()) this.Clear();
            
            valueType = (ushort)VarEnum.VT_I4;
            valueData = (IntPtr)value;
        }

        /// <summary>
        /// Set an uint vector
        /// </summary>
        /// <param name="array">The new value to set.</param>
        public void SetUIntVector(uint[] array)
        {
            if (!this.IsNull()) this.Clear();

            if (array == null) return;

            PropVariant propVar;
            UnsafeNativeMethods.InitPropVariantFromUInt32Vector(array, (uint)array.Length, out propVar);
            CopyData(propVar);
        }

        /// <summary>
        /// Set a string vector
        /// </summary>
        /// <param name="array">The new value to set.</param>
        public void SetStringVector(string[] array)
        {
            if (!this.IsNull()) this.Clear();

            if (array == null) return;

            PropVariant propVar;
            UnsafeNativeMethods.InitPropVariantFromStringVector(array, (uint)array.Length, out propVar);
            CopyData(propVar);
        }

        /// <summary>
        /// Set a bool vector
        /// </summary>
        /// <param name="array">The new value to set.</param>
        public void SetBoolVector(bool[] array)
        {
            if (!this.IsNull()) this.Clear();

            if (array == null) return;

            PropVariant propVar;
            UnsafeNativeMethods.InitPropVariantFromBooleanVector(array, (uint)array.Length, out propVar);
            CopyData(propVar);
        }

        /// <summary>
        /// Set a short vector
        /// </summary>
        /// <param name="array">The new value to set.</param>
        public void SetShortVector(short[] array)
        {
            if (!this.IsNull()) this.Clear();

            if (array == null) return;
            
            PropVariant propVar;
            UnsafeNativeMethods.InitPropVariantFromInt16Vector(array, (uint)array.Length, out propVar);
            CopyData(propVar);
        }

        /// <summary>
        /// Set a short vector
        /// </summary>
        /// <param name="array">The new value to set.</param>
        public void SetUShortVector(ushort[] array)
        {
            if (!this.IsNull()) this.Clear();

            if (array == null) return;
            
            PropVariant propVar;
            UnsafeNativeMethods.InitPropVariantFromUInt16Vector(array, (uint)array.Length, out propVar);
            CopyData(propVar);
        }

        /// <summary>
        /// Set an int vector
        /// </summary>
        /// <param name="array">The new value to set.</param>
        public void SetIntVector(int[] array)
        {
            if (!this.IsNull()) this.Clear();

            if (array == null) return;

            PropVariant propVar;
            UnsafeNativeMethods.InitPropVariantFromInt32Vector(array, (uint)array.Length, out propVar);
            CopyData(propVar);
        }

        /// <summary>
        /// Set a long vector
        /// </summary>
        /// <param name="array">The new value to set.</param>
        public void SetLongVector(long[] array)
        {
            if (!this.IsNull()) this.Clear();

            if (array == null) return;

            PropVariant propVar;
            UnsafeNativeMethods.InitPropVariantFromInt64Vector(array, (uint)array.Length, out propVar);
            CopyData(propVar);
        }

        /// <summary>
        /// Set a ulong vector
        /// </summary>
        /// <param name="value">The new value to set.</param>
        public void SetULongVector(ulong[] array)
        {
            if (!this.IsNull()) this.Clear();

            if (array == null) return;

            PropVariant propVar;
            UnsafeNativeMethods.InitPropVariantFromUInt64Vector(array, (uint)array.Length, out propVar);
            CopyData(propVar);
        }

        /// <summary>
        /// Set a double vector
        /// </summary>
        /// <param name="array">The new value to set.</param>
        public void SetDoubleVector(double[] array)
        {
            if (!this.IsNull()) this.Clear();

            if (array == null) return;

            PropVariant propVar;
            UnsafeNativeMethods.InitPropVariantFromDoubleVector(array, (uint)array.Length, out propVar);
            CopyData(propVar);
        }

        /// <summary>
        /// Set a DateTime vector
        /// </summary>
        /// <param name="array">The new value to set.</param>
        public void SetDateTimeVector(DateTime[] array)
        {
            if (!this.IsNull()) this.Clear();

            if (array == null) return;

            System.Runtime.InteropServices.ComTypes.FILETIME[] fileTimeArr =
                new System.Runtime.InteropServices.ComTypes.FILETIME[array.Length];

            for (int i = 0; i < array.Length; i++)
            {
                fileTimeArr[i] = DateTimeTotFileTime(array[i]);
            }

            PropVariant propVar;
            UnsafeNativeMethods.InitPropVariantFromFileTimeVector(fileTimeArr, (uint)fileTimeArr.Length, out propVar);
            CopyData(propVar);
        }

        /// <summary>
        /// Set a decimal  value
        /// </summary>
        /// <param name="value">The new value to set.</param>
        public void SetDecimal(decimal value)
        {
            if (!this.IsNull()) this.Clear();

            PVDecimalOuterUnion union = new PVDecimalOuterUnion();
            union.decVal = value;

            this = union.propVar;

            // Note we must set vt after writing the 16-byte decimal value (because it overwrites all 16 bytes)!
            valueType = (ushort)VarEnum.VT_DECIMAL;
        }

        /// <summary>
        /// Set a long
        /// </summary>
        /// <param name="value">The new value to set.</param>
        public void SetLong(long value)
        {
            if (!this.IsNull()) this.Clear();
            
            long[] valueArr = new long[] { value };

            PropVariant propVar;
            UnsafeNativeMethods.InitPropVariantFromInt64Vector(valueArr, 1, out propVar);

            CreatePropVariantFromVectorElement(propVar);
        }

        /// <summary>
        /// Set a ulong
        /// </summary>
        /// <param name="value">The new value to set.</param>
        public void SetULong(ulong value)
        {
            if (!this.IsNull()) this.Clear();
            
            PropVariant propVar;
            ulong[] valueArr = new ulong[] { value };
            UnsafeNativeMethods.InitPropVariantFromUInt64Vector(valueArr, 1, out propVar);

            CreatePropVariantFromVectorElement(propVar);
        }

        /// <summary>
        /// Set a double
        /// </summary>
        /// <param name="value">The new value to set.</param>
        public void SetDouble(double value)
        {
            if (!this.IsNull()) this.Clear();

            double[] valueArr = new double[] { value };

            PropVariant propVar;
            UnsafeNativeMethods.InitPropVariantFromDoubleVector(valueArr, 1, out propVar);

            CreatePropVariantFromVectorElement(propVar);
        }


        public static bool operator ==(PropVariant left, PropVariant right)
        {
            throw new NotImplementedException();
        }

        public static bool operator !=(PropVariant left, PropVariant right)
        {
            throw new NotImplementedException();
        }

        public override string ToString()
        {
            if (IsNull()) return "(null)";

            return "PropVariant: " + (VarEnum)valueType + ":" + Value;
        }

        public override bool Equals(object obj)
        {
            throw new NotImplementedException();
        }

        public override int GetHashCode()
        {
            throw new NotImplementedException();
        }

        #endregion

        #region Public properties

        public VarEnum VarType
        {
            get { return (VarEnum)valueType; }
        }

        public object Value
        {
            get
            {
                switch ((VarEnum)valueType)
                {
                    case VarEnum.VT_I1:
                        return cVal;
                    case VarEnum.VT_UI1:
                        return bVal;
                    case VarEnum.VT_I2:
                        return iVal;
                    case VarEnum.VT_UI2:
                        return uiVal;
                    case VarEnum.VT_I4:
                    case VarEnum.VT_INT:
                        return lVal;
                    case VarEnum.VT_UI4:
                    case VarEnum.VT_UINT:
                        return ulVal;
                    case VarEnum.VT_I8:
                        return hVal;
                    case VarEnum.VT_UI8:
                        return uhVal;
                    case VarEnum.VT_R4:
                        return fltVal;
                    case VarEnum.VT_R8:
                        return dblVal;
                    case VarEnum.VT_BOOL:
                        return boolVal;
                    case VarEnum.VT_ERROR:
                        return scode;
                    case VarEnum.VT_CY:
                        return cyVal;
                    case VarEnum.VT_DATE:
                        return date;
                    case VarEnum.VT_FILETIME:
                        return DateTime.FromFileTime(hVal);
                    case VarEnum.VT_BSTR:
                        return Marshal.PtrToStringBSTR(valueData);
                    case VarEnum.VT_BLOB:
                        return GetBlobData();
                    case VarEnum.VT_LPSTR:
                        return Marshal.PtrToStringAnsi(valueData);
                    case VarEnum.VT_LPWSTR:
                        return Marshal.PtrToStringUni(valueData);
                    case VarEnum.VT_UNKNOWN:
                        return Marshal.GetObjectForIUnknown(valueData);
                    case VarEnum.VT_DISPATCH:
                        return Marshal.GetObjectForIUnknown(valueData);
                    case VarEnum.VT_DECIMAL:
                        return CrackDecimal();
                    case VarEnum.VT_ARRAY | VarEnum.VT_UNKNOWN:
                        return CrackSingleDimSafeArray(valueData);
                    case (VarEnum.VT_VECTOR | VarEnum.VT_LPWSTR):
                        return GetStringVector();
                    case (VarEnum.VT_VECTOR | VarEnum.VT_I2):
                        return GetVector<Int16>();
                    case (VarEnum.VT_VECTOR | VarEnum.VT_UI2):
                        return GetVector<UInt16>();
                    case (VarEnum.VT_VECTOR | VarEnum.VT_I4):
                        return GetVector<Int32>();
                    case (VarEnum.VT_VECTOR | VarEnum.VT_UI4):
                        return GetVector<UInt32>();
                    case (VarEnum.VT_VECTOR | VarEnum.VT_I8):
                        return GetVector<Int64>();
                    case (VarEnum.VT_VECTOR | VarEnum.VT_UI8):
                        return GetVector<UInt64>();
                    case (VarEnum.VT_VECTOR | VarEnum.VT_R8):
                        return GetVector<Double>();
                    case (VarEnum.VT_VECTOR | VarEnum.VT_BOOL):
                        return GetVector<Boolean>();
                    case (VarEnum.VT_VECTOR | VarEnum.VT_FILETIME):
                        return GetVector<DateTime>();
                    default:
                        throw new NotSupportedException("The type of this variable is not support ('" + valueType.ToString(CultureInfo.CurrentCulture.NumberFormat) + "')");
                }
            }
        }

        #endregion

        #region Private methods

        private void CopyData(PropVariant propVar)
        {
            this.valueType = propVar.valueType;
            this.valueData = propVar.valueData;
            this.valueDataExt = propVar.valueDataExt;
        }

        private void CreatePropVariantFromVectorElement(PropVariant propVar)
        {
            //Copy the first vector element to a new PropVariant
            CopyData(propVar);
            UnsafeNativeMethods.InitPropVariantFromPropVariantVectorElem(ref this, 0, out propVar);

            //Overwrite the existing data
            CopyData(propVar);
        }

        private static long FileTimeToDateTime(ref System.Runtime.InteropServices.ComTypes.FILETIME val)
        {
            return (((long)val.dwHighDateTime) << 32) + val.dwLowDateTime;
        }

        private static System.Runtime.InteropServices.ComTypes.FILETIME DateTimeTotFileTime(DateTime value)
        {
            long hFT = value.ToFileTime();
            System.Runtime.InteropServices.ComTypes.FILETIME ft =
                new System.Runtime.InteropServices.ComTypes.FILETIME();
            ft.dwLowDateTime = (int)(hFT & 0xFFFFFFFF);
            ft.dwHighDateTime = (int)(hFT >> 32);
            return ft;
        }

        private object GetBlobData()
        {
            byte[] blobData = new byte[lVal];
            IntPtr pBlobData;
            if (IntPtr.Size == 4)
            {
                pBlobData = new IntPtr(valueDataExt);
            }
            else if (IntPtr.Size == 8)
            {
                // In this case, we need to derive a pointer at offset 12,
                // because the size of the blob is represented as a 4-byte int
                // but the pointer is immediately after that.
                pBlobData = new IntPtr(
                    (Int64)(BitConverter.ToInt32(GetDataBytes(), sizeof(int))) +
                    (Int64)(BitConverter.ToInt32(GetDataBytes(), 2 * sizeof(int)) << 32));
            }
            else
            {
                throw new NotSupportedException();
            }
            Marshal.Copy(pBlobData, blobData, 0, lVal);

            return blobData;
        }

        private Array GetVector<T>() where T : struct
        {
            int count = UnsafeNativeMethods.PropVariantGetElementCount(ref this);
            if (count <= 0)
                return null;

            Array arr = new T[count];

            for (uint i = 0; i < count; i++)
            {
                if (typeof(T) == typeof(Int16))
                {
                    short val;
                    UnsafeNativeMethods.PropVariantGetInt16Elem(ref this, i, out val);
                    arr.SetValue(val, i);
                }
                else if (typeof(T) == typeof(UInt16))
                {
                    ushort val;
                    UnsafeNativeMethods.PropVariantGetUInt16Elem(ref this, i, out val);
                    arr.SetValue(val, i);
                }
                else if (typeof(T) == typeof(Int32))
                {
                    int val;
                    UnsafeNativeMethods.PropVariantGetInt32Elem(ref this, i, out val);
                    arr.SetValue(val, i);
                }
                else if (typeof(T) == typeof(UInt32))
                {
                    uint val;
                    UnsafeNativeMethods.PropVariantGetUInt32Elem(ref this, i, out val);
                    arr.SetValue(val, i);
                }
                else if (typeof(T) == typeof(Int64))
                {
                    long val;
                    UnsafeNativeMethods.PropVariantGetInt64Elem(ref this, i, out val);
                    arr.SetValue(val, i);
                }
                else if (typeof(T) == typeof(UInt64))
                {
                    ulong val;
                    UnsafeNativeMethods.PropVariantGetUInt64Elem(ref this, i, out val);
                    arr.SetValue(val, i);
                }
                else if (typeof(T) == typeof(DateTime))
                {
                    System.Runtime.InteropServices.ComTypes.FILETIME val;
                    UnsafeNativeMethods.PropVariantGetFileTimeElem(ref this, i, out val);

                    long fileTime = FileTimeToDateTime(ref val);


                    arr.SetValue(DateTime.FromFileTime(fileTime), i);
                }
                else if (typeof(T) == typeof(Boolean))
                {
                    bool val;
                    UnsafeNativeMethods.PropVariantGetBooleanElem(ref this, i, out val);
                    arr.SetValue(val, i);
                }
                else if (typeof(T) == typeof(Double))
                {
                    double val;
                    UnsafeNativeMethods.PropVariantGetDoubleElem(ref this, i, out val);
                    arr.SetValue(val, i);
                }
                else if (typeof(T) == typeof(String))
                {
                    string val;
                    UnsafeNativeMethods.PropVariantGetStringElem(ref this, i, out val);
                    arr.SetValue(val, i);
                }
            }

            return arr;
        }

        // A string requires a special case because it's not a struct or value type
        private string[] GetStringVector()
        {
            int count = UnsafeNativeMethods.PropVariantGetElementCount(ref this);
            if (count <= 0)
                return null;

            string[] strArr = new string[count];
            for (uint i = 0; i < count; i++)
            {
                UnsafeNativeMethods.PropVariantGetStringElem(ref this, i, out strArr[i]);
            }

            return strArr;
        }

        /// <summary>
        /// Gets a byte array containing the data bits of the struct.
        /// </summary>
        /// <returns>A byte array that is the combined size of the data bits.</returns>
        byte[] GetDataBytes()
        {
            byte[] ret = new byte[IntPtr.Size + sizeof(int)];
            if (IntPtr.Size == 4)
                BitConverter.GetBytes(valueData.ToInt32()).CopyTo(ret, 0);
            else if (IntPtr.Size == 8)
                BitConverter.GetBytes(valueData.ToInt64()).CopyTo(ret, 0);
            BitConverter.GetBytes(valueDataExt).CopyTo(ret, IntPtr.Size);
            return ret;
        }

        /// <summary>
        /// Marshals an unmanaged SafeArray to a managed Array object.
        /// </summary>
        static Array CrackSingleDimSafeArray(IntPtr psa)
        {
            uint cDims = UnsafeNativeMethods.SafeArrayGetDim(psa);
            if (cDims != 1) throw new ArgumentException("Multi-dimensional SafeArrays not supported.");

            int lBound = UnsafeNativeMethods.SafeArrayGetLBound(psa, 1U);
            int uBound = UnsafeNativeMethods.SafeArrayGetUBound(psa, 1U);

            int n = uBound - lBound + 1; // uBound is inclusive

            object[] array = new object[n];
            for (int i = lBound; i <= uBound; ++i)
            {
                array[i] = UnsafeNativeMethods.SafeArrayGetElement(psa, ref i);
            }

            return array;
        }

        decimal CrackDecimal()
        {
            // Use a [FieldOffset] union to magically transform our PropVariant into a decimal.
            PVDecimalOuterUnion union = new PVDecimalOuterUnion();
            union.propVar = this;
            decimal value = union.decVal;
            return value;
        }

        //uint[] CrackUIntVector()
        //{
        //    PVVectorOuterUnion union = new PVVectorOuterUnion();
        //    union.propVar = this;
        //    uint[] value = new uint[union.cElems];
        //    for (int i = 0; i < union.cElems; ++i)
        //    {
        //        value[i] = (uint)Marshal.ReadInt32(union.pElems, i * IntPtr.Size);
        //    }
            
        //    return value;
        //}

        //string[] CrackStringVector()
        //{
        //    PVVectorOuterUnion union = new PVVectorOuterUnion();
        //    union.propVar = this;
        //    string[] value = new string[union.cElems];
        //    for (int i = 0; i < union.cElems; ++i)
        //    {
        //        IntPtr pstr = (IntPtr)Marshal.ReadIntPtr(union.pElems, i * IntPtr.Size);
        //        value[i] = Marshal.PtrToStringUni(pstr);
        //    }

        //    return value;
        //}

        #endregion

        #region Private properties

        sbyte cVal // CHAR cVal;
        {
            get { return (sbyte)GetDataBytes()[0]; }
        }

        byte bVal // UCHAR bVal;
        {
            get { return GetDataBytes()[0]; }
        }

        short iVal // SHORT iVal;
        {
            get { return BitConverter.ToInt16(GetDataBytes(), 0); }
        }

        ushort uiVal // USHORT uiVal;
        {
            get { return BitConverter.ToUInt16(GetDataBytes(), 0); }
        }

        int lVal // LONG lVal;
        {
            get { return BitConverter.ToInt32(GetDataBytes(), 0); }
        }

        uint ulVal // ULONG ulVal;
        {
            get { return BitConverter.ToUInt32(GetDataBytes(), 0); }
        }

        long hVal // LARGE_INTEGER hVal;
        {
            get { return BitConverter.ToInt64(GetDataBytes(), 0); }
        }

        ulong uhVal // ULARGE_INTEGER uhVal;
        {
            get { return BitConverter.ToUInt64(GetDataBytes(), 0); }
        }

        float fltVal // FLOAT fltVal;
        {
            get { return BitConverter.ToSingle(GetDataBytes(), 0); }
        }

        double dblVal // DOUBLE dblVal;
        {
            get { return BitConverter.ToDouble(GetDataBytes(), 0); }
        }

        bool boolVal // VARIANT_BOOL boolVal;
        {
            get { return (iVal == 0 ? false : true); }
        }

        int scode // SCODE scode;
        {
            get { return lVal; }
        }

        decimal cyVal // CY cyVal;
        {
            get { return decimal.FromOACurrency(hVal); }
        }

        DateTime date // DATE date;
        {
            get { return DateTime.FromOADate(dblVal); }
        }

        #endregion

        #region Private fields

        // The layout of these elements needs to be maintained.
        //
        // NOTE: We could use LayoutKind.Explicit, but we want
        //       to maintain that the IntPtr may be 8 bytes on
        //       64-bit architectures, so we'll let the CLR keep
        //       us aligned.
        //
        // NOTE: In order to allow x64 compat, we need to allow for
        //       expansion of the IntPtr. However, the BLOB struct
        //       uses a 4-byte int, followed by an IntPtr, so
        //       although the p field catches most pointer values,
        //       we need an additional 4-bytes to get the BLOB
        //       pointer. The p2 field provides this, as well as
        //       the last 4-bytes of an 8-byte value on 32-bit
        //       architectures.

        ushort valueType;
        ushort wReserved1;
        ushort wReserved2;
        ushort wReserved3;
        IntPtr valueData;
        int valueDataExt;

        #endregion

    }

    // It is sometimes useful to represent the struct as a reference-type 
    // (eg, for methods that allow passing a null PropertyKey pointer).
    [StructLayout(LayoutKind.Sequential)]
    public class PropVariantRef
    {
        public PropVariant PropVariant;

        public static PropVariantRef From(PropVariant value)
        {
            PropVariantRef obj = new PropVariantRef();
            obj.PropVariant = value;
            return obj;
        }
    }
}
