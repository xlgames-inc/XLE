// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../AssetUtils.h"

namespace Assets { namespace Legacy 
{
    /// <summary>Container for a asset filename in string format<summary>
    /// Just a simple generalisation of a path and file name in char array form.
    /// Avoids scattering references to ResChar and MaxPath about
    /// the code (and provide some future-proof-ness).
    ///
    /// Note that in this form there is a strict limit on the max length of 
    /// and asset file name. This is in line with the MAX_PATH soft limit
    /// on some filesystems and standard library implementations... But most
    /// filesystems can actually support much longer path names (even if an
    /// individual directory name or filename is limited)
    class ResolvedAssetFile
    {
    public:
        ResChar _fn[MaxPath];

        const ResChar* get() const  { return _fn; }
        const bool IsGood() const   { return _fn[0] != '\0'; }

        ResolvedAssetFile() { _fn[0] = '\0'; }
    };

    /// @{
    /// Converts an input filename to a form that is best suited for the assets system.
    /// This includes converting absolute filenames into relative format (relative to the
    /// primary mount point).
    /// This is intended for GUI tools that allow the user to enter filenames of any form.
    void MakeAssetName(ResolvedAssetFile& dest, const StringSection<ResChar> src);
    void MakeAssetName(ResolvedAssetFile& dest, const StringSection<utf8> src);
    /// @}

        ////////////////////////////////////////////////////////////////////////////////////////////////

}}

