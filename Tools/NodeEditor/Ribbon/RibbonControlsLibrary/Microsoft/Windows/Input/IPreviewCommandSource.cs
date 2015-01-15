//---------------------------------------------------------------------------
//
// Copyright (C) Microsoft Corporation.  All rights reserved.
//
//---------------------------------------------------------------------------

using System.Windows.Input;

namespace Microsoft.Windows.Input
{
    /// <summary>
    ///     An interface for classes that know how to invoke a PreviewCommand.
    /// </summary>
    public interface IPreviewCommandSource : ICommandSource
    {
        /// <summary>
        ///     The parameter that will be passed to the command when previewing the command.
        ///     The property may be implemented as read-write if desired.
        /// </summary>
        object PreviewCommandParameter
        {
            get;
        }
    }
}
