using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace RibbonGenerator
{
    public class Target
    {
        public string CultureName { get; set; }
        public string ResourceFilename { get; set; }
        public string RibbonFilename { get; set; }
        public bool Localize { get; set; }
    }
}
