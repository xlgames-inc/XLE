using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Globalization;

namespace RibbonGenerator.Console
{
    class Program
    {
        static void Main(string[] args)
        {
            if (args.Length == 0)
                return;

            try
            {
                string fileName = args[0];
                string content = File.ReadAllText(fileName);
                Manager manager = new Manager(new ConsoleMessageOutput(), fileName, content);

                var targets = manager.Targets;
                foreach (var target in targets)
                {
                    var buffer = manager.CreateRibbon(target);
                    File.WriteAllBytes(target.RibbonFilename, buffer);
                }
            }
            catch (Exception ex)
            {
                System.Console.Error.WriteLine(ex.Message);
            }
        }
    }
}
