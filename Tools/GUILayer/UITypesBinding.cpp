// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4512)

#include "UITypesBinding.h"
#include "EngineForward.h"
#include "ExportedNativeTypes.h"
#include "../ToolsRig/ModelVisualisation.h"
#include "../ToolsRig/VisualisationUtils.h"
#include "../../RenderCore/Assets/Material.h"
#include "../../RenderCore/Metal/State.h"
#include "../../Assets/DivergentAsset.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/InvalidAssetManager.h"
#include "../../Assets/ConfigFileContainer.h"
#include "../../RenderCore/Techniques/RenderStateResolver.h"
#include "../../Utility/StringFormat.h"
#include <msclr/auto_gcroot.h>
#include <iomanip>

namespace GUILayer
{

///////////////////////////////////////////////////////////////////////////////////////////////////

    class InvalidatePropertyGrid : public OnChangeCallback
    {
    public:
        void    OnChange();

        InvalidatePropertyGrid(PropertyGrid^ linked);
        ~InvalidatePropertyGrid();
    protected:
        msclr::auto_gcroot<PropertyGrid^> _linked;
    };

    void    InvalidatePropertyGrid::OnChange()
    {
        if (_linked.get()) {
            _linked->Refresh();
        }
    }

    InvalidatePropertyGrid::InvalidatePropertyGrid(PropertyGrid^ linked) : _linked(linked) {}
    InvalidatePropertyGrid::~InvalidatePropertyGrid() {}

    void ModelVisSettings::AttachCallback(PropertyGrid^ callback)
    {
        _object->_changeEvent._callbacks.push_back(
            std::shared_ptr<OnChangeCallback>(new InvalidatePropertyGrid(callback)));
    }

    ModelVisSettings^ ModelVisSettings::CreateDefault()
    {
        auto attached = std::make_shared<ToolsRig::ModelVisSettings>();
        return gcnew ModelVisSettings(std::move(attached));
    }

    void ModelVisSettings::ModelName::set(String^ value)
    {
            //  we need to make a filename relative to the current working
            //  directory
        auto nativeName = clix::marshalString<clix::E_UTF8>(value);
        ::Assets::ResolvedAssetFile resName;
        ::Assets::MakeAssetName(resName, nativeName.c_str());
                
        _object->_modelName = resName._fn;

            // also set the material name (the old material file probably won't match the new model file)
        XlChopExtension(resName._fn);
        XlCatString(resName._fn, dimof(resName._fn), ".material");
        _object->_materialName = resName._fn;

        _object->_pendingCameraAlignToModel = true; 
        _object->_changeEvent.Trigger(); 
    }

    void ModelVisSettings::MaterialName::set(String^ value)
    {
            //  we need to make a filename relative to the current working
            //  directory
        auto nativeName = clix::marshalString<clix::E_UTF8>(value);
        ::Assets::ResolvedAssetFile resName;
        ::Assets::MakeAssetName(resName, nativeName.c_str());
        _object->_materialName = resName._fn;
        _object->_changeEvent.Trigger(); 
    }

    void ModelVisSettings::Supplements::set(String^ value)
    {
        _object->_supplements = clix::marshalString<clix::E_UTF8>(value);
        _object->_changeEvent.Trigger(); 
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    System::String^ VisMouseOver::IntersectionPt::get()
    {
        if (_object->_hasMouseOver) {
            return clix::marshalString<clix::E_UTF8>(
                std::string(StringMeld<64>()
                    << std::setprecision(5)
                    << _object->_intersectionPt[0] << ","
                    << _object->_intersectionPt[1] << ","
                    << _object->_intersectionPt[2]));
        } else {
            return "<<no intersection>>";
        }
    }

    unsigned VisMouseOver::DrawCallIndex::get() 
    { 
        if (_object->_hasMouseOver) {
            return _object->_drawCallIndex;
        } else {
            return ~unsigned(0x0);
        }
    }

    System::String^ VisMouseOver::MaterialName::get() 
    {
        auto fullName = FullMaterialName;
        if (fullName) {
            auto split = fullName->Split(';');
            if (split && split->Length > 0) {
                auto s = split[split->Length-1];
                int index = s->LastIndexOf(':');
                return s->Substring((index>=0) ? (index+1) : 0);
            }
        }
        return "<<no material>>";
    }

    System::String^ VisMouseOver::ModelName::get() 
    {
        return clix::marshalString<clix::E_UTF8>(_modelSettings->_modelName);
    }

    bool VisMouseOver::HasMouseOver::get()
    {
        return _object->_hasMouseOver;
    }

    System::String^ VisMouseOver::FullMaterialName::get()
    {
        if (_object->_hasMouseOver) {
            auto scaffolds = _modelCache->GetScaffolds(_modelSettings->_modelName.c_str(), _modelSettings->_materialName.c_str());
            if (scaffolds._material) {
                TRY {
                    auto matName = scaffolds._material->GetMaterialName(_object->_materialGuid);
                    if (matName) {
                        return clix::marshalString<clix::E_UTF8>(std::string(matName));
                    }
                } CATCH (const ::Assets::Exceptions::PendingAsset&) { return "<<pending>>"; }
                CATCH_END
            }
        }
        return nullptr;
    }

    uint64 VisMouseOver::MaterialBindingGuid::get()
    {
        if (_object->_hasMouseOver) {
            return _object->_materialGuid;
        } else {
            return ~uint64(0x0);
        }
    }

    void VisMouseOver::AttachCallback(PropertyGrid^ callback)
    {
        _object->_changeEvent._callbacks.push_back(
            std::shared_ptr<OnChangeCallback>(new InvalidatePropertyGrid(callback)));
    }

    VisMouseOver::VisMouseOver(
        std::shared_ptr<ToolsRig::VisMouseOver> attached,
        std::shared_ptr<ToolsRig::ModelVisSettings> settings,
        std::shared_ptr<RenderCore::Assets::ModelCache> cache)
    {
        _object = std::move(attached);
        _modelSettings = std::move(settings);
        _modelCache = std::move(cache);
    }

    VisMouseOver::VisMouseOver()
    {
        _object = std::make_shared<ToolsRig::VisMouseOver>();
    }

    VisMouseOver::~VisMouseOver() 
    { 
        _object.reset(); 
        _modelSettings.reset(); 
        _modelCache.reset(); 
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename NameType, typename ValueType>
        NameType PropertyPair<NameType, ValueType>::Name::get() { return _name; }

    template<typename NameType, typename ValueType>
        void PropertyPair<NameType, ValueType>::Name::set(NameType newValue)
    {
        _name = newValue;
        NotifyPropertyChanged("Name");
    }

    template<typename NameType, typename ValueType>
        ValueType PropertyPair<NameType, ValueType>::Value::get() { return _value; } 

    template<typename NameType, typename ValueType>
        void PropertyPair<NameType, ValueType>::Value::set(ValueType newValue)
    {
        _value = newValue;
        NotifyPropertyChanged("Value");
    }

    template<typename NameType, typename ValueType>
        void PropertyPair<NameType, ValueType>::NotifyPropertyChanged(System::String^ propertyName)
    {
        PropertyChanged(this, gcnew PropertyChangedEventArgs(propertyName));
        // _propertyChangedContext->Send(
        //     gcnew System::Threading::SendOrPostCallback(
        //         o => PropertyChanged(this, gcnew PropertyChangedEventArgs(propertyName))
        //     ), nullptr);
    }

    public ref class BindingConv
    {
    public:
        static BindingList<StringStringPair^>^ AsBindingList(const ParameterBox& paramBox);
        static ParameterBox AsParameterBox(BindingList<StringStringPair^>^);
        static ParameterBox AsParameterBox(BindingList<StringIntPair^>^);
    };

    BindingList<StringStringPair^>^ BindingConv::AsBindingList(const ParameterBox& paramBox)
    {
        auto result = gcnew BindingList<StringStringPair^>();
        std::vector<std::pair<const utf8*, std::string>> stringTable;
        BuildStringTable(stringTable, paramBox);

        for (auto i=stringTable.cbegin(); i!=stringTable.cend(); ++i) {
            result->Add(
                gcnew StringStringPair(
                    clix::marshalString<clix::E_UTF8>(i->first),
                    clix::marshalString<clix::E_UTF8>(i->second)));
        }

        return result;
    }

    ParameterBox BindingConv::AsParameterBox(BindingList<StringStringPair^>^ input)
    {
        ParameterBox result;
        for each(auto i in input) {
                //  We get items with null names when they are being added, but
                //  not quite finished yet. We have to ignore in this case.
            if (i->Name && i->Name->Length > 0) {
                result.SetParameter(
                    (const utf8*)clix::marshalString<clix::E_UTF8>(i->Name).c_str(),
                    i->Value ? clix::marshalString<clix::E_UTF8>(i->Value).c_str() : nullptr);
            }
        }
        return result;
    }

    ParameterBox BindingConv::AsParameterBox(BindingList<StringIntPair^>^ input)
    {
        ParameterBox result;
        for each(auto i in input) {
                //  We get items with null names when they are being added, but
                //  not quite finished yet. We have to ignore in this case.
            if (i->Name && i->Name->Length > 0) {
                result.SetParameter(
                    (const utf8*)clix::marshalString<clix::E_UTF8>(i->Name).c_str(),
                    i->Value);
            }
        }
        return result;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    BindingList<StringStringPair^>^ 
        RawMaterial::MaterialParameterBox::get()
    {
        if (!_underlying) { return nullptr; }
        if (!_materialParameterBox) {
            _materialParameterBox = BindingConv::AsBindingList(_underlying->GetAsset()._asset._matParamBox);
            _materialParameterBox->ListChanged += 
                gcnew ListChangedEventHandler(
                    this, &RawMaterial::ParameterBox_Changed);
            _materialParameterBox->AllowNew = true;
            _materialParameterBox->AllowEdit = true;
        }
        return _materialParameterBox;
    }

    BindingList<StringStringPair^>^ RawMaterial::ShaderConstants::get()
    {
        if (!_underlying) { return nullptr; }
        if (!_shaderConstants) {
            _shaderConstants = BindingConv::AsBindingList(_underlying->GetAsset()._asset._constants);
            _shaderConstants->ListChanged += 
                gcnew ListChangedEventHandler(
                    this, &RawMaterial::ParameterBox_Changed);
            _shaderConstants->AllowNew = true;
            _shaderConstants->AllowEdit = true;
        }
        return _shaderConstants;
    }

    BindingList<StringStringPair^>^ RawMaterial::ResourceBindings::get()
    {
        if (!_underlying) { return nullptr; }
        if (!_resourceBindings) {
            _resourceBindings = BindingConv::AsBindingList(_underlying->GetAsset()._asset._resourceBindings);
            _resourceBindings->ListChanged += 
                gcnew ListChangedEventHandler(
                    this, &RawMaterial::ResourceBinding_Changed);
            _resourceBindings->AllowNew = true;
            _resourceBindings->AllowEdit = true;
        }
        return _resourceBindings;
    }

    void RawMaterial::ParameterBox_Changed(System::Object^ obj, ListChangedEventArgs^e)
    {
            //  Commit these changes back to the native object by re-creating the parameter box
            //  Ignore a couple of cases... 
            //      - moving an item is unimportant
            //      - added a new item with a null name (this occurs when the new item
            //          hasn't been fully filled in yet)
            //   Similarly, don't we really need to process a removal of an item with 
            //   an empty name.. but there's no way to detect this case
        if (e->ListChangedType == ListChangedType::ItemMoved) {
            return;
        }

        using Box = BindingList<StringStringPair^>;

        if (e->ListChangedType == ListChangedType::ItemAdded) {
            assert(e->NewIndex < ((Box^)obj)->Count);
            if (!((Box^)obj)[e->NewIndex]->Name || ((Box^)obj)[e->NewIndex]->Name->Length > 0) {
                return;
            }
        }

        if (!!_underlying) {
            if (obj == _materialParameterBox) {
                auto transaction = _underlying->Transaction_Begin("Material parameter");
                if (transaction) {
                    transaction->GetAsset()._asset._matParamBox = BindingConv::AsParameterBox(_materialParameterBox);
                    transaction->Commit();
                }
            } else if (obj == _shaderConstants) {
                auto transaction = _underlying->Transaction_Begin("Material constant");
                if (transaction) {
                    transaction->GetAsset()._asset._constants = BindingConv::AsParameterBox(_shaderConstants);
                    transaction->Commit();
                }
            }
        }
    }

    void RawMaterial::ResourceBinding_Changed(System::Object^ obj, ListChangedEventArgs^ e)
    {
        if (e->ListChangedType == ListChangedType::ItemMoved) {
            return;
        }

        using Box = BindingList<StringStringPair^>;

        if (e->ListChangedType == ListChangedType::ItemAdded) {
            assert(e->NewIndex < ((Box^)obj)->Count);
            if (!((Box^)obj)[e->NewIndex]->Name || ((Box^)obj)[e->NewIndex]->Name->Length > 0) {
                return;
            }
        }

        if (!!_underlying) {
            assert(obj == _resourceBindings);
            auto transaction = _underlying->Transaction_Begin("Resource Binding");
            if (transaction) {
                transaction->GetAsset()._asset._resourceBindings = BindingConv::AsParameterBox(_resourceBindings);
                transaction->Commit();
            }
        }
    }

    System::String^ RawMaterial::BuildInheritanceList()
    {
        if (!!_underlying) {
            auto& asset = _underlying->GetAsset();
            auto searchRules = ::Assets::DefaultDirectorySearchRules(
                MakeStringSection(clix::marshalString<clix::E_UTF8>(Filename)));
            
            System::String^ result = "";
            auto inheritted = asset._asset.ResolveInherited(searchRules);
            for (auto i = inheritted.cbegin(); i != inheritted.cend(); ++i) {
                if (result->Length != 0) result += ";";
                result += clix::marshalString<clix::E_UTF8>(*i);
            }
            return result;
        }
        return nullptr;
    }

    void RawMaterial::Resolve(RenderCore::Assets::ResolvedMaterial& destination)
    {
        if (!!_underlying) {
            auto searchRules = Assets::DefaultDirectorySearchRules(
                MakeStringSection(clix::marshalString<clix::E_UTF8>(Filename)));
            _underlying->GetAsset()._asset.Resolve(destination, searchRules);
        }
    }

    void RawMaterial::AddInheritted(String^ item)
    {
            // we could consider converting the filename "item"
            // into a path relative to the main material file here..?
        auto transaction = _underlying->Transaction_Begin("Add inheritted");
        if (transaction)
            transaction->GetAsset()._asset._inherit.push_back(clix::marshalString<clix::E_UTF8>(item));
    }

    void RawMaterial::RemoveInheritted(String^ item)
    {
        assert(0); // not implemented!
    }

    System::String^ RawMaterial::Filename::get()
    { 
        auto native = MakeFileNameSplitter(clix::marshalString<clix::E_UTF8>(_initializer)).AllExceptParameters();
        return clix::marshalString<clix::E_UTF8>(native);
    }
    System::String^ RawMaterial::Initializer::get() { return _initializer; }

    const RenderCore::Assets::RawMaterial* RawMaterial::GetUnderlying() 
    { 
        return (!!_underlying) ? &_underlying->GetAsset()._asset : nullptr; 
    }

    static RawMaterial::RawMaterial()
    {
        s_table = gcnew Dictionary<String^, WeakReference^>();
    }

    RawMaterial^ RawMaterial::Get(String^ initializer)
    {
        // Creates a raw material for the given initializer, or constructs
        // a new one if one hasn't been created yet.
        // Note -- there's a problem here because different initializers could
        // end up resolving to the same native object. That may not be a problem
        // in all cases... But it could throw off the change tracking.
        System::Diagnostics::Debug::Assert(initializer && initializer->Length > 0);
        
        WeakReference^ ref;
        if (s_table->TryGetValue(initializer, ref)) {
            auto o = ref->Target;
            if (o) return (RawMaterial^)o;
        
            // if the reference expired, we have to remove it -- we will create it again afterwards...
            s_table->Remove(initializer);
        }
        
        auto result = gcnew RawMaterial(initializer);
        s_table->Add(initializer, gcnew WeakReference(result));
        return result;
    }

    RawMaterial^ RawMaterial::CreateUntitled() 
    { 
        static unsigned counter = 0;
        return gcnew RawMaterial("untitled" + (counter++) + ".material");
    }

    RawMaterial::RawMaterial(System::String^ initialiser)
    {
        _initializer = initialiser;
        auto nativeInit = clix::marshalString<clix::E_UTF8>(initialiser);
        _underlying = RenderCore::Assets::RawMaterial::GetDivergentAsset(nativeInit.c_str());
        _renderStateSet = gcnew RenderStateSet(_underlying.GetNativePtr());
    }

    RawMaterial::~RawMaterial()
    {
        delete _renderStateSet;
        _underlying.reset();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    auto RenderStateSet::DoubleSided::get() -> CheckState
    {
        auto& stateSet = _underlying->GetAsset()._asset._stateSet;
        if (stateSet._flag & RenderCore::Techniques::RenderStateSet::Flag::DoubleSided) {
            if (stateSet._doubleSided) return CheckState::Checked;
            else return CheckState::Unchecked;
        }
        return CheckState::Indeterminate;
    }
    
    void RenderStateSet::DoubleSided::set(CheckState checkState)
    {
        auto transaction = _underlying->Transaction_Begin("RenderState");
        auto& stateSet = transaction->GetAsset()._asset._stateSet;
        if (checkState == CheckState::Indeterminate) {
            stateSet._flag &= ~RenderCore::Techniques::RenderStateSet::Flag::DoubleSided;
        } else {
            stateSet._flag |= RenderCore::Techniques::RenderStateSet::Flag::DoubleSided;
            stateSet._doubleSided = (checkState == CheckState::Checked);
        }
        transaction->Commit();
        NotifyPropertyChanged("DoubleSided");
    }

    CheckState RenderStateSet::Wireframe::get()
    {
        auto& stateSet = _underlying->GetAsset()._asset._stateSet;
        if (stateSet._flag & RenderCore::Techniques::RenderStateSet::Flag::Wireframe) {
            if (stateSet._wireframe) return CheckState::Checked;
            else return CheckState::Unchecked;
        }
        return CheckState::Indeterminate;
    }

    void RenderStateSet::Wireframe::set(CheckState checkState)
    {
        auto transaction = _underlying->Transaction_Begin("RenderState");
        auto& stateSet = transaction->GetAsset()._asset._stateSet;
        if (checkState == CheckState::Indeterminate) {
            stateSet._flag &= ~RenderCore::Techniques::RenderStateSet::Flag::Wireframe;
        } else {
            stateSet._flag |= RenderCore::Techniques::RenderStateSet::Flag::Wireframe;
            stateSet._wireframe = (checkState == CheckState::Checked);
        }
        transaction->Commit();
        NotifyPropertyChanged("Wireframe");
    }

    auto RenderStateSet::DeferredBlend::get() -> DeferredBlendState     { return DeferredBlendState::Unset; }
    void RenderStateSet::DeferredBlend::set(DeferredBlendState)         { NotifyPropertyChanged("DeferredBlend"); }

    namespace BlendOp = RenderCore::Metal::BlendOp;
    using namespace RenderCore::Metal::Blend;
    using BlendType = RenderCore::Techniques::RenderStateSet::BlendType;

    class StandardBlendDef
    {
    public:
        StandardBlendModes _standardMode;
        BlendType _blendType;
        RenderCore::Metal::BlendOp::Enum _op;
        RenderCore::Metal::Blend::Enum _src;
        RenderCore::Metal::Blend::Enum _dst;
    };

    static const StandardBlendDef s_standardBlendDefs[] = 
    {
        { StandardBlendModes::NoBlending, BlendType::Basic, BlendOp::NoBlending, One, RenderCore::Metal::Blend::Zero },
        
        { StandardBlendModes::Transparent, BlendType::Basic, BlendOp::Add, SrcAlpha, InvSrcAlpha },
        { StandardBlendModes::TransparentPremultiplied, BlendType::Basic, BlendOp::Add, One, InvSrcAlpha },

        { StandardBlendModes::Add, BlendType::Basic, BlendOp::Add, One, One },
        { StandardBlendModes::AddAlpha, BlendType::Basic, BlendOp::Add, SrcAlpha, One },
        { StandardBlendModes::Subtract, BlendType::Basic, BlendOp::Subtract, One, One },
        { StandardBlendModes::SubtractAlpha, BlendType::Basic, BlendOp::Subtract, SrcAlpha, One },

        { StandardBlendModes::Min, BlendType::Basic, BlendOp::Min, One, One },
        { StandardBlendModes::Max, BlendType::Basic, BlendOp::Max, One, One },

        { StandardBlendModes::OrderedTransparent, BlendType::Ordered, BlendOp::Add, SrcAlpha, InvSrcAlpha },
        { StandardBlendModes::OrderedTransparentPremultiplied, BlendType::Ordered, BlendOp::Add, One, InvSrcAlpha },
        { StandardBlendModes::Decal, BlendType::DeferredDecal, BlendOp::NoBlending, One, RenderCore::Metal::Blend::Zero }
    };

    StandardBlendModes AsStandardBlendMode(
        const RenderCore::Techniques::RenderStateSet& stateSet)
    {
        auto op = stateSet._forwardBlendOp;
        auto src = stateSet._forwardBlendSrc;
        auto dst = stateSet._forwardBlendDst;

        if (!(stateSet._flag & RenderCore::Techniques::RenderStateSet::Flag::ForwardBlend)) {
            if (    stateSet._flag & RenderCore::Techniques::RenderStateSet::Flag::BlendType
                &&  stateSet._blendType == BlendType::DeferredDecal)
                return StandardBlendModes::Decal;
            return StandardBlendModes::Inherit;
        }

        if (op == BlendOp::NoBlending) {
            if (    stateSet._flag & RenderCore::Techniques::RenderStateSet::Flag::BlendType
                &&  stateSet._blendType == BlendType::DeferredDecal)
                    return StandardBlendModes::Decal;
            return StandardBlendModes::NoBlending;
        }

        auto blendType = BlendType::Basic;
        if (stateSet._flag & RenderCore::Techniques::RenderStateSet::Flag::BlendType)
            blendType = stateSet._blendType;

        for (unsigned c=0; c<dimof(s_standardBlendDefs); ++c)
            if (    op == s_standardBlendDefs[c]._op
                &&  src == s_standardBlendDefs[c]._src
                &&  dst == s_standardBlendDefs[c]._dst
                &&  blendType == s_standardBlendDefs[c]._blendType)
                return s_standardBlendDefs[c]._standardMode;

        return StandardBlendModes::Complex;
    }

    auto RenderStateSet::StandardBlendMode::get() -> StandardBlendModes
    {
        const auto& underlying = _underlying->GetAsset();
        return AsStandardBlendMode(underlying._asset._stateSet);
    }
    
    void RenderStateSet::StandardBlendMode::set(StandardBlendModes newMode)
    {
        if (newMode == StandardBlendModes::Complex) return;
        if (newMode == StandardBlendMode) return;

        if (newMode == StandardBlendModes::Inherit) {
            auto transaction = _underlying->Transaction_Begin("RenderState");
            auto& stateSet = transaction->GetAsset()._asset._stateSet;
            stateSet._forwardBlendOp = BlendOp::NoBlending;
            stateSet._forwardBlendSrc = One;
            stateSet._forwardBlendDst = RenderCore::Metal::Blend::Zero;
            stateSet._blendType = RenderCore::Techniques::RenderStateSet::BlendType::Basic;
            stateSet._flag &= ~RenderCore::Techniques::RenderStateSet::Flag::ForwardBlend;
            stateSet._flag &= ~RenderCore::Techniques::RenderStateSet::Flag::BlendType;
            NotifyPropertyChanged("StandardBlendMode");
            transaction->Commit();
            return;
        }

        for (unsigned c=0; c<dimof(s_standardBlendDefs); ++c)
            if (s_standardBlendDefs[c]._standardMode == newMode) {
                auto transaction = _underlying->Transaction_Begin("RenderState");
                auto& stateSet = transaction->GetAsset()._asset._stateSet;

                stateSet._forwardBlendOp = s_standardBlendDefs[c]._op;
                stateSet._forwardBlendSrc = s_standardBlendDefs[c]._src;
                stateSet._forwardBlendDst = s_standardBlendDefs[c]._dst;
                stateSet._flag |= RenderCore::Techniques::RenderStateSet::Flag::ForwardBlend;

                stateSet._blendType = s_standardBlendDefs[c]._blendType;
                if (s_standardBlendDefs[c]._blendType == BlendType::Basic) {
                    stateSet._flag &= ~RenderCore::Techniques::RenderStateSet::Flag::BlendType;
                } else {
                    stateSet._flag |= RenderCore::Techniques::RenderStateSet::Flag::BlendType;
                }

                transaction->Commit();
                NotifyPropertyChanged("StandardBlendMode");
                return;
            }
    }

    RenderStateSet::RenderStateSet(std::shared_ptr<NativeConfig> underlying)
    {
        _underlying = std::move(underlying);
        _propertyChangedContext = System::Threading::SynchronizationContext::Current;
    }

    RenderStateSet::~RenderStateSet()
    {
        _underlying.reset();
    }

    void RenderStateSet::NotifyPropertyChanged(System::String^ propertyName)
    {
            //  This only works correctly in the UI thread. However, given that
            //  this event can be raised by low-level engine code, we might be
            //  in some other thread. We can get around that by using the 
            //  synchronisation functionality in .net to post a message to the
            //  UI thread... That requires creating a delegate type and passing
            //  propertyName to it. It's easy in C#. But it's a little more difficult
            //  in C++/CLI.
        PropertyChanged(this, gcnew PropertyChangedEventArgs(propertyName));
        // _propertyChangedContext->Send(
        //     gcnew System::Threading::SendOrPostCallback(
        //         o => PropertyChanged(this, gcnew PropertyChangedEventArgs(propertyName))
        //     ), nullptr);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static void InvokeChangeEvent(gcroot<InvalidAssetList^> ptr)
    {
        return ptr->RaiseChangeEvent();
    }

    InvalidAssetList::InvalidAssetList()
    {
        _eventId = 0;

            // get the list of assets from the underlying manager
        if (::Assets::Services::GetInvalidAssetMan()) {
            auto& man = *::Assets::Services::GetInvalidAssetMan();
            gcroot<InvalidAssetList^> ptrToThis = this;
            _eventId = man.AddOnChangeEvent(std::bind(InvokeChangeEvent, ptrToThis));
        }
    }

    InvalidAssetList::~InvalidAssetList()
    {
        if (::Assets::Services::GetInvalidAssetMan())
            ::Assets::Services::GetInvalidAssetMan()->RemoveOnChangeEvent(_eventId);
    }

    IEnumerable<Tuple<String^, String^>^>^ InvalidAssetList::AssetList::get() 
    { 
        auto result = gcnew List<Tuple<String^, String^>^>();
        result->Clear();
        if (::Assets::Services::GetInvalidAssetMan()) {
            auto list = ::Assets::Services::GetInvalidAssetMan()->GetAssets();
            for (const auto& i : list) {
                result->Add(gcnew Tuple<String^, String^>(
                    clix::marshalString<clix::E_UTF8>(i._name),
                    clix::marshalString<clix::E_UTF8>(i._errorString)));
            }
        }
        return result;
    }

    void InvalidAssetList::RaiseChangeEvent()
    {
        _onChange();
    }

    bool InvalidAssetList::HasInvalidAssets()
    {
        return ::Assets::Services::GetInvalidAssetMan() ? ::Assets::Services::GetInvalidAssetMan()->HasInvalidAssets() : false;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    template PropertyPair<System::String^, unsigned>;
    template PropertyPair<System::String^, System::String^>;
}

