// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Scaffold.h"
#include "ScaffoldDataFlow.h"
#include "ScaffoldParsingUtil.h"
#include "../Math/Vector.h"
#include "../Math/Transformations.h"
#include "../Utility/UTFUtils.h"
#include "../Utility/Conversion.h"
#include "../Utility/ParameterBox.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/IteratorUtils.h"
#include "../ConsoleRig/Log.h"
#include <queue>
#include <cctype>

namespace ColladaConversion
{
    template <typename Type, int Count>
        cml::vector<Type, cml::fixed<Count>> ReadCDataAsList(
            Formatter& formatter, // Formatter::InteriorSection storedType,
            const cml::vector<Type, cml::fixed<Count>>& def)
    {
        Formatter::InteriorSection cdata;
        if (formatter.TryCharacterData(cdata)) {

            cml::vector<Type, cml::fixed<Count>> result = def;
            ParseXMLList(&result[0], Count, cdata);
            return result;

        } else {
            Log(Warning) << "Expecting vector data at " << formatter.GetLocation() << std::endl;
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
            Log(Warning) << "Expecting scalar data at " << formatter.GetLocation() << std::endl;
            return def;
        }
    }

    static void SkipAllAttributes(Formatter& formatter)
    {
        while (formatter.PeekNext() == Formatter::Blob::AttributeName) {
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

    using RootElementParser = void (DocumentScaffold::*)(XmlInputStreamFormatter<utf8>&);

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
        std::make_pair(u("library_effects"), &DocumentScaffold::Parse_LibraryEffects),
        std::make_pair(u("library_geometries"), &DocumentScaffold::Parse_LibraryGeometries),
        std::make_pair(u("library_visual_scenes"), &DocumentScaffold::Parse_LibraryVisualScenes),
        std::make_pair(u("library_controllers"), &DocumentScaffold::Parse_LibraryControllers),
        std::make_pair(u("library_materials"), &DocumentScaffold::Parse_LibraryMaterials),
        std::make_pair(u("library_images"), &DocumentScaffold::Parse_LibraryImages),
        std::make_pair(u("library_animations"), &DocumentScaffold::Parse_LibraryAnimations),
        std::make_pair(u("scene"), &DocumentScaffold::Parse_Scene)
    };


    static bool TryParseAssetDescElement(AssetDesc&desc, Formatter& formatter, Formatter::InteriorSection eleName)
    {
        if (Is(eleName, u("unit"))) {
            // Utility::StreamDOM<Formatter> doc(formatter);
            // _metersPerUnit = doc(u("meter"), _metersPerUnit);
            auto meter = ExtractSingleAttribute(formatter, u("meter"));
            desc._metersPerUnit = Parse(meter, desc._metersPerUnit);
            return true;
        } else if (Is(eleName, u("up_axis"))) {
            if (formatter.TryCharacterData(eleName)) {
                if ((eleName._end - eleName._start) >= 1) {
                    switch (std::tolower(*eleName._start)) {
                    case 'x': desc._upAxis = AssetDesc::UpAxis::X; break;
                    case 'y': desc._upAxis = AssetDesc::UpAxis::Y; break;
                    case 'z': desc._upAxis = AssetDesc::UpAxis::Z; break;
                    }
                }
            }
            return true;
        }
        return false;
    }

    AssetDesc::AssetDesc()
    {
        _metersPerUnit = 1.f;
        _upAxis = UpAxis::Z;
    }

    AssetDesc::AssetDesc(Formatter& formatter)
        : AssetDesc()
    {
        ON_ELEMENT
            if (!TryParseAssetDescElement(*this, formatter, eleName))
                formatter.SkipElement();
        ON_ATTRIBUTE
        PARSE_END
    }
    


    void DocumentScaffold::Parse(Formatter& formatter)
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

                        if (!found)
                            found = TryParseAssetDescElement(_rootAsset, formatter, ele);

                        if (!found) {
                            Log(Warning) << "Skipping element " << ele << " at " << formatter.GetLocation() << std::endl;
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
        static Enum ReadCDataAsEnum(Formatter& formatter, const std::pair<Enum, const utf8*> (&table)[Count])
    {
        Formatter::InteriorSection section;
        if (!formatter.TryCharacterData(section)) return table[0].first;
        return ParseEnum(section, table);
    }

    ParameterSet::SamplerParameter::SamplerParameter(Formatter& formatter, Section sid, Section inputEleName)
        : SamplerParameter()
    {
        _sid = sid;
        _type = inputEleName;

        ON_ELEMENT
            if (Is(eleName, u("instance_image"))) {
                // collada 1.5 uses "instance_image" (Collada 1.4 equivalent is <source>)
                _instanceImage = ExtractSingleAttribute(formatter, u("url"));
            } else if (Is(eleName, u("source"))) {
                // collada 1.4 uses "source" (which cannot have extra data attached)
                formatter.TryCharacterData(_source);
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
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
            Log(Warning) << "<sampler> elements should not have any attributes. At " << formatter.GetLocation() << std::endl;
        PARSE_END
    }

    ParameterSet::SamplerParameter::~SamplerParameter() {}

    ParameterSet::SurfaceParameter::SurfaceParameter(Formatter& formatter, Section sid, Section inputEleName)
    {
        _sid = sid;
        _type = inputEleName;

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
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
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
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();
            } else {
                if (BeginsWith(eleName, u("sampler"))) {
                    _samplerParameters.emplace_back(SamplerParameter(formatter, sid, eleName));
                } else if (Is(eleName, u("surface"))) {

                        // "surface" is depreciated in collada 1.5. But it's a very import
                        // param in Collada 1.4.1, because it's a critical link between a
                        // "sampler" and a "image"
                    _surfaceParameters.push_back(SurfaceParameter(formatter, sid, eleName));

                } else if (Is(eleName, u("array")) || Is(eleName, u("usertype")) || Is(eleName, u("string")) || Is(eleName, u("enum"))) {
                    Log(Warning) << "<array>, <usertype>, <string> and <enum> params not supported (depreciated in Collada 1.5). At: " << formatter.GetLocation() << std::endl;
                    formatter.SkipElement();
                } else {
                        // this is a basic parameter, typically a scalar, vector or matrix
                        // we don't need to parse it fully now; just get the location of the 
                        // data and store it as a new parameter
                    Formatter::InteriorSection cdata;
                    if (formatter.TryCharacterData(cdata)) {
                        _parameters.push_back(BasicParameter{sid, eleName, cdata});
                    } else {
                        Log(Warning) << "Expecting element with parameter data at: " << formatter.GetLocation() << std::endl;
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
    ParameterSet::ParameterSet(ParameterSet&& moveFrom) never_throws
    :   _parameters(std::move(moveFrom._parameters))
    ,   _samplerParameters(std::move(moveFrom._samplerParameters))
    ,   _surfaceParameters(std::move(moveFrom._surfaceParameters))
    {}

    ParameterSet& ParameterSet::operator=(ParameterSet&& moveFrom) never_throws
    {
        _parameters = std::move(moveFrom._parameters);
        _samplerParameters = std::move(moveFrom._samplerParameters);
        _surfaceParameters = std::move(moveFrom._surfaceParameters);
        return *this;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

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
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
        PARSE_END
    }

    Effect::Profile::Profile(Profile&& moveFrom) never_throws
    : _params(std::move(moveFrom._params))
    , _profileType(std::move(moveFrom._profileType))
    , _shaderName(std::move(moveFrom._shaderName))
    , _values(std::move(moveFrom._values))
    , _extra(std::move(moveFrom._extra))
    , _techniqueExtra(std::move(moveFrom._techniqueExtra))
    , _techniqueSid(moveFrom._techniqueSid)
    {
    }

    auto Effect::Profile::operator=(Profile&& moveFrom) never_throws -> Profile&
    {
        _params = std::move(moveFrom._params);
        _profileType = std::move(moveFrom._profileType);
        _shaderName = std::move(moveFrom._shaderName);
        _values = std::move(moveFrom._values);
        _extra = std::move(moveFrom._extra);
        _techniqueExtra = std::move(moveFrom._techniqueExtra);
        _techniqueSid = moveFrom._techniqueSid;
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
                _techniqueExtra = SubDoc(formatter);
            } else if (Is(eleName, u("asset")) || Is(eleName, u("annotate")) || Is(eleName, u("pass"))) {
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();
            } else if (Is(eleName, u("newparam"))) {
                    // the Collada 1.5 docs don't mention <newparam> inside of <technique>. But some of the
                    // files in the Collada test kit have it. We must skip it, so that it's not interpreted
                    // as a shader type
                Log(Warning) << "<newparam> is not supported within <technique>. Ignoring." << std::endl;
                formatter.SkipElement();
            } else {
                // Any other elements are seen as a shader definition
                // There should be exactly 1.
                _shaderName = String(eleName._start, eleName._end);
                ParseShaderType(formatter);
            }

        ON_ATTRIBUTE
            if (Is(name, u("sid"))) _techniqueSid = value;

        PARSE_END
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
                Log(Warning) << "no data in color value at " << formatter.GetLocation() << std::endl;

        } else if (Is(eleName, u("texture"))) {

                // <texture> can contain <extra> -- so we need
                // to do a full parse
            for (;;) {
                auto next = formatter.PeekNext();
                switch (next) {
                case Formatter::Blob::BeginElement:
                    {
                        Formatter::InteriorSection name;
                        formatter.TryBeginElement(name);
                        Log(Warning) << "Skipping (" << name << ") in technique <texture> at " << formatter.GetLocation() << std::endl;
                        formatter.SkipElement();
                        formatter.TryEndElement();
                        continue;
                    }

                case Formatter::Blob::AttributeName:
                    {
                        Formatter::InteriorSection name, value; 
                        formatter.TryAttribute(name, value);
                        if (Is(name, u("texture"))) _reference = value;
                        else if (Is(name, u("texcoord"))) _texCoord = value;
                        else Log(Warning) << "Unknown attribute for texture (" << name << ") at " << formatter.GetLocation() << std::endl;
                        continue;
                    }

                default: break;
                }
                break;
            }
            _type = Type::Texture;

        } else if (Is(eleName, u("param"))) {

            Formatter::InteriorSection name, value; 
            if (!formatter.TryAttribute(name, value) || !Is(name, u("ref"))) {
                Log(Warning) << "Expecting ref attribute in param technique value at " << formatter.GetLocation() << std::endl;
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

    const Effect::Profile* Effect::FindProfile(const utf8 name[]) const
    {
        for (const auto& p:_profiles)
            if (!XlCompareString(p._profileType.c_str(), name))
                return &p;
        return nullptr;
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
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
            if (Is(name, u("name"))) _name = value;
            else if (Is(name, u("id"))) _id = value;

        PARSE_END
    }

    Effect::Effect(Effect&& moveFrom) never_throws
    : _name(moveFrom._name)
    , _id(moveFrom._id)
    , _params(std::move(moveFrom._params))
    , _profiles(std::move(moveFrom._profiles))
    , _extra(std::move(moveFrom._extra))
    {}

    Effect& Effect::operator=(Effect&& moveFrom) never_throws
    {
        _name = moveFrom._name;
        _id = moveFrom._id;
        _params = std::move(moveFrom._params);
        _profiles = std::move(moveFrom._profiles);
        _extra = std::move(moveFrom._extra);
        return *this;
    }

    void DocumentScaffold::Parse_LibraryEffects(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("effect"))) {
                _effects.push_back(Effect(formatter));
            } else {
                    // "annotate", "asset" and "extra" are also valid
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
        PARSE_END
    }

    void DocumentScaffold::Parse_LibraryAnimations(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("animation"))) {
                _animations.push_back(Animation(formatter, *this));
            } else {
                    // "asset" and "extra" are also valid
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
        PARSE_END
    }

    namespace DataFlow
    {
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


        

        Accessor::Accessor()
        : _count(0), _stride(0), _offset(0), _paramCount(0)
        {}

        Accessor::Accessor(Formatter& formatter)
        : Accessor()
        {
            unsigned workingParamOffset = 0;

            Param unnamedParam;
            unsigned unnamedParamCount = 0;
            bool foundStrideAttribute = false;

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
                            newParam._type = value;
                        } else if (Is(name, u("semantic"))) {
                            newParam._semantic = value;
                        }
                    }

                        // Do not record <param>s without names -- those are only used to skip over a slots
                        // (except when we find an accessor with just a single unnamed param... Then we still need it)
                    if (!newParam._name.IsEmpty()) {
                        if (_paramCount < dimof(_params)) {
                            _params[_paramCount] = newParam;
                        } else
                            _paramsOverflow.push_back(newParam);
                        ++_paramCount;
                    } else {
                        unnamedParam = newParam;
                        ++unnamedParamCount;
                    }

                } else {
                    Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                    formatter.SkipElement();
                }

            ON_ATTRIBUTE
                if (Is(name, u("count"))) {
                    _count = Parse(value, _count);
                } else if (Is(name, u("stride"))) {
                    _stride = Parse(value, _stride);
                    foundStrideAttribute = true;
                } else if (Is(name, u("source"))) {
                    _source = value;
                } else if (Is(name, u("offset"))) {
                    _offset = Parse(value, _offset);
                }

            PARSE_END

            if (_paramCount==0) {
                if (unnamedParamCount > 1)
                    Log(Warning) << "Found multiple unnamed parameters in an accessor with no nammed params. This is invalid because there's no way to distinguish between the parameters. Using only the last one." << std::endl;
                _params[_paramCount++] = unnamedParam;
            }

            // if the "stride" attribute is omitted, we must imply it by the number of parameters.
            if (!foundStrideAttribute)
                _stride = (unsigned)_paramCount;
        }

        Accessor::Accessor(Accessor&& moveFrom) never_throws
        : _paramsOverflow(std::move(_paramsOverflow))
        {
            _source = moveFrom._source;
            _count = moveFrom._count;
            _stride = moveFrom._stride;
            std::copy(moveFrom._params, &moveFrom._params[dimof(moveFrom._params)], _params);
            _paramCount = moveFrom._paramCount;
        }

        Accessor& Accessor::operator=(Accessor&& moveFrom) never_throws
        {
            _source = moveFrom._source;
            _count = moveFrom._count;
            _stride = moveFrom._stride;
            std::copy(moveFrom._params, &moveFrom._params[dimof(moveFrom._params)], _params);
            _paramsOverflow = std::move(_paramsOverflow);
            _paramCount = moveFrom._paramCount;
            return *this;
        }

        Accessor::~Accessor() {}

        

        const Accessor* Source::FindAccessorForTechnique(const utf8 techniqueProfile[]) const
        {
            for (unsigned c=0; c<std::min(unsigned(dimof(_accessors)), _accessorsCount); ++c)
                if (Is(_accessors[c].second, techniqueProfile))
                    return &_accessors[c].first;

            for (auto i=_accessorsOverflow.cbegin(); i!=_accessorsOverflow.cend(); ++i)
                if (Is(i->second, techniqueProfile))
                    return &i->first;

            return nullptr;
        }

        Source::Source(Formatter& formatter)
            : Source()
        {
            _location = formatter.GetLocation();

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
                        while (formatter.PeekNext() == Formatter::Blob::AttributeName) {
                            Section name, value;
                            formatter.TryAttribute(name, value);

                            if (Is(name, u("count"))) {
                                _arrayCount = Parse(value, 0u);
                            } else if (Is(name, u("id"))) {
                                _arrayId = value;
                            } // "name also valid
                        }
                    }

                    // note --  when we call TryCharacterData here, the formatter
                    //          will scan through the file to find the end of the 
                    //          character data section. In most practical collada
                    //          files, most of the file should be dedicated to character
                    //          data in <source> elements. That mean that this scanning
                    //          process can take some time.
                    //          At some later point, we will probably need to parse
                    //          the contents of the character data. That will require
                    //          scanning through it again (doing string to float conversion
                    //          and other conversion). That means we parse over that 
                    //          data twice: one just to find the end of it, and one to
                    //          build an actual list of data.
                    //          We could potentially get a speed-up by parsing the list
                    //          here, also. That will mean just a single pass over that data.
                    //          But the disadvantage is will would need to store the parsed
                    //          data somewhere. With the 2 pass model, we are more likely
                    //          to parse the data, use it, and then release the memory in
                    //          quick succession.
                    Section cdata;
                    if (formatter.TryCharacterData(cdata)) {
                        _arrayData = cdata;
                    } else {
                        Log(Warning) << "Data source contains no data! At: " << formatter.GetLocation() << std::endl;
                    }

                } else if (BeginsWith(eleName, u("technique"))) {

                    ParseTechnique(formatter, eleName);

                } else {
                        // "asset" also valid
                    Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                    formatter.SkipElement();
                }

            ON_ATTRIBUTE
                if (Is(name, u("id"))) {
                    _id = value;
                } // "name" is also valid
            PARSE_END
        }

        void Source::ParseTechnique(Formatter& formatter, Section techniqueProfile)
        {
            ON_ELEMENT
                if (Is(eleName, u("accessor"))) {
                    if (_accessorsCount < dimof(_accessors)) {
                        _accessors[_accessorsCount] = std::make_pair(Accessor(formatter), techniqueProfile);
                    } else {
                        _accessorsOverflow.push_back(std::make_pair(Accessor(formatter), techniqueProfile));
                    }
                    ++_accessorsCount;
                } else {
                        // documentation doesn't clearly specify what is valid here
                    Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                    formatter.SkipElement();
                }

            ON_ATTRIBUTE
                if (Is(name, u("profile"))) techniqueProfile = value;

            PARSE_END
        }

        Source::Source(Source&& moveFrom) never_throws
        : _accessorsOverflow(std::move(moveFrom._accessorsOverflow))
        {
            _id = moveFrom._id;
            _arrayId = moveFrom._arrayId;
            _arrayData = moveFrom._arrayData;
            _type = moveFrom._type;
            _arrayCount = moveFrom._arrayCount;
            _accessorsCount = moveFrom._accessorsCount;
            for (unsigned c=0; c<dimof(_accessors); ++c)
                _accessors[c] = std::move(moveFrom._accessors[c]);
            _location = moveFrom._location;
        }

        Source& Source::operator=(Source&& moveFrom) never_throws
        {
            _id = moveFrom._id;
            _arrayId = moveFrom._arrayId;
            _arrayData = moveFrom._arrayData;
            _type = moveFrom._type;
            _arrayCount = moveFrom._arrayCount;

            _accessorsCount = moveFrom._accessorsCount;
            for (unsigned c=0; c<dimof(_accessors); ++c)
                _accessors[c] = std::move(moveFrom._accessors[c]);

            _accessorsOverflow = std::move(moveFrom._accessorsOverflow);
            _location = moveFrom._location;
            return *this;
        }

        Source::~Source() {}



        

        Input::Input() : _indexInPrimitive(0), _semanticIndex(0) {}

        Input::Input(Formatter& formatter)
            : Input()
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

    

    GeometryPrimitives::GeometryPrimitives(Formatter& formatter, Section type)
        : GeometryPrimitives()
    {
        _type = type;
        _location = formatter.GetLocation();

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
                    if (_primitiveDataCount < dimof(_primitiveData)) {
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
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
            if (Is(name, u("count"))) _primitiveCount = Parse(value, _primitiveCount);
            else if (Is(name, u("material"))) _materialBinding = value;

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
        _primitiveCount = moveFrom._primitiveCount;
        _location = moveFrom._location;
        _materialBinding = moveFrom._materialBinding;
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
        _primitiveCount = moveFrom._primitiveCount;
        _location = moveFrom._location;
        _materialBinding = moveFrom._materialBinding;
        return *this;
    }

    

    void MeshGeometry::ParseMesh(Formatter& formatter, DocumentScaffold& pub)
    {
        ON_ELEMENT
            if (Is(eleName, u("source"))) {
                // _sources.push_back(DataFlow::Source(formatter));
                pub.Add(DataFlow::Source(formatter));
            } else if (Is(eleName, u("vertices"))) {
                // must have exactly one <vertices>. But we only refernce it by
                // it's global "id" value. That means it's position within the element
                // hierarchy is irrelevant. And it's only by convention that the <vertices>
                // element appears within the <mesh> element that is it associated with.
                pub.Add(InputsCollection(formatter));
            } else if (Is(eleName, u("extra"))) {
                _extra = SubDoc(formatter);
            } else {
                    // anything else must be geometry list (such as polylist, triangles)
                _geoPrimitives.push_back(GeometryPrimitives(formatter, eleName));
            }

        ON_ATTRIBUTE
        PARSE_END
    }

    MeshGeometry::MeshGeometry(Formatter& formatter, DocumentScaffold& pub)
    : MeshGeometry()
    {
        ON_ELEMENT
            if (Is(eleName, u("mesh"))) {

                ParseMesh(formatter, pub);

            } else if (Is(eleName, u("convex_mesh")) || Is(eleName, u("spline")) || Is(eleName, u("brep"))) {
                Log(Warning) << "convex_mesh, spline and brep geometries are not supported. At: " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();
            } else {
                    // "asset" and "extra" are also valid, but it's unlikely that they would be present here
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
            if (Is(name, u("id"))) _id = value;
            else if (Is(name, u("name"))) _name = value;

        PARSE_END
    }

    MeshGeometry::MeshGeometry() {}

    MeshGeometry::MeshGeometry(MeshGeometry&& moveFrom) never_throws
    : _geoPrimitives(std::move(moveFrom._geoPrimitives))
    , _extra(std::move(moveFrom._extra))
    , _id(moveFrom._id)
    , _name(moveFrom._name)
    {}

    MeshGeometry& MeshGeometry::operator=(MeshGeometry&& moveFrom) never_throws
    {
        _geoPrimitives = std::move(moveFrom._geoPrimitives);
        _extra = std::move(moveFrom._extra);
        _id = moveFrom._id;
        _name =moveFrom._name;
        return *this;
    }

    InputsCollection::InputsCollection(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("input"))) {
                _vertexInputs.push_back(DataFlow::InputUnshared(formatter));
            } else {
                    // extra is possible
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
            if (Is(name, u("id"))) _id = value;

        PARSE_END
    }

    InputsCollection::InputsCollection()
    {}

    InputsCollection::InputsCollection(InputsCollection&& moveFrom) never_throws
    : _vertexInputs(std::move(moveFrom._vertexInputs))
    , _id(moveFrom._id)
    {}

    InputsCollection& InputsCollection::operator=(InputsCollection&& moveFrom) never_throws
    {
        _vertexInputs = std::move(moveFrom._vertexInputs);
        _id = moveFrom._id;
        return *this;
    }

    auto InputsCollection::FindInputBySemantic(const utf8 semantic[]) const -> const DataFlow::InputUnshared*
    {
        for (const auto& i:_vertexInputs)
            if (Is(i._semantic, semantic))
                return &i;
        return nullptr;
    }

    void DocumentScaffold::Parse_LibraryGeometries(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("geometry"))) {
                _geometries.push_back(MeshGeometry(formatter, *this));
            } else {
                    // "asset" and "extra" are also valid, but uninteresting
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
            // name and id. Not interesting if we have only a single library
        PARSE_END
    }

    

    SkinController::SkinController(Formatter& formatter, Section id, Section inputName, DocumentScaffold& pub)
        : SkinController()
    {
        _id = id;
        _name = inputName;
        _location = formatter.GetLocation();

        ON_ELEMENT
            if (Is(eleName, u("bind_shape_matrix"))) {
                SkipAllAttributes(formatter);
                formatter.TryCharacterData(_bindShapeMatrix);
            } else if (Is(eleName, u("source"))) {
                pub.Add(DataFlow::Source(formatter));
            } else if (Is(eleName, u("joints"))) {
                _jointInputs = InputsCollection(formatter);     // should be exactly one in each skin controller
            } else if (Is(eleName, u("vertex_weights"))) {
                ParseVertexWeights(formatter);
            } else if (Is(eleName, u("extra"))) {
                _extra = SubDoc(formatter);
            } else {
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
            if (Is(name, u("source"))) _baseMesh = value;
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
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();        // <extra> is possible
            }

        ON_ATTRIBUTE
            if (Is(name, u("count")))
                _verticesWithWeightsCount = Parse(value, _verticesWithWeightsCount);

        PARSE_END
    }

    const DataFlow::Input* SkinController::GetInfluenceInputBySemantic(const utf8 semantic[]) const
    {
        for (const auto& i:_influenceInputs)
            if (Is(i._semantic, semantic))
                return &i;
        return nullptr;
    }

    SkinController::SkinController()
    : _verticesWithWeightsCount(0)
    {}

    SkinController::SkinController(SkinController&& moveFrom) never_throws
    : _extra(std::move(moveFrom._extra))
    , _influenceInputs(std::move(moveFrom._influenceInputs))
    , _jointInputs(std::move(moveFrom._jointInputs))
    {
        _baseMesh = moveFrom._baseMesh;
        _id = moveFrom._id;
        _name = moveFrom._name;
        _bindShapeMatrix = moveFrom._bindShapeMatrix;
        _verticesWithWeightsCount = moveFrom._verticesWithWeightsCount;
        _influenceCountPerVertex = moveFrom._influenceCountPerVertex;
        _influences = moveFrom._influences;
    }

    SkinController& SkinController::operator=(SkinController&& moveFrom) never_throws
    {
        _baseMesh = moveFrom._baseMesh;
        _id = moveFrom._id;
        _name = moveFrom._name;
        _extra = std::move(moveFrom._extra);
        _bindShapeMatrix = moveFrom._bindShapeMatrix;
        _verticesWithWeightsCount = moveFrom._verticesWithWeightsCount;
        _influenceCountPerVertex = moveFrom._influenceCountPerVertex;
        _influences = moveFrom._influences;
        _influenceInputs = std::move(moveFrom._influenceInputs);
        _jointInputs = std::move(moveFrom._jointInputs);
        return *this;
    }

    SkinController::~SkinController() {}

    void DocumentScaffold::Parse_LibraryControllers(Formatter& formatter)
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
                            Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                            formatter.SkipElement();    // <asset> and <extra> possible
                        }
                    } else {
                        if (Is(eleName, u("skin"))) {
                            _skinControllers.push_back(SkinController(formatter, controllerId, controllerName, *this));
                        } else if (Is(eleName, u("morph"))) {
                            Log(Warning) << "<morph> controllers not supported" << std::endl;
                            formatter.SkipElement();
                        } else {
                            Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
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

    

    Material::Material(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("instance_effect"))) {
                ParseInstanceEffect(formatter);
            } else {
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();        // <asset> and <extra> is possible
            }

        ON_ATTRIBUTE
            if (Is(name, u("id"))) _id = value;
            else if (Is(name, u("name"))) _name = value;
        PARSE_END
    }

    void Material::ParseInstanceEffect(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("setparam")) || Is(eleName, u("technique_hint"))) {
                Log(Warning) << "<setparam> and/or <technique_hint> not supported in <instance_effect>. At: " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();
            } else {
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();        // <asset> and <extra> is possible
            }

        ON_ATTRIBUTE
            if (Is(name, u("url"))) _effectReference = value;
                // sid & name possible
        PARSE_END
    }

    void DocumentScaffold::Parse_LibraryMaterials(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("material"))) {
                _materials.push_back(Material(formatter));
            } else {
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();        // <asset> and <extra> is possible
            }

        ON_ATTRIBUTE
            // id & name possible -- but not interesting
        PARSE_END
    }

    Image::Image() {}
    Image::Image(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("init_from"))) {
                SkipAllAttributes(formatter);
                formatter.TryCharacterData(_initFrom);
            } else {
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement(); // many elements possible
            }

        ON_ATTRIBUTE
            if (Is(name, u("id"))) _id = value;
            else if (Is(name, u("name"))) _name = value;

        PARSE_END
    }

    Image::Image(Image&& moveFrom) never_throws
    : _extra(std::move(moveFrom._extra))
    {
        _id = moveFrom._id;
        _name = moveFrom._name;
        _initFrom = moveFrom._initFrom;
    }

    Image& Image::operator=(Image&& moveFrom) never_throws
    {
        _id = moveFrom._id;
        _name = moveFrom._name;
        _initFrom = moveFrom._initFrom;
        _extra = std::move(moveFrom._extra);
        return *this;
    }

    void DocumentScaffold::Parse_LibraryImages(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("image"))) {
                _images.push_back(Image(formatter));
            } else {
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
            // id and name possible

        PARSE_END
    }

    class TransformationSet::RawOperation
    {
    public:
        Type _type;
        uint8 _buffer[sizeof(Float4x4)];
        Section _sid;
        unsigned _next;

        RawOperation();
        RawOperation(const RawOperation& copyFrom) never_throws;
        RawOperation& operator=(const RawOperation& copyFrom) never_throws;
        ~RawOperation();
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

        RawOperation newOp;

        while (formatter.PeekNext() == Formatter::Blob::AttributeName) {
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
            newOp._type = Type::LookAt;
            auto& dst = *(LookAt*)newOp._buffer;
            cdata._start = ParseXMLList(&dst._origin[0], 3, cdata);
            cdata._start = ParseXMLList(&dst._focusPosition[0], 3, cdata);
            cdata._start = ParseXMLList(&dst._upDirection[0], 3, cdata);
        } else if (Is(elementName, u("matrix"))) {
            newOp._type = Type::Matrix4x4;
            auto& dst = *(Float4x4*)newOp._buffer;
            cdata._start = ParseXMLList(&dst(0,0), 16, cdata);
        } else if (Is(elementName, u("rotate"))) {
            newOp._type = Type::Rotate;
            auto& dst = *(ArbitraryRotation*)newOp._buffer;
            cdata._start = ParseXMLList(&dst._axis[0], 3, cdata);
            cdata._start = ParseXMLList(&dst._angle, 1, cdata);
            dst._angle = ConvertAngle(dst._angle);
        } else if (Is(elementName, u("scale"))) {
            newOp._type = Type::Scale;
            auto& dst = *(ArbitraryScale*)newOp._buffer;
            cdata._start = ParseXMLList(&dst._scale[0], 3, cdata);
        } else if (Is(elementName, u("skew"))) {
            newOp._type = Type::Skew;
            auto& dst = *(Skew*)newOp._buffer;
            cdata._start = ParseXMLList(&dst._angle, 1, cdata);
            cdata._start = ParseXMLList(&dst._axisA[0], 3, cdata);
            cdata._start = ParseXMLList(&dst._axisB[0], 3, cdata);
            dst._angle = ConvertAngle(dst._angle);
        } else if (Is(elementName, u("translate"))) {
            newOp._type = Type::Translate;
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

    Transformation TransformationSet::Get(unsigned index) const
    {
        return Transformation(*this, index);
    }

    TransformationSet::TransformationSet() {}
    TransformationSet::TransformationSet(TransformationSet&& moveFrom) never_throws
    : _operations(std::move(moveFrom._operations))
    {}

    TransformationSet& TransformationSet::operator=(TransformationSet&& moveFrom) never_throws
    {
        _operations = std::move(moveFrom._operations);
        return *this;
    }

    TransformationSet::~TransformationSet() {}

    TransformationSet::RawOperation::RawOperation()
    : _type(Type::None), _next(~unsigned(0))
    {
        XlZeroMemory(_buffer);
    }

    TransformationSet::RawOperation::RawOperation(const RawOperation& copyFrom) never_throws
    {
        _type = copyFrom._type;
        _next = copyFrom._next;
        _sid = copyFrom._sid;
        XlCopyMemory(_buffer, copyFrom._buffer, sizeof(_buffer));
    }

    auto TransformationSet::RawOperation::operator=(const RawOperation& copyFrom) never_throws -> RawOperation&
    {
        _type = copyFrom._type;
        _next = copyFrom._next;
        _sid = copyFrom._sid;
        XlCopyMemory(_buffer, copyFrom._buffer, sizeof(_buffer));
        return *this;
    }

    TransformationSet::RawOperation::~RawOperation() {}



    TransformationSet::Type Transformation::GetType() const
    {
        assert(_index != ~unsigned(0));
        return _set->_operations[_index]._type;
    }

    Transformation Transformation::GetNext() const
    {
        assert(_index != ~unsigned(0));
        return Transformation(*_set, _set->_operations[_index]._next);
    }

    const void* Transformation::GetUnionData() const
    {
        assert(_index != ~unsigned(0));
        return _set->_operations[_index]._buffer;
    }

    Section Transformation::GetSid() const
    {
        assert(_index != ~unsigned(0));
        return _set->_operations[_index]._sid;
    }

    Transformation::operator bool() const
    {
        return _index != ~unsigned(0);
    }

    bool Transformation::operator!() const
    {
        return _index == ~unsigned(0);
    }

    Transformation::Transformation(const TransformationSet& set, unsigned index)
    : _set(&set), _index(index)
    {}

    InstanceGeometry::InstanceGeometry(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("bind_material"))) {
                ParseBindMaterial(formatter);
            } else {
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();
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
                    Log(Warning) << "Element " << eleName << " is not currently supported in <instance_geometry>" << std::endl;
                    formatter.SkipElement();
                } else {
                    // <extra> and <param> also possible
                    Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                    formatter.SkipElement();
                }
            }

        ON_ATTRIBUTE
            // should be no attributes in <bind_material>

        PARSE_END
    }

    void InstanceGeometry::ParseTechnique(Formatter& formatter, Section techniqueProfile)
    {
        ON_ELEMENT
            if (Is(eleName, u("instance_material"))) {
                _matBindings.push_back(MaterialBinding(formatter, techniqueProfile));
            } else {
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
            if (Is(name, u("profile"))) techniqueProfile = value;

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
                Log(Warning) << "Element " << eleName << " is not currently supported in <instance_material>" << std::endl;
                formatter.SkipElement();
            } else {
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
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


    InstanceController::InstanceController(Formatter& formatter) 
    {
        ON_ELEMENT
            if (Is(eleName, u("bind_material"))) {
                ParseBindMaterial(formatter);
            } else if (eleName, u("skeleton")) {
                SkipAllAttributes(formatter);
                formatter.TryCharacterData(_skeleton);
            } else {
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
            if (Is(name, u("url"))) _reference = value;

        PARSE_END
    }

    InstanceController::InstanceController(InstanceController&& moveFrom) never_throws
    : InstanceGeometry(std::forward<InstanceController>(moveFrom))
    {
        _skeleton = moveFrom._skeleton;
    }

    InstanceController& InstanceController::operator=(InstanceController&& moveFrom) never_throws
    {
        _skeleton = moveFrom._skeleton;
        return *this;
    }

    class VisualScene::RawNode
    {
    public:
        DocScopeId _id;
        DocScopeId _sid;
        Section _name;

        IndexIntoNodes _parent;
        IndexIntoNodes _nextSibling;
        IndexIntoNodes _firstChild;

        TransformationSetIndex _transformChain;

        SubDoc _extra;

        RawNode() 
        : _parent(IndexIntoNodes_Invalid), _nextSibling(IndexIntoNodes_Invalid), _firstChild(IndexIntoNodes_Invalid)
        , _transformChain(TransformationSetIndex_Invalid) {}
        ~RawNode();

        RawNode(RawNode&& moveFrom) never_throws;
        RawNode& operator=(RawNode&& moveFrom) never_throws;
    };

    VisualScene::VisualScene(Formatter& formatter)
    {
            // Collada can have multiple nodes in the root of the visual scene
            // So let's make a virtual root node to make them all siblings
        std::stack<IndexIntoNodes> workingNodes;
        workingNodes.push(0);
        _nodes.push_back(RawNode());

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

                        RawNode newNode;
                        auto newNodeIndex = IndexIntoNodes(_nodes.size());
                        assert(!workingNodes.empty());

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

                        workingNodes.push(newNodeIndex);
                        _nodes.emplace_back(std::move(newNode));
                        eatEndElement = false;

                    } else if (Is(eleName, u("instance_geometry"))) {

                            // <instance_geometry> should contain a reference to the particular geometry
                            // as well as material binding information.
                            // each <instance_geometry> belongs inside of a 
                        assert(!workingNodes.empty());
                        _geoInstances.push_back(std::make_pair(workingNodes.top(), InstanceGeometry(formatter)));

                    } else if (Is(eleName, u("instance_controller"))) {

                            // <instance_geometry> should contain a reference to the particular geometry
                            // as well as material binding information.
                            // each <instance_geometry> belongs inside of a 
                        assert(!workingNodes.empty());
                        _controllerInstances.push_back(std::make_pair(workingNodes.top(), InstanceController(formatter)));

                    } else if (TransformationSet::IsTransform(eleName)) {

                        assert(!workingNodes.empty());
                        auto& node = _nodes[workingNodes.top()];
                        node._transformChain = _transformSet.ParseTransform(
                            formatter, eleName, node._transformChain);

                    } else if (Is(eleName, u("extra"))) {

                        assert(!workingNodes.empty());
                        if (workingNodes.size() > 1) {
                            auto& node = _nodes[workingNodes.top()];
                            node._extra = SubDoc(formatter);
                        } else {
                            _extra = SubDoc(formatter);
                        }
                    
                    } else {

                            // "asset" and "evaluate_scene" are also valid, but uninteresting
                        Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                        formatter.SkipElement();

                    }

                    if (eatEndElement && !formatter.TryEndElement())
                        Throw(FormatException("Expecting end element", formatter.GetLocation()));

                    continue;
                }

            case Formatter::Blob::EndElement:
                    // size 1 means only the "virtual" root node is there
                if (workingNodes.size() == 1) break;
                formatter.TryEndElement();
                workingNodes.pop();
                continue;

            case Formatter::Blob::AttributeName:
                {
                    Formatter::InteriorSection name, value;
                    formatter.TryAttribute(name, value);

                    if (workingNodes.size() > 1) {

                            // this is actually an attribute inside of a node item
                        auto& node = _nodes[workingNodes.top()];
                        if (Is(name, u("id"))) node._id = value;
                        else if (Is(name, u("sid"))) node._sid = value;
                        else if (Is(name, u("name"))) node._name = value;

                    } else {
                        // visual_scene can have "id" and "name"
                        if (Is(name, u("id"))) _id = value;
                        if (Is(name, u("name"))) _name = value;
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
    , _controllerInstances(std::move(moveFrom._controllerInstances))
    , _transformSet(std::move(moveFrom._transformSet))
    , _id(moveFrom._id)
    , _name(moveFrom._name)
    {
    }

    VisualScene& VisualScene::operator=(VisualScene&& moveFrom) never_throws 
    {
        _extra = std::move(moveFrom._extra);
        _nodes = std::move(moveFrom._nodes);
        _geoInstances = std::move(moveFrom._geoInstances);
        _controllerInstances = std::move(moveFrom._controllerInstances);
        _transformSet = std::move(moveFrom._transformSet);
        _id = moveFrom._id;
        _name = moveFrom._name;
        return *this;
    }

    VisualScene::RawNode::~RawNode() {}
    VisualScene::RawNode::RawNode(RawNode&& moveFrom) never_throws
    : _id(moveFrom._id)
    , _sid(moveFrom._sid)
    , _name(moveFrom._name)
    , _parent(moveFrom._parent)
    , _nextSibling(moveFrom._nextSibling)
    , _firstChild(moveFrom._firstChild)
    , _transformChain(moveFrom._transformChain)
    , _extra(std::move(moveFrom._extra))
    {}

    auto VisualScene::RawNode::operator=(RawNode&& moveFrom) never_throws -> RawNode&
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

    Node VisualScene::GetRootNode() const 
    {
        if (_nodes.empty()) return Node(*this, IndexIntoNodes_Invalid);
        return Node(*this, 0);
    }

    const InstanceGeometry& VisualScene::GetInstanceGeometry(unsigned index) const
    {
        return _geoInstances[index].second;
    }

    Node VisualScene::GetInstanceGeometry_Attach(unsigned index) const
    {
        return Node(*this, _geoInstances[index].first);
    }

    unsigned VisualScene::GetInstanceGeometryCount() const
    {
        return (unsigned)_geoInstances.size();
    }

    const InstanceController& VisualScene::GetInstanceController(unsigned index) const
    {
        return _controllerInstances[index].second;
    }

    Node VisualScene::GetInstanceController_Attach(unsigned index) const
    {
        return Node(*this, _controllerInstances[index].first);
    }

    unsigned VisualScene::GetInstanceControllerCount() const
    {
        return (unsigned)_controllerInstances.size();
    }


    Channel::Channel(Formatter& formatter)
    {
        ON_ELEMENT
            Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
            formatter.SkipElement();
        ON_ATTRIBUTE
            if (Is(name, u("source"))) _source = value;
            else if (Is(name, u("target"))) _target = value;
        PARSE_END
    }

    Channel::Channel() {}

    std::pair<Sampler::Behaviour, const utf8*> s_SamplerBehaviourNames[] = 
    {
        std::make_pair(Sampler::Behaviour::Unspecified, u("UNDEFINED")),
        std::make_pair(Sampler::Behaviour::Constant, u("CONSTANT")),
        std::make_pair(Sampler::Behaviour::Gradient, u("GRADIENT")),
        std::make_pair(Sampler::Behaviour::Cycle, u("CYCLE")),
        std::make_pair(Sampler::Behaviour::Oscillate, u("OSCILLATE")),
        std::make_pair(Sampler::Behaviour::CycleRelative, u("CYCLE_RELATIVE"))
    };

    Sampler::Sampler(Formatter& formatter)
        : Sampler()
    {
        ON_ELEMENT
            if (Is(eleName, u("input"))) {
                _inputs.Add(DataFlow::InputUnshared(formatter));
            } else {
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
            if (Is(name, u("id"))) _id = value;
            else if (Is(name, u("pre_behaviour"))) _prebehaviour = ParseEnum(value, s_SamplerBehaviourNames);
            else if (Is(name, u("post_behaviour"))) _postbehaviour = ParseEnum(value, s_SamplerBehaviourNames);
        PARSE_END
    }

    Sampler::Sampler() 
    {
        _prebehaviour = _postbehaviour = Behaviour::Unspecified;
    }

    Sampler::Sampler(Sampler&& moveFrom) never_throws
    : _inputs(std::move(moveFrom._inputs))
    {
        _id = moveFrom._id;
        _prebehaviour = moveFrom._prebehaviour;
        _postbehaviour = moveFrom._postbehaviour;
    }

    Sampler& Sampler::operator=(Sampler&& moveFrom) never_throws
    {
        _id = moveFrom._id;
        _prebehaviour = moveFrom._prebehaviour;
        _postbehaviour = moveFrom._postbehaviour;
        _inputs = std::move(moveFrom._inputs);
        return *this;
    }

    Animation::Animation(Formatter& formatter, DocumentScaffold& pub)
    {
        ON_ELEMENT
            if (Is(eleName, u("animation"))) {
                _subAnimations.emplace_back(Animation(formatter, pub));
            } else if (Is(eleName, u("source"))) {
                pub.Add(DataFlow::Source(formatter));
            } else if (Is(eleName, u("sampler"))) {
                pub.Add(Sampler(formatter));
            } else if (Is(eleName, u("channel"))) {
                _channels.emplace_back(Channel(formatter));
            } else if (Is(eleName, u("extra"))) {
                _extra = SubDoc(formatter);
            } else {
                // asset also possible
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
            if (Is(name, u("id"))) _id = value;
            else if (Is(name, u("name"))) _name = value;
        PARSE_END
    }

    Animation::Animation() {}
    Animation::Animation(Animation&& moveFrom) never_throws
    : _channels(std::move(moveFrom._channels))
    , _subAnimations(std::move(moveFrom._subAnimations))
    , _id(moveFrom._id)
    , _name(moveFrom._name)
    , _extra(std::move(moveFrom._extra))
    {
    }

    Animation& Animation::operator=(Animation&& moveFrom) never_throws
    {
        _channels = std::move(moveFrom._channels);
        _subAnimations = std::move(moveFrom._subAnimations);
        _id = moveFrom._id;
        _name = moveFrom._name;
        _extra = std::move(moveFrom._extra);
        return *this;
    }

    void DocumentScaffold::Parse_LibraryVisualScenes(Formatter& formatter)
    {
        ON_ELEMENT
            if (Is(eleName, u("visual_scene"))) {
                _visualScenes.push_back(VisualScene(formatter));
            } else {
                    // "asset" and "extra" are also valid, but uninteresting
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();
            }

        ON_ATTRIBUTE
            // name and id. Not interesting if we have only a single library
        PARSE_END
    }

    void DocumentScaffold::Parse_Scene(Formatter& formatter)
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
                Log(Warning) << "Skipping element " << eleName << " at " << formatter.GetLocation() << std::endl;
                formatter.SkipElement();
            }
        ON_ATTRIBUTE
        PARSE_END
    }

    void DocumentScaffold::Add(DataFlow::Source&& element)
    {
        auto hashedId = element.GetId().GetHash();
        auto i = LowerBound(_sources, hashedId);
        if (i != _sources.end() && i->first == hashedId)
            Throw(::Exceptions::BasicLabel("Duplicated id when publishing <source> element"));

        _sources.insert(i, std::make_pair(hashedId, std::move(element)));
    }

    void DocumentScaffold::Add(InputsCollection&& vertexInputs)
    {
        auto hashedId = vertexInputs.GetId().GetHash();
        auto i = LowerBound(_vertexInputs, hashedId);
        if (i != _vertexInputs.end() && i->first == hashedId)
            Throw(::Exceptions::BasicLabel("Duplicated id when publishing <vertices> element"));

        _vertexInputs.insert(i, std::make_pair(hashedId, std::move(vertexInputs)));
    }

    void DocumentScaffold::Add(Sampler&& sampler)
    {
        auto hashedId = sampler.GetId().GetHash();
        auto i = LowerBound(_samplers, hashedId);
        if (i != _samplers.end() && i->first == hashedId)
            Throw(::Exceptions::BasicLabel("Duplicated id when publishing <sampler> element"));

        _samplers.insert(i, std::make_pair(hashedId, std::move(sampler)));
    }

    const DataFlow::Source* DocumentScaffold::FindSource(uint64 id) const
    {
        auto i = LowerBound(_sources, id);
        if (i!=_sources.cend() && i->first == id) 
            return &i->second;
        return nullptr;
    }

    const InputsCollection* DocumentScaffold::FindVertexInputs(uint64 id) const
    {
        auto i = LowerBound(_vertexInputs, id);
        if (i!=_vertexInputs.cend() && i->first == id) 
            return &i->second;
        return nullptr;
    }

    const MeshGeometry* DocumentScaffold::FindMeshGeometry(uint64 guid) const
    {
        for (const auto& m:_geometries)
            if (m.GetId().GetHash() == guid)
                return &m;
        return nullptr;
    }

    const Material* DocumentScaffold::FindMaterial(uint64 guid) const
    {
        for (const auto& m:_materials)
            if (m.GetId().GetHash() == guid)
                return &m;
        return nullptr;
    }

    const VisualScene* DocumentScaffold::FindVisualScene(uint64 guid) const
    {
        for (const auto& s:_visualScenes)
            if (s.GetId().GetHash() == guid)
                return &s;
        return nullptr;
    }

    const Image* DocumentScaffold::FindImage(uint64 guid) const
    {
        for (const auto& i:_images)
            if (i.GetId().GetHash() == guid)
                return &i;
        return nullptr;
    }

    const SkinController*   DocumentScaffold::FindSkinController(uint64 guid) const
    {
        for (const auto& i:_skinControllers)
            if (i.GetId().GetHash() == guid)
                return &i;
        return nullptr;
    }

    static Node SearchForNode(Node n, uint64 guid)
    {
        if (n.GetId().GetHash() == guid) return n;

        for (auto child = n.GetFirstChild(); child; child = child.GetNextSibling()) {
            auto r = SearchForNode(child, guid);
            if (r) return r;
        }

		return Node {};
    }

    Node                    DocumentScaffold::FindNode(uint64 guid) const
    {
            // We need to look in every visual scene to find the node with
            // the given id. Unfortunately we don't have a global map of
            // nodes and ids. So we need to look through the hierarchy
        for (const auto& vs:_visualScenes) {
            auto r = SearchForNode(vs.GetRootNode(), guid);
            if (r) return r;
        }
		return Node {};
    }

	static Node SearchForNodeBySid(Node n, uint64 guid)
    {
        if (n.GetSid().GetHash() == guid) return n;

        for (auto child = n.GetFirstChild(); child; child = child.GetNextSibling()) {
            auto r = SearchForNodeBySid(child, guid);
            if (r) return r;
        }

		return Node {};
    }

	Node                    DocumentScaffold::FindNodeBySid(uint64 guid) const
	{
		for (const auto& vs:_visualScenes) {
            auto r = SearchForNodeBySid(vs.GetRootNode(), guid);
            if (r) return r;
        }
		return Node {};
	}

    const Sampler*          DocumentScaffold::FindSampler(uint64 guid) const
    {
        auto i = LowerBound(_samplers, guid);
        if (i!=_samplers.cend() && i->first == guid) 
            return &i->second;
        return nullptr;
    }

    DocumentScaffold::DocumentScaffold() 
    {}

    DocumentScaffold::~DocumentScaffold() {}


    



    DocScopeId::DocScopeId(Section section)
    : _section(section)
    {
        _hash = Hash64(_section._start, _section._end);
    }

    GuidReference::GuidReference(Section uri)
    {
        _fileHash = _id = 0;

            // Parse the section, and extract a hashed id for the name of the file
            // and the id of the element
            // currently, we'll only support references in to the local file
            // (which take the form of "#..."
        if (uri._end > uri._start && *uri._start == '#') {
            _fileHash = 0;
            _id = Hash64(uri._start+1, uri._end);
        } else {
            // some references don't have the preceeding "#" -- but still
            // match the "id" field of other elements. 
            _id = Hash64(uri._start, uri._end);
        }
    }

    IDocScopeIdResolver::~IDocScopeIdResolver() {}

    const IDocScopeIdResolver* URIResolveContext::FindFile(uint64 fileId) const
    {
        if (!fileId) {
                // local file is always the first
            if (!_files.empty()) return _files[0].second.get();
            return nullptr;
        } else {
            auto i = LowerBound(_files, fileId);
            if (i != _files.end() && i->first == fileId)
                return i->second.get();
            return nullptr;
        }
    }

    URIResolveContext::URIResolveContext(std::shared_ptr<IDocScopeIdResolver> localDoc)
    {
        _files.push_back(std::make_pair(0, std::move(localDoc)));
    }

    URIResolveContext::URIResolveContext() {}

    URIResolveContext::~URIResolveContext() {}


    Node Node::GetNextSibling() const 
    {
        assert(_index != VisualScene::IndexIntoNodes_Invalid);
        return Node(*_scene, _scene->_nodes[_index]._nextSibling);
    }

    Node Node::GetFirstChild() const
    {
        assert(_index != VisualScene::IndexIntoNodes_Invalid);
        return Node(*_scene, _scene->_nodes[_index]._firstChild);
    }

    Node Node::GetParent() const
    {
        assert(_index != VisualScene::IndexIntoNodes_Invalid);
        return Node(*_scene, _scene->_nodes[_index]._parent);
    }

    Transformation Node::GetFirstTransform() const
    {
        assert(_index != VisualScene::IndexIntoNodes_Invalid);
        return _scene->_transformSet.Get(_scene->_nodes[_index]._transformChain);
    }

    Section Node::GetName() const
    {
        assert(_index != VisualScene::IndexIntoNodes_Invalid);
        return _scene->_nodes[_index]._name;
    }

    const DocScopeId& Node::GetId() const
    {
        assert(_index != VisualScene::IndexIntoNodes_Invalid);
        return _scene->_nodes[_index]._id;
    }

    const DocScopeId& Node::GetSid() const
    {
        assert(_index != VisualScene::IndexIntoNodes_Invalid);
        return _scene->_nodes[_index]._sid;
    }

    Node Node::FindBreadthFirst(std::function<bool(const Node&)>&& predicate) const
    {
        // Search through the child nodes to look for a node with a 
        // "sid" that matches the given.
        // Due to the way these work, it may not be an immediate child
        // But we want to return the one with the least depth.
        // So we should do a breadth first check.

        std::queue<VisualScene::IndexIntoNodes> workingQueue;
        workingQueue.push(_index);

        while (!workingQueue.empty()) {
            Node front(*_scene, workingQueue.front());
            workingQueue.pop();
            if (predicate(front)) return front;
            for (auto child = front.GetFirstChild(); child; child = child.GetNextSibling())
                workingQueue.push(child._index);
        }

        return Node();
    }

    std::vector<Node> Node::FindAllBreadthFirst(std::function<bool(const Node&)>&& predicate) const
    {
        std::vector<Node> result;
        std::queue<VisualScene::IndexIntoNodes> workingQueue;
        workingQueue.push(_index);

        while (!workingQueue.empty()) {
            Node front(*_scene, workingQueue.front());
            workingQueue.pop();
            if (predicate(front))
                result.push_back(front);
            for (auto child = front.GetFirstChild(); child; child = child.GetNextSibling())
                workingQueue.push(child._index);
        }

        return result;
    }

    Node::operator bool() const
    {
        return _index != VisualScene::IndexIntoNodes_Invalid;
    }

    bool Node::operator!() const
    {
        return _index == VisualScene::IndexIntoNodes_Invalid;
    }

    Node::Node(const VisualScene& scene, VisualScene::IndexIntoNodes index)
    : _scene(&scene), _index(index)
    {
    }

    Node::Node() : _scene(nullptr), _index(VisualScene::IndexIntoNodes_Invalid) {}
    Node::Node(nullptr_t) : _scene(nullptr), _index(VisualScene::IndexIntoNodes_Invalid) {}

}


