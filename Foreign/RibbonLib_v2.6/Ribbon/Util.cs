using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Resources;
using System.Reflection;
using System.Xml.Serialization;

namespace RibbonLib
{
    static class Util
    {
        /// <summary>
        /// Contains true, if we are in design mode of Visual Studio
        /// </summary>
        private static bool _designMode;

        /// <summary>
        /// Initializes an instance of Util class
        /// </summary>
        static Util()
        {
            _designMode = (System.Diagnostics.Process.GetCurrentProcess().ProcessName.ToLower() == "devenv");
        }

        /// <summary>
        /// Gets true, if we are in design mode of Visual Studio
        /// </summary>
        /// <remarks>
        /// In Visual Studio 2008 SP1 the designer is crashing sometimes on windows forms. 
        /// The DesignMode property of Control class is buggy and cannot be used, so use our own implementation instead.
        /// </remarks>
        public static bool DesignMode
        {
            get
            {
                return _designMode;
            }
        }

        public static byte[] GetEmbeddedResource(string resourceName, Assembly assembly)
        {
            ResourceManager rm = new ResourceManager(resourceName, assembly);
            using (var stream = assembly.GetManifestResourceStream(resourceName))
            {
                var buffer = new byte[stream.Length];
                stream.Read(buffer, 0, buffer.Length);
                return buffer;
            }
        }

        public static T DeserializeEmbeddedResource<T>(string resourceName, Assembly assembly)
        {
            ResourceManager rm = new ResourceManager(resourceName, assembly);
            using (var stream = assembly.GetManifestResourceStream(resourceName))
            {
                if (stream == null)
                    throw new ArgumentException(string.Format("resourceName is unknown '{0}'", resourceName));

                XmlSerializer serializer = new XmlSerializer(typeof(T));
                var result = (T)serializer.Deserialize(stream);
                return result;
            }
        }

    }
}
