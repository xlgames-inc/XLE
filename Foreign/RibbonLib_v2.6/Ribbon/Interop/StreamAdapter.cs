//*****************************************************************************
//
//  File:       StreamAdapter.cs
//
//  Contents:   Helper class that wraps a .NET stream class as a COM IStream
//
//*****************************************************************************

using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;

namespace RibbonLib.Interop
{
    public class StreamAdapter : IStream
    {
        private Stream _stream;

        public StreamAdapter(Stream stream)
        {
            if (stream == null)
            {
                throw new ArgumentNullException("stream");
            }

            _stream = stream;
        }

        #region IStream Members

        public void Clone(out IStream streamCopy)
        {
            streamCopy = null;
            throw new NotSupportedException();
        }

        public void Commit(int flags)
        {
            throw new NotSupportedException();
        }

        public void CopyTo(IStream targetStream, long bufferSize, IntPtr buffer, IntPtr bytesWrittenPtr)
        {
            throw new NotSupportedException();
        }

        public void LockRegion(long offset, long byteCount, int lockType)
        {
            throw new NotSupportedException();
        }

        public void Read(byte[] buffer, int bufferSize, IntPtr bytesReadPtr)
        {
            int val = _stream.Read(buffer, 0, bufferSize);
            if (bytesReadPtr != IntPtr.Zero)
            {
                Marshal.WriteInt32(bytesReadPtr, val);
            }
        }

        public void Revert()
        {
            throw new NotSupportedException();
        }

        public void Seek(long offset, int origin, IntPtr newPositionPtr)
        {
            SeekOrigin begin;
            switch (origin)
            {
                case 0:
                    begin = SeekOrigin.Begin;
                    break;

                case 1:
                    begin = SeekOrigin.Current;
                    break;

                case 2:
                    begin = SeekOrigin.End;
                    break;

                default:
                    throw new ArgumentOutOfRangeException("origin");
            }
            long val = _stream.Seek(offset, begin);
            if (newPositionPtr != IntPtr.Zero)
            {
                Marshal.WriteInt64(newPositionPtr, val);
            }
        }

        public void SetSize(long libNewSize)
        {
            _stream.SetLength(libNewSize);
        }

        public void Stat(out System.Runtime.InteropServices.ComTypes.STATSTG streamStats, int grfStatFlag)
        {
            streamStats = new System.Runtime.InteropServices.ComTypes.STATSTG();
            streamStats.type = 2;
            streamStats.cbSize = _stream.Length;
            streamStats.grfMode = 0;
            if (_stream.CanRead && _stream.CanWrite)
            {
                streamStats.grfMode |= 2;
            }
            else if (_stream.CanRead)
            {
                //streamStats.grfMode = streamStats.grfMode;
            }
            else
            {
                if (!_stream.CanWrite)
                {
                    throw new IOException("StreamObjectDisposed");
                }
                streamStats.grfMode |= 1;
            }
        }

        public void UnlockRegion(long offset, long byteCount, int lockType)
        {
            throw new NotSupportedException();
        }

        public void Write(byte[] buffer, int bufferSize, IntPtr bytesWrittenPtr)
        {
            _stream.Write(buffer, 0, bufferSize);
            if (bytesWrittenPtr != IntPtr.Zero)
            {
                Marshal.WriteInt32(bytesWrittenPtr, bufferSize);
            }
        }

        #endregion
    }
}
