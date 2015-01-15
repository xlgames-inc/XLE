using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Reflection;
using Microsoft.Win32;

namespace RibbonGenerator
{
    /// <summary>
    /// Contains helper methods
    /// </summary>
    public static class Util
    {
        static Util()
        {
            // delete log file if file is bigger than...
            try
            {
                FileInfo info = new FileInfo(LogFile);
                if (info.Length > 1024 * 512)
                    File.Delete(LogFile);
            }
            catch { }
        }

        /// <summary>
        /// Writes a message to the log file
        /// </summary>
        /// <param name="message">the message</param>
        public static void LogMessage(string message)
        {
            string content = string.Format("{0} - {1}\r\n", DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"), message);
            File.AppendAllText(LogFile, content);
        }
        /// <summary>
        /// Writes a message to the log file
        /// </summary>
        /// <param name="message">the message</param>
        /// <param name="args">the args(look at string format for more info)</param>
        public static void LogMessage(string message, params object[] args)
        {
            LogMessage(string.Format(message, args));
        }
        /// <summary>
        /// Writes an exception to the log file
        /// </summary>
        /// <param name="ex"></param>
        public static void LogError(Exception ex)
        {
            string value = string.Format("{0}\r\n{1}", ex.Message, ex.ToString());
            LogMessage(value);
        }
        /// <summary>
        /// Contains the log filename
        /// </summary>
        static string _logFile;
        /// <summary>
        /// Gets the log filename. For example: C:\Users\<user>\AppData\Local\RibbonGenerator\RibbonGenerator.log
        /// </summary>
        public static string LogFile
        {
            get
            {
                if (_logFile == null)
                    _logFile = Path.Combine(GeneratorLocalAppData, "RibbonGenerator.log");
                return _logFile;
            }
        }

        /// <summary>
        /// Gets the local app data path for ribbongenerator. For example: C:\Users\<user>\AppData\Local\RibbonGenerator\
        /// </summary>
        public static string GeneratorLocalAppData
        {
            get
            {
                string localAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
                string path = Path.Combine(localAppData, "RibbonGenerator");
                if (!Directory.Exists(path))
                    Directory.CreateDirectory(path);
                return path;
            }
        }
        /// <summary>
        /// Contains the template bat filename. For example: C:\Users\<user>\AppData\Local\RibbonGenerator\Template.bat
        /// </summary>
        static string _templateBatFilename;
        /// <summary>
        /// Gets the template bat filename. For example: C:\Users\<user>\AppData\Local\RibbonGenerator\Template.bat.
        /// </summary>
        public static string TemplateBatFilename
        {
            get
            {
                if (_templateBatFilename == null)
                {
                    string template = Path.Combine(GeneratorLocalAppData, "Template.bat");
                    var uri = new Uri(template);
                    _templateBatFilename = uri.LocalPath;
                }

                return _templateBatFilename;
            }
        }

        public static string DetectAppropariateWindows7SdkPath()
        {
            RegistryKey hklm = Environment.Is64BitOperatingSystem
                ? RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64)
                : Registry.LocalMachine;

            var winSdkKey = hklm.OpenSubKey(@"SOFTWARE\Microsoft\Microsoft SDKs\Windows");

            // if no windows sdk is installed return default path
            if (winSdkKey == null)
                return DEFAULTWINDOWS7SDKPATH;

            var versions = winSdkKey.GetSubKeyNames();
            var desc = from s in versions
                       orderby s descending
                       select s;

            // search folder that contains uicc.exe to verify windows 7 sdk path
            string sdkToolsPath = null;
            foreach (var version in desc)
            {
                var versionKeyName = string.Format(@"SOFTWARE\Microsoft\Microsoft SDKs\Windows\{0}\WinSDKTools", version);
                var sdkToolsKey = hklm.OpenSubKey(versionKeyName);
                if (sdkToolsKey == null)
                    continue;
                var installationFolder = sdkToolsKey.GetValue("InstallationFolder");
                if (installationFolder == null)
                    continue;
                var uiccexePath = Path.Combine((string)installationFolder, "UICC.exe");
                if (File.Exists(uiccexePath))
                {
                    sdkToolsPath = (string)installationFolder;
                    break;
                }
            }

            // if no path found return default path
            if (sdkToolsPath == null)
                sdkToolsPath = DEFAULTWINDOWS7SDKPATH;

            if (!sdkToolsPath.EndsWith("\\"))
                sdkToolsPath += "\\";
            return sdkToolsPath;
        }

        public const string DEFAULTWINDOWS7SDKPATH = @"%PROGRAMFILES%\Microsoft SDKs\Windows\v7.1\Bin\";
    }
}
