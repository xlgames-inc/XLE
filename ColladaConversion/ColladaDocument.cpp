// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Math/Vector.h"
#include "../Utility/Streams/XmlStreamFormatter.h"
#include "../Utility/Streams/StreamDom.h"
#include "../Utility/UTFUtils.h"
#include "../Utility/Conversion.h"
#include "../Utility/ParameterBox.h"
#include "../ConsoleRig/Log.h"

namespace std   // adding these to std is awkward, but it's the only way to make sure easylogging++ can see them
{
    inline MAKE_LOGGABLE(StreamLocation, loc, os) 
    {
        os << "Line: " << loc._lineIndex << ", Char: " << loc._charIndex;
        return os;
    }

    // inline MAKE_LOGGABLE(XmlInputStreamFormatter<utf8>::InteriorSection, section, os) 
    inline std::ostream& operator<<(std::ostream& os, XmlInputStreamFormatter<utf8>::InteriorSection section)
    {
        os << Conversion::Convert<std::basic_string<char>>(
            std::basic_string<utf8>(section._start, section._end));
        return os;
    }
}

namespace ColladaConversion
{
    using Formatter = XmlInputStreamFormatter<utf8>;
    using Section = Formatter::InteriorSection;
    using SubDoc = Utility::Document<Formatter>;
    using String = std::basic_string<Formatter::value_type>;

    template <typename Type, int Count>
        cml::vector<Type, cml::fixed<Count>> ParseValueType(
            Formatter& formatter, const cml::vector<Type, cml::fixed<Count>>& def);

    template <typename Type>
        Type ParseValueType(Formatter& formatter, Type& def);

    template<typename Section>
        static std::string AsString(const Section& section)
    {
        using CharType = std::remove_const<std::remove_reference<decltype(*section._start)>::type>::type;
        return Conversion::Convert<std::string>(
            std::basic_string<CharType>(section._start, section._end));
    }


    class AssetDesc
    {
    public:
        float _metersPerUnit;
        enum class UpAxis { X, Y, Z };
        UpAxis _upAxis;

        AssetDesc();
        AssetDesc(XmlInputStreamFormatter<utf8>& formatter);
    };

    class Effect;

    class ColladaDocument
    {
    public:
        void Parse(XmlInputStreamFormatter<utf8>& formatter);

        void Parse_LibraryEffects(XmlInputStreamFormatter<utf8>& formatter);
        void Parse_LibraryGeometries(XmlInputStreamFormatter<utf8>& formatter);

        ColladaDocument();
        ~ColladaDocument();

    protected:
        AssetDesc _rootAsset;

        std::vector<Effect> _effects;
    };

    bool Is(XmlInputStreamFormatter<utf8>::InteriorSection& section, const utf8 match[])
    {
        return !XlComparePrefix(match, section._start, section._end - section._start);
    }

    bool StartsWith(XmlInputStreamFormatter<utf8>::InteriorSection& section, const utf8 match[])
    {
        auto matchLen = XlStringLen(match);
        if ((section._end - section._start) < ptrdiff_t(matchLen)) return false;
        return !XlComparePrefix(section._start, match, matchLen);
    }

    using RootElementParser = void (ColladaDocument::*)(XmlInputStreamFormatter<utf8>&);

    static std::pair<const utf8*, RootElementParser> s_rootElements[] = 
    {
        std::make_pair(u("library_effects"), &ColladaDocument::Parse_LibraryEffects),
        std::make_pair(u("library_geometries"), &ColladaDocument::Parse_LibraryGeometries)
    };


    

    AssetDesc::AssetDesc()
    {
        _metersPerUnit = 1.f;
        _upAxis = UpAxis::Z;
    }

    AssetDesc::AssetDesc(XmlInputStreamFormatter<utf8>& formatter)
        : AssetDesc()
    {
        using Formatter = XmlInputStreamFormatter<utf8>;

        for (;;) {
            switch (formatter.PeekNext()) {
            case Formatter::Blob::AttributeName:
                {
                    Formatter::InteriorSection name, value;
                    formatter.TryAttribute(name, value);
                    continue;
                }

            case Formatter::Blob::BeginElement:
                {
                    Formatter::InteriorSection eleName;
                    formatter.TryBeginElement(eleName);
                    if (Is(eleName, u("unit"))) {
                        // Utility::Document<Formatter> doc(formatter);
                        // _metersPerUnit = doc(u("meter"), _metersPerUnit);
                        auto meter = ExtractSingleAttribute(formatter, u("meter"));
                        _metersPerUnit = Parse(meter, _metersPerUnit);
                    } else if (Is(eleName, u("up_axis"))) {
                        if (formatter.TryCharacterData(eleName)) {
                            if ((eleName._end - eleName._start) >= 1) {
                                switch (std::tolower(*eleName._start)) {
                                case 'x': _upAxis = UpAxis::X; break;
                                case 'y': _upAxis = UpAxis::Y; break;
                                case 'z': _upAxis = UpAxis::Z; break;
                                }
                            }
                        }
                    } else
                        formatter.SkipElement();

                    if (!formatter.TryEndElement())
                        Throw(FormatException("Expecting end element", formatter.GetLocation()));

                    continue;
                }

            default:
                break;
            }

            break;
        }
    }
    


    void ColladaDocument::Parse(XmlInputStreamFormatter<utf8>& formatter)
    {
        using Formatter = XmlInputStreamFormatter<utf8>;
        Formatter::InteriorSection rootEle;
        if (!formatter.TryBeginElement(rootEle) || !Is(rootEle, u("COLLADA")))
            Throw(FormatException("Expecting root COLLADA element", formatter.GetLocation()));
        
        for (;;) {
            switch (formatter.PeekNext()) {
            case Formatter::Blob::BeginElement:
                {
                    Formatter::InteriorSection ele;
                    formatter.TryBeginElement(ele);

                    if (Is(ele, u("asset"))) {
                        _rootAsset = AssetDesc(formatter);
                    } else {
                        bool found = false;
                        for (unsigned c=0; c<dimof(s_rootElements); ++c)
                            if (Is(ele, s_rootElements[c].first)) {
                                (this->*(s_rootElements[c].second))(formatter);
                                found = true;
                                break;
                            }

                        if (!found) {
                            LogWarning << "Skipping element " << AsString(ele);
                            formatter.SkipElement();
                        }
                    }

                    if (!formatter.TryEndElement())
                        Throw(FormatException("Expecting end element", formatter.GetLocation()));

                    continue;
                }

            case Formatter::Blob::EndElement:
                break; // hit the end of file

            case Formatter::Blob::AttributeName:
                {
                    // we should scan for collada version here
                    Formatter::InteriorSection name, value;
                    formatter.TryAttribute(name, value);
                    continue;
                }

            default:
                Throw(FormatException("Unexpected blob", formatter.GetLocation()));
            }

            break;
        }
    }

    enum class SamplerAddress
    {
        Wrap, Mirror, Clamp, Border, MirrorOnce
    };

    enum class SamplerFilter { Point, Linear, Anisotropic };
    enum class SamplerDimensionality { T2D, T3D, Cube };
    
    class ParameterSet
    {
    public:
        class BasicParameter
        {
        public:
            Section _sid;
            Section _type;
            Section _value;
        };
        std::vector<BasicParameter> _parameters;

        class SamplerParameter
        {
        public:
            Section _sid;
            Section _type;
            Section _image;
            SamplerDimensionality _dimensionality;
            SamplerAddress _addressS;
            SamplerAddress _addressT;
            SamplerAddress _addressQ;
            SamplerFilter _minFilter;
            SamplerFilter _maxFilter;
            SamplerFilter _mipFilter;
            Float4 _borderColor;
            unsigned _minMipLevel;
            unsigned _maxMipLevel;
            float _mipMapBias;
            unsigned _maxAnisotrophy;
            SubDoc _extra;

            SamplerParameter();
            SamplerParameter(Formatter& formatter);
            ~SamplerParameter();
        };
        std::vector<SamplerParameter> _samplerParameters;

        void ParseParam(Formatter& formatter);

        ParameterSet();
        ~ParameterSet();
        ParameterSet(ParameterSet&& moveFrom) never_throws;
        ParameterSet& operator=(ParameterSet&&) never_throws;
    };

    ParameterSet::SamplerParameter::SamplerParameter()
    {
        _dimensionality = SamplerDimensionality::T2D;
        _addressS = _addressT = _addressQ = SamplerAddress::Wrap;
        _minFilter = _maxFilter = _mipFilter = SamplerFilter::Point;
        _borderColor = Float4(1.f, 1.f, 1.f, 1.f);
        _minMipLevel = 0;
        _maxMipLevel = 255;
        _mipMapBias = 0.f;
        _maxAnisotrophy = 1;
    }

    static Section ExtractSingleAttribute(Formatter& formatter, const Formatter::value_type attribName[])
    {
        Section result;

        for (;;) {
            switch (formatter.PeekNext()) {
            case Formatter::Blob::BeginElement:
                {
                    Formatter::InteriorSection eleName;
                    formatter.TryBeginElement(eleName);
                    formatter.SkipElement();
                    if (!formatter.TryEndElement())
                        Throw(FormatException("Expecting end element", formatter.GetLocation()));
                    continue;
                }

            case Formatter::Blob::AttributeName:
                {
                    Formatter::InteriorSection name, value;
                    formatter.TryAttribute(name, value);
                    if (Is(name, attribName)) result = value;
                    continue;
                }
            }

            break;
        }

        return result;
    }

    std::pair<SamplerAddress, const utf8*> s_SamplerAddressNames[] = 
    {
        std::make_pair(SamplerAddress::Wrap, u("WRAP")),
        std::make_pair(SamplerAddress::Mirror, u("MIRROR")),
        std::make_pair(SamplerAddress::Clamp, u("CLAMP")),
        std::make_pair(SamplerAddress::Border, u("BORDER")),
        std::make_pair(SamplerAddress::MirrorOnce, u("MIRROR_ONE")),

        std::make_pair(SamplerAddress::Wrap, u("REPEAT")),
        std::make_pair(SamplerAddress::Mirror, u("MIRROR_REPEAT"))
        // CLAMP_TO_EDGE not supported
    };

    std::pair<SamplerFilter, const utf8*> s_SamplerFilterNames[] = 
    {
        std::make_pair(SamplerFilter::Point, u("NONE")),
        std::make_pair(SamplerFilter::Point, u("NEAREST")),
        std::make_pair(SamplerFilter::Linear, u("LINEAR")),
        std::make_pair(SamplerFilter::Anisotropic, u("ANISOTROPIC"))
    };

    template <typename Enum, unsigned Count>
        static Enum ParseEnum(Formatter& formatter, const std::pair<Enum, const utf8*> (&table)[Count])
    {
        Formatter::InteriorSection section;
        if (!formatter.TryCharacterData(section)) return table[0].first;

        for (unsigned c=0; c<Count; ++c)
            if (!Is(section, table[c].second))
                return table[c].first;

        return table[0].first;  // first one is the default
    }

    ParameterSet::SamplerParameter::SamplerParameter(Formatter& formatter)
        : SamplerParameter()
    {
        for (;;) {
            switch (formatter.PeekNext()) {
            case Formatter::Blob::BeginElement:
                {
                    Formatter::InteriorSection eleName;
                    formatter.TryBeginElement(eleName);

                    if (Is(eleName, u("instance_image"))) {
                        // collada 1.5 uses "instance_image" (Collada 1.4 equivalent is <source>)
                        _image = ExtractSingleAttribute(formatter, u("url"));
                    } else if (Is(eleName, u("source"))) {
                        // collada 1.4 uses "source" (which cannot have extra data attached)
                        formatter.TryCharacterData(_image);
                    } else if (Is(eleName, u("wrap_s"))) {
                        _addressS = ParseEnum(formatter, s_SamplerAddressNames);
                    } else if (Is(eleName, u("wrap_t"))) {
                        _addressT = ParseEnum(formatter, s_SamplerAddressNames);
                    } else if (Is(eleName, u("wrap_p"))) {
                        _addressQ = ParseEnum(formatter, s_SamplerAddressNames);
                    } else if (Is(eleName, u("minfilter"))) {
                        _minFilter = ParseEnum(formatter, s_SamplerFilterNames);
                    } else if (Is(eleName, u("magfilter"))) {
                        _maxFilter = ParseEnum(formatter, s_SamplerFilterNames);
                    } else if (Is(eleName, u("mipfilter"))) {
                        _mipFilter = ParseEnum(formatter, s_SamplerFilterNames);
                    } else if (Is(eleName, u("border_color"))) {
                        _borderColor = ParseValueType(formatter, _borderColor);
                    } else if (Is(eleName, u("mip_max_level"))) {
                        _maxMipLevel = ParseValueType(formatter, _maxMipLevel);
                    } else if (Is(eleName, u("mip_min_level"))) {
                        _minMipLevel = ParseValueType(formatter, _minMipLevel);
                    } else if (Is(eleName, u("mip_bias"))) {
                        _mipMapBias = ParseValueType(formatter, _mipMapBias);
                    } else if (Is(eleName, u("max_anisotropy"))) {
                        _maxAnisotrophy = ParseValueType(formatter, _maxAnisotrophy);
                    } else if (Is(eleName, u("extra"))) {
                        _extra = SubDoc(formatter);
                    } else {
                        formatter.SkipElement();
                    }

                    if (!formatter.TryEndElement())
                        Throw(FormatException("Expecting end element", formatter.GetLocation()));
                    continue;
                }

            case Formatter::Blob::AttributeName:
                {
                    Formatter::InteriorSection name, value;
                    formatter.TryAttribute(name, value);
                    LogWarning << "Sampler objects should not have any attributes. At " << formatter.GetLocation();
                    continue;
                }
            }

            break;
        }
    }

    ParameterSet::SamplerParameter::~SamplerParameter() {}

    void ParameterSet::ParseParam(Formatter& formatter)
    {
        Formatter::InteriorSection sid;

        for (;;) {
            switch (formatter.PeekNext()) {
            case Formatter::Blob::BeginElement:
                {
                    Formatter::InteriorSection eleName;
                    formatter.TryBeginElement(eleName);

                    if (Is(eleName, u("annotate")) || Is(eleName, u("semantic")) || Is(eleName, u("modifier"))) {
                        formatter.SkipElement();
                    } else {
                        if (!Is(eleName, u("sampler"))) {
                            _samplerParameters.push_back(SamplerParameter(formatter));
                        } else {
                                // this is a basic parameter, typically a scalar, vector or matrix
                                // we don't need to parse it fully now; just get the location of the 
                                // data and store it as a new parameter
                            Formatter::InteriorSection cdata;
                            if (formatter.TryCharacterData(cdata)) {
                                _parameters.push_back(BasicParameter{sid, eleName, cdata});
                            } else 
                                LogWarning << "Expecting element with parameter data " << formatter.GetLocation();
                        }
                    }

                    if (!formatter.TryEndElement())
                        Throw(FormatException("Expecting end element", formatter.GetLocation()));
                    continue;
                }

            case Formatter::Blob::AttributeName:
                {
                    Formatter::InteriorSection name, value;
                    formatter.TryAttribute(name, value);
                    if (Is(name, u("sid"))) sid = value;
                    continue;
                }
            }

            break;
        }
    }

    ParameterSet::ParameterSet() {}
    ParameterSet::~ParameterSet() {}
    ParameterSet::ParameterSet(ParameterSet&& moveFrom)
    :   _parameters(std::move(moveFrom._parameters))
    ,   _samplerParameters(std::move(moveFrom._samplerParameters))
    {
    }

    ParameterSet& ParameterSet::operator=(ParameterSet&& moveFrom)
    {
        _parameters = std::move(moveFrom._parameters);
        _samplerParameters = std::move(moveFrom._samplerParameters);
        return *this;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class TechniqueValue
    {
    public:
        enum class Type { Color, Texture, Float, Param, None };
        Type _type;
        Section _reference; // texture or parameter reference
        Section _texCoord;
        Float4 _value;

        TechniqueValue(Formatter& formatter);
    };

    class Effect
    {
    public:
        Section _name;
        Section _id;
        ParameterSet _params;

        class Profile
        {
        public:
            ParameterSet _params;
            String _profileType;
            String _shaderName;        // (phong, blinn, etc)
            std::vector<std::pair<String, TechniqueValue>> _values;

            SubDoc _extra;
            SubDoc _techniqueExtra;

            Profile(Formatter& formatter, String profileType);
            Profile(Profile&& moveFrom) never_throws;
            Profile& operator=(Profile&& moveFrom) never_throws;

        protected:
            void ParseTechnique(Formatter& formatter);
            void ParseShaderType(Formatter& formatter);

        };
        std::vector<Profile> _profiles;

        SubDoc _extra;

        Effect(Formatter& formatter);
        Effect(Effect&& moveFrom) never_throws;
        Effect& operator=(Effect&& moveFrom) never_throws;
    };

    Effect::Profile::Profile(Formatter& formatter, String profileType)
    : _profileType(profileType)
    {
        for (;;) {
            switch (formatter.PeekNext()) {
            case Formatter::Blob::BeginElement:
                {
                    Formatter::InteriorSection eleName;
                    formatter.TryBeginElement(eleName);

                    if (Is(eleName, u("newparam"))) {
                        _params.ParseParam(formatter);
                    } else if (Is(eleName, u("extra"))) {
                        _extra = SubDoc(formatter);
                    } else if (Is(eleName, u("technique"))) {
                        ParseTechnique(formatter);
                    } else {
                        // asset
                        formatter.SkipElement();
                    }

                    if (!formatter.TryEndElement())
                        Throw(FormatException("Expecting end element", formatter.GetLocation()));
                    continue;
                }

            case Formatter::Blob::AttributeName:
                {
                    Formatter::InteriorSection name, value;
                    formatter.TryAttribute(name, value);
                    continue;
                }
            }

            break;
        }
    }

    Effect::Profile::Profile(Profile&& moveFrom)
    : _params(std::move(moveFrom._params))
    , _profileType(std::move(moveFrom._profileType))
    , _shaderName(std::move(moveFrom._shaderName))
    , _values(std::move(moveFrom._values))
    , _extra(std::move(moveFrom._extra))
    , _techniqueExtra(std::move(moveFrom._techniqueExtra))
    {
    }

    auto Effect::Profile::operator=(Profile&& moveFrom) -> Profile&
    {
        _params = std::move(moveFrom._params);
        _profileType = std::move(moveFrom._profileType);
        _shaderName = std::move(moveFrom._shaderName);
        _values = std::move(moveFrom._values);
        _extra = std::move(moveFrom._extra);
        _techniqueExtra = std::move(moveFrom._techniqueExtra);
        return *this;
    }

    void Effect::Profile::ParseTechnique(Formatter& formatter)
    {
        for (;;) {
            switch (formatter.PeekNext()) {
            case Formatter::Blob::BeginElement:
                {
                    Formatter::InteriorSection eleName;
                    formatter.TryBeginElement(eleName);

                    // Note that we skip a lot of the content in techniques
                    // importantly, we skip "pass" elements. "pass" is too tightly
                    // bound to the structure of Collada FX. It makes it difficult
                    // to extract the particular properties we want, and transform
                    // them into something practical.

                    if (Is(eleName, u("extra"))) {
                        _extra = SubDoc(formatter);
                    } else if (Is(eleName, u("asset")) || Is(eleName, u("annotate")) || Is(eleName, u("pass"))) {
                        formatter.SkipElement();
                    } else {
                        // Any other elements are seen as a shader definition
                        // There should be exactly 1.
                        _shaderName = String(eleName._start, eleName._end);
                        ParseShaderType(formatter);
                    }

                    if (!formatter.TryEndElement())
                        Throw(FormatException("Expecting end element", formatter.GetLocation()));
                    continue;
                }

            case Formatter::Blob::AttributeName:
                {
                    Formatter::InteriorSection name, value;
                    formatter.TryAttribute(name, value);
                    continue;
                }
            }

            break;
        }
    }

    template<typename CharType>
        bool IsWhitespace(CharType chr)
    {
        return chr == 0x20 || chr == 0x9 || chr == 0xD || chr == 0xA;
    }

    template<typename CharType>
        static const CharType* FastParseElement(int64& dst, const CharType* start, const CharType* end)
    {
        bool positive = true;
        dst = 0;

        if (start >= end) return start;
        if (*start == '-') { positive = false; ++start; }
        else if (*start == '+') ++start;

        uint64 result = 0;
        for (;;) {
            if (start >= end) break;
            if (*start < '0' || *start > '9') break;

            result = (result * 10ull) + uint64((*start) - '0');
        }
        return positive ? result : -result;
    }

    template<typename CharType>
        static const CharType* FastParseElement(uint64& dst, const CharType* start, const CharType* end)
    {
        uint64 result = 0;
        for (;;) {
            if (start >= end) break;
            if (*start < '0' || *start > '9') break;

            result = (result * 10ull) + uint64((*start) - '0');
        }
        return result;
    }

    template<typename CharType>
        static const CharType* FastParseElement(float& dst, const CharType* start, const CharType* end)
    {
        // this code found on stack exchange...
        //      (http://stackoverflow.com/questions/98586/where-can-i-find-the-worlds-fastest-atof-implementation)
        // But there are some problems!
        // Most importantly:
        //      Sub-normal numbers are not handled properly. Subnormal numbers happen when the exponent
        //      is the smallest is can be. In this case, values in the mantissa are evenly spaced around
        //      zero. 
        //
        // It does other things right. But I don't think it's reliable enough to use. It's a pity because
        // the standard library functions require null terminated strings, and seems that it may be possible
        // to get a big performance improvement loss of few features.

            // to avoid making a copy, we're going do a hack and 
            // We're assuming that "end" is writable memory. This will be the case when parsing
            // values from XML. But in other cases, it may not be reliable.
            // Also, consider that there might be threading implications in some cases!
        CharType replaced = *end;
        *const_cast<CharType*>(end) = '\0';
        char* newEnd = nullptr;
        dst = std::strtof((const char*)start, &newEnd);
        *const_cast<CharType*>(end) = replaced;

        return (const CharType*)newEnd;
    }

    template<typename Type>
        static unsigned ParseXMLList(Type dest[], unsigned destCount, Section section)
    {
        assert(destCount > 0);

        // in xml, lists are deliminated by white space
        unsigned elementCount = 0;
        auto* eleStart = section._start;
        for (;;) {
            while (eleStart < section._end && IsWhitespace(*eleStart)) ++eleStart;

            auto* eleEnd = FastParseElement(dest[elementCount], eleStart, section._end);

            if (eleStart == eleEnd) return elementCount;
            ++elementCount;
            if (elementCount >= destCount) return elementCount;
            eleStart = eleEnd;
        }
    }

    template <typename Type, int Count>
        cml::vector<Type, cml::fixed<Count>> ParseValueType(
            Formatter& formatter, // Formatter::InteriorSection storedType,
            const cml::vector<Type, cml::fixed<Count>>& def)
    {
        Formatter::InteriorSection cdata;

        // auto type = HLSLTypeNameAsTypeDesc(
        //     Conversion::Convert<std::string>(String(storedType._start, storedType._end)).c_str());
        // if (type._type == ImpliedTyping::TypeCat::Void) {
        //     type._type = ImpliedTyping::TypeCat::Float;
        //     type._arrayCount = 1;
        // }

        if (formatter.TryCharacterData(cdata)) {

            cml::vector<Type, cml::fixed<Count>> result;
            unsigned elementsRead = ParseXMLList(&result[0], Count, cdata);
            for (unsigned c=elementsRead; c<Count; ++c)
                result[c] = def[c];
            return result;

        } else {
            LogWarning << "Expecting vector data at " << formatter.GetLocation();
            return def;
        }
    }

    template <typename Type>
        Type ParseValueType(Formatter& formatter, Type& def)
    {
        Formatter::InteriorSection cdata;
        if (formatter.TryCharacterData(cdata)) {

            auto p = ImpliedTyping::Parse<Type>(cdata._start, cdata._end);
            if (!p.first) return def;
            return p.second;

        } else {
            LogWarning << "Expecting scalar data at " << formatter.GetLocation();
            return def;
        }
    }

    TechniqueValue::TechniqueValue(Formatter& formatter)
    {
        _type = Type::None;

        // there can be attributes in the containing element -- which should should skip now
        {
            Formatter::InteriorSection name, value;
            while (formatter.TryAttribute(name, value)) {}
        }

        // We should find exactly one element, which should be
        // of "color", "param", "texture" or "float" type
        Formatter::InteriorSection eleName; 
        if (!formatter.TryBeginElement(eleName))
            Throw(FormatException("Expecting element for technique value", formatter.GetLocation()));
        
        {
                // skip all attributes (sometimes get "sid" tags)
            while (formatter.PeekNext(true) == Formatter::Blob::AttributeName) {
                Formatter::InteriorSection name, value;
                formatter.TryAttribute(name, value);
            }
        }

        if (Is(eleName, u("float"))) {

            _value = ParseValueType(formatter, _value);
            _type = Type::Float;

        } else if (Is(eleName, u("color"))) {

            Formatter::InteriorSection cdata; 
            if (formatter.TryCharacterData(cdata)) {
                ParseXMLList(&_value[0], 4, cdata);
                _type = Type::Color;
            } else
                LogWarning << "no data in color value at " << formatter.GetLocation();

        } else if (Is(eleName, u("texture"))) {

            for (;;) {
                Formatter::InteriorSection name, value; 
                if (!formatter.TryAttribute(name, value)) break;
                if (Is(name, u("texture"))) _reference = value;
                else if (Is(name, u("texcoord"))) _texCoord = value;
                else LogWarning << "Unknown attribute for texture (" << name << ") at " << formatter.GetLocation();
            }
            _type = Type::Texture;

        } else if (Is(eleName, u("param"))) {

            Formatter::InteriorSection name, value; 
            if (!formatter.TryAttribute(name, value) || !Is(name, u("ref"))) {
                LogWarning << "Expecting ref attribute in param technique value at " << formatter.GetLocation();
            } else {
                _reference = value;
                _type = Type::Param;
            }

        } else 
            Throw(FormatException("Expect either float, color, param or texture element", formatter.GetLocation()));

        if (!formatter.TryEndElement())
            Throw(FormatException("Expecting end element", formatter.GetLocation()));
    }

    void Effect::Profile::ParseShaderType(Formatter& formatter)
    {
        // This is one of several types of shading equations defined
        // by Collada (eg, blinn, phong, lambert)
        //
        // "shader type" elements just contain a list of parameters,
        // each of which can have a value of any of these types:
        // <color> <float> <texture> or <param>
        //
        // Also, there's are special cases for <transparent> and <transparency>
        //      -- they seem a little strange, actually

        for (;;) {
            switch (formatter.PeekNext()) {
            case Formatter::Blob::BeginElement:
                {
                    Formatter::InteriorSection eleName;
                    formatter.TryBeginElement(eleName);

                    _values.push_back(
                        std::make_pair(
                            String(eleName._start, eleName._end),
                            TechniqueValue(formatter)));

                    if (!formatter.TryEndElement())
                        Throw(FormatException("Expecting end element", formatter.GetLocation()));
                    continue;
                }

            case Formatter::Blob::AttributeName:
                {
                    Formatter::InteriorSection name, value;
                    formatter.TryAttribute(name, value);
                    continue;
                }
            }

            break;
        }
    }

    Effect::Effect(Formatter& formatter)
    {
        for (;;) {
            switch (formatter.PeekNext()) {
            case Formatter::Blob::BeginElement:
                {
                    Formatter::InteriorSection eleName;
                    formatter.TryBeginElement(eleName);

                    if (Is(eleName, u("newparam"))) {
                        _params.ParseParam(formatter);
                    } else if (StartsWith(eleName, u("profile_"))) {
                        _profiles.push_back(Profile(formatter, String(eleName._start+8, eleName._end)));
                    } else if (Is(eleName, u("extra"))) {
                        _extra = SubDoc(formatter);
                    } else {
                        // asset, annotate
                        formatter.SkipElement();
                    }

                    if (!formatter.TryEndElement())
                        Throw(FormatException("Expecting end element", formatter.GetLocation()));
                    continue;
                }

            case Formatter::Blob::AttributeName:
                {
                    Formatter::InteriorSection name, value;
                    formatter.TryAttribute(name, value);
                    if (Is(name, u("name"))) _name = value;
                    else if (Is(name, u("id"))) _id = value;
                    continue;
                }
            }

            break;
        }
    }

    Effect::Effect(Effect&& moveFrom)
    : _name(moveFrom._name)
    , _id(moveFrom._id)
    , _params(std::move(moveFrom._params))
    , _profiles(std::move(moveFrom._profiles))
    , _extra(std::move(moveFrom._extra))
    {}

    Effect& Effect::operator=(Effect&& moveFrom)
    {
        _name = moveFrom._name;
        _id = moveFrom._id;
        _params = std::move(moveFrom._params);
        _profiles = std::move(moveFrom._profiles);
        _extra = std::move(moveFrom._extra);
        return *this;
    }

    void ColladaDocument::Parse_LibraryEffects(XmlInputStreamFormatter<utf8>& formatter)
    {
        using Formatter = XmlInputStreamFormatter<utf8>;
        for (;;) {
            switch (formatter.PeekNext()) {
            case Formatter::Blob::BeginElement:
                {
                    Formatter::InteriorSection eleName;
                    formatter.TryBeginElement(eleName);

                    if (Is(eleName, u("effect"))) {
                        _effects.push_back(Effect(formatter));
                    } else {
                            // "annotate", "asset" and "extra" are also valid
                        formatter.SkipElement();
                    }

                    if (!formatter.TryEndElement())
                        Throw(FormatException("Expecting end element", formatter.GetLocation()));
                    continue;
                }

            case Formatter::Blob::AttributeName:
                {
                    Formatter::InteriorSection name, value;
                    formatter.TryAttribute(name, value);
                    continue;
                }
            }

            break;
        }
    }

    class Source
    {
    public:
        Section _id;
        Section _arrayData;
        Section _arrayType;
        Section _arrayId;
        unsigned _arrayCount;

        Source(Formatter& formatter);
    };

    class Geometry
    {
    public:

        
        std::vector<Source> _sources;

        Geometry(Formatter& formatter);

        Geometry();
        Geometry(Geometry&& moveFrom) never_throws;
        Geometry& operator=(Geometry&& moveFrom) never_throws;

    protected:
        void ParseMesh(Formatter& formatter);
    };

    #define ON_ELEMENT                                              \
        for (;;) {                                                  \
            switch (formatter.PeekNext()) {                         \
            case Formatter::Blob::BeginElement:                     \
                {                                                   \
                    Formatter::InteriorSection eleName;             \
                    formatter.TryBeginElement(eleName);             \
        /**/

    #define ON_ATTRIBUTE                                            \
                    if (!formatter.TryEndElement())                 \
                        Throw(FormatException("Expecting end element", formatter.GetLocation()));       \
                    continue;                                       \
                }                                                   \
                                                                    \
            case Formatter::Blob::AttributeName:                    \
                {                                                   \
                    Formatter::InteriorSection name, value;         \
                    formatter.TryAttribute(name, value);            \
        /**/

    #define PARSE_END                                               \
                    continue;                                       \
                }                                                   \
            }                                                       \
                                                                    \
            break;                                                  \
        }                                                           \
        /**/


    Source::Source(Formatter& formatter)
    {
        ON_ELEMENT
            if (EndsWith(eleName, "_array")) {

                    // This is an array of elements, typically with an id and count
                    // we should have only a single array in each source. But it can
                    // be one of many different types.
                    // Most large data in Collada should appear in blocks like this. 
                    // We don't attempt to parse it here, just record it's location in
                    // the file for later;
                
                _arrayType = Section(eleName._start, eleName._end-6);
                
                    // this element should have no sub-elements. But it has important
                    // attributes. Given that the source is XML, the attributes
                    // will always come first.
                {
                    while (formatter.PeekNext(true) == Formatter::Blob::AttributeName) {
                        Section name, value;
                        formatter.TryAttribute(name, value);

                        if (Is(name, u("count"))) {
                            _arrayCount = Parse(value, 0u);
                        } else if (Is(name, u("id"))) {
                            _arrayId = value;
                        } // "name also valid
                    }
                }

                Section cdata;
                if (formatter.TryCharacterData(cdata)) {
                    _arrayData = cdata;
                } else {
                    LogWarning << "Data source contains no data! At: " << formatter.GetLocation();
                }

            } else if (Is(eleName, "technique")) {

                // this can contain special case non-standard information
                // But it's not clear what this would be used for
                formatter.SkipElement();

            } else if (Is(eleName, "technique_common")) {

                // This specifies the way we interpret the information in the array.
                // It should occur after the array object. So we can resolve the reference
                // in the accessor -- but only assuming that the reference in the accessor
                // refers to an array in this document. That would be logical and typical.
                // But the standard suggests the accessor can actually refer to a data array
                // anywhere (including in other files)
                // Anyway, it looks like we should have only a single accessor per technique

            } else {
                    // "asset" also valid
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
            if (Is(name, u("id"))) {
                _id = value;
            } // "name" is also valid
        PARSE_END
    }

    void Geometry::ParseMesh(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("source"))) {
                _sources.push_back(Source(formatter));
            }

        ON_ATTRIBUTE
        PARSE_END
    }

    Geometry::Geometry(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("mesh"))) {

                ParseMesh(formatter);

            } else if (Is(eleName, u("convex_mesh")) || Is(eleName, u("spline")) || Is(eleName, u("brep"))) {
                LogWarning << "convex_mesh, spline and brep geometries are not supported. At: " << formatter.GetLocation();
                formatter.SkipElement();
            } else {
                    // "asset" and "extra" are also valid, but it's unlikely that they would be present here
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
        PARSE_END
    }

    void ColladaDocument::Parse_LibraryGeometries(XmlInputStreamFormatter<utf8>& formatter)
    {
        for (;;) {
            switch (formatter.PeekNext()) {
            case Formatter::Blob::BeginElement:
                {
                    Formatter::InteriorSection eleName;
                    formatter.TryBeginElement(eleName);

                    if (Is(eleName, u("geometry"))) {
                        _geometries.push_back(Geometry(formatter));
                    } else {
                            // "asset" and "extra" are also valid, but uninteresting
                        formatter.SkipElement();
                    }

                    if (!formatter.TryEndElement())
                        Throw(FormatException("Expecting end element", formatter.GetLocation()));
                    continue;
                }

            case Formatter::Blob::AttributeName:
                {
                    Formatter::InteriorSection name, value;
                    formatter.TryAttribute(name, value);
                    // name and id. Not interesting if we have only a single library
                    continue;
                }
            }

            break;
        }
    }

    ColladaDocument::ColladaDocument() {}
    ColladaDocument::~ColladaDocument() {}

}

#include "../Utility/Streams/FileUtils.h"
#include "NascentModel.h"

void TestParser()
{
    size_t size;
    auto block = LoadFileAsMemoryBlock("game/testmodels/nyra/Nyra_pose.dae", &size);
    using Formatter = XmlInputStreamFormatter<utf8>;
    Formatter formatter(MemoryMappedInputStream(block.get(), PtrAdd(block.get(), size)));
    ColladaConversion::ColladaDocument doc;
    doc.Parse(formatter);

    int t = 0;
    (void)t;
}


