using System;
using System.Globalization;
using System.Windows.Data;

namespace Microsoft.Windows.Controls
{
    /// <summary>
    ///   Used for comparing an array of objects for referential equality.
    /// </summary>
    internal sealed class ReferentialEqualityConverter : IMultiValueConverter
    {
        /// <summary>
        ///   Compares an array of objects for referential equality.
        /// </summary>
        /// <param name="values">Array of values to compare.</param>
        /// <param name="targetType">Unused</param>
        /// <param name="parameter">Unused</param>
        /// <param name="culture">Unused</param>
        /// <returns>True if the values are all the same references; False otherwise.</returns>
        /// <remarks>This conversion operation doesn't make sense if values.Length is less than 2.  So we just return False in that situation.</remarks>
        public object Convert(object[] values, Type targetType, object parameter, CultureInfo culture)
        {
            if (values.Length < 2)
            {
                return false;
            }

            for (int i = 1; i < values.Length; i++)
            {
                if (!Object.ReferenceEquals(values[0], values[i]))
                {
                    return false;
                }
            }
            
            return true;
        }

        /// <summary>
        ///   Not implemented.  Throws a NotSupportedException.
        /// </summary>
        /// <param name="value">N/A</param>
        /// <param name="targetTypes">N/A</param>
        /// <param name="parameter">N/A</param>
        /// <param name="culture">N/A</param>
        /// <returns>N/A</returns>
        /// <exception cref="NotSupportedException">Operation not supported.</exception>
        public object[] ConvertBack(object value, Type[] targetTypes, object parameter, CultureInfo culture)
        {
            throw new NotSupportedException();
        }
    }
}
