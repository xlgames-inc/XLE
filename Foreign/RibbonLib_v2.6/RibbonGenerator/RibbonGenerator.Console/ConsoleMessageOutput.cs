using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace RibbonGenerator.Console
{
    /// <summary>
    /// Writes the output to the console window.
    /// </summary>
    class ConsoleMessageOutput : IMessageOutput
    {
        #region IOutput Members

        public void WriteLine(string value)
        {
            System.Console.WriteLine(value);
        }
        
        #endregion
    }
}
