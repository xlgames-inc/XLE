using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Resources;
using System.Globalization;
using System.Reflection;

namespace RibbonGenerator
{
    public class Manager
    {
        public const string RESXEXTENSION = ".resx";
        public const string RIBBONEXTENSION = ".ribbon";

        IMessageOutput _output;

        public Manager(IMessageOutput output, string ribbonXmlFilename, string ribbonXmlContent)
        {
            _output = output;

            _ribbonXmlFilename = ribbonXmlFilename;
            _ribbonXmlContent = ribbonXmlContent;

            Initialize();
        }

        List<string> _cleanupFiles = new List<string>();

        public List<string> CleanupFiles
        {
            get { return _cleanupFiles; }
            set { _cleanupFiles = value; }
        }

        string _ribbonXmlFilename;

        public string RibbonXmlFilename
        {
            get
            { 
                return _ribbonXmlFilename; 
            }
        }

        string _ribbonXmlContent;

        public string RibbonXmlContent
        {
            get
            { 
                return _ribbonXmlContent; 
            }
        }

        List<Target> _targets;

        public List<Target> Targets
        {
            get
            {
                return _targets;
            }
        }

        /// <summary>
        /// Returns ".ribbon" for empty culturename and ".de.ribbon" for cultureName 'de'.
        /// </summary>
        /// <param name="cultureName">the name of the culture</param>
        /// <returns>the localized ribbon file extension</returns>
        string GetRibbonExtension(string cultureName)
        {
            if (string.IsNullOrEmpty(cultureName))
                return RIBBONEXTENSION;

            var result = string.Format(".{0}{1}", cultureName, RIBBONEXTENSION);
            return result;
        }

        void Initialize()
        {
            try
            {
                string path = Path.GetDirectoryName(RibbonXmlFilename);
                string filenameWithoutExtension = Path.GetFileNameWithoutExtension(RibbonXmlFilename);

                string fullFilenameWithoutExtension = Path.Combine(path, filenameWithoutExtension);
                string resourceFullFilename = AddFileExtension(fullFilenameWithoutExtension, RESXEXTENSION);

                List<Target> targets = new List<Target>();

                var localize = File.Exists(resourceFullFilename);

                // create the default target = .ribbon file
                var target = new Target()
                {
                    Localize = localize,
                    ResourceFilename = localize ? resourceFullFilename : null,
                    RibbonFilename = AddFileExtension(fullFilenameWithoutExtension, GetRibbonExtension(null))
                };

                targets.Add(target);

                // search for localized ResX files
                string searchPattern = string.Format("{0}.*{1}", Path.GetFileName(fullFilenameWithoutExtension), RESXEXTENSION);

                var localizedFiles = Directory.GetFiles(path, searchPattern);
                foreach (var file in localizedFiles)
                {
                    // create localized targets = for example: .de.ribbon file
                    string cultureName = GetCultureName(file);
                    target = new Target()
                    {
                        Localize = true,
                        CultureName = cultureName,
                        RibbonFilename = AddFileExtension(fullFilenameWithoutExtension, GetRibbonExtension(cultureName)),
                        ResourceFilename = file
                    };
                    targets.Add(target);
                }

                _targets = targets; 

                // if there are ResX files for the ribbons create a ResXReader
                if (target.Localize)
                    _resXReader = new ResXReader(targets);

                Util.LogMessage("Manager.Initialize returns {0} targets and localize set to {1}", targets.Count, _resXReader != null);
            }
            catch (Exception ex)
            {
                Util.LogError(ex);
                throw ex;
            }
        }

        string GetCultureName(string file)
        {
            var firstExtensionExcluded = Path.GetFileNameWithoutExtension(file);
            var cultureNameExtension = Path.GetExtension(firstExtensionExcluded);
            if (!cultureNameExtension.StartsWith("."))
                return null;
            var cultureName = cultureNameExtension.Substring(1);
            return cultureName;
        }

        string AddFileExtension(string fullFilenameWithoutExtension, string extension)
        {
            return string.Format("{0}{1}", fullFilenameWithoutExtension, extension);
        }

        ResXReader _resXReader;

        ResXReader ResXReader
        {
            get
            {
                return _resXReader;
            }
        }

        public byte[] CreateRibbon(Target element)
        {
            CleanupFiles.Clear();

            try
            {
                VerifyTemplateBat();

                // use the following format to specify localization info {Resource:key}
                string localizedRibbonXmlFilename = LocalizeRibbon(element);

                // create the ribbon dll
                string ribbonDll = ConvertXmlToDll(localizedRibbonXmlFilename);

                // return the content of the ribbon dll
                var result = File.ReadAllBytes(ribbonDll);

                Util.LogMessage("Manager.CreateRibbon returns {0} bytes for file '{1}'", result.Length, ribbonDll);
                return result;
            }
            catch (Exception ex)
            {
                Util.LogError(ex);
                throw ex;
            }
            finally
            {
                Cleanup();
            }
        }

        /// <summary>
        /// Verifies that templte.bat file exists, if not, it will be created here.
        /// </summary>
        void VerifyTemplateBat()
        {
            if (File.Exists(Util.TemplateBatFilename))
            {
                this._output.WriteLine(string.Format("Verify Template.bat: File already exists '{0}'", Util.TemplateBatFilename));
                return;
            }

            using (var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("RibbonGenerator.Template.bat"))
            {
                StreamReader reader = new StreamReader(stream);
                var content = reader.ReadToEnd();
                content = content.Replace("{Windows7SDKToolsPath}", Util.DetectAppropariateWindows7SdkPath());
                File.WriteAllText(Util.TemplateBatFilename, content);
            }

            this._output.WriteLine(string.Format("Verify Template.bat: File is created '{0}'", Util.TemplateBatFilename));
        }

        /// <summary>
        /// Delete all temporary files created during CreateRibbon
        /// </summary>
        void Cleanup()
        {
            foreach (string cleanupFile in this.CleanupFiles)
            {
                try
                {
                    if(File.Exists(cleanupFile))
                        File.Delete(cleanupFile);
                }
                catch(Exception ex) 
                {
                    Util.LogError(new Exception(string.Format("Cleanup fails for file '{0}'", cleanupFile), ex));
                }
            }
        }

        /// <summary>
        /// Create and execute the bat file to create a ribbon dll.
        /// </summary>
        /// <param name="localizedRibbonXmlFilename">the name of the ribbon xaml</param>
        /// <returns>the name of the dll file</returns>
        string ConvertXmlToDll(string localizedRibbonXmlFilename)
        {
            string batFilename = Path.ChangeExtension(localizedRibbonXmlFilename, ".bat");
            string bmlFilename = Path.ChangeExtension(localizedRibbonXmlFilename, ".bml");
            string rcFilename = Path.ChangeExtension(localizedRibbonXmlFilename, ".rc");
            string resFilename = Path.ChangeExtension(localizedRibbonXmlFilename, ".res");
            string dllFilename = Path.ChangeExtension(localizedRibbonXmlFilename, ".ribbondll");

            this.CleanupFiles.AddRange(new string[] {batFilename, bmlFilename, rcFilename, resFilename, dllFilename });

            var bat = File.ReadAllText(Util.TemplateBatFilename);
            bat = bat.Replace("{XmlFilename}", localizedRibbonXmlFilename);
            bat = bat.Replace("{BmlFilename}", bmlFilename);
            bat = bat.Replace("{RcFilename}", rcFilename);
            bat = bat.Replace("{DllFilename}", dllFilename);
            bat = bat.Replace("{ResFilename}", resFilename);
            File.WriteAllText(batFilename, bat);

            System.Diagnostics.Process proc = new System.Diagnostics.Process();
            proc.StartInfo.FileName = batFilename;
            proc.StartInfo.RedirectStandardOutput = true;
            proc.StartInfo.UseShellExecute = false;
            proc.StartInfo.CreateNoWindow = true;
            proc.OutputDataReceived += new System.Diagnostics.DataReceivedEventHandler(proc_OutputDataReceived);
            
            proc.Start();
            proc.BeginOutputReadLine();

            proc.WaitForExit();

            if (!File.Exists(bmlFilename) || !File.Exists(rcFilename))
                throw new FaildException("uicc.exe failed to generate .bml or .rc file!");
            if (!File.Exists(resFilename))
                throw new FaildException("rc.exe failed to generate binary .res file!");
            if(!File.Exists(dllFilename))
                throw new FaildException("link.exe failed to generate binary resource .dll file!");

            return dllFilename;
        }

        /// <summary>
        /// Forward the sub console window output to the message output
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        void proc_OutputDataReceived(object sender, System.Diagnostics.DataReceivedEventArgs e)
        {
            if(_output != null)
                _output.WriteLine(e.Data);
        }

        string LocalizeRibbon(Target element)
        {
            if (!element.Localize)
                return this.RibbonXmlFilename;

            var localizedContent = this.RibbonXmlContent;
            StringBuilder sb = new StringBuilder();

            this.ResXReader.SetCulture(element.CultureName);

            int pos = 0;
            const string LOCALIZEBEGINTOKEN = "{Resource:";
            const string LOCALIZEENDTOKEN = "}";
            while (true)
            {
                int nextTokenBegin = localizedContent.IndexOf(LOCALIZEBEGINTOKEN, pos);
                if (nextTokenBegin < 0)
                    break;

                int nextTokenEnd = localizedContent.IndexOf(LOCALIZEENDTOKEN, nextTokenBegin);
                if (nextTokenEnd < 0)
                    break;

                int tokenLength = nextTokenEnd - nextTokenBegin + 1;
                string token = localizedContent.Substring(nextTokenBegin, tokenLength);

                //if (token.Contains("Home_"))
                //    System.Diagnostics.Debugger.Break();

                int resourceKeyBegin = nextTokenBegin + LOCALIZEBEGINTOKEN.Length;
                int resourceKeyLength = nextTokenEnd - resourceKeyBegin;
                
                string resourceKey = localizedContent.Substring(resourceKeyBegin, resourceKeyLength);

                string localizedString = this.ResXReader.GetString(resourceKey);
                localizedContent = localizedContent.Replace(token, localizedString);

                pos++;
            }

            string cultureName = !string.IsNullOrEmpty(element.CultureName) ? element.CultureName : "default";
            string localizedFilename = Path.ChangeExtension(this.RibbonXmlFilename, string.Format(".{0}.xml", cultureName));
            File.WriteAllText(localizedFilename, localizedContent);
            this.CleanupFiles.Add(localizedFilename);
            return localizedFilename;

        }
    }
}
