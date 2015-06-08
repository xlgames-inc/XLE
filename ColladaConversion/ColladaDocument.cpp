// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../ConsoleRig/Log.h"
#include "../Utility/Streams/XmlStreamFormatter.h"
#include "../Utility/Streams/StreamDom.h"
#include "../Utility/UTFUtils.h"
#include "../Utility/Conversion.h"

namespace ColladaConversion
{
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

    class ColladaDocument
    {
    public:
        void Parse(XmlInputStreamFormatter<utf8>& formatter);

        void Parse_LibraryEffects(XmlInputStreamFormatter<utf8>& formatter);
        void Parse_LibraryGeometries(XmlInputStreamFormatter<utf8>& formatter);

        ColladaDocument();

    protected:
        AssetDesc _rootAsset;
    };

    bool Is(XmlInputStreamFormatter<utf8>::InteriorSection& section, const utf8 match[])
    {
        return !XlComparePrefix(match, section._start, section._end - section._start);
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
        _upAxis = UpAxis::X;
    }

    AssetDesc::AssetDesc(XmlInputStreamFormatter<utf8>& formatter)
        : AssetDesc()
    {
        using Formatter = XmlInputStreamFormatter<utf8>;

        for (;;) {
            switch (formatter.PeekNext()) {
            case Formatter::Blob::AttributeName:
                Formatter::InteriorSection name, value;
                formatter.TryReadAttribute(name, value);
                if (Is(name, "up_axis")) {
                    if ((value._end - value._start) >= 1) {
                        switch (std::tolower(*value._start)) {
                        case 'x': _upAxis = UpAxis::X; break;
                        case 'y': _upAxis = UpAxis::Y; break;
                        case 'z': _upAxis = UpAxis::Z; break;
                        }
                    }
                }
                break;

            case Formatter::Blob::BeginElement:
                Formatter::InteriorSection eleName;
                formatter.TryReadBeginElement(eleName);
                if (Is(eleName, u("unit"))) {
                    Document<Formatter> doc(formatter);
                    _metersPerUnit = doc(u("meter"), _metersPerUnit);
                } else
                    formatter.SkipElement();
                break;

            default:
                return;
            }
        }
    }
    


    void ColladaDocument::Parse(XmlInputStreamFormatter<utf8>& formatter)
    {
        using Formatter = XmlInputStreamFormatter<utf8>;
        Formatter::InteriorSection rootEle;
        if (!formatter.TryReadBeginElement(rootEle) || !Is(rootEle, u("COLLADA")))
            Throw(FormatException("Expecting root COLLADA element", formatter.GetLocation()));
        
        switch (formatter.PeekNext()) {
        case Formatter::Blob::BeginElement:
            Formatter::InteriorSection ele;
            formatter.TryReadBeginElement(ele);

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

            if (!formatter.TryReadEndElement())
                Throw(FormatException("Expecting end element", formatter.GetLocation()));

            break;

        case Formatter::Blob::EndElement:
            return; // hit the end of file

        case Formatter::Blob::AttributeName:
        default:
            Throw(FormatException("Unexpected blob", formatter.GetLocation()));
            break;
        }
    }


    void ColladaDocument::Parse_LibraryEffects(XmlInputStreamFormatter<utf8>& formatter)
    {
        using Formatter = XmlInputStreamFormatter<utf8>;
        for (;;) {
            switch (formatter.PeekNext()) {
            case Formatter::Blob::BeginElement:
                Formatter::InteriorSection eleName;
                formatter.TryReadBeginElement(eleName);

                if (Is(eleName, "effect")) {

                } else {
                        // "asset" and "extra" are also valid
                    formatter.SkipElement();
                }
                break;

            case Formatter::Blob::AttributeName:
                Formatter::InteriorSection name, value;
                formatter.TryReadAttribute(name, value);
                break;

            default:
                return;
            }
        }
    }

    void ColladaDocument::Parse_LibraryGeometries(XmlInputStreamFormatter<utf8>& formatter)
    {

    }

}