using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Xml.Serialization;

namespace RibbonLib
{
    public class RibbonShortcut
    {
        [XmlAttribute]
        public uint CommandId { get; set; }

        string _shortcut;

        [XmlAttribute("Shortcut")]
        public string Shortcut
        {
            get 
            { 
                return _shortcut; 
            }
            set
            {
                _shortcut = value;
                _shortcutKeys = ConvertToKeys(value);
            }
        }

        public Keys ConvertToKeys(string value)
        {
            if (string.IsNullOrEmpty(value))
                return Keys.None;

            Keys result = Keys.None;

            string[] keys = value.Split('+');
            foreach (string key in keys)
            {
                string formattedKey;
                if (key == "Ctrl")
                    formattedKey = "Control";
                else
                    formattedKey = key;

                try
                {
                    Keys k = (Keys)Enum.Parse(typeof(Keys), formattedKey, true);
                    result |= k;
                }
                catch (Exception ex)
                {
                    throw new ArgumentException(string.Format("The ShortcutKey '{0}' is invalid. The token '{1}' is unknown", value, key), ex);
                }
            }

            return result;
        }

        Keys _shortcutKeys;
        [XmlIgnore]
        public Keys ShortcutKeys
        {
            get { return _shortcutKeys; }
        }
    }

    public class RibbonShortcutTable
    {
        public RibbonShortcut[] RibbonShortcutArray
        {
            get
            {
                if (_ribbonShortcuts == null)
                    return null;
                return _ribbonShortcuts.ToArray();
            }
            set
            {
                if (value == null)
                    _ribbonShortcuts = new List<RibbonShortcut>();
                else
                    _ribbonShortcuts = new List<RibbonShortcut>(value);
            }
        }

        List<RibbonShortcut> _ribbonShortcuts = new List<RibbonShortcut>();

        [XmlIgnore()]
        public List<RibbonShortcut> RibbonShortcuts
        {
            get { return _ribbonShortcuts; }
        }

        /// <summary>
        /// Tests if the shortcut has an underlying command id
        /// </summary>
        /// <param name="shortcutKeys">the shortcut keys</param>
        /// <returns>the command name</returns>
        public uint HitTest(Keys shortcutKeys)
        {
            var ribbonShortcut = this.RibbonShortcuts.FirstOrDefault(s => s.ShortcutKeys == shortcutKeys);
            if(ribbonShortcut == null)
                return 0;
            return ribbonShortcut.CommandId;
        }
    }
}
