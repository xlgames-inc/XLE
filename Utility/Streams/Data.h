// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Core/Types.h"

namespace Utility
{
    class OutputStream;
	class BasicFile;

    class Data {
    public:
        char* value;
        Data* child;
        Data* next;
        Data* prev;
        Data* parent;
        char* preComment;
        char* postComment;
        Data* meta;
        int lineNum;

        // constructor/destructor
        Data(const char* value = "__none__");
        ~Data();

        // access by index
        int Index() const;
        int Size() const;
        Data* ChildAt(int i) const;

        // access by value
        Data* ChildWithValue(const char* value) const;
        Data* NextWithValue(const char* value) const;
        Data* PrevWithValue(const char* value) const;

        const char* ValueAt(int i, const char* def=0) const;

        // access by path
        void Path(char* dst, int count);
        Data* Find(const char* path) const;
        // DataVec FindRange(const char* path) const;

        // attributes
        Data* Attribute(const char* path) const;
        bool BoolAttribute(const char *path, bool def=false) const;
        int IntAttribute(const char* path, int def=0) const;
        int64 Int64Attribute(const char* path, int64 def = 0) const;
        float FloatAttribute(const char* path, float def=0.0f) const;
        double DoubleAttribute(const char* path, double def=0.0f) const;
        const char* StrAttribute(const char* path, const char* def="") const;

        bool HasBoolAttribute(const char *path, bool* out) const;
        bool HasIntAttribute(const char* path, int* out) const;
        bool HasInt64Attribute(const char* path, int64* out) const;
        bool HasFloatAttribute(const char* path, float* out) const;
        bool HasDoubleAttribute(const char* path, double* out) const;
        bool HasStrAttribute(const char* path, const char** out) const;

        void GetAttribute(const char* name, bool& value)    const { value = BoolAttribute(name); }
        void GetAttribute(const char* name, int& value)     const { value = IntAttribute(name); }
        void GetAttribute(const char* name, int16& value)   const { value = (int16)IntAttribute(name); }
        void GetAttribute(const char* name, uint8& value)   const { value = (uint8)IntAttribute(name); }
        void GetAttribute(const char* name, uint16& value)  const { value = (uint16)IntAttribute(name); }
        void GetAttribute(const char* name, uint32& value)  const { value = (uint32)IntAttribute(name); }
        void GetAttribute(const char* name, long& value)    const { value = IntAttribute(name); }
        void GetAttribute(const char* name, float& value)   const { value = FloatAttribute(name); }
        void GetAttribute(const char* name, double& value)  const { value = DoubleAttribute(name); }

        void SetAttribute(const char* name, Data* value);
        void SetAttribute(const char* name, bool value);
        void SetAttribute(const char* name, int value);
        void SetAttribute(const char* name, uint32 value);
        void SetAttribute(const char* name, float value);
        void SetAttribute(const char* name, double value);
        void SetAttribute(const char* name, const char* value);
        void SetAttribute(const char* name, int64 value);

        // value helpers
        bool BoolValue() const;
        int IntValue() const;
        int64 Int64Value() const;
        float FloatValue() const;
        double DoubleValue() const;
        const char* StrValue() const;

        void SetValue(bool value);
        void SetValue(int value);
        void SetValue(float value);
        void SetValue(double value);
        void SetValue(const char* value);

        // operators
        bool operator==(const Data& n) const;

        void Clear();
        Data* Clone() const;
        void Add(Data* child);
        void Remove();

        void SetPreComment(const char* comment);
        void SetPostComment(const char* comment);
        void SetMeta(Data* meta);

        // serialization
        bool Load(const char* ptr, int len);
        // bool LoadFromFile(const char* filename, bool* noFile = 0);
        // bool Save(const char* filename, bool includeComment = true) const;
		bool LoadFromFile(BasicFile& file);

        bool SaveToBuffer(char* s, int* len) const ;
        bool SavePrettyValue(char* s, int* len) const;
        void SaveToOutputStream(OutputStream& f, bool includeComment = true) const;
    };

    #define foreachData(i, d) \
        for (Data* i = (d)->child; i; i = i->next)
    #define foreachDataValue(i, d, v) \
        for (Data* i = (d)->ChildWithValue(v); i; i = i->NextWithValue(v))

    #define SetVector2Attr(d, name, vec2) \
        { \
            Data* __vecData = new Data(name); \
            __vecData->SetAttribute("x", vec2.x); \
            __vecData->SetAttribute("y", vec2.y); \
            d->Add(__vecData); \
        }

    #define GetVector2Attr(d, name, vec2) \
        { \
            Data* __vecData = d->Find(name); \
            if (!__vecData) { \
                vec2 = Vec2(ZERO); \
            } else { \
                vec2.x = __vecData->FloatAttribute("x"); \
                vec2.y = __vecData->FloatAttribute("y"); \
            } \
        }

    #define SetVector3Attr(d, name, vec3) \
        { \
            Data* __vecData = new Data(name); \
          __vecData->SetAttribute("x", (float)vec3.x); \
          __vecData->SetAttribute("y", (float)vec3.y); \
          __vecData->SetAttribute("z", (float)vec3.z); \
          d->Add(__vecData); \
        }

    #define GetVector3Attr(d, name, vec3, def) \
        { \
            Data* __vecData = d->Find(name); \
            if (!__vecData) { \
                vec3 = def; \
            } else { \
                vec3.x = __vecData->FloatAttribute("x"); \
                vec3.y = __vecData->FloatAttribute("y"); \
                vec3.z = __vecData->FloatAttribute("z"); \
            } \
        }

    #define SetQuatAttr(d, name, q) \
        { \
            Data* __qData = new Data(name); \
            __qData->SetAttribute("x", q.v.x); \
            __qData->SetAttribute("y", q.v.y); \
            __qData->SetAttribute("z", q.v.z); \
            __qData->SetAttribute("w", q.w); \
            d->Add(__qData); \
        }

    #define GetQuatAttr(d, name, q) \
        { \
            Data* __qData = d->Find(name); \
            if (!__qData) { \
                q.SetIdentity(); \
            } else { \
                q.v.x = __qData->FloatAttribute("x"); \
                q.v.y = __qData->FloatAttribute("y"); \
                q.v.z = __qData->FloatAttribute("z"); \
                q.w = __qData->FloatAttribute("w"); \
            } \
        }

}

using namespace Utility;

