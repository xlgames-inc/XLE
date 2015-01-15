//---------------------------------------------------------------------------
// <copyright file="RibbonGroupTemplateSizeDefinition.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

using System.Windows;
using System.Windows.Markup;

namespace Microsoft.Windows.Controls.Ribbon
{
    [ContentProperty("ContentTemplate")]
    public class RibbonGroupTemplateSizeDefinition : RibbonGroupSizeDefinitionBase
    {
        #region Public Properties

        public DataTemplate ContentTemplate
        {
            get { return (DataTemplate)GetValue(ContentTemplateProperty); }
            set { SetValue(ContentTemplateProperty, value); }
        }

        // Using a DependencyProperty as the backing store for ContentTemplate.  This enables animation, styling, binding, etc...
        public static readonly DependencyProperty ContentTemplateProperty =
            DependencyProperty.Register("ContentTemplate",
                typeof(DataTemplate),
                typeof(RibbonGroupTemplateSizeDefinition),
                new FrameworkPropertyMetadata(null));

        #endregion

        #region Freezable

        protected override Freezable CreateInstanceCore()
        {
            return new RibbonGroupTemplateSizeDefinition();
        }

        #endregion
    }
}