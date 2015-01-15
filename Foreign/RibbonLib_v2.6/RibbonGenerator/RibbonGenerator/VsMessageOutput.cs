using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.IO;

namespace RibbonGenerator
{
    /// <summary>
    /// Used in custom tool. Write the output to a stream.
    /// </summary>
    class VsMessageOutput : IMessageOutput, IDisposable
    {
        StringWriter _writer = new StringWriter();

        public string GetOutput()
        {
            var result = _writer.ToString();
            return result;
        }

        #region IMessageOutput Members

        public void WriteLine(string value)
        {
            Trace.WriteLine(value);
            _writer.WriteLine(value);
        }

        #endregion

        #region IDisposable Members

        public void Dispose()
        {
            _writer.Dispose();
        }

        #endregion
    }
}
