// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MarshalString.h"
#include "MathLayer.h"
#include "../../ConsoleRig/Console.h"
#include "../../Math/Vector.h"

namespace GUILayer 
{
    public ref class TweakableBridge : public ::System::Dynamic::DynamicObject
    {
    public:
        bool TryGetMember(System::Dynamic::GetMemberBinder^ binder, Object^% result) override
        {
            return TryGetMember(binder->Name, binder->IgnoreCase, result);
        }

        bool TrySetMember(System::Dynamic::SetMemberBinder^ binder, Object^ value) override
        {
            return TrySetMember(binder->Name, binder->IgnoreCase, value);
        }

        bool TryGetMember(System::String^ name, bool ignoreCase, Object^% result)
        {
            auto nativeName = clix::marshalString<clix::E_UTF8>(name);
            
            auto tweakable0 = ConsoleRig::Detail::FindTweakable<int>(nativeName.c_str());
            if (tweakable0) { result = *tweakable0; return true; }
            auto tweakable1 = ConsoleRig::Detail::FindTweakable<bool>(nativeName.c_str());
            if (tweakable1) { result = *tweakable1; return true; }
            auto tweakable2 = ConsoleRig::Detail::FindTweakable<float>(nativeName.c_str());
            if (tweakable2) { result = *tweakable2; return true; }

            auto tweakable3 = ConsoleRig::Detail::FindTweakable<std::string>(nativeName.c_str());
            if (tweakable3) { result = clix::marshalString<clix::E_UTF8>(*tweakable3); return true; }

            auto tweakable4 = ConsoleRig::Detail::FindTweakable<Float3>(nativeName.c_str());
            if (tweakable4) { result = AsVector3(*tweakable4); return true; }
            auto tweakable5 = ConsoleRig::Detail::FindTweakable<Float4>(nativeName.c_str());
            if (tweakable5) { result = AsVector4(*tweakable5); return true; }
            
            return false;
        }

        bool TrySetMember(System::String^ name, bool ignoreCase, Object^ value)
        {
            auto nativeName = clix::marshalString<clix::E_UTF8>(name);

            auto tweakable0 = ConsoleRig::Detail::FindTweakable<int>(nativeName.c_str());
            if (tweakable0 && dynamic_cast<int^>(value)) { *tweakable0 = *dynamic_cast<int^>(value); return true; }
            auto tweakable1 = ConsoleRig::Detail::FindTweakable<bool>(nativeName.c_str());
            if (tweakable1 && dynamic_cast<bool^>(value)) { *tweakable1 = *dynamic_cast<bool^>(value); return true; }
            auto tweakable2 = ConsoleRig::Detail::FindTweakable<float>(nativeName.c_str());
            if (tweakable2 && dynamic_cast<float^>(value)) { *tweakable2 = *dynamic_cast<float^>(value); return true; }
            if (tweakable2 && dynamic_cast<System::Double^>(value)) { *tweakable2 = (float)*dynamic_cast<System::Double^>(value); return true; }

            auto tweakable3 = ConsoleRig::Detail::FindTweakable<std::string>(nativeName.c_str());
            if (tweakable3 && dynamic_cast<System::String^>(value)) { 
                *tweakable3 = clix::marshalString<clix::E_UTF8>(dynamic_cast<System::String^>(value));
                return true; 
            }

            auto tweakable4 = ConsoleRig::Detail::FindTweakable<Float3>(nativeName.c_str());
            if (tweakable4 && dynamic_cast<Vector3^>(value)) { *tweakable4 = AsFloat3(*dynamic_cast<Vector3^>(value)); return true; }
            auto tweakable5 = ConsoleRig::Detail::FindTweakable<Float4>(nativeName.c_str());
            if (tweakable5 && dynamic_cast<Vector4^>(value)) { *tweakable5 = AsFloat4(*dynamic_cast<Vector4^>(value)); return true; }
            
            return false;
        }

        TweakableBridge() {}
    };
}

