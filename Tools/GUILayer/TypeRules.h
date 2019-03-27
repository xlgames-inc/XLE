// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

namespace GUILayer
{
    public ref class TypeRules
    {
    public:
        ref class TypeBreakdown
        {
        public:
            property int Dimension0;
            property int Dimension1;
            property System::String^ RawType;
            property bool UnsignedFlag;

            TypeBreakdown(System::String^ fullTypeName);
        };

        static bool             HasAutomaticConversion(System::String^ sourceType, System::String^ destinationType);
        static System::Object^  CreateDefaultObject(System::String^ type);

        static bool             CopyToBytes(void* destination, System::Object^ source, 
                                            System::String^ destinationType, System::String^ sourceType,
                                            const void* destinationEnd);

        static System::Object^  CreateFromString(System::String^ source, System::String^ type);

        static System::String^  ExtractTypeName(System::Object^ obj);
     };
}


