// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Stdafx.h"
#include "TypeRules.h"

namespace ShaderPatcherLayer
{
    TypeRules::TypeBreakdown::TypeBreakdown(System::String^ fullTypeName)
    {
        Dimension0 = 1;
        Dimension1 = 1;
        UnsignedFlag = false;

        int rawNameStart = 0, rawNameEnd = fullTypeName->Length;
        if (fullTypeName->Length > 1
            && System::Char::IsDigit(fullTypeName[fullTypeName->Length - 1]))
        {
            if (fullTypeName->Length > 3
                && System::Char::IsDigit(fullTypeName[fullTypeName->Length - 3])
                && fullTypeName[fullTypeName->Length - 2] == 'x')
            {
                Dimension0 = fullTypeName[fullTypeName->Length - 3] - '0';
                Dimension1 = fullTypeName[fullTypeName->Length - 1] - '0';
                rawNameEnd -= 3;
            }
            else
            {
                Dimension0 = fullTypeName[fullTypeName->Length - 1] - '0';
                rawNameEnd -= 1;
            }
        }

        if (fullTypeName->Length > 1 && fullTypeName[0] == 'u') {
            ++rawNameStart;
            UnsignedFlag = true;
        }

        RawType = fullTypeName->Substring(rawNameStart, rawNameEnd - rawNameStart);
    }

	static bool IsConvertableType(System::String^ type)
	{
			// bool, half, double not supported
		return	type->Equals("float", System::StringComparison::CurrentCultureIgnoreCase)
			||	type->Equals("int", System::StringComparison::CurrentCultureIgnoreCase)
			||	type->Equals("uint", System::StringComparison::CurrentCultureIgnoreCase)
			;
	}

    bool TypeRules::HasAutomaticConversion(System::String^ sourceType, System::String^ destinationType)
    {
        auto sourceBreakdown = gcnew TypeBreakdown(sourceType);
        auto destinationBreakdown = gcnew TypeBreakdown(destinationType);

            //
            //      We can always convert between types with the same raw type, or
			//		two scalar raw types, so long at Dimension1 is identical.
            //
            //      So float -> float2 and float2 -> float;
            //      also, uint4 -> int2, float2 -> int4;
            //      But float4x4 cannot be converted to float4.
            //
		bool sameBaseType = sourceBreakdown->RawType->Equals(destinationBreakdown->RawType, System::StringComparison::CurrentCultureIgnoreCase);
        return 
			(sameBaseType || (IsConvertableType(sourceBreakdown->RawType) && IsConvertableType(destinationBreakdown->RawType)))
            && sourceBreakdown->Dimension1 == destinationBreakdown->Dimension1;
    }

    System::Object^ TypeRules::CreateDefaultObject(System::String^ type)
    {
        auto breakdown = gcnew TypeBreakdown(type);
        if (breakdown->RawType->Equals("float", System::StringComparison::CurrentCultureIgnoreCase))
        {
            if (breakdown->Dimension1 == 1)
            {
                if (breakdown->Dimension0 == 1)
                {
                    return gcnew float(0.f);
                }
                else if (breakdown->Dimension0 == 2)
                {
                    return gcnew array<float> { 0.0f, 0.0f };
                }
                else if (breakdown->Dimension0 == 3)
                {
                    return gcnew array<float> { 0.0f, 0.0f, 0.0f };
                }
                else if (breakdown->Dimension0 == 4)
                {
                    return gcnew array<float> { 0.0f, 0.0f, 0.0f, 1.0f };
                }
            }
            else if (breakdown->Dimension1 == 3)
            {
                if (breakdown->Dimension0 == 3)
                {
                    return gcnew array<array<float>^>
                        {   { 1.0f, 0.0f, 0.0f },
                            { 0.0f, 1.0f, 0.0f },
                            { 0.0f, 0.0f, 1.0f } };
                }
                else if (breakdown->Dimension0 == 4)
                {
                    return gcnew array<array<float>^>
                        {   { 1.0f, 0.0f, 0.0f },
                            { 0.0f, 1.0f, 0.0f },
                            { 0.0f, 0.0f, 1.0f },
                            { 0.0f, 0.0f, 0.0f } };
                }
            }
            else if (breakdown->Dimension1 == 4)
            {
                if (breakdown->Dimension0 == 3)
                {
                    return gcnew array<array<float>^>
                        {   { 1.0f, 0.0f, 0.0f, 0.0f },
                            { 0.0f, 1.0f, 0.0f, 0.0f },
                            { 0.0f, 0.0f, 1.0f, 0.0f } };
                }
                else if (breakdown->Dimension0 == 4)
                {
                    return gcnew array<array<float>^>
                        {   { 1.0f, 0.0f, 0.0f, 0.0f },
                            { 0.0f, 1.0f, 0.0f, 0.0f },
                            { 0.0f, 0.0f, 1.0f, 0.0f },
                            { 0.0f, 0.0f, 0.0f, 1.0f } };
                }
            }
        }
        else if (breakdown->RawType->Equals("int", System::StringComparison::CurrentCultureIgnoreCase))
        {
            if (breakdown->UnsignedFlag)
            {
                if (breakdown->Dimension1 == 1)
                {
                    if (breakdown->Dimension0 == 1)
                    {
                        auto result = gcnew System::UInt32();
                        *result = 0;
                        return result;
                    }
                    else if (breakdown->Dimension0 == 2)
                    {
                        return gcnew array<System::UInt32> { 0, 0 };
                    }
                    else if (breakdown->Dimension0 == 3)
                    {
                        return gcnew array<System::UInt32> { 0, 0, 0 };
                    }
                    else if (breakdown->Dimension0 == 4)
                    {
                        return gcnew array<System::UInt32> { 0, 0, 0, 1 };
                    }
                }
            }
            else
            {
                if (breakdown->Dimension1 == 1)
                {
                    if (breakdown->Dimension0 == 1)
                    {
                        auto result = gcnew System::Int32();
                        *result = 0;
                        return result;
                    }
                    else if (breakdown->Dimension0 == 2)
                    {
                        return gcnew array<System::Int32> { 0, 0 };
                    }
                    else if (breakdown->Dimension0 == 3)
                    {
                        return gcnew array<System::Int32> { 0, 0, 0 };
                    }
                    else if (breakdown->Dimension0 == 4)
                    {
                        return gcnew array<System::Int32> { 0, 0, 0, 1 };
                    }
                }
            }
        } 
        else if (   breakdown->RawType->Equals("texture",       System::StringComparison::CurrentCultureIgnoreCase)
                ||  breakdown->RawType->Equals("Texture1D",     System::StringComparison::CurrentCultureIgnoreCase)
                ||  breakdown->RawType->Equals("Texture2D",     System::StringComparison::CurrentCultureIgnoreCase)
                ||  breakdown->RawType->Equals("Texture3D",     System::StringComparison::CurrentCultureIgnoreCase)
                ||  breakdown->RawType->Equals("TextureCube",   System::StringComparison::CurrentCultureIgnoreCase))
        {
            return "testtexture.dds";
        }

        return gcnew array<float> { 0.0f, 0.0f, 0.0f, 1.0f };
    }

    #pragma warning(disable:4244)       // conversion from 'float' to 'unsigned int', possible loss of data

    static void     CopySingle( void* destination, System::String^ destinationType,
                                System::Object^ sourceMember, System::String^ sourceType)
    {
        if (destinationType->Equals("float", System::StringComparison::CurrentCultureIgnoreCase)) {
            if (sourceType->Equals("float", System::StringComparison::CurrentCultureIgnoreCase)) {
                *(float*)destination = *(float^)sourceMember;
            } else if (sourceType->Equals("int", System::StringComparison::CurrentCultureIgnoreCase)) {
                *(float*)destination = *(System::UInt32^)sourceMember;
            } else {
                *(float*)destination = 0.f;
            }
        } else if (destinationType->Equals("int", System::StringComparison::CurrentCultureIgnoreCase)) {
            if (sourceType->Equals("float", System::StringComparison::CurrentCultureIgnoreCase)) {
                *(unsigned*)destination = *(float^)sourceMember;
            } else if (sourceType->Equals("int", System::StringComparison::CurrentCultureIgnoreCase)) {
                *(unsigned*)destination = *(System::UInt32^)sourceMember;
            } else {
                *(unsigned*)destination = 0;
            }
        }
    }

    System::Object^ ExtractMember(int x, int y, System::Object^ source, TypeRules::TypeBreakdown^ breakdown)
    {
        if (breakdown->RawType->Equals("float", System::StringComparison::CurrentCultureIgnoreCase)) {
            if (breakdown->Dimension1 <= 1) {
                if (breakdown->Dimension0 <= 1) {
                    if (x==0) {
                        return source;
                    } else {
                        return nullptr;
                    }
                }
                if (x < breakdown->Dimension0) {
                    return gcnew float(((array<float>^)source)[x]);
                } else {
                    return nullptr;
                }
            } else {
                if (x < breakdown->Dimension0) {
                    return gcnew float(((array<array<float>^>^)source)[x][y]);
                } else {
                    return nullptr;
                }
            }
        } else if (breakdown->RawType->Equals("int", System::StringComparison::CurrentCultureIgnoreCase)) {
            if (breakdown->Dimension1 <= 1) {
                if (breakdown->Dimension0 <= 1) {
                    if (x==0) {
                        return source;
                    } else {
                        return nullptr;
                    }
                }
                if (x < breakdown->Dimension0) {
                    return gcnew System::UInt32(((array<System::UInt32>^)source)[x]);
                } else {
                    return nullptr;
                }
            } else {
                if (x < breakdown->Dimension0) {
                    return gcnew System::UInt32(((array<array<System::UInt32>^>^)source)[x][y]);
                } else {
                    return nullptr;
                }
            }
        }
        return nullptr;
    }

    bool             TypeRules::CopyToBytes(    void* destination, System::Object^ source, 
                                                System::String^ destinationType, System::String^ sourceType,
                                                const void * destinationEnd)
    {
            //  
            //      Inefficient, but practical marshalling of data from a managed object of
            //      a given type, to an output native array in some other format
            //  

        auto destinationBreakdown   = gcnew TypeBreakdown(destinationType);
        auto sourceBreakdown        = gcnew TypeBreakdown(sourceType);

        if (    destinationBreakdown->RawType->Equals("float", System::StringComparison::CurrentCultureIgnoreCase)
            ||  destinationBreakdown->RawType->Equals("int", System::StringComparison::CurrentCultureIgnoreCase)) {

                //      write floating point values into the output array
                //      sometimes we may need to fill in extra columns or rows

            for (int y=0; y<destinationBreakdown->Dimension1; ++y) {
                for (int x=0; x<destinationBreakdown->Dimension0; ++x) {
                    
                    void* outputPointer = 
                            (char*)destination 
                        +   y * destinationBreakdown->Dimension0 * sizeof(float)
                        +   x * sizeof(float)
                        ;

                    System::Diagnostics::Debug::Assert(((char*)outputPointer + sizeof(float)) <= destinationEnd);

                    System::Object^ sourceMember = ExtractMember(x, y, source, sourceBreakdown);
                    if (sourceMember != nullptr) {
                        CopySingle(
                            outputPointer, destinationBreakdown->RawType,
                            sourceMember, sourceBreakdown->RawType);
                    } else {
                        CopySingle(
                            outputPointer, destinationBreakdown->RawType,
                            gcnew System::UInt32((x == 3)?1:0), "int");
                    }
                }
            }

            return true;
        }

        return false;
    }

    template < class T, class U > 
    static bool isinst(U u) {
       return dynamic_cast< T >(u) != nullptr;
    }

    System::String^  TypeRules::ExtractTypeName(System::Object^ obj)
    {
            //  find out what type "obj" is, and attempt to make a string name for
            //  it
        if (isinst<System::UInt32^>(obj)) {
            return "uint";
        } else if (isinst<System::Int32^>(obj)) {
            return "int";
        } else if (isinst<System::Single^>(obj)) {
            return "float";
        } 
        
        else if (isinst<array<System::UInt32>^>(obj)) {
            auto length = ((array<System::UInt32>^)obj)->Length;
            return "uint" + length;
        } else if (isinst<array<System::Int32>^>(obj)) {
            auto length = ((array<System::Int32>^)obj)->Length;
            return "int" + length;
        } else if (isinst<array<System::Single>^>(obj)) {
            auto length = ((array<System::Single>^)obj)->Length;
            return "float" + length;
        }

        else if (isinst<array<array<System::UInt32>^>^>(obj)) {
            auto length0 = ((array<array<System::UInt32>^>^)obj)->Length;
            auto length1 = ((array<array<System::UInt32>^>^)obj)[0]->Length;
            return "uint" + length0 + 'x' + length1;
        } else if (isinst<array<array<System::UInt32>^>^>(obj)) {
            auto length0 = ((array<array<System::Int32>^>^)obj)->Length;
            auto length1 = ((array<array<System::Int32>^>^)obj)[0]->Length;
            return "int" + length0 + 'x' + length1;
        } else if (isinst<array<array<System::Single>^>^>(obj)) {
            auto length0 = ((array<array<System::Single>^>^)obj)->Length;
            auto length1 = ((array<array<System::Single>^>^)obj)[0]->Length;
            return "float" + length0 + 'x' + length1;
        }

        return nullptr;
    }

    System::Object^  TypeRules::CreateFromString(System::String^ source, System::String^ type)
    {
            //  Only non-array types supported currently...
        auto breakdown = gcnew TypeBreakdown(type);
        if (breakdown->RawType->Equals("float", System::StringComparison::CurrentCultureIgnoreCase)) {
            if (breakdown->Dimension0 == 1 && breakdown->Dimension1 == 1) {
                return gcnew float(System::Single::Parse(source));
            }
        }
        else
        if (breakdown->RawType->Equals("int", System::StringComparison::CurrentCultureIgnoreCase)) {
            if (breakdown->Dimension0 == 1 && breakdown->Dimension1 == 1) {
                if (breakdown->UnsignedFlag) {
                    return gcnew System::UInt32(System::UInt32::Parse(source));
                } else {
                    return gcnew System::Int32(System::Int32::Parse(source));
                }
            }
        }

        return nullptr;
    }
}
