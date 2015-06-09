// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "../Math/Vector.h"
#include "../Math/Transformations.h"
#include "../Utility/Streams/XmlStreamFormatter.h"
#include "../Utility/Streams/StreamDom.h"
#include "../Utility/UTFUtils.h"
#include "../Utility/Conversion.h"
#include "../Utility/ParameterBox.h"
#include "../Utility/MemoryUtils.h"
#include "../ConsoleRig/Log.h"

namespace ColladaConversion
{
    template<typename Section>
        static std::string AsString(const Section& section)
    {
        using CharType = std::remove_const<std::remove_reference<decltype(*section._start)>::type>::type;
        return Conversion::Convert<std::string>(
            std::basic_string<CharType>(section._start, section._end));
    }
}

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
        os << ColladaConversion::AsString(section);
        return os;
    }
}

namespace ColladaConversion
{
    using Formatter = XmlInputStreamFormatter<utf8>;
    using Section = Formatter::InteriorSection;
    using SubDoc = Utility::Document<Formatter>;
    using String = std::basic_string<Formatter::value_type>;

    class AssetDesc
    {
    public:
        float _metersPerUnit;
        enum class UpAxis { X, Y, Z };
        UpAxis _upAxis;

        AssetDesc();
        AssetDesc(Formatter& formatter);
    };

    class Effect;
    class Geometry;
    class VisualScene;
    class SkinController;

    class ColladaDocument
    {
    public:
        void Parse(Formatter& formatter);

        void Parse_LibraryEffects(Formatter& formatter);
        void Parse_LibraryGeometries(Formatter& formatter);
        void Parse_LibraryVisualScenes(Formatter& formatter);
        void Parse_LibraryControllers(Formatter& formatter);
        void Parse_Scene(Formatter& formatter);

        ColladaDocument();
        ~ColladaDocument();

    protected:
        AssetDesc _rootAsset;

        std::vector<Effect> _effects;
        std::vector<Geometry> _geometries;
        std::vector<VisualScene> _visualScenes;
        std::vector<SkinController> _skinControllers;

        Section _visualScene;
        Section _physicsScene;
        Section _kinematicsScene;
    };

    static bool Is(const XmlInputStreamFormatter<utf8>::InteriorSection& section, const utf8 match[])
    {
        const auto* a = section._start;
        const auto* b = match;
        for (;;) {
            if (a == section._end)
                return !(*b);   // success if both strings have terminated at the same time
            if (*b != *a) return false;
            assert(*b); // potentially hit this assert if there are null characters in "section"... that isn't supported
            ++b; ++a;
        }
    }

    static bool BeginsWith(const XmlInputStreamFormatter<utf8>::InteriorSection& section, const utf8 match[])
    {
        auto matchLen = XlStringLen(match);
        if ((section._end - section._start) < ptrdiff_t(matchLen)) return false;
        return Is(Section(section._start, section._start + matchLen), match);
    }

    static bool EndsWith(const XmlInputStreamFormatter<utf8>::InteriorSection& section, const utf8 match[])
    {
        auto matchLen = XlStringLen(match);
        if ((section._end - section._start) < ptrdiff_t(matchLen)) return false;
        return Is(Section(section._end - matchLen, section._end), match);
    }

    template<typename Type>
        static Type Parse(const XmlInputStreamFormatter<utf8>::InteriorSection& section, const Type& def)
    {
            // ImpliedType::Parse is actually a fairly expensing parsing operation...
            // maybe we could get a faster result by just calling the standard library
            // type functions.
        auto d = ImpliedTyping::Parse<Type>(section._start, section._end);
        if (!d.first) return def;
        return d.second;
    }

    template <typename Type, int Count>
        cml::vector<Type, cml::fixed<Count>> ReadCDataAsList(
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

            cml::vector<Type, cml::fixed<Count>> result = def;
            ParseXMLList(&result[0], Count, cdata);
            return result;

        } else {
            LogWarning << "Expecting vector data at " << formatter.GetLocation();
            return def;
        }
    }

    template <typename Type>
        Type ReadCDataAsValue(Formatter& formatter, Type& def)
    {
        Formatter::InteriorSection cdata;
        if (formatter.TryCharacterData(cdata)) {
            return Parse<Type>(cdata, def);
        } else {
            LogWarning << "Expecting scalar data at " << formatter.GetLocation();
            return def;
        }
    }

    static void SkipAllAttributes(Formatter& formatter)
    {
        while (formatter.PeekNext(true) == Formatter::Blob::AttributeName) {
            Formatter::InteriorSection name, value;
            formatter.TryAttribute(name, value);
        }
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

    using RootElementParser = void (ColladaDocument::*)(XmlInputStreamFormatter<utf8>&);

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

    static std::pair<const utf8*, RootElementParser> s_rootElements[] = 
    {
        std::make_pair(u("library_effects"), &ColladaDocument::Parse_LibraryEffects),
        std::make_pair(u("library_geometries"), &ColladaDocument::Parse_LibraryGeometries),
        std::make_pair(u("library_visual_scenes"), &ColladaDocument::Parse_LibraryVisualScenes),
        std::make_pair(u("library_controllers"), &ColladaDocument::Parse_LibraryControllers),
        std::make_pair(u("scene"), &ColladaDocument::Parse_Scene)
    };


    

    AssetDesc::AssetDesc()
    {
        _metersPerUnit = 1.f;
        _upAxis = UpAxis::Z;
    }

    AssetDesc::AssetDesc(Formatter& formatter)
        : AssetDesc()
    {
        ON_ELEMENT
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

        ON_ATTRIBUTE
        PARSE_END
    }
    


    void ColladaDocument::Parse(Formatter& formatter)
    {
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
                            LogWarning << "Skipping element " << ele << " at " << formatter.GetLocation();
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
            SamplerParameter(Formatter& formatter, Section sid, Section eleName);
            ~SamplerParameter();
        };
        std::vector<SamplerParameter> _samplerParameters;

        class SurfaceParameter
        {
        public:
            Section _sid;
            Section _type;
            Section _initFrom;

            SurfaceParameter() {}
            SurfaceParameter(Formatter& formatter, Section sid, Section eleName);
        };
        std::vector<SurfaceParameter> _surfaceParameters;

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
        static Enum ParseEnum(const Section& section, const std::pair<Enum, const utf8*> (&table)[Count])
    {
        static_assert(Count > 0, "Enum names table must have at least entry");
        for (unsigned c=0; c<Count; ++c)
            if (!Is(section, table[c].second))
                return table[c].first;

        return table[0].first;  // first one is the default
    }

    template <typename Enum, unsigned Count>
        static Enum ReadCDataAsEnum(Formatter& formatter, const std::pair<Enum, const utf8*> (&table)[Count])
    {
        Formatter::InteriorSection section;
        if (!formatter.TryCharacterData(section)) return table[0].first;
        return ParseEnum(section, table);
    }

    ParameterSet::SamplerParameter::SamplerParameter(Formatter& formatter, Section sid, Section eleName)
        : SamplerParameter()
    {
        _sid = sid;
        _type = eleName;

        ON_ELEMENT
            if (Is(eleName, u("instance_image"))) {
                // collada 1.5 uses "instance_image" (Collada 1.4 equivalent is <source>)
                _image = ExtractSingleAttribute(formatter, u("url"));
            } else if (Is(eleName, u("source"))) {
                // collada 1.4 uses "source" (which cannot have extra data attached)
                formatter.TryCharacterData(_image);
            } else if (Is(eleName, u("wrap_s"))) {
                _addressS = ReadCDataAsEnum(formatter, s_SamplerAddressNames);
            } else if (Is(eleName, u("wrap_t"))) {
                _addressT = ReadCDataAsEnum(formatter, s_SamplerAddressNames);
            } else if (Is(eleName, u("wrap_p"))) {
                _addressQ = ReadCDataAsEnum(formatter, s_SamplerAddressNames);
            } else if (Is(eleName, u("minfilter"))) {
                _minFilter = ReadCDataAsEnum(formatter, s_SamplerFilterNames);
            } else if (Is(eleName, u("magfilter"))) {
                _maxFilter = ReadCDataAsEnum(formatter, s_SamplerFilterNames);
            } else if (Is(eleName, u("mipfilter"))) {
                _mipFilter = ReadCDataAsEnum(formatter, s_SamplerFilterNames);
            } else if (Is(eleName, u("border_color"))) {
                _borderColor = ReadCDataAsList(formatter, _borderColor);
            } else if (Is(eleName, u("mip_max_level"))) {
                _maxMipLevel = ReadCDataAsValue(formatter, _maxMipLevel);
            } else if (Is(eleName, u("mip_min_level"))) {
                _minMipLevel = ReadCDataAsValue(formatter, _minMipLevel);
            } else if (Is(eleName, u("mip_bias"))) {
                _mipMapBias = ReadCDataAsValue(formatter, _mipMapBias);
            } else if (Is(eleName, u("max_anisotropy"))) {
                _maxAnisotrophy = ReadCDataAsValue(formatter, _maxAnisotrophy);
            } else if (Is(eleName, u("extra"))) {
                _extra = SubDoc(formatter);
            } else {
                LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
            LogWarning << "<sampler> elements should not have any attributes. At " << formatter.GetLocation();
        PARSE_END
    }

    ParameterSet::SamplerParameter::~SamplerParameter() {}

    ParameterSet::SurfaceParameter::SurfaceParameter(Formatter& formatter, Section sid, Section eleName)
    {
        _sid = sid;
        _type = eleName;

            // <surface> is an important Collada 1.4.1 element. But it's depricated in Collada 1.5, 
            // and merged into other functionality.
            // We only want to support the init_from sub-element
            // There are other subelements for specifying format, mipmap generation flags,
            // other surfaces (etc). But it's this is not the best place for this information
            // for us.

        ON_ELEMENT
            if (Is(eleName, u("init_from"))) {
                SkipAllAttributes(formatter);
                formatter.TryCharacterData(_initFrom);
            } else {
                LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
            // only "type" is value
        PARSE_END
    }

    void ParameterSet::ParseParam(Formatter& formatter)
    {
        Formatter::InteriorSection sid;

        ON_ELEMENT
            if (Is(eleName, u("annotate")) || Is(eleName, u("semantic")) || Is(eleName, u("modifier"))) {
                LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                formatter.SkipElement();
            } else {
                if (BeginsWith(eleName, u("sampler"))) {
                    _samplerParameters.push_back(SamplerParameter(formatter, sid, eleName));
                } else if (Is(eleName, u("surface"))) {

                        // "surface" is depreciated in collada 1.5. But it's a very import
                        // param in Collada 1.4.1, because it's a critical link between a
                        // "sampler" and a "image"
                    _surfaceParameters.push_back(SurfaceParameter(formatter, sid, eleName));

                } else if (Is(eleName, u("array")) || Is(eleName, u("usertype")) || Is(eleName, u("string")) || Is(eleName, u("enum"))) {
                    LogWarning << "<array>, <usertype>, <string> and <enum> params not supported (depreciated in Collada 1.5). At: " << formatter.GetLocation();
                    formatter.SkipElement();
                } else {
                        // this is a basic parameter, typically a scalar, vector or matrix
                        // we don't need to parse it fully now; just get the location of the 
                        // data and store it as a new parameter
                    Formatter::InteriorSection cdata;
                    if (formatter.TryCharacterData(cdata)) {
                        _parameters.push_back(BasicParameter{sid, eleName, cdata});
                    } else {
                        LogWarning << "Expecting element with parameter data at: " << formatter.GetLocation();
                        formatter.SkipElement();
                    }
                }
            }

        ON_ATTRIBUTE
            if (Is(name, u("sid"))) sid = value;
                continue;
        PARSE_END
    }

    ParameterSet::ParameterSet() {}
    ParameterSet::~ParameterSet() {}
    ParameterSet::ParameterSet(ParameterSet&& moveFrom)
    :   _parameters(std::move(moveFrom._parameters))
    ,   _samplerParameters(std::move(moveFrom._samplerParameters))
    ,   _surfaceParameters(std::move(moveFrom._surfaceParameters))
    {}

    ParameterSet& ParameterSet::operator=(ParameterSet&& moveFrom)
    {
        _parameters = std::move(moveFrom._parameters);
        _samplerParameters = std::move(moveFrom._samplerParameters);
        _surfaceParameters = std::move(moveFrom._surfaceParameters);
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
            std::vector<std::pair<Section, TechniqueValue>> _values;

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
        ON_ELEMENT
            if (Is(eleName, u("newparam"))) {
                _params.ParseParam(formatter);
            } else if (Is(eleName, u("extra"))) {
                _extra = SubDoc(formatter);
            } else if (Is(eleName, u("technique"))) {
                ParseTechnique(formatter);
            } else {
                // asset
                LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
        PARSE_END
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
        ON_ELEMENT
            // Note that we skip a lot of the content in techniques
            // importantly, we skip "pass" elements. "pass" is too tightly
            // bound to the structure of Collada FX. It makes it difficult
            // to extract the particular properties we want, and transform
            // them into something practical.

            if (Is(eleName, u("extra"))) {
                _extra = SubDoc(formatter);
            } else if (Is(eleName, u("asset")) || Is(eleName, u("annotate")) || Is(eleName, u("pass"))) {
                LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                formatter.SkipElement();
            } else {
                // Any other elements are seen as a shader definition
                // There should be exactly 1.
                _shaderName = String(eleName._start, eleName._end);
                ParseShaderType(formatter);
            }

        ON_ATTRIBUTE
        PARSE_END
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
        static decltype(Section::_start) ParseXMLList(Type dest[], unsigned destCount, Section section)
    {
        assert(destCount > 0);

        // in xml, lists are deliminated by white space
        unsigned elementCount = 0;
        auto* eleStart = section._start;
        for (;;) {
            while (eleStart < section._end && IsWhitespace(*eleStart)) ++eleStart;

            auto* eleEnd = FastParseElement(dest[elementCount], eleStart, section._end);

            if (eleStart == eleEnd) return eleEnd;
            ++elementCount;
            if (elementCount >= destCount) return eleEnd;
            eleStart = eleEnd;
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
        
        if (Is(eleName, u("float"))) {

                // skip all attributes (sometimes get "sid" tags)
            SkipAllAttributes(formatter);

            _value[0] = ReadCDataAsValue(formatter, _value[0]);
            _type = Type::Float;

        } else if (Is(eleName, u("color"))) {

                // skip all attributes (sometimes get "sid" tags)
            SkipAllAttributes(formatter);

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

        ON_ELEMENT
            _values.push_back(
                std::make_pair(eleName, TechniqueValue(formatter)));

        ON_ATTRIBUTE
        PARSE_END
    }

    Effect::Effect(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("newparam"))) {
                _params.ParseParam(formatter);
            } else if (BeginsWith(eleName, u("profile_"))) {
                _profiles.push_back(Profile(formatter, String(eleName._start+8, eleName._end)));
            } else if (Is(eleName, u("extra"))) {
                _extra = SubDoc(formatter);
            } else {
                // asset, annotate
                LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
            if (Is(name, u("name"))) _name = value;
            else if (Is(name, u("id"))) _id = value;

        PARSE_END
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

    void ColladaDocument::Parse_LibraryEffects(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("effect"))) {
                _effects.push_back(Effect(formatter));
            } else {
                    // "annotate", "asset" and "extra" are also valid
                LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
        PARSE_END
    }

    namespace DataFlow
    {
        /// <summary>Data type for a collada array</summary>
        /// Collada only supports a limited number of different types within
        /// "source" arrays. These store the most of the "big" information within
        /// Collada files (like vertex data, animation curves, etc).
        enum class ArrayType
        {
            Unspecified, Int, Float, Name, Bool, IdRef, SidRef
        };

        std::pair<ArrayType, const utf8*> s_ArrayTypeNames[] = 
        {
            std::make_pair(ArrayType::Unspecified, u("")),
            std::make_pair(ArrayType::Int, u("int")),
            std::make_pair(ArrayType::Float, u("float")),
            std::make_pair(ArrayType::Name, u("Name")),
            std::make_pair(ArrayType::Bool, u("bool")),
            std::make_pair(ArrayType::IdRef, u("IDREF")),
            std::make_pair(ArrayType::SidRef, u("SIDREF"))
        };

        class Source
        {
        public:
            Section _id;
            Section _arrayId;
            Section _arrayData;
            ArrayType _type;
            unsigned _arrayCount;

            Source() : _type(ArrayType::Unspecified), _arrayCount(0) {}
            Source(Formatter& formatter);
        };

        Source::Source(Formatter& formatter)
            : Source()
        {
            ON_ELEMENT
                if (EndsWith(eleName, u("_array"))) {

                        // This is an array of elements, typically with an id and count
                        // we should have only a single array in each source. But it can
                        // be one of many different types.
                        // Most large data in Collada should appear in blocks like this. 
                        // We don't attempt to parse it here, just record it's location in
                        // the file for later;
                
                    _type = ParseEnum(Section(eleName._start, eleName._end-6), s_ArrayTypeNames);
                
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

                } else if (Is(eleName, u("technique"))) {

                    // this can contain special case non-standard information
                    // But it's not clear what this would be used for
                    LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                    formatter.SkipElement();

                } else if (Is(eleName, u("technique_common"))) {

                    // This specifies the way we interpret the information in the array.
                    // It should occur after the array object. So we can resolve the reference
                    // in the accessor -- but only assuming that the reference in the accessor
                    // refers to an array in this document. That would be logical and typical.
                    // But the standard suggests the accessor can actually refer to a data array
                    // anywhere (including in other files)
                    // Anyway, it looks like we should have only a single accessor per technique
                    LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                    formatter.SkipElement();

                } else {
                        // "asset" also valid
                    LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                    formatter.SkipElement();
                }

            ON_ATTRIBUTE
                if (Is(name, u("id"))) {
                    _id = value;
                } // "name" is also valid
            PARSE_END
        }

        class Accessor
        {
        public:
            Section _source;
            unsigned _count;
            unsigned _stride;
            unsigned _offset;

            class Param
            {
            public:
                Section _name;
                ArrayType _type;
                unsigned _offset;
                Section _semantic;
                Param() : _offset(~unsigned(0)), _type(ArrayType::Float) {}
            };

            Param _params[4];
            std::vector<Param> _paramsOverflow;
            unsigned _paramCount;

            Accessor();
            Accessor(Formatter& formatter);
            Accessor(Accessor&& moveFrom) never_throws;
            Accessor& operator=(Accessor&&moveFrom) never_throws;
        };


        Accessor::Accessor()
        : _count(0), _stride(0), _offset(0), _paramCount(0)
        {}

        Accessor::Accessor(Formatter& formatter)
        : Accessor()
        {
            unsigned workingParamOffset = 0;

            ON_ELEMENT
                if (Is(eleName, u("param"))) {
                        // <param> should have only attributes.
                    Param newParam;
                    newParam._offset = workingParamOffset++;

                    Formatter::InteriorSection name, value;
                    while (formatter.TryAttribute(name, value)) {
                        if (Is(name, u("name"))) {
                            newParam._name = value;
                        } else if (Is(name, u("type"))) {
                            newParam._type = ParseEnum(value, s_ArrayTypeNames);
                        } else if (Is(name, u("semantic"))) {
                            newParam._semantic = value;
                        }
                    }

                    if (newParam._name._start > newParam._name._end) {
                        if (_paramCount < dimof(_params)) {
                            _params[_paramCount] = newParam;
                        } else
                            _paramsOverflow.push_back(newParam);
                        ++_paramCount;
                    }

                } else {
                    LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                    formatter.SkipElement();
                }

            ON_ATTRIBUTE
                if (Is(name, u("count"))) {
                    _count = Parse(value, _count);
                } else if (Is(name, u("stride"))) {
                    _stride = Parse(value, _stride);
                } else if (Is(name, u("source"))) {
                    _source = value;
                } else if (Is(name, u("offset"))) {
                    _offset = Parse(value, _offset);
                }

            PARSE_END
        }

        Accessor::Accessor(Accessor&& moveFrom)
        : _paramsOverflow(std::move(_paramsOverflow))
        {
            _source = moveFrom._source;
            _count = moveFrom._count;
            _stride = moveFrom._stride;
            std::copy(moveFrom._params, &moveFrom._params[dimof(moveFrom._params)], _params);
            _paramCount = moveFrom._paramCount;
        }

        Accessor& Accessor::operator=(Accessor&& moveFrom)
        {
            _source = moveFrom._source;
            _count = moveFrom._count;
            _stride = moveFrom._stride;
            std::copy(moveFrom._params, &moveFrom._params[dimof(moveFrom._params)], _params);
            _paramsOverflow = std::move(_paramsOverflow);
            _paramCount = moveFrom._paramCount;
            return *this;
        }


        class Input
        {
        public:
            unsigned _indexInPrimitive; // this is the index into the <p> or <v> in the parent
            Section _semantic;
            Section _source;            // urifragment_type
            unsigned _semanticIndex;

            Input();
            Input(Formatter& formatter);
        };

        Input::Input() : _indexInPrimitive(0), _semanticIndex(0) {}

        Input::Input(Formatter& formatter)
        {
                // inputs should have only attributes
            Formatter::InteriorSection name, value;
            while (formatter.TryAttribute(name, value)) {
                if (Is(name, u("offset"))) {
                    _indexInPrimitive = Parse(value, _indexInPrimitive);
                } else if (Is(name, u("semantic"))) {
                    _semantic = value;
                } else if (Is(name, u("source"))) {
                    _source = value;
                } else if (Is(name, u("set"))) {
                    _semanticIndex = Parse(value, _semanticIndex);
                }
            }
        }

        class InputUnshared
        {
        public:
            Section _semantic;
            Section _source;        // urifragment_type

            InputUnshared();
            InputUnshared(Formatter& formatter);
        };

        InputUnshared::InputUnshared(Formatter& formatter)
        {
                // inputs should have only attributes
            Formatter::InteriorSection name, value;
            while (formatter.TryAttribute(name, value)) {
                if (Is(name, u("semantic"))) {
                    _semantic = value;
                } else if (Is(name, u("source"))) {
                    _source = value;
                }
            }
        }

    }

    class GeometryPrimitives
    {
    public:
        Section _type;

        DataFlow::Input _inputs[6];
        std::vector<DataFlow::Input> _inputsOverflow;
        unsigned _inputCount;

            // in most cases, there is only one "_primitiveData" element
            // but for trianglestrip, there may be multiple
        Section _primitiveData[1];
        std::vector<Section> _primitiveDataOverflow;
        unsigned _primitiveDataCount;

        Section _vcount;

        GeometryPrimitives(Formatter& formatter, Section type);

        GeometryPrimitives();
        GeometryPrimitives(GeometryPrimitives&& moveFrom) never_throws;
        GeometryPrimitives& operator=(GeometryPrimitives&& moveFrom) never_throws;
    };

    GeometryPrimitives::GeometryPrimitives(Formatter& formatter, Section type)
        : GeometryPrimitives()
    {
        _type = type;

        ON_ELEMENT
            if (Is(eleName, u("input"))) {

                if (_inputCount < dimof(_inputs)) {
                    _inputs[_inputCount] = DataFlow::Input(formatter);
                } else {
                    _inputsOverflow.push_back(DataFlow::Input(formatter));
                }
                ++_inputCount;

            } else if (Is(eleName, u("p"))) {

                    // a p element should have only character data, and nothing else
                    // the meaning of this type is defined by the type of geometry
                    // primitive element, and the <input> sub elements
                Formatter::InteriorSection cdata;
                if (formatter.TryCharacterData(cdata)) {
                    if (_primitiveDataCount < dimof(_inputs)) {
                        _primitiveData[_primitiveDataCount] = cdata;
                    } else {
                        _primitiveDataOverflow.push_back(cdata);
                    }
                    ++_primitiveDataCount;
                }

            } else if (Is(eleName, u("vcount"))) {

                Formatter::InteriorSection cdata;
                if (formatter.TryCharacterData(cdata)) {
                    _vcount = cdata;
                }

            } else {
                    // ph and extra (and maybe others) are possible
                LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
        PARSE_END
    }

    GeometryPrimitives::GeometryPrimitives()
    : _inputCount(0), _primitiveDataCount(0)
    {}

    GeometryPrimitives::GeometryPrimitives(GeometryPrimitives&& moveFrom) never_throws
    : _inputsOverflow(std::move(moveFrom._inputsOverflow))
    , _primitiveDataOverflow(std::move(moveFrom._primitiveDataOverflow))
    {
        _type = moveFrom._type;
        std::copy(moveFrom._inputs, &moveFrom._inputs[dimof(moveFrom._inputs)], _inputs);
        _inputCount = moveFrom._inputCount;
        std::copy(moveFrom._primitiveData, &moveFrom._primitiveData[dimof(moveFrom._primitiveData)], _primitiveData);
        _primitiveDataCount = moveFrom._primitiveDataCount;
        _vcount = moveFrom._vcount;
    }

    GeometryPrimitives& GeometryPrimitives::operator=(GeometryPrimitives&& moveFrom) never_throws
    {
        _type = moveFrom._type;
        std::copy(moveFrom._inputs, &moveFrom._inputs[dimof(moveFrom._inputs)], _inputs);
        _inputsOverflow = std::move(moveFrom._inputsOverflow);
        _inputCount = moveFrom._inputCount;
        std::copy(moveFrom._primitiveData, &moveFrom._primitiveData[dimof(moveFrom._primitiveData)], _primitiveData);
        _primitiveDataOverflow = std::move(moveFrom._primitiveDataOverflow);
        _primitiveDataCount = moveFrom._primitiveDataCount;
        _vcount = moveFrom._vcount;
        return *this;
    }

    class Geometry
    {
    public:
        std::vector<DataFlow::Source> _sources;
        std::vector<DataFlow::InputUnshared> _inputs;
        std::vector<GeometryPrimitives> _geoPrimitives;
        SubDoc _extra;

        Geometry(Formatter& formatter);

        Geometry();
        Geometry(Geometry&& moveFrom) never_throws;
        Geometry& operator=(Geometry&& moveFrom) never_throws;

    protected:
        void ParseMesh(Formatter& formatter);
        void ParseVertices(Formatter& formatter);
    };

    void Geometry::ParseMesh(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("source"))) {
                _sources.push_back(DataFlow::Source(formatter));
            } else if (Is(eleName, u("vertices"))) {
                // must have exactly one <vertices>
                ParseVertices(formatter);
            } else if (Is(eleName, u("extra"))) {
                _extra = SubDoc(formatter);
            } else {
                    // anything else must be geometry list (such as polylist, triangles)
                _geoPrimitives.push_back(GeometryPrimitives(formatter, eleName));
            }

        ON_ATTRIBUTE
        PARSE_END
    }

    void Geometry::ParseVertices(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("input"))) {
                _inputs.push_back(DataFlow::InputUnshared(formatter));
            } else {
                    // extra is possible
                LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
        PARSE_END
    }

    Geometry::Geometry(Formatter& formatter)
    : Geometry()
    {
        ON_ELEMENT
            if (Is(eleName, u("mesh"))) {

                ParseMesh(formatter);

            } else if (Is(eleName, u("convex_mesh")) || Is(eleName, u("spline")) || Is(eleName, u("brep"))) {
                LogWarning << "convex_mesh, spline and brep geometries are not supported. At: " << formatter.GetLocation();
                formatter.SkipElement();
            } else {
                    // "asset" and "extra" are also valid, but it's unlikely that they would be present here
                LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
        PARSE_END
    }

    Geometry::Geometry() {}

    Geometry::Geometry(Geometry&& moveFrom) never_throws
    : _sources(std::move(moveFrom._sources))
    , _inputs(std::move(moveFrom._inputs))
    , _geoPrimitives(std::move(moveFrom._geoPrimitives))
    , _extra(std::move(moveFrom._extra))
    {}

    Geometry& Geometry::operator=(Geometry&& moveFrom) never_throws
    {
        _sources = std::move(moveFrom._sources);
        _inputs = std::move(moveFrom._inputs);
        _geoPrimitives = std::move(moveFrom._geoPrimitives);
        _extra = std::move(moveFrom._extra);
        return *this;
    }

    void ColladaDocument::Parse_LibraryGeometries(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("geometry"))) {
                _geometries.push_back(Geometry(formatter));
            } else {
                    // "asset" and "extra" are also valid, but uninteresting
                LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
            // name and id. Not interesting if we have only a single library
        PARSE_END
    }

    class SkinController
    {
    public:
        Section _baseMesh;
        Section _id;
        Section _name;
        SubDoc _extra;

        Float4x4 _bindShapeMatrix;
        unsigned _weightCount;
        Section _influenceCountPerVertex;   // (this the <vcount> element)
        Section _influences;                // (this is the <v> element)
        std::vector<DataFlow::Input> _influenceInputs;

        std::vector<DataFlow::Source> _sources;
        std::vector<DataFlow::InputUnshared> _jointInputs;

        SkinController(Formatter& formatter, Section id, Section name);
        SkinController(SkinController&& moveFrom) never_throws;
        SkinController& operator=(SkinController&& moveFrom) never_throws;
        SkinController();
        ~SkinController();

    protected:
        void ParseJoints(Formatter& formatter);
        void ParseVertexWeights(Formatter& formatter);
    };

    SkinController::SkinController(Formatter& formatter, Section id, Section name)
        : SkinController()
    {
        _id = id;
        _name = name;

        ON_ELEMENT
            if (Is(eleName, u("bind_shape_matrix"))) {
                SkipAllAttributes(formatter);
                Section cdata;
                if (formatter.TryCharacterData(cdata))
                    ParseXMLList(&_bindShapeMatrix(0,0), 16, cdata);
            } else if (Is(eleName, u("source"))) {
                _sources.push_back(DataFlow::Source(formatter));
            } else if (Is(eleName, u("joints"))) {
                ParseJoints(formatter);
            } else if (Is(eleName, u("vertex_weights"))) {
                ParseVertexWeights(formatter);
            } else if (Is(eleName, u("extra"))) {
                _extra = SubDoc(formatter);
            } else {
                LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
            if (Is(name, u("source"))) _baseMesh = value;
        PARSE_END
    }

    void SkinController::ParseJoints(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("input"))) {
                _jointInputs.push_back(DataFlow::InputUnshared(formatter));
            } else {
                LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                formatter.SkipElement();        // <extra> is possible
            }

        ON_ATTRIBUTE
        PARSE_END
    }

    void SkinController::ParseVertexWeights(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("vcount"))) {
                SkipAllAttributes(formatter);
                formatter.TryCharacterData(_influenceCountPerVertex);
            } else if (Is(eleName, u("v"))) {
                SkipAllAttributes(formatter);
                formatter.TryCharacterData(_influences);
            } else if (Is(eleName, u("input"))) {
                _influenceInputs.push_back(DataFlow::Input(formatter));
            } else {
                LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                formatter.SkipElement();        // <extra> is possible
            }

        ON_ATTRIBUTE
            if (Is(name, u("count")))
                _weightCount = Parse(value, _weightCount);

        PARSE_END
    }

    SkinController::SkinController()
    : _bindShapeMatrix(Identity<Float4x4>())
    , _weightCount(0)
    {}

    SkinController::SkinController(SkinController&& moveFrom)
    : _extra(std::move(moveFrom._extra))
    , _influenceInputs(std::move(moveFrom._influenceInputs))
    , _sources(std::move(moveFrom._sources))
    , _jointInputs(std::move(moveFrom._jointInputs))
    {
        _baseMesh = moveFrom._baseMesh;
        _id = moveFrom._id;
        _name = moveFrom._name;
        _bindShapeMatrix = moveFrom._bindShapeMatrix;
        _weightCount = moveFrom._weightCount;
        _influenceCountPerVertex = moveFrom._influenceCountPerVertex;
        _influences = moveFrom._influences;
    }

    SkinController& SkinController::operator=(SkinController&& moveFrom)
    {
        _baseMesh = moveFrom._baseMesh;
        _id = moveFrom._id;
        _name = moveFrom._name;
        _extra = std::move(moveFrom._extra);
        _bindShapeMatrix = moveFrom._bindShapeMatrix;
        _weightCount = moveFrom._weightCount;
        _influenceCountPerVertex = moveFrom._influenceCountPerVertex;
        _influences = moveFrom._influences;
        _influenceInputs = std::move(moveFrom._influenceInputs);
        _sources = std::move(moveFrom._sources);
        _jointInputs = std::move(moveFrom._jointInputs);
        return *this;
    }

    SkinController::~SkinController() {}

    void ColladaDocument::Parse_LibraryControllers(Formatter& formatter)
    {
        Section controllerName;
        Section controllerId;
        bool inController = false;
            
        for (;;) {
            switch (formatter.PeekNext()) {
            case Formatter::Blob::BeginElement:
                {
                    Section eleName;
                    formatter.TryBeginElement(eleName);

                    bool eatEndElement = true;

                    if (!inController) {
                        if (Is(eleName, u("controller"))) {
                            inController = true;
                            eatEndElement = false;
                        } else {
                            LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                            formatter.SkipElement();    // <asset> and <extra> possible
                        }
                    } else {
                        if (Is(eleName, u("skin"))) {
                            _skinControllers.push_back(SkinController(formatter, controllerId, controllerName));
                        } else if (Is(eleName, u("morph"))) {
                            LogWarning << "<morph> controllers not supported";
                            formatter.SkipElement();
                        } else {
                            LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                            formatter.SkipElement();    // <asset> and <extra> possible
                        }
                    }

                    if (eatEndElement && !formatter.TryEndElement())
                        Throw(FormatException("Expecting end element", formatter.GetLocation()));

                    continue;
                }

            case Formatter::Blob::EndElement:
                if (!inController) break;
                formatter.TryEndElement();
                inController = false;
                continue;

            case Formatter::Blob::AttributeName:
                {
                    Section name, value;
                    formatter.TryAttribute(name, value);

                    if (inController) {
                        if (Is(name, u("id"))) controllerId = value;
                        else if (Is(name, u("name"))) controllerName = value;
                    }

                    continue;
                }
            }

            break;
        }
    }

    class TransformationSet
    {
    public:
        class Operation
        {
        public:
            enum class Type 
            {
                None,
                LookAt, Matrix4x4, Rotate, 
                Scale, Skew, Translate 
            };

            Type _type;
            uint8 _buffer[sizeof(Float4x4)];
            Section _sid;
            unsigned _next;

            Operation();
            Operation(const Operation& copyFrom) never_throws;
            Operation& operator=(const Operation& copyFrom) never_throws;
            ~Operation();
        };

        std::vector<Operation> _operations;

        static bool IsTransform(Section section);
        unsigned ParseTransform(
            Formatter& formatter, Section elementName, 
            unsigned previousSibling = ~unsigned(0));

        TransformationSet();
        TransformationSet(TransformationSet&& moveFrom) never_throws;
        TransformationSet& operator=(TransformationSet&& moveFrom) never_throws;
        ~TransformationSet();
    };

    static float ConvertAngle(float input)
    {
        // In collada, angles are specified in degrees. But that
        // is a little confusion when working with XLE functions,
        // because XLE always uses radians. So we need to do a conversion...
        return input * gPI / 180.f;
    }

    unsigned TransformationSet::ParseTransform(
        Formatter& formatter, Section elementName, 
        unsigned chainStart)
    {
        // Read a transformation element, which exists as part of a chain
        // of transformations;
        // There are a finite number of difference transformation operations
        // supported:
        //      LookAt
        //      Matrix (4x4)
        //      Rotate
        //      Scale
        //      Skew
        //      Translate
        // These parameters are animatable. So this stage, we can't perform
        // optimisations to convert arbitrary scales into uniform scales or
        // matrices into orthonormal transforms

        Operation newOp;

        while (formatter.PeekNext(true) == Formatter::Blob::AttributeName) {
            Section attribName, attribValue;
            formatter.TryAttribute(attribName, attribValue);
            if (Is(attribName, u("sid"))) newOp._sid = attribValue;
        }

        Section cdata;
        if (!formatter.TryCharacterData(cdata))
            return chainStart;

            // Note that there can be problems here if there are comments
            // in the middle of the cdata. We can only properly parse
            // a continuous block of cdata... Anything that is interrupted
            // by comments or <CDATA[ type blocks will not work correctly.

        if (Is(elementName, u("lookat"))) {
            newOp._type = Operation::Type::LookAt;
            auto& dst = *(LookAt*)newOp._buffer;
            cdata._start = ParseXMLList(&dst._origin[0], 3, cdata);
            cdata._start = ParseXMLList(&dst._focusPosition[0], 3, cdata);
            cdata._start = ParseXMLList(&dst._upDirection[0], 3, cdata);
        } else if (Is(elementName, u("matrix"))) {
            newOp._type = Operation::Type::Matrix4x4;
            auto& dst = *(Float4x4*)newOp._buffer;
            cdata._start = ParseXMLList(&dst(0,0), 16, cdata);
        } else if (Is(elementName, u("rotate"))) {
            newOp._type = Operation::Type::Rotate;
            auto& dst = *(ArbitraryRotation*)newOp._buffer;
            cdata._start = ParseXMLList(&dst._axis[0], 3, cdata);
            cdata._start = ParseXMLList(&dst._angle, 1, cdata);
            dst._angle = ConvertAngle(dst._angle);
        } else if (Is(elementName, u("scale"))) {
            newOp._type = Operation::Type::Scale;
            auto& dst = *(ArbitraryScale*)newOp._buffer;
            cdata._start = ParseXMLList(&dst._scale[0], 3, cdata);
        } else if (Is(elementName, u("skew"))) {
            newOp._type = Operation::Type::Skew;
            auto& dst = *(Skew*)newOp._buffer;
            cdata._start = ParseXMLList(&dst._angle, 1, cdata);
            cdata._start = ParseXMLList(&dst._axisA[0], 3, cdata);
            cdata._start = ParseXMLList(&dst._axisB[0], 3, cdata);
            dst._angle = ConvertAngle(dst._angle);
        } else if (Is(elementName, u("translate"))) {
            newOp._type = Operation::Type::Translate;
            auto& dst = *(Float3*)newOp._buffer;
            cdata._start = ParseXMLList(&dst[0], 3, cdata);
        } else {
            return chainStart;
        }

        if (chainStart != ~unsigned(0)) {
            unsigned tail = chainStart;
            for (;;) {
                if (_operations[tail]._next == ~unsigned(0)) break;
                tail = _operations[tail]._next;
            }
            _operations[tail]._next = unsigned(_operations.size());
        } else {
            chainStart = unsigned(_operations.size());
        }

        _operations.push_back(newOp);
        return chainStart;
    }

    bool TransformationSet::IsTransform(Section section)
    {
        return  Is(section, u("lookat")) || Is(section, u("matrix"))
            ||  Is(section, u("rotate")) || Is(section, u("scale"))
            ||  Is(section, u("skew")) || Is(section, u("translate"));
    }

    TransformationSet::TransformationSet() {}
    TransformationSet::TransformationSet(TransformationSet&& moveFrom)
    : _operations(std::move(moveFrom._operations))
    {}

    TransformationSet& TransformationSet::operator=(TransformationSet&& moveFrom)
    {
        _operations = std::move(moveFrom._operations);
        return *this;
    }

    TransformationSet::~TransformationSet() {}

    TransformationSet::Operation::Operation()
    : _type(Type::None), _next(~unsigned(0))
    {
        XlZeroMemory(_buffer);
    }

    TransformationSet::Operation::Operation(const Operation& copyFrom) never_throws
    {
        _type = copyFrom._type;
        _next = copyFrom._next;
        _sid = copyFrom._sid;
        XlCopyMemory(_buffer, copyFrom._buffer, sizeof(_buffer));
    }

    auto TransformationSet::Operation::operator=(const Operation& copyFrom) never_throws -> Operation&
    {
        _type = copyFrom._type;
        _next = copyFrom._next;
        _sid = copyFrom._sid;
        XlCopyMemory(_buffer, copyFrom._buffer, sizeof(_buffer));
        return *this;
    }

    TransformationSet::Operation::~Operation() {}

    class InstanceGeometry
    {
    public:
        class MaterialBinding
        {
        public:
            Section _technique;
            Section _reference;
            Section _bindingSymbol;

            MaterialBinding(Formatter& formatter, Section technique);
        };

        Section _reference;
        std::vector<MaterialBinding> _matBindings;

        InstanceGeometry(Formatter& formatter);
        InstanceGeometry();
        InstanceGeometry(InstanceGeometry&& moveFrom) never_throws;
        InstanceGeometry& operator=(InstanceGeometry&& moveFrom) never_throws;

    protected:
        void ParseBindMaterial(Formatter& formatter);
        void ParseTechnique(Formatter& formatter, Section techniqueName);
    };

    InstanceGeometry::InstanceGeometry(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("bind_material"))) {
                ParseBindMaterial(formatter);
            }

        ON_ATTRIBUTE
            if (Is(name, u("url"))) _reference = value;

        PARSE_END
    }

    void InstanceGeometry::ParseBindMaterial(Formatter& formatter)
    {
        ON_ELEMENT
            if (BeginsWith(eleName, u("technique"))) {
                ParseTechnique(formatter, eleName);
            } else {
                if (Is(eleName, u("param"))) {
                    // support for <param> might be useful for animating material parameters
                    LogWarning << "Element " << eleName << " is not currently supported in <instance_geometry>";
                    formatter.SkipElement();
                } else {
                    // <extra> and <param> also possible
                    LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                    formatter.SkipElement();
                }
            }

        ON_ATTRIBUTE
            // should be no attributes in <bind_material>

        PARSE_END
    }

    void InstanceGeometry::ParseTechnique(Formatter& formatter, Section techniqueName)
    {
        ON_ELEMENT
            if (BeginsWith(eleName, u("instance_material"))) {
                _matBindings.push_back(MaterialBinding(formatter, techniqueName));
            } else {
                LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
            // should be no attributes in <technique...>

        PARSE_END
    }

    InstanceGeometry::MaterialBinding::MaterialBinding(Formatter& formatter, Section technique)
    : _technique(technique)
    {
        ON_ELEMENT
                // <bind> and <bind_vertex_input> are supported... These can override the shader
                // inputs for this material instance. It's an interesting feature, but not supported
                // currently.
                // also, <extra> is possible
            if (Is(eleName, u("bind")) || Is(eleName, u("bind_vertex_input"))) {
                LogWarning << "Element " << eleName << " is not currently supported in <instance_material>";
                formatter.SkipElement();
            } else {
                LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
            if (Is(name, u("target"))) _reference = value;
            else if (Is(name, u("symbol"))) _bindingSymbol = value;

        PARSE_END
    }

    InstanceGeometry::InstanceGeometry() {}
    InstanceGeometry::InstanceGeometry(InstanceGeometry&& moveFrom) never_throws
    : _matBindings(std::move(moveFrom._matBindings))
    {
        _reference = moveFrom._reference;
    }

    InstanceGeometry& InstanceGeometry::operator=(InstanceGeometry&& moveFrom) never_throws
    {
        _reference = moveFrom._reference;
        _matBindings = std::move(moveFrom._matBindings);
        return *this;
    }

    class VisualScene
    {
    public:
        SubDoc _extra;

        VisualScene(Formatter& formatter);
        VisualScene();
        VisualScene(VisualScene&& moveFrom) never_throws;
        VisualScene& operator=(VisualScene&& moveFrom) never_throws;

    protected:
        using IndexIntoNodes = unsigned;
        using TransformationSetIndex = unsigned;
        static const IndexIntoNodes IndexIntoNodes_Invalid = ~IndexIntoNodes(0);
        static const TransformationSetIndex TransformationSetIndex_Invalid = ~TransformationSetIndex(0);

        class Node
        {
        public:
            Section _id;
            Section _sid;
            Section _name;

            IndexIntoNodes _parent;
            IndexIntoNodes _nextSibling;
            IndexIntoNodes _firstChild;

            TransformationSetIndex _transformChain;

            SubDoc _extra;

            Node() 
            : _parent(IndexIntoNodes_Invalid), _nextSibling(IndexIntoNodes_Invalid), _firstChild(IndexIntoNodes_Invalid)
            , _transformChain(TransformationSetIndex_Invalid) {}
            ~Node();

            Node(Node&& moveFrom) never_throws;
            Node& operator=(Node&& moveFrom) never_throws;
        };

        std::vector<Node> _nodes;
        std::vector<std::pair<IndexIntoNodes, InstanceGeometry>> _geoInstances;
        TransformationSet _transformSet;
    };

    VisualScene::VisualScene(Formatter& formatter)
    {
        std::stack<IndexIntoNodes> workingNodes;
        for (;;) {
            switch (formatter.PeekNext()) {
            case Formatter::Blob::BeginElement:
                {
                    Formatter::InteriorSection eleName;
                    formatter.TryBeginElement(eleName);

                    bool eatEndElement = true;
                    if (Is(eleName, u("node"))) {

                            // create a new node, and add it to 
                            // our working tree...

                        Node newNode;
                        auto newNodeIndex = IndexIntoNodes(_nodes.size());
                        if (!workingNodes.empty()) {

                            newNode._parent = workingNodes.top();
                            auto lastChildOfParent = _nodes[workingNodes.top()]._firstChild;
                            if (lastChildOfParent != IndexIntoNodes_Invalid) {

                                for (;;) {
                                    auto& sib = _nodes[lastChildOfParent];
                                    if (sib._nextSibling == IndexIntoNodes_Invalid) {
                                        sib._nextSibling = newNodeIndex;
                                        break;
                                    }
                                    lastChildOfParent = sib._nextSibling;
                                }

                            } else {
                                _nodes[workingNodes.top()]._firstChild = newNodeIndex;
                            }

                        }

                        workingNodes.push(newNodeIndex);
                        _nodes.push_back(newNode);
                        eatEndElement = false;

                    } else if (Is(eleName, u("instance_geometry"))) {

                            // <instance_geometry> should contain a reference to the particular geometry
                            // as well as material binding information.
                            // each <instance_geometry> belongs inside of a 
                        auto node = IndexIntoNodes_Invalid;
                        if (!workingNodes.empty()) node = workingNodes.top();
                        _geoInstances.push_back(std::make_pair(node, InstanceGeometry(formatter)));

                    } else if (TransformationSet::IsTransform(eleName)) {

                        if (!workingNodes.empty()) {
                            auto& node = _nodes[workingNodes.top()];
                            node._transformChain = _transformSet.ParseTransform(
                                formatter, eleName, node._transformChain);
                        } else {
                            formatter.SkipElement();    // wierd -- transform in the root node
                        }

                    } else if (Is(eleName, u("extra"))) {

                        if (!workingNodes.empty()) {
                            auto& node = _nodes[workingNodes.top()];
                            node._extra = SubDoc(formatter);
                        } else {
                            LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                            formatter.SkipElement();
                        }
                    
                    } else {
                        if (workingNodes.empty() && Is(eleName, u("extra"))) {
                            _extra = SubDoc(formatter);
                        } else {
                                // "asset" and "evaluate_scene" are also valid, but uninteresting
                            LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                            formatter.SkipElement();
                        }
                    }

                    if (eatEndElement && !formatter.TryEndElement())
                        Throw(FormatException("Expecting end element", formatter.GetLocation()));

                    continue;
                }

            case Formatter::Blob::EndElement:
                if (workingNodes.empty()) break;
                formatter.TryEndElement();
                workingNodes.pop();
                continue;

            case Formatter::Blob::AttributeName:
                {
                    Formatter::InteriorSection name, value;
                    formatter.TryAttribute(name, value);

                    if (!workingNodes.empty()) {

                            // this is actually an attribute inside of a node item
                        auto& node = _nodes[workingNodes.top()];
                        if (Is(name, u("id"))) node._id = value;
                        else if (Is(name, u("sid"))) node._sid = value;
                        else if (Is(name, u("name"))) node._name = value;

                    } else {
                        // visual_scene can have "id" and "name"
                    }

                    continue;
                }
            }

            break;
        }
    }

    VisualScene::VisualScene() {}
    VisualScene::VisualScene(VisualScene&& moveFrom) never_throws 
    : _extra(std::move(moveFrom._extra))
    , _nodes(std::move(moveFrom._nodes))
    , _geoInstances(std::move(moveFrom._geoInstances))
    , _transformSet(std::move(moveFrom._transformSet))
    {
    }

    VisualScene& VisualScene::operator=(VisualScene&& moveFrom) never_throws 
    {
        _extra = std::move(moveFrom._extra);
        _nodes = std::move(moveFrom._nodes);
        _geoInstances = std::move(moveFrom._geoInstances);
        _transformSet = std::move(moveFrom._transformSet);
        return *this;
    }

    VisualScene::Node::~Node() {}
    VisualScene::Node::Node(Node&& moveFrom) never_throws
    : _id(moveFrom._id)
    , _sid(moveFrom._sid)
    , _name(moveFrom._name)
    , _parent(moveFrom._parent)
    , _nextSibling(moveFrom._nextSibling)
    , _firstChild(moveFrom._firstChild)
    , _transformChain(moveFrom._transformChain)
    , _extra(std::move(moveFrom._extra))
    {}

    auto VisualScene::Node::operator=(Node&& moveFrom) never_throws -> Node&
    {
        _id = moveFrom._id;
        _sid = moveFrom._sid;
        _name = moveFrom._name;
        _parent = moveFrom._parent;
        _nextSibling = moveFrom._nextSibling;
        _firstChild = moveFrom._firstChild;
        _transformChain = moveFrom._transformChain;
        _extra = std::move(moveFrom._extra);
        return *this;
    }

    void ColladaDocument::Parse_LibraryVisualScenes(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("visual_scene"))) {
                _visualScenes.push_back(VisualScene(formatter));
            } else {
                    // "asset" and "extra" are also valid, but uninteresting
                LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
            // name and id. Not interesting if we have only a single library
        PARSE_END
    }

    void ColladaDocument::Parse_Scene(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("instance_physics_scene"))) {
                _physicsScene = ExtractSingleAttribute(formatter, u("url"));
            } else if (Is(eleName, u("instance_visual_scene"))) {
                _visualScene = ExtractSingleAttribute(formatter, u("url"));
            } else if (Is(eleName, u("instance_kinematics_scene"))) {
                _kinematicsScene = ExtractSingleAttribute(formatter, u("url"));
            } else {
                // <extra> also possible
                LogWarning << "Skipping element " << eleName << " at " << formatter.GetLocation();
                formatter.SkipElement();
            }
        ON_ATTRIBUTE
        PARSE_END
    }

    ColladaDocument::ColladaDocument() {}
    ColladaDocument::~ColladaDocument() {}

}

#include "../Utility/Streams/FileUtils.h"
#include "NascentModel.h"

void TestParser()
{
    size_t size;
    // auto block = LoadFileAsMemoryBlock("game/testmodels/nyra/Nyra_pose.dae", &size);
    auto block = LoadFileAsMemoryBlock("Game/chr/nu_f/skin/dragon003.dae", &size);
    XmlInputStreamFormatter<utf8> formatter(MemoryMappedInputStream(block.get(), PtrAdd(block.get(), size)));
    ColladaConversion::ColladaDocument doc;
    doc.Parse(formatter);

    Float4 t2;
    Float4x4 temp;
    int t = 0;
    (void)t;
}


