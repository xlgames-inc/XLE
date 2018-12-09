// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RawMaterial.h"
#include "../Types.h"
#include "../Techniques/TechniqueMaterial.h"
#include "../../Assets/Assets.h"
#include "../../Assets/IntermediateAssets.h"		// (for GetDependentFileState)
#include "../../Assets/DeferredConstruction.h"
#include "../../Assets/ConfigFileContainer.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include "../../Utility/Streams/StreamDOM.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/StringFormat.h"

namespace RenderCore { namespace Assets
{

	static const auto s_MaterialCompileProcessType = ConstHash64<'RawM', 'at'>::Value;

///////////////////////////////////////////////////////////////////////////////////////////////////

    static const std::pair<Blend, const utf8*> s_blendNames[] =
    {
        std::make_pair(Blend::Zero, u("zero")),
        std::make_pair(Blend::One, u("one")),
            
        std::make_pair(Blend::SrcColor, u("srccolor")),
        std::make_pair(Blend::InvSrcColor, u("invsrccolor")),
        std::make_pair(Blend::DestColor, u("destcolor")),
        std::make_pair(Blend::InvDestColor, u("invdestcolor")),

        std::make_pair(Blend::SrcAlpha, u("srcalpha")),
        std::make_pair(Blend::InvSrcAlpha, u("invsrcalpha")),
        std::make_pair(Blend::DestAlpha, u("destalpha")),
        std::make_pair(Blend::InvDestAlpha, u("invdestalpha")),
    };

    static const std::pair<BlendOp, const utf8*> s_blendOpNames[] =
    {
        std::make_pair(BlendOp::NoBlending, u("noblending")),
        std::make_pair(BlendOp::NoBlending, u("none")),
        std::make_pair(BlendOp::NoBlending, u("false")),

        std::make_pair(BlendOp::Add, u("add")),
        std::make_pair(BlendOp::Subtract, u("subtract")),
        std::make_pair(BlendOp::RevSubtract, u("revSubtract")),
        std::make_pair(BlendOp::Min, u("min")),
        std::make_pair(BlendOp::Max, u("max"))
    };
    
    static Blend DeserializeBlend(
        DocElementHelper<InputStreamFormatter<utf8>> ele, const utf8 name[])
    {
        if (ele) {
            auto child = ele.Attribute(name);
            if (child) {
                auto value = child.Value();
                for (unsigned c=0; c<dimof(s_blendNames); ++c)
                    if (XlEqStringI(value, s_blendNames[c].second))
                        return s_blendNames[c].first;
                return (Blend)XlAtoI32((const char*)child.Value().AsString().c_str());
            }
        }

        return Blend::Zero;
    }

    static BlendOp DeserializeBlendOp(
        DocElementHelper<InputStreamFormatter<utf8>> ele, const utf8 name[])
    {
        if (ele) {
            auto child = ele.Attribute(name);
            if (child) {
                auto value = child.Value();
                for (unsigned c=0; c<dimof(s_blendOpNames); ++c)
                    if (XlEqStringI(value, s_blendOpNames[c].second))
                        return s_blendOpNames[c].first;
                return (BlendOp)XlAtoI32((const char*)child.Value().AsString().c_str());
            }
        }

        return BlendOp::NoBlending;
    }

    static Techniques::RenderStateSet DeserializeStateSet(InputStreamFormatter<utf8>& formatter)
    {
        using RenderStateSet = Techniques::RenderStateSet;
        RenderStateSet result;

        Document<InputStreamFormatter<utf8>> doc(formatter);

        {
            auto child = doc.Attribute(u("DoubleSided")).As<bool>();
            if (child.first) {
                result._doubleSided = child.second;
                result._flag |= RenderStateSet::Flag::DoubleSided;
            }
        }
        {
            auto child = doc.Attribute(u("Wireframe")).As<bool>();
            if (child.first) {
                result._wireframe = child.second;
                result._flag |= RenderStateSet::Flag::Wireframe;
            }
        }
        {
            auto child = doc.Attribute(u("WriteMask")).As<unsigned>();
            if (child.first) {
                result._writeMask = child.second;
                result._flag |= RenderStateSet::Flag::WriteMask;
            }
        }
        {
            auto child = doc.Attribute(u("BlendType"));
            if (child) {
                if (XlEqStringI(child.Value(), u("decal"))) {
                    result._blendType = RenderStateSet::BlendType::DeferredDecal;
                } else if (XlEqStringI(child.Value(), u("ordered"))) {
                    result._blendType = RenderStateSet::BlendType::Ordered;
                } else {
                    result._blendType = RenderStateSet::BlendType::Basic;
                }
                result._flag |= RenderStateSet::Flag::BlendType;
            }
        }
        {
            auto child = doc.Attribute(u("DepthBias")).As<int>();
            if (child.first) {
                result._depthBias = child.second;
                result._flag |= RenderStateSet::Flag::DepthBias;
            }
        }
        {
            auto child = doc.Element(u("ForwardBlend"));
            if (child) {
                result._forwardBlendSrc = DeserializeBlend(child, u("Src"));
                result._forwardBlendDst = DeserializeBlend(child, u("Dst"));
                result._forwardBlendOp = DeserializeBlendOp(child, u("Op"));
                result._flag |= RenderStateSet::Flag::ForwardBlend;
            }
        }
        return result;
    }

    static const utf8* AsString(Techniques::RenderStateSet::BlendType blend)
    {
        switch (blend) {
        case Techniques::RenderStateSet::BlendType::DeferredDecal: return u("decal");
        case Techniques::RenderStateSet::BlendType::Ordered: return u("ordered");
        default:
        case Techniques::RenderStateSet::BlendType::Basic: return u("basic");
        }
    }

    static const utf8* AsString(Blend input)
    {
        for (unsigned c=0; c<dimof(s_blendNames); ++c) {
            if (s_blendNames[c].first == input) {
                return s_blendNames[c].second;
            }
        }
        return u("one");
    }

    static const utf8* AsString(BlendOp input)
    {
        for (unsigned c=0; c<dimof(s_blendOpNames); ++c) {
            if (s_blendOpNames[c].first == input) {
                return s_blendOpNames[c].second;
            }
        }
        return u("noblending");
    }

    template<typename Type>
        std::basic_string<utf8> AutoAsString(const Type& type)
        {
            return Conversion::Convert<std::basic_string<utf8>>(
                ImpliedTyping::AsString(type, true));
        }

    static bool HasSomethingToSerialize(const Techniques::RenderStateSet& stateSet)
    {
        return stateSet._flag != 0;
    }

    static void SerializeStateSet(OutputStreamFormatter& formatter, const Techniques::RenderStateSet& stateSet)
    {
        using RenderStateSet = Techniques::RenderStateSet;
        if (stateSet._flag & RenderStateSet::Flag::DoubleSided)
            formatter.WriteAttribute(u("DoubleSided"), AutoAsString(stateSet._doubleSided));

        if (stateSet._flag & RenderStateSet::Flag::Wireframe)
            formatter.WriteAttribute(u("Wireframe"), AutoAsString(stateSet._wireframe));

        if (stateSet._flag & RenderStateSet::Flag::WriteMask)
            formatter.WriteAttribute(u("WriteMask"), AutoAsString(stateSet._writeMask));

        if (stateSet._flag & RenderStateSet::Flag::BlendType)
            formatter.WriteAttribute(u("BlendType"), AsString(stateSet._blendType));

        if (stateSet._flag & RenderStateSet::Flag::DepthBias)
            formatter.WriteAttribute(u("DepthBias"), AutoAsString(stateSet._depthBias));

        if (stateSet._flag & RenderStateSet::Flag::ForwardBlend) {
            auto ele = formatter.BeginElement(u("ForwardBlend"));
            formatter.WriteAttribute(u("Src"), AsString(stateSet._forwardBlendSrc));
            formatter.WriteAttribute(u("Dst"), AsString(stateSet._forwardBlendDst));
            formatter.WriteAttribute(u("Op"), AsString(stateSet._forwardBlendOp));
            formatter.EndElement(ele);
        }
    }

    static Techniques::RenderStateSet Merge(
        Techniques::RenderStateSet underride,
        Techniques::RenderStateSet override)
    {
        using StateSet = Techniques::RenderStateSet;
        StateSet result = underride;
        if (override._flag & StateSet::Flag::DoubleSided) {
            result._doubleSided = override._doubleSided;
            result._flag |= StateSet::Flag::DoubleSided;
        }
        if (override._flag & StateSet::Flag::Wireframe) {
            result._wireframe = override._wireframe;
            result._flag |= StateSet::Flag::Wireframe;
        }
        if (override._flag & StateSet::Flag::WriteMask) {
            result._writeMask = override._writeMask;
            result._flag |= StateSet::Flag::WriteMask;
        }
        if (override._flag & StateSet::Flag::BlendType) {
            result._blendType = override._blendType;
            result._flag |= StateSet::Flag::BlendType;
        }
        if (override._flag & StateSet::Flag::ForwardBlend) {
            result._forwardBlendSrc = override._forwardBlendSrc;
            result._forwardBlendDst = override._forwardBlendDst;
            result._forwardBlendOp = override._forwardBlendOp;
            result._flag |= StateSet::Flag::ForwardBlend;
        }
        if (override._flag & StateSet::Flag::DepthBias) {
            result._depthBias = override._depthBias;
            result._flag |= StateSet::Flag::DepthBias;
        }
        return result;
    }

    RawMaterial::RawMaterial() {}

    std::vector<::Assets::rstring> 
        DeserializeInheritList(InputStreamFormatter<utf8>& formatter)
    {
        std::vector<::Assets::rstring> result;

        for (;;) {
            using Blob = InputStreamFormatter<utf8>::Blob;
            switch (formatter.PeekNext()) {
            case Blob::AttributeName:
                {
                    InputStreamFormatter<utf8>::InteriorSection name, value;
                    formatter.TryAttribute(name, value);
                    result.push_back(
                        Conversion::Convert<::Assets::rstring>(std::basic_string<utf8>(name._start, name._end)));
                    break;
                }

            case Blob::BeginElement:
                {
                    InputStreamFormatter<utf8>::InteriorSection eleName;
                    formatter.TryBeginElement(eleName);
                    break;
                }

            case Blob::AttributeValue:
            case Blob::CharacterData:
                Throw(FormatException("Unexpected element", formatter.GetLocation()));
                break;

            case Blob::EndElement:
            case Blob::None:
                return result;
            default:
                assert(0);
            }
        }
    }

    RawMaterial::RawMaterial(
		InputStreamFormatter<utf8>& formatter, 
		const ::Assets::DirectorySearchRules& searchRules, 
		const ::Assets::DepValPtr& depVal)
	: _depVal(depVal), _searchRules(searchRules)
    {
        for (;;) {
            using Blob = InputStreamFormatter<utf8>::Blob;
            switch (formatter.PeekNext()) {
            case Blob::AttributeName:
                {
                    InputStreamFormatter<utf8>::InteriorSection name, value;
                    formatter.TryAttribute(name, value);
                    if (XlEqString(name, u("TechniqueConfig"))) {
                        _techniqueConfig = Conversion::Convert<AssetName>(value.AsString());
                    } 
                    break;
                }

            case Blob::BeginElement:
                {
                    InputStreamFormatter<utf8>::InteriorSection eleName;
                    formatter.TryBeginElement(eleName);

                       // first, load inherited settings.
                    if (XlEqString(eleName, u("Inherit"))) {
                        _inherit = DeserializeInheritList(formatter);
                    } else if (XlEqString(eleName, u("ShaderParams"))) {
                        _matParamBox = ParameterBox(formatter);
                    } else if (XlEqString(eleName, u("Constants"))) {
                        _constants = ParameterBox(formatter);
                    } else if (XlEqString(eleName, u("ResourceBindings"))) {
                        _resourceBindings = ParameterBox(formatter);
                    } else if (XlEqString(eleName, u("States"))) {
                        _stateSet = DeserializeStateSet(formatter);
                    } else {
                        formatter.SkipElement();
                    }

                    if (!formatter.TryEndElement())
                        Throw(FormatException("Expecting end element", formatter.GetLocation()));
                    break;
                }

            case Blob::AttributeValue:
            case Blob::CharacterData:
                Throw(FormatException("Unexpected element", formatter.GetLocation()));
                break;

            case Blob::EndElement:
            case Blob::None:
                return;
            default:
                assert(0);
            }
        }
    }

    RawMaterial::~RawMaterial() {}

    void RawMaterial::Serialize(OutputStreamFormatter& formatter) const
    {
        if (!_techniqueConfig.empty())
            formatter.WriteAttribute(u("TechniqueConfig"), Conversion::Convert<std::basic_string<utf8>>(_techniqueConfig));

        if (!_inherit.empty()) {
            auto ele = formatter.BeginElement(u("Inherit"));
            for (auto i=_inherit.cbegin(); i!=_inherit.cend(); ++i) {
                auto str = Conversion::Convert<std::basic_string<utf8>>(*i);
                formatter.WriteAttribute(
                    AsPointer(str.cbegin()), AsPointer(str.cend()),
                    (const utf8*)nullptr, (const utf8*)nullptr);
            }
            formatter.EndElement(ele);
        }

        if (_matParamBox.GetCount() > 0) {
            auto ele = formatter.BeginElement(u("ShaderParams"));
            _matParamBox.Serialize<utf8>(formatter);
            formatter.EndElement(ele);
        }

        if (_constants.GetCount() > 0) {
            auto ele = formatter.BeginElement(u("Constants"));
            _constants.Serialize<utf8>(formatter);
            formatter.EndElement(ele);
        }

        if (_resourceBindings.GetCount() > 0) {
            auto ele = formatter.BeginElement(u("ResourceBindings"));
            _resourceBindings.Serialize<utf8>(formatter);
            formatter.EndElement(ele);
        }

        if (HasSomethingToSerialize(_stateSet)) {
            auto ele = formatter.BeginElement(u("States"));
            SerializeStateSet(formatter, _stateSet);
            formatter.EndElement(ele);
        }
    }

    void RawMaterial::MergeInto(Techniques::Material& dest) const
    {
        dest._matParams.MergeIn(_matParamBox);
        dest._stateSet = Merge(dest._stateSet, _stateSet);
        dest._constants.MergeIn(_constants);

		// Resolve all of the directory names here, as we write into the Techniques::Material
		for (const auto&b:_resourceBindings) {
			auto unresolvedName = b.ValueAsString();
			if (!unresolvedName.empty()) {
				char resolvedName[MaxPath];
				_searchRules.ResolveFile(resolvedName, unresolvedName);
				dest._bindings.SetParameter(b.Name(), MakeStringSection(resolvedName));
			} else {
				dest._bindings.SetParameter(b.Name(), MakeStringSection(unresolvedName));
			}
		}

        if (!_techniqueConfig.empty()) {
            // Shader names in "ResolvedMaterial" are kept very short. We
            // want the material object to be fairly light-weight. 
            if (_techniqueConfig.size() > (dimof(dest._techniqueConfig)-1))
                Throw(::Exceptions::BasicLabel("Shader name is too long during resolve"));

            XlCopyString(dest._techniqueConfig, _techniqueConfig);
        }
    }

	void RawMaterial::MergeInto(RawMaterial& dest) const
	{
		dest._matParamBox.MergeIn(_matParamBox);
        dest._stateSet = Merge(dest._stateSet, _stateSet);
        dest._constants.MergeIn(_constants);
        dest._resourceBindings.MergeIn(_resourceBindings);
        if (!_techniqueConfig.empty())
            dest._techniqueConfig = _techniqueConfig;
	}

	void ResolveMaterialFilename(
        ::Assets::ResChar resolvedFile[], unsigned resolvedFileCount,
        const ::Assets::DirectorySearchRules& searchRules, StringSection<char> baseMatName)
    {
		auto splitName = MakeFileNameSplitter(baseMatName);
        searchRules.ResolveFile(resolvedFile, resolvedFileCount, splitName.AllExceptParameters());
		XlCatString(resolvedFile, resolvedFileCount, splitName.ParametersWithDivider());
    }

    auto RawMaterial::ResolveInherited(
        const ::Assets::DirectorySearchRules& searchRules) const -> std::vector<AssetName>
    {
        std::vector<AssetName> result;

        for (auto i=_inherit.cbegin(); i!=_inherit.cend(); ++i) {
            auto name = *i;

            auto* colon = XlFindCharReverse(name.c_str(), ':');
            if (colon) {
                ::Assets::ResChar resolvedFile[MaxPath];
                XlCopyNString(resolvedFile, name.c_str(), colon-name.c_str());
                ResolveMaterialFilename(resolvedFile, dimof(resolvedFile), searchRules, resolvedFile);
                
                StringMeld<MaxPath, ::Assets::ResChar> finalRawMatName;
                finalRawMatName << resolvedFile << colon;
                result.push_back((AssetName)finalRawMatName);
            } else {
                result.push_back(name);
            }
        }

        return result;
    }

	RawMatConfigurations::RawMatConfigurations(
		const ::Assets::Blob& blob,
		const ::Assets::DepValPtr& depVal,
		StringSection<::Assets::ResChar>)
    {
            //  Get associated "raw" material information. This is should contain the material information attached
            //  to the geometry export (eg, .dae file).

        if (!blob || blob->size() == 0)
            Throw(::Exceptions::BasicLabel("Missing or empty file"));

        InputStreamFormatter<utf8> formatter(
            MemoryMappedInputStream(MakeIteratorRange(*blob)));
        Document<decltype(formatter)> doc(formatter);
            
        for (auto config=doc.FirstChild(); config; config=config.NextSibling()) {
            auto name = config.Name();
            if (name.IsEmpty()) continue;
            _configurations.push_back(name.AsString());
        }

        _validationCallback = depVal;
    }

    static void AddDep(
        std::vector<::Assets::DependentFileState>& deps,
        StringSection<::Assets::ResChar> newDep)
    {
            // we need to call "GetDependentFileState" first, because this can change the
            // format of the filename. String compares alone aren't working well for us here
        auto depState = ::Assets::IntermediateAssets::Store::GetDependentFileState(newDep);
        auto existing = std::find_if(
            deps.cbegin(), deps.cend(),
            [&](const ::Assets::DependentFileState& test) { return test._filename == depState._filename; });
        if (existing == deps.cend())
            deps.push_back(depState);
    }

	static bool IsMaterialFile(StringSection<::Assets::ResChar> extension) { return XlEqStringI(extension, "material"); }

	void RawMaterial::ConstructToFuture(
		::Assets::AssetFuture<RawMaterial>& future,
		StringSection<::Assets::ResChar> initializer)
	{
		// If we're loading from a .material file, then just go head and use the
		// default asset construction
		// Otherwise, we need to invoke a compile and load of a ConfigFileContainer
		auto splitName = MakeFileNameSplitter(initializer);
		if (IsMaterialFile(splitName.Extension())) {
			AutoConstructToFutureDirect(future, initializer);
			return;
		}

		// 
		auto containerInitializer = splitName.AllExceptParameters();
		auto containerFuture = std::make_shared<::Assets::AssetFuture<::Assets::ConfigFileContainer<>>>(containerInitializer.AsString());
		::Assets::DefaultCompilerConstruction(
			*containerFuture, 
			&containerInitializer, 1,
			s_MaterialCompileProcessType);

		std::string containerInitializerString = containerInitializer.AsString();
		std::string section = splitName.Parameters().AsString();
		future.SetPollingFunction(
			[containerFuture, section, containerInitializerString](::Assets::AssetFuture<RawMaterial>& thatFuture) -> bool {

			auto containerActual = containerFuture->TryActualize();
			if (!containerActual) {
				auto containerState = containerFuture->GetAssetState();
				if (containerState == ::Assets::AssetState::Invalid) {
					thatFuture.SetInvalidAsset(containerFuture->GetDependencyValidation(), nullptr);
					return false;
				}
				return true;
			}

			auto fmttr = containerActual->GetFormatter(MakeStringSection(section).Cast<utf8>());
			auto newShader = std::make_shared<RawMaterial>(
				fmttr, 
				::Assets::DefaultDirectorySearchRules(containerInitializerString),
				containerActual->GetDependencyValidation());
			thatFuture.SetAsset(std::move(newShader), {});
			return false;
		});

	}

	void MergeIn_Stall(
		Techniques::Material& result,
		StringSection<> sourceMaterialName,
        const ::Assets::DirectorySearchRules& searchRules,
        std::vector<::Assets::DependentFileState>& deps)
    {

            // resolve all of the inheritance options and generate a final 
            // ResolvedMaterial object. We need to start at the bottom of the
            // inheritance tree, and merge in new parameters as we come across them.

			// we still need to add a dependency, even if it's a missing file
		AddDep(deps, MakeFileNameSplitter(sourceMaterialName).AllExceptParameters());

		auto dependencyMat = ::Assets::MakeAsset<RawMaterial>(sourceMaterialName);
		auto state = dependencyMat->StallWhilePending();
		if (state == ::Assets::AssetState::Ready) {
			auto source = dependencyMat->Actualize();

			auto childSearchRules = source->GetDirectorySearchRules();
			childSearchRules.Merge(searchRules);

			for (const auto& child: source->ResolveInherited(searchRules))
				MergeIn_Stall(result, child, childSearchRules, deps);

			source->MergeInto(result);
		}
    }

	void MergeIn_Stall(
		RenderCore::Techniques::Material& result,
		const RenderCore::Assets::RawMaterial& src,
		const ::Assets::DirectorySearchRules& searchRules)
	{
		auto childSearchRules = searchRules;
		childSearchRules.Merge(src.GetDirectorySearchRules());

		for (const auto& child : src.ResolveInherited(childSearchRules)) {
			auto dependencyMat = ::Assets::MakeAsset<RenderCore::Assets::RawMaterial>(child);
			auto state = dependencyMat->StallWhilePending();
			if (state == ::Assets::AssetState::Ready) {
				MergeIn_Stall(result, *dependencyMat->Actualize(), childSearchRules);
			}
		}

		src.MergeInto(result);
	}


}}

