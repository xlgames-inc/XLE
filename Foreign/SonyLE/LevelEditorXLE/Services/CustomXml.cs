using Sce.Atf.Dom;
using System;

namespace LevelEditorXLE
{
    internal class CustomDomXmlReader : DomXmlReader
    {
        public CustomDomXmlReader(Uri documentRoot, XmlSchemaTypeLoader typeLoader) : base(typeLoader)
            { _documentRoot = documentRoot; }

        protected override void ReadAttribute(DomNode node, AttributeInfo attributeInfo, string valueString)
        {
            if (!string.IsNullOrEmpty(valueString))
            {
                object value = attributeInfo.Type.Convert(valueString);
                if (value is Uri)
                {
                    var ur = (Uri)value;
                    if (!ur.IsAbsoluteUri)
                        value = new Uri(_documentRoot, ur);
                }
                node.SetAttribute(attributeInfo, value);
            }
        }

        private Uri _documentRoot;
    }

    internal class CustomDomXmlWriter : DomXmlWriter
    {
        public CustomDomXmlWriter(Uri documentRoot, XmlSchemaTypeCollection typeCollection)
            : base(typeCollection)
        {
            _documentRoot = documentRoot;
            PreserveSimpleElements = true;
            PersistDefaultAttributes = true;
        }

        protected override string Convert(DomNode node, AttributeInfo attributeInfo)
        {
            string valueString = string.Empty;
            object value = node.GetAttribute(attributeInfo);
            if (value == null) return valueString;

            if (attributeInfo.Type.Type == AttributeTypes.Uri)
            {
                var ur = (Uri)value;
                if (ur.IsAbsoluteUri)
                {
                    ur = _documentRoot.MakeRelativeUri(ur);
                    ur = new Uri(Uri.UnescapeDataString(ur.ToString()), UriKind.Relative);
                    valueString = ur.ToString();
                }
            }
            else
            {
                valueString = attributeInfo.Type.Convert(value);
            }

            return valueString;
        }

        private Uri _documentRoot;
    }

}
