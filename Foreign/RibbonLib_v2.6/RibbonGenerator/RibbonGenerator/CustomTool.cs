using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;
using Microsoft.Win32;
using System.IO;
using System.Globalization;
using Microsoft.VisualStudio.Shell.Interop;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Shell;

namespace RibbonGenerator
{
    /// <summary>
    /// Defines the ribbon generator custom tool for visual studio
    /// </summary>
    [Guid("B64582D9-A489-42F4-BA55-BB6039D82916")]
    public class CustomTool : VsMultipleFileGenerator<Target>
    {
        /// <summary>
        /// Contains the ribbon generator manager class.
        /// </summary>
        Manager _manager;
        /// <summary>
        /// Contains the output class for visual studio. 
        /// </summary>
        VsMessageOutput _outputWindow;
        /// <summary>
        /// Returns ".log"
        /// </summary>
        /// <returns>.log</returns>
        public override int DefaultExtension(out string extension)        
        {
            extension = ".log";
            return VSConstants.S_OK;
        }
        /// <summary>
        /// Returns ".ribbon"
        /// </summary>
        /// <returns>.ribbon</returns>
        public override string GetMultiExtension()
        {
            return Manager.RIBBONEXTENSION;
        }
        /// <summary>
        /// Creates the report of the generation
        /// </summary>
        /// <returns>string in bytes</returns>
        public override byte[] GenerateSummaryContent()
        {
            StringBuilder sb = new StringBuilder();
            sb.AppendLine("This file was generated. The .ribbon files contains are the localized ribbon ui dlls");
            sb.AppendLine(string.Format("Last execution: {0}", DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss")));
            sb.AppendLine(string.Format("Targets: {0}", this._manager.Targets.Count));
            sb.AppendLine("List of targets:");
            foreach (var target in this._manager.Targets)
            {
                sb.AppendLine(string.Format("- RibbonFilename: {0}, Localize: {1}, Culture: {2}, ResourceFilename: {3}",
                    target.RibbonFilename, target.Localize, target.CultureName, target.ResourceFilename));
            }

            sb.AppendLine("Output:");
            sb.AppendLine(_outputWindow.GetOutput());

            var result = ASCIIEncoding.Unicode.GetBytes(sb.ToString());
            return result;
        }

        /// <summary>
        /// Returns an enumerator of targets
        /// </summary>
        /// <returns>target enumerator</returns>
        public override IEnumerator<Target> GetEnumerator()
        {
            if (_outputWindow != null)
                _outputWindow.Dispose();

            _outputWindow = new VsMessageOutput();
            _manager = new Manager(_outputWindow, this.InputFilePath, this.InputFileContents);
            Util.LogMessage("GetEnumerator returns a list of targets. Count is {0}", _manager.Targets.Count);

            return _manager.Targets.GetEnumerator();
        }
        /// <summary>
        /// Generates the content of the ribbon ui dll.
        /// </summary>
        /// <param name="element">the target element</param>
        /// <returns>the ribbon ui dll in bytes</returns>
        public override byte[] GenerateContent(Target element)
        {
            try
            {
                Util.LogMessage("GenerateContent with target '{0}'", element.RibbonFilename);
                byte[] result = _manager.CreateRibbon(element);
                return result;
            }
            catch (Exception ex)
            {
                string message = string.Format("Generation failed. Error: {0}", ex.Message);
                this.GeneratorProgress.GeneratorError(Convert.ToInt32(false), (uint)0, message, (uint)0, (uint)0);
                throw ex;
            }
        }
        /// <summary>
        /// Gets the name of the ribbon ui dll file
        /// </summary>
        /// <param name="element">the target element</param>
        /// <returns>the filename</returns>
        protected override string GetFileName(Target element)
        {
            string filename = Path.GetFileName(element.RibbonFilename);
            Util.LogMessage("GetFileName returns {0}", filename);
            return filename;
        }
    }
}
