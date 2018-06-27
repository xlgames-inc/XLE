// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using namespace System::Windows::Forms;

namespace GUILayer
{
    public ref class CalendarEditingControl : DateTimePicker, IDataGridViewEditingControl
    {
    public:
        DataGridView^ dataGridView;
        bool valueChanged = false;
        int rowIndex;

        CalendarEditingControl()
        {
            Format = DateTimePickerFormat::Short;
        }

        // Implements the IDataGridViewEditingControl.EditingControlFormattedValue  
        // property. 
        property Object^ EditingControlFormattedValue
        {
            virtual Object^ get()
            {
                return Value.ToShortDateString();
            }

            virtual void set(Object^ value)
            {
                auto str = dynamic_cast<System::String^>(value);
                if (str!=nullptr)
                {
                    try
                    {
                        // This will throw an exception of the string is  
                        // null, empty, or not in the format of a date. 
                        Value = System::DateTime::Parse(str);
                    }
                    catch (...)
                    {
                        // In the case of an exception, just use the  
                        // default value so we're not left with a null 
                        // value. 
                        Value = System::DateTime::Now;
                    }
                }
            }
        }

        // Implements the  
        // IDataGridViewEditingControl.GetEditingControlFormattedValue method. 
        virtual Object^ GetEditingControlFormattedValue(DataGridViewDataErrorContexts context)
        {
            return EditingControlFormattedValue;
        }

        // Implements the  
        // IDataGridViewEditingControl.ApplyCellStyleToEditingControl method. 
        virtual void ApplyCellStyleToEditingControl(DataGridViewCellStyle^ dataGridViewCellStyle)
        {
            Font = dataGridViewCellStyle->Font;
            CalendarForeColor = dataGridViewCellStyle->ForeColor;
            CalendarMonthBackground = dataGridViewCellStyle->BackColor;
        }

        // Implements the IDataGridViewEditingControl.EditingControlRowIndex  
        // property. 
        property int EditingControlRowIndex
        {
            virtual int get()
            {
                return rowIndex;
            }
            virtual void set(int value)
            {
                rowIndex = value;
            }
        }

        // Implements the IDataGridViewEditingControl.EditingControlWantsInputKey  
        // method. 
        virtual bool EditingControlWantsInputKey(Keys key, bool dataGridViewWantsInputKey)
        {
            // Let the DateTimePicker handle the keys listed. 
            switch (key & Keys::KeyCode)
            {
                case Keys::Left:
                case Keys::Up:
                case Keys::Down:
                case Keys::Right:
                case Keys::Home:
                case Keys::End:
                case Keys::PageDown:
                case Keys::PageUp:
                    return true;
                default:
                    return !dataGridViewWantsInputKey;
            }
        }

        // Implements the IDataGridViewEditingControl.PrepareEditingControlForEdit  
        // method. 
        virtual void PrepareEditingControlForEdit(bool selectAll)
        {
            // No preparation needs to be done.
        }

        // Implements the IDataGridViewEditingControl 
        // .RepositionEditingControlOnValueChange property. 
        property bool RepositionEditingControlOnValueChange
        {
            virtual bool get()
            {
                return false;
            }
        }

        // Implements the IDataGridViewEditingControl 
        // .EditingControlDataGridView property. 
        property DataGridView^ EditingControlDataGridView
        {
            virtual DataGridView^ get()
            {
                return dataGridView;
            }
            virtual void set(DataGridView^ value)
            {
                dataGridView = value;
            }
        }

        // Implements the IDataGridViewEditingControl 
        // .EditingControlValueChanged property. 
        property bool EditingControlValueChanged
        {
            virtual bool get()
            {
                return valueChanged;
            }
            virtual void set(bool value)
            {
                valueChanged = value;
            }
        }

        // Implements the IDataGridViewEditingControl 
        // .EditingPanelCursor property. 
        property System::Windows::Forms::Cursor^ EditingPanelCursor
        {
            virtual System::Windows::Forms::Cursor^ get()
            {
                return __super::Cursor;
            }
        }

    protected:
        void OnValueChanged(System::EventArgs^ eventargs) override 
        {
            // Notify the DataGridView that the contents of the cell 
            // have changed.
            valueChanged = true;
            EditingControlDataGridView->NotifyCurrentCellDirty(true);
            __super::OnValueChanged(eventargs);
        }
    };

    public ref class CalendarCell : DataGridViewTextBoxCell
    {
    public:
        CalendarCell()
        {
            // Use the short date format. 
            Style->Format = "d";
        }

        void InitializeEditingControl(int rowIndex, 
            Object^ initialFormattedValue, DataGridViewCellStyle^ dataGridViewCellStyle) override
        {
            // Set the value of the editing control to the current cell value. 
            __super::InitializeEditingControl(rowIndex, initialFormattedValue, dataGridViewCellStyle);
            auto ctl =  dynamic_cast<CalendarEditingControl^>(DataGridView->EditingControl);

                // Use the default row value when Value property is null. 
            if (Value == nullptr)
            {
                ctl->Value = (System::DateTime)DefaultNewRowValue;
            }
            else
            {
                ctl->Value = (System::DateTime)Value;
            }
        }

        property System::Type^ EditType
        {
            virtual System::Type^ get() override
            {
                // Return the type of the editing control that CalendarCell uses. 
                return CalendarEditingControl::typeid;
            }
        }

        property System::Type^ ValueType
        {
            virtual System::Type^ get() override
            {
                // Return the type of the value that CalendarCell contains. 
                return System::DateTime::typeid;
            }
        }

        property Object^ DefaultNewRowValue
        {
            virtual Object^ get() override
            {
                // Use the current date and time as the default value. 
                return System::DateTime::Now;
            }
        }
    };

    public ref class CalendarColumn : DataGridViewColumn
    {
    public:
        CalendarColumn() : DataGridViewColumn(gcnew CalendarCell())
        {
        }

        property DataGridViewCell^ CellTemplate
        {
            virtual DataGridViewCell^ get() override
            {
                return __super::CellTemplate;
            }
            virtual void set(DataGridViewCell^ value) override
            {
                // Ensure that the cell used for the template is a CalendarCell. 
                if (value != nullptr && 
                    !value->GetType()->IsAssignableFrom(CalendarCell::typeid))
                {
                    throw gcnew System::InvalidCastException("Must be a CalendarCell");
                }
                __super::CellTemplate = value;
            }
        }
    };
}
