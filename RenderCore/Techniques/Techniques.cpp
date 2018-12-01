// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Techniques.h"
#include "RenderStateResolver.h"
#include "../UniformsStream.h"
#include "../../Assets/Assets.h"
#include "../../Assets/DepVal.h"
#include "../../Assets/IFileSystem.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/IteratorUtils.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/Conversion.h"
#include "../../Utility/PtrUtils.h"
#include <algorithm>

namespace RenderCore { namespace Techniques
{
	class TechniqueSetFile
	{
	public:
		std::vector<std::pair<uint64_t, TechniqueEntry>> _settings;
		const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }

		TechniqueSetFile(
            Utility::InputStreamFormatter<utf8>& formatter, 
			const ::Assets::DirectorySearchRules& searchRules, 
			const ::Assets::DepValPtr& depVal);
		~TechniqueSetFile();
	private:
		::Assets::DepValPtr _depVal;
	};

    static const char* s_parameterBoxNames[] = 
        { "Geometry", "GlobalEnvironment", "Runtime", "Material" };

    using Formatter = InputStreamFormatter<utf8>;

    static bool Is(const char name[], Utility::InputStreamFormatter<utf8>::InteriorSection section)
    {
        return !XlComparePrefixI(section._start, (const utf8*)name, section._end - section._start);
    }

    static std::basic_string<Formatter::value_type> AsString(Formatter::InteriorSection& input)
    {
        return std::basic_string<Formatter::value_type>(input._start, input._end);
    }

    static void LoadInheritedParameterBoxes(
        TechniqueEntry& dst, Formatter& source, 
		IteratorRange<const std::pair<uint64_t, TechniqueEntry>*> localSettings,
        const ::Assets::DirectorySearchRules& searchRules,
		std::vector<::Assets::DepValPtr>& inherited)
    {
            //  We will serialize in a list of 
            //  shareable settings that we can inherit from
            //  Inherit lists should take the form "FileName:Setting"
            //  The "setting" should be a top-level item in the file

        for (;;) {

            auto next = source.PeekNext();
            if (next == Formatter::Blob::EndElement) return;
            if (next != Formatter::Blob::AttributeName)
                Throw(FormatException("Unexpected blob when serializing inheritted list", source.GetLocation()));
            
            Formatter::InteriorSection name, value;
            if (!source.TryAttribute(name, value))
                Throw(FormatException("Bad attribute in inheritted list", source.GetLocation()));
        
            auto colon = std::find(name._start, name._end, ':');
            if (colon != name._end) {
				::Assets::ResChar resolvedFile[MaxPath];
				XlCopyNString(resolvedFile, (const ::Assets::ResChar*)name._start, colon-name._start);
				searchRules.ResolveFile(resolvedFile, dimof(resolvedFile), resolvedFile);

				auto& settingsTable = ::Assets::GetAssetDep<TechniqueSetFile>(resolvedFile);
				auto settingHash = Hash64(colon+1, name._end);
                    
				auto s = LowerBound(settingsTable._settings, settingHash);
				if (s != settingsTable._settings.end() && s->first == settingHash) {
					dst.MergeIn(s->second);
				} else 
					Throw(FormatException("Inheritted object not found", source.GetLocation()));

				if (std::find(inherited.begin(), inherited.end(), settingsTable.GetDependencyValidation()) == inherited.end()) {
					inherited.push_back(settingsTable.GetDependencyValidation());
				}
			} else {
				// this setting is in the same file
				auto settingHash = Hash64(name._start, name._end);
				auto s = std::lower_bound(localSettings.begin(), localSettings.end(), settingHash, CompareFirst<uint64_t, TechniqueEntry>());
				if (s != localSettings.end() && s->first == settingHash) {
					dst.MergeIn(s->second);
				} else
					Throw(FormatException("Inheritted object not found", source.GetLocation()));
			}
        }
    }

    static void LoadParameterBoxes(Formatter& source, ParameterBox dst[4])
    {
        for (;;) {
            auto next = source.PeekNext();
            if (next == Formatter::Blob::EndElement) return;
            if (next != Formatter::Blob::BeginElement)
                Throw(FormatException("Unexpected blob when serializing parameter box list", source.GetLocation()));

            Formatter::InteriorSection eleName;
            if (!source.TryBeginElement(eleName))
                Throw(FormatException("Bad begin element in parameter box list", source.GetLocation()));

            bool matched = false;
            for (unsigned q=0; q<dimof(s_parameterBoxNames); ++q)
                if (Is(s_parameterBoxNames[q], eleName)) {
                        // When a value is not specified, we should default to "0u"
                        // This works well with the C preprocessor; it just means
                        // things will generally default to off
                    unsigned zero = 0u;
                    dst[q].MergeIn(ParameterBox(source, AsOpaqueIteratorRange(zero), ImpliedTyping::TypeOf<unsigned>()));
                    matched = true;
                }

            if (!matched)
                Throw(FormatException("Unknown parameter box name", source.GetLocation()));

            if (!source.TryEndElement())
                Throw(FormatException("Bad end element in parameter box list", source.GetLocation()));
        }
    }
    
    static TechniqueEntry ParseTechniqueEntry(
		Formatter& formatter, 
		IteratorRange<const std::pair<uint64_t, TechniqueEntry>*> localSettings,
		const ::Assets::DirectorySearchRules& searchRules, 
		std::vector<::Assets::DepValPtr>& inherited)
    {
        using ParsingString = std::basic_string<Formatter::value_type>;

		TechniqueEntry result;
        for (;;) {
            switch (formatter.PeekNext())
            {
            case Formatter::Blob::BeginElement:
                {
                    Formatter::InteriorSection eleName;
                    if (!formatter.TryBeginElement(eleName)) break;

                    if (Is("Inherit", eleName)) {
                        LoadInheritedParameterBoxes(result, formatter, localSettings, searchRules, inherited);
                    } else if (Is("Parameters", eleName)) {
                        LoadParameterBoxes(formatter, result._baseSelectors._selectors);
                    } else break;

                    if (!formatter.TryEndElement()) break;
                    continue;
                }

            case Formatter::Blob::AttributeName:
                {
                    Formatter::InteriorSection name, value;
                    if (!formatter.TryAttribute(name, value)) break;
                    if (Is("VertexShader", name)) {
                        result._vertexShaderName = Conversion::Convert<decltype(result._vertexShaderName)>(AsString(value));
                    } else if (Is("PixelShader", name)) {
                        result._pixelShaderName = Conversion::Convert<decltype(result._pixelShaderName)>(AsString(value));
                    } else if (Is("GeometryShader", name)) {
                        result._geometryShaderName = Conversion::Convert<decltype(result._geometryShaderName)>(AsString(value));
                    }
                    continue;
                }

            case Formatter::Blob::EndElement:
                return result;

            default:
				Throw(FormatException("Unexpected blob while reading technique", formatter.GetLocation()));
            }
        }
    }

	TechniqueSetFile::TechniqueSetFile(
		Utility::InputStreamFormatter<utf8>& formatter, 
		const ::Assets::DirectorySearchRules& searchRules, 
		const ::Assets::DepValPtr& depVal)
	: _depVal(depVal)
    {
		std::vector<::Assets::DepValPtr> inherited;
            
            //  each top-level entry is a "Setting", which can contain parameter
            //  boxes (and possibly inherit statements and shaders)

        for (;;) {
            bool cleanQuit = false;
            switch (formatter.PeekNext()) {
            case Formatter::Blob::BeginElement:
                {
                    Formatter::InteriorSection settingName;
                    if (!formatter.TryBeginElement(settingName)) break;

					if (XlEqString(settingName, u("Inherit")) || XlEqString(settingName, u("Technique"))) {
						formatter.SkipElement();
					} else {
						auto hash = Hash64(settingName._start, settingName._end);
						auto i = LowerBound(_settings, hash);
						_settings.insert(i, std::make_pair(hash, ParseTechniqueEntry(formatter, MakeIteratorRange(_settings), searchRules, inherited)));
					}

                    if (!formatter.TryEndElement()) break;
                }
                continue;

            case Formatter::Blob::None:
                cleanQuit = true;
                break;

            default:
                break;
            }

            if (!cleanQuit)
                Throw(FormatException("Unexpected blob while reading stream", formatter.GetLocation()));
            break;
        }

        for (auto i=inherited.begin(); i!=inherited.end(); ++i) {
            ::Assets::RegisterAssetDependency(_depVal, *i);
        }
    }

	TechniqueSetFile::~TechniqueSetFile() {}


    void TechniqueEntry::MergeIn(const TechniqueEntry& source)
    {
        if (!source._vertexShaderName.empty()) _vertexShaderName = source._vertexShaderName;
        if (!source._pixelShaderName.empty()) _pixelShaderName = source._pixelShaderName;
        if (!source._geometryShaderName.empty()) _geometryShaderName = source._geometryShaderName;

        for (unsigned c=0; c<ShaderSelectors::Source::Max; ++c) {
            const auto& s = source._baseSelectors._selectors[c];
            auto& d = _baseSelectors._selectors[c];
            for (const auto& i:s)
                d.SetParameter(i.Name(), i.RawValue(), i.Type());
        }
    }

	template<typename Char>
		static void ReplaceInString(
			std::basic_string<Char>& str, 
			StringSection<Char> oldText, 
			StringSection<Char> newText)
		{
			size_t i = 0;
			for (;;) {
				i = str.find(oldText.begin(), i, oldText.Length());
				if (i == std::basic_string<Char>::npos) return;
				str.replace(i, oldText.Length(), newText.begin());
				i += newText.Length(); // prevent infinite loops if newText contains oldText
			}
		}

	static void ReplaceSelfReference(TechniqueEntry& entry, StringSection<::Assets::ResChar> filename)
	{
		auto selfRef = MakeStringSection("<.>");
		ReplaceInString(entry._vertexShaderName, selfRef, filename);
		ReplaceInString(entry._pixelShaderName, selfRef, filename);
		ReplaceInString(entry._geometryShaderName, selfRef, filename);
	}

    TechniqueEntry::TechniqueEntry() 
	{
			//
            //      There are some parameters that will we always have an effect on the
            //      binding. We need to make sure these are initialized with sensible
            //      values.
            //
        auto& globalParam = _baseSelectors._selectors[ShaderSelectors::Source::GlobalEnvironment];
        globalParam.SetParameter((const utf8*)"vs_", 50);
        globalParam.SetParameter((const utf8*)"ps_", 50);
	}
    TechniqueEntry::~TechniqueEntry() {}

    T1(Pair) class CompareFirstString
    {
    public:
        bool operator()(const Pair& lhs, const Pair& rhs) const { return XlCompareString(lhs.first, rhs.first) < 0; }
        using Section = StringSection<
            typename std::remove_const<typename std::remove_reference<decltype(*std::declval<typename Pair::first_type>())>::type>::type
            >;
        bool operator()(const Pair& lhs, const Section& rhs) const { return XlCompareString(lhs.first, rhs) < 0; }
        bool operator()(const Section& lhs, const Pair& rhs) const { return XlCompareString(lhs, rhs.first) < 0; }
    };

    static unsigned AsTechniqueIndex(StringSection<utf8> name)
    {
        using Pair = std::pair<const utf8*, unsigned>;
        const Pair bindingNames[] = 
        {
                // note -- lexographically sorted!
            { u("Deferred"),                       unsigned(TechniqueIndex::Deferred) },
            { u("DepthOnly"),                      unsigned(TechniqueIndex::DepthOnly) },
            { u("DepthWeightedTransparency"),      unsigned(TechniqueIndex::DepthWeightedTransparency) },
            { u("Forward"),                        unsigned(TechniqueIndex::Forward) },
            { u("OrderIndependentTransparency"),   unsigned(TechniqueIndex::OrderIndependentTransparency) },
            { u("PrepareVegetationSpawn"),         unsigned(TechniqueIndex::PrepareVegetationSpawn) },
            { u("RayTest"),                        unsigned(TechniqueIndex::RayTest) },
            { u("ShadowGen"),                      unsigned(TechniqueIndex::ShadowGen) },
            { u("StochasticTransparency"),         unsigned(TechniqueIndex::StochasticTransparency) },
            { u("VisNormals"),                     unsigned(TechniqueIndex::VisNormals) },
            { u("VisWireframe"),                   unsigned(TechniqueIndex::VisWireframe) },
            { u("WriteTriangleIndex"),             unsigned(TechniqueIndex::WriteTriangleIndex) }
        };

        auto i = std::lower_bound(bindingNames, ArrayEnd(bindingNames), name, CompareFirstString<Pair>());
        if (XlEqString(name, i->first))
            return i->second;
        return ~0u;
    }

    Technique::Technique(StringSection<::Assets::ResChar> resourceName)
    {
		_validationCallback = std::make_shared<::Assets::DependencyValidation>();
		::Assets::RegisterFileDependency(_validationCallback, resourceName);

		TRY {
			size_t sourceFileSize = 0;
			auto sourceFile = ::Assets::TryLoadFileAsMemoryBlock(resourceName, &sourceFileSize);
        
			if (sourceFile && sourceFileSize) {
				auto searchRules = ::Assets::DefaultDirectorySearchRules(resourceName);
				std::vector<std::shared_ptr<::Assets::DependencyValidation>> inheritedAssets;

				StringSection<char> configSection(
					(const char*)sourceFile.get(), 
					(const char*)PtrAdd(sourceFile.get(), sourceFileSize));

				auto compoundDoc = ::Assets::ReadCompoundTextDocument(configSection);
				if (!compoundDoc.empty()) {
					auto i = std::find_if(
						compoundDoc.cbegin(), compoundDoc.cend(),
						[](const ::Assets::TextChunk<char>& chunk)
						{ return XlEqString(chunk._type, "TechniqueConfig"); });

					if (i != compoundDoc.cend())
						configSection = i->_content;
				}

				Formatter formatter(MemoryMappedInputStream(configSection.begin(), configSection.end()));
				ParseConfigFile(formatter, resourceName, searchRules, inheritedAssets);

					// Do some patch-up after parsing...
					// we want to replace <.> with the name of the asset
					// This allows the asset to reference itself (without complications
					// for related to directories, etc)
				for (unsigned c=0; c<dimof(_entries); ++c)
					ReplaceSelfReference(_entries[c], resourceName);

				for (auto i=inheritedAssets.begin(); i!=inheritedAssets.end(); ++i)
					::Assets::RegisterAssetDependency(_validationCallback, *i);
			}
		} CATCH(const ::Assets::Exceptions::ConstructionError& e) {
			Throw(::Assets::Exceptions::ConstructionError(e, _validationCallback));
		} CATCH(const std::exception& e) {
			Throw(::Assets::Exceptions::ConstructionError(e, _validationCallback));
		} CATCH_END
    }

    Technique::~Technique()
    {}

    void Technique::ParseConfigFile(
        Formatter& formatter, 
		StringSection<::Assets::ResChar> containingFileName,
        const ::Assets::DirectorySearchRules& searchRules,
        std::vector<::Assets::DepValPtr>& inheritedAssets)
    {
        for (;;) {
            bool cleanQuit = false;
            switch (formatter.PeekNext()) {
            case Formatter::Blob::BeginElement:
                {
                    Formatter::InteriorSection eleName;
                    if (!formatter.TryBeginElement(eleName)) break;

                    if (XlEqString(eleName, u("Inherit"))) {
                        // we should find a list of other technique configuation files to inherit from
                        for (;;) {
                            auto next = formatter.PeekNext();
                            if (next == Formatter::Blob::EndElement) break;
                            if (next != Formatter::Blob::AttributeName)
                                Throw(FormatException("Unexpected blob when serializing inheritted list", formatter.GetLocation()));
            
                            Formatter::InteriorSection name, value;
                            if (!formatter.TryAttribute(name, value))
                                Throw(FormatException("Bad attribute in inheritted list", formatter.GetLocation()));

                            ::Assets::ResChar resolvedFile[MaxPath];
                            XlCopyNString(resolvedFile, (const ::Assets::ResChar*)name._start, name._end-name._start);
                            searchRules.ResolveFile(resolvedFile, resolvedFile);

                            // exceptions thrown by from the inheritted asset will not be suppressed
                            const auto& inheritFrom = ::Assets::GetAssetDep<Technique>(resolvedFile);
                            inheritedAssets.push_back(inheritFrom.GetDependencyValidation());

                            // we should merge in the content from all the inheritted's assets
                            for (unsigned c=0; c<dimof(_entries); ++c)
                                _entries[c].MergeIn(inheritFrom._entries[c]);
							_cbLayout = inheritFrom._cbLayout;
                        }
                    } else if (XlEqString(eleName, u("Technique"))) {
						// We should find a list of the actual techniques to use, as attributes
						// The attribute name defines the how to apply the technique, and the attribute value is
						// the name of the technique itself
						for (;;) {
							auto next = formatter.PeekNext();
                            if (next == Formatter::Blob::EndElement) break;
                            if (next != Formatter::Blob::AttributeName)
                                Throw(FormatException("Unexpected blob when serializing technique list", formatter.GetLocation()));

							Formatter::InteriorSection name, value;
                            if (!formatter.TryAttribute(name, value))
                                Throw(FormatException("Bad attribute in technique list", formatter.GetLocation()));

							if (XlEqString(name, u("CBLayout"))) {
								_cbLayout = PredefinedCBLayout(MakeStringSection((const char*)value.begin(), (const char*)value.end()), true);
							} else {
								auto index = AsTechniqueIndex(name);
								if (index != ~0) {		// (silent failure if the technique name is unknown)
									StringSection<::Assets::ResChar> t((const char*)value.begin(), (const char*)value.end());
									StringSection<::Assets::ResChar> containerName, settingName;
									const auto* colon = XlFindChar(t, ':');
									if (colon) { 
										containerName = MakeStringSection(t.begin(), colon);
										settingName = MakeStringSection(colon+1, t.end());
									} else {
										containerName = containingFileName;
										settingName = t;
									}

									const auto& setFile = ::Assets::GetAssetDep<TechniqueSetFile>(containerName);
									auto hash = Hash64(settingName);
									auto i = LowerBound(setFile._settings, hash);
									if (i != setFile._settings.end() && i->first == hash) {
										_entries[index] = i->second;		// (don't merge in; this a replace)
									} else 
										Throw(FormatException("Could not resolve requested technique setting", formatter.GetLocation()));

									if (std::find(inheritedAssets.begin(), inheritedAssets.end(), setFile.GetDependencyValidation()) == inheritedAssets.end()) {
										inheritedAssets.push_back(setFile.GetDependencyValidation());
									}
								}
							}
						}
					} else if (XlEqString(eleName, u("*"))) {
						// This is an override that applies to all techniques in this file
						auto overrideTechnique = ParseTechniqueEntry(formatter, {}, searchRules, inheritedAssets);
						for (unsigned c=0; c<dimof(_entries); ++c) 
							_entries[c].MergeIn(overrideTechnique);
					} else {
						// other elements are packed in here, as well (such as the actual technique definitions)
						formatter.SkipElement();
					}

                    if (!formatter.TryEndElement()) break;
                }
                continue;

            case Formatter::Blob::None:
                cleanQuit = true;
                break;

            default:
                break;
            }

            if (!cleanQuit)
                Throw(FormatException("Unexpected blob while reading stream", formatter.GetLocation()));
            break;
        }
    }

	TechniqueEntry& Technique::GetEntry(unsigned idx)
	{
		assert(idx < dimof(_entries));
		return _entries[idx];
	}

	const TechniqueEntry& Technique::GetEntry(unsigned idx) const
	{
		assert(idx < dimof(_entries));
		return _entries[idx];
	}

        //////////////////////-------//////////////////////

    uint64      ShaderSelectors::CalculateFilteredHash(const ParameterBox* globalState[Source::Max]) const
    {
		uint64 filteredState = _selectors[0].CalculateFilteredHashValue(*globalState[0]);
        for (unsigned c=1; c<Source::Max; ++c) {
              // filteredState ^= _selectors[c].TranslateHash(*globalState[c]) << (c*6);     // we have to be careful of cases where 2 boxes have their filtered tables sort of swapped... Those cases should produce distinctive hashes

			filteredState = HashCombine(_selectors[c].CalculateFilteredHashValue(*globalState[c]), filteredState);
        }
        return filteredState;
    }

    uint64      ShaderSelectors::CalculateFilteredHash(uint64 inputHash, const ParameterBox* globalState[Source::Max]) const
    {
            //      Find a local state to match
        auto i = LowerBound(_globalToFilteredTable, inputHash);
        if (i!=_globalToFilteredTable.cend() && i->first == inputHash) {
            return i->second;
        }

            //  The call to "CalculateFilteredHash" here is quite expensive... Ideally we should only get here during
            //  initialisation steps (or perhaps on the first few frames. We ideally don't want to be going through
            //  all of this during normal frames.
        uint64 filteredState = CalculateFilteredHash(globalState);
        _globalToFilteredTable.insert(i, std::make_pair(inputHash, filteredState));
        return filteredState;
    }

    void        ShaderSelectors::BuildStringTable(std::vector<std::pair<const utf8*, std::string>>& defines) const
    {
        for (unsigned c=0; c<dimof(_selectors); ++c) {
            Utility::BuildStringTable(defines, _selectors[c]);
        }
    }

		        //////////////////////-------//////////////////////

	const UniformsStreamInterface& TechniqueContext::GetGlobalUniformsStreamInterface()
	{
			//  We need to specify the order of resources as they appear in 
			//  _globalUniformsStream
		static auto HashGlobalTransform = Hash64("GlobalTransform");
		static auto HashGlobalState = Hash64("GlobalState");
		static auto HashShadowProjection = Hash64("ArbitraryShadowProjection");
		static auto HashOrthoShadowProjection = Hash64("OrthogonalShadowProjection");
		static auto HashBasicLightingEnvironment = Hash64("BasicLightingEnvironment");
		static UniformsStreamInterface globalUniforms;
		static bool setupGlobalUniforms = false;
		if (!setupGlobalUniforms) {
			globalUniforms.BindConstantBuffer(CB_GlobalTransform, { HashGlobalTransform });
			globalUniforms.BindConstantBuffer(CB_GlobalState, { HashGlobalState });
			globalUniforms.BindConstantBuffer(CB_ShadowProjection, { HashShadowProjection });
			globalUniforms.BindConstantBuffer(CB_OrthoShadowProjection, { HashOrthoShadowProjection });
			globalUniforms.BindConstantBuffer(CB_BasicLightingEnvironment, { HashBasicLightingEnvironment });
			setupGlobalUniforms = true;
		}
		return globalUniforms;
	}

    TechniqueContext::TechniqueContext()
    {
        _defaultRenderStateDelegate = CreateRenderStateDelegate_Default();
    }


}}

