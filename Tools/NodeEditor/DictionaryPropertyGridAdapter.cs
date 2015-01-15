using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.ComponentModel;

namespace NodeEditor
{
    class DictionaryPropertyGridAdapter : ICustomTypeDescriptor, INotifyPropertyChanged
    {
        //
        //      Based on code from 
        //          http://www.differentpla.net/content/2005/02/using-propertygrid-with-dictionary
        //
        System.Collections.IDictionary _dictionary;

        public DictionaryPropertyGridAdapter(System.Collections.IDictionary d)
        {
            _dictionary = d;
        }

        #region PropertyDescriptor
            class DictionaryPropertyDescriptor : PropertyDescriptor
            {
                System.Collections.IDictionary _dictionary;
                object _key;

                internal DictionaryPropertyDescriptor(System.Collections.IDictionary d, object key)
                    : base(key.ToString(), null)
                {
                    _dictionary = d;
                    _key = key;
                }

                public override Type PropertyType
                {
                    get { return _dictionary[_key].GetType(); }
                }

                public override void SetValue(object component, object value)
                {
                    _dictionary[_key] = value;
                }

                public override object GetValue(object component)
                {
                    return _dictionary[_key];
                }

                public override bool IsReadOnly
                {
                    get { return false; }
                }

                public override Type ComponentType
                {
                    get { return null; }
                }

                public override string Category { get { return "Material Parameters"; } }

                public override bool CanResetValue(object component)
                {
                    return false;
                }

                public override void ResetValue(object component)
                {
                }

                public override bool ShouldSerializeValue(object component)
                {
                    return false;
                }
            }
        #endregion

        public PropertyDescriptorCollection GetProperties(Attribute[] attributes)
        {
            var properties = new System.Collections.ArrayList();
            foreach (System.Collections.DictionaryEntry e in _dictionary)
            {
                properties.Add(new DictionaryPropertyDescriptor(_dictionary, e.Key));
            }

            PropertyDescriptor[] props =
                (PropertyDescriptor[])properties.ToArray(typeof(PropertyDescriptor));

            return new PropertyDescriptorCollection(props);
        }

        #region Common methods
            //      Common adapter methods... 
            public string GetComponentName() { return TypeDescriptor.GetComponentName(this, true); }
            public EventDescriptor GetDefaultEvent() { return TypeDescriptor.GetDefaultEvent(this, true); }
            public string GetClassName() { return TypeDescriptor.GetClassName(this, true); }
            public TypeConverter GetConverter() { return TypeDescriptor.GetConverter(this, true); }
            public object GetPropertyOwner(PropertyDescriptor pd) { return _dictionary; }
            public AttributeCollection GetAttributes() { return TypeDescriptor.GetAttributes(this, true); }
            public object GetEditor(Type editorBaseType) { return TypeDescriptor.GetEditor(this, editorBaseType, true); }
            public PropertyDescriptor GetDefaultProperty() { return null; }

            public EventDescriptorCollection GetEvents(Attribute[] attributes) { return TypeDescriptor.GetEvents(this, attributes, true); }
            EventDescriptorCollection System.ComponentModel.ICustomTypeDescriptor.GetEvents() { return TypeDescriptor.GetEvents(this, true); }

            PropertyDescriptorCollection
                System.ComponentModel.ICustomTypeDescriptor.GetProperties()
            {
                return ((ICustomTypeDescriptor)this).GetProperties(new Attribute[0]);
            }
        #endregion

        #region INotifyPropertyChanged
            //      Implementation of INotifyPropertyChanged
            public event PropertyChangedEventHandler PropertyChanged;
            private void OnPropertyChanged(string propertyName)
            {
                if (PropertyChanged == null)
                {
                    return;
                }

                var eventArgs = new PropertyChangedEventArgs(propertyName);
                PropertyChanged(this, eventArgs);
            }

            public void NotifyToRefreshAllProperties()
            {
                OnPropertyChanged(String.Empty);
            }
        #endregion
    }
}
