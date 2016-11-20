// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "ConversionConfig.h"
#include "../../Assets/Assets.h"       // (for RegisterFileDependency)
#include "../../ConsoleRig/Log.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/Streams/StreamDOM.h"
#include "../../Utility/Streams/StreamFormatter.h"

namespace RenderCore { namespace ColladaConversion
{
    bool ImportCameras = true;

    ImportConfiguration::ImportConfiguration(const ::Assets::ResChar filename[])
    {
        TRY 
        {
            size_t fileSize = 0;
            auto sourceFile = LoadFileAsMemoryBlock(filename, &fileSize);
            InputStreamFormatter<utf8> formatter(
                MemoryMappedInputStream(sourceFile.get(), PtrAdd(sourceFile.get(), fileSize)));
            Document<InputStreamFormatter<utf8>> doc(formatter);

            _resourceBindings = BindingConfig(doc.Element(u("Resources")));
            _constantsBindings = BindingConfig(doc.Element(u("Constants")));
            _vertexSemanticBindings = BindingConfig(doc.Element(u("VertexSemantics")));

        } CATCH(...) {
            LogWarning << "Problem while loading configuration file (" << filename << "). Using defaults.";
        } CATCH_END

        _depVal = std::make_shared<::Assets::DependencyValidation>();
        RegisterFileDependency(_depVal, filename);
    }
    ImportConfiguration::ImportConfiguration() {}
    ImportConfiguration::~ImportConfiguration()
    {}

    BindingConfig::BindingConfig(const DocElementHelper<InputStreamFormatter<utf8>>& source)
    {
        auto bindingRenames = source.Element(u("Rename"));
        if (bindingRenames) {
            auto child = bindingRenames.FirstAttribute();
            for (; child; child = child.Next())
                if (child)
                    _exportNameToBinding.push_back(
                        std::make_pair(child.Name().AsString(), child.Value().AsString()));
        }

        auto bindingSuppress = source.Element(u("Suppress"));
        if (bindingSuppress) {
            auto child = bindingSuppress.FirstAttribute();
            for (; child; child = child.Next())
                _bindingSuppressed.push_back(child.Name().AsString());
        }
    }

    BindingConfig::BindingConfig() {}
    BindingConfig::~BindingConfig() {}

    std::basic_string<utf8> BindingConfig::AsNative(StringSection<utf8> input) const
    {
            //  we need to define a mapping between the names used by the max exporter
            //  and the native XLE shader names. The meaning might not match perfectly
            //  but let's try to get as close as possible
        auto i = std::find_if(
            _exportNameToBinding.cbegin(), _exportNameToBinding.cend(),
            [=](const std::pair<String, String>& e) 
            { return XlEqString(input, e.first); });

        if (i != _exportNameToBinding.cend()) 
            return i->second;
        return input.AsString();
    }

    bool BindingConfig::IsSuppressed(StringSection<utf8> input) const
    {
        auto i = std::find_if(
            _bindingSuppressed.cbegin(), _bindingSuppressed.cend(),
            [=](const String& e) { return XlEqString(input, e); });

        return (i != _bindingSuppressed.cend());
    }

}}
