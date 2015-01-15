using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace RibbonGenerator
{
    /// <summary>
    /// Common interface to forward message output for console and visual studio custom tool
    /// </summary>
    public interface IMessageOutput
    {
        void WriteLine(string value);
    }
}
