using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Xml.XPath;
using System.Xml;
using System.Globalization;

namespace RibbonGenerator
{
    public class ResXReader
    {
        List<Target> _targets;
        Dictionary<Target, XPathNavigator> _navigators = new Dictionary<Target,XPathNavigator>();

        Target _currentTarget;
        Target _fallBackTarget;
        Target _defaultTarget;

        public ResXReader(List<Target> targets)
        {
            _targets = targets;
            foreach (var target in _targets)
            {
                var navigator = new XPathDocument(target.ResourceFilename).CreateNavigator();
                _navigators.Add(target, navigator);
            }
        }

        public void SetCulture(string cultureName)
        {
            _currentTarget = null;
            _fallBackTarget = null;
            if (!string.IsNullOrEmpty(cultureName))
            {
                var culture = new CultureInfo(cultureName);
                _currentTarget = _targets.FirstOrDefault(t => t.CultureName == cultureName);

                var parentCulture = culture.Parent;
                
                if (parentCulture != null && !string.IsNullOrEmpty(parentCulture.Name))
                {
                    var parentCultureName = parentCulture.Name;
                    _fallBackTarget = _targets.FirstOrDefault(t => t.CultureName == parentCultureName);
                }
            }

            _defaultTarget = _targets.FirstOrDefault(t => string.IsNullOrEmpty(t.CultureName));
        }

        public string GetString(string key)
        {
            XPathNavigator valueNavigator = null;

            if (_currentTarget != null)
            {
                var navigator = _navigators[_currentTarget];
                valueNavigator = GetValue(navigator, key);
            }

            // check fallback resource
            if (valueNavigator == null && _fallBackTarget != null)
            {
                var navigator = _navigators[_fallBackTarget];
                valueNavigator = GetValue(navigator, key);
            }

            // use default resource
            if (valueNavigator == null)
            {
                var navigator = _navigators[_defaultTarget];
                valueNavigator = GetValue(navigator, key);
            }

            // put out messages here if no value exists
            //if (valueNavigator == null)
            //    throw new Exception(string.Format("no value found for key '{0}'!", );

            if (valueNavigator == null)
                return null;

            var result = valueNavigator.Value;
            return result;
        }

        private XPathNavigator GetValue(XPathNavigator navigator, string key)
        {
            var query = string.Format("//root/data[@name='{0}']/value", key);
            var result = navigator.SelectSingleNode(query);
            return result;
        }
    }
}
