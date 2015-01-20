// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Browser.h"
#include "../Font.h"

#include "../../RenderCore/Assets/ModelSimple.h"
#include "../../RenderCore/Metal/State.h"
#include "../../RenderCore/RenderUtils.h"
#include "../../RenderCore/Assets/SharedStateSet.h"
#include "../../RenderCore/Assets/IModelFormat.h"
#include "../../RenderCore/Metal/DeviceContextImpl.h"

#include "../../SceneEngine/SceneEngineUtility.h"
#include "../../SceneEngine/LightDesc.h"
#include "../../SceneEngine/LightingParser.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/SceneParser.h"
#include "../../SceneEngine/Techniques.h"

#include "../../BufferUploads/IBufferUploads.h"
#include "../../Math/Transformations.h"

#include "../../Utility/StringUtils.h"
#include "../../Utility/UTFUtils.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/HeapUtils.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Streams/FileUtils.h"

#include "../../Core/WinAPI/IncludeWindows.h"
#include "../../RenderCore/DX11/Metal/IncludeDX11.h"
#include "../../Utility/WinApI/WinAPIWrapper.h"

namespace RenderCore { namespace Assets {

    class PluggableModelFormat : public IModelFormat
    {
    public:
        virtual std::shared_ptr<ModelScaffold>      CreateModel(const ::Assets::ResChar initializer[]);
        virtual std::shared_ptr<MaterialScaffold>   CreateMaterial(const ::Assets::ResChar initializer[]);
        virtual std::shared_ptr<ModelRenderer>      CreateRenderer(
            ModelScaffold& scaffold, MaterialScaffold& material,
            SharedStateSet& sharedStateSet, unsigned levelOfDetail);

        virtual std::string DefaultMaterialName(const ModelScaffold&);

        PluggableModelFormat();
        ~PluggableModelFormat();

    protected:
        class Plugin
        {
        public:
            std::pair<const char*, const char*> _versionInfo;
            std::string _name;
            std::unique_ptr<IModelFormat> _pluginInterface;

            Plugin() {}
            Plugin(Plugin&& moveFrom)
                : _versionInfo(moveFrom._versionInfo), _name(moveFrom._name), _pluginInterface(std::move(moveFrom._pluginInterface)) {}
            Plugin& operator=(Plugin&& moveFrom)
            {
                _versionInfo = moveFrom._versionInfo; _name = moveFrom._name; _pluginInterface = std::move(moveFrom._pluginInterface);
                return *this;
            }
        private:
            Plugin& operator=(const Plugin&);
            Plugin(const Plugin&);
        };

        std::vector<Plugin> _plugins;

    private:
        PluggableModelFormat& operator=(const PluggableModelFormat&);
        PluggableModelFormat(const PluggableModelFormat&);
    };

    auto PluggableModelFormat::CreateModel(const ::Assets::ResChar initializer[]) -> std::shared_ptr<ModelScaffold>
    {
        for (auto p=_plugins.begin(); p!=_plugins.end(); ++p) {
            auto res = p->_pluginInterface->CreateModel(initializer);
            if (res) return res;
        }
        return nullptr;
    }

    auto PluggableModelFormat::CreateMaterial(const ::Assets::ResChar initializer[]) -> std::shared_ptr<MaterialScaffold>
    {
        for (auto p=_plugins.begin(); p!=_plugins.end(); ++p) {
            auto res = p->_pluginInterface->CreateMaterial(initializer);
            if (res) return res;
        }
        return nullptr;
    }

    auto PluggableModelFormat::CreateRenderer(
        ModelScaffold& scaffold, MaterialScaffold& material,
        SharedStateSet& sharedStateSet, unsigned levelOfDetail) -> std::shared_ptr<ModelRenderer>
    {
        for (auto p=_plugins.begin(); p!=_plugins.end(); ++p) {
            auto res = p->_pluginInterface->CreateRenderer(scaffold, material, sharedStateSet, levelOfDetail);
            if (res) return res;
        }
        return std::make_shared<ModelRenderer>(scaffold, material, sharedStateSet, levelOfDetail);
    }

    std::string PluggableModelFormat::DefaultMaterialName(const ModelScaffold& model)
    {
        for (auto p=_plugins.begin(); p!=_plugins.end(); ++p) {
            auto res = p->_pluginInterface->DefaultMaterialName(model);
            if (!res.empty()) return res;
        }
        return std::string();
    }

    PluggableModelFormat::PluggableModelFormat()
    {
        std::vector<Plugin> plugins;

        const char pluginSearch[] = "../PluginNonDist/*.dll";
        auto files = FindFiles(pluginSearch, FindFilesFilter::File);
        for (auto i=files.cbegin(); i!=files.cend(); ++i) {
            auto lib = (*Windows::Fn_LoadLibrary)(i->c_str());
            if (lib && lib != INVALID_HANDLE_VALUE) {
                const char CreateInterfaceName[] = "?CreateModelFormatInterface@@YA?AV?$unique_ptr@VIModelFormat@Assets@RenderCore@@U?$default_delete@VIModelFormat@Assets@RenderCore@@@std@@@std@@XZ";
                const char GetVersionName[] = "?GetVersionInformation@@YA?AU?$pair@PBDPBD@std@@XZ";

                typedef std::pair<const char*, const char*> (GetVersionFn)();
                typedef std::unique_ptr<RenderCore::Assets::IModelFormat> (CreateInterfaceFn)();
                GetVersionFn* fn0 = (GetVersionFn*)(*Windows::Fn_GetProcAddress)(lib, GetVersionName);
                CreateInterfaceFn* fn1 = (CreateInterfaceFn*)(*Windows::Fn_GetProcAddress)(lib, CreateInterfaceName);

                if (fn0 && fn1) {
                    Plugin newPlugin;
                    newPlugin._pluginInterface = (*fn1)();
                    newPlugin._versionInfo = (*fn0)();
                    newPlugin._name = *i;
                    if (newPlugin._pluginInterface) {
                        plugins.push_back(std::move(newPlugin));
                    }
                }
            }
        }

        _plugins = std::move(plugins);
    }

    PluggableModelFormat::~PluggableModelFormat()
    {}

}}

namespace Overlays
{
    typedef std::basic_string<ucs2> ucs2string;

    static RenderCore::Assets::IModelFormat& GetModelFormat() 
    {
        static RenderCore::Assets::PluggableModelFormat format;
        return format;
    }

    class DirectoryQuery
    {
    public:
        class Entry 
        {
        public:
            ucs2string  _filename;
        };
        std::vector<Entry> _files;
        std::vector<Entry> _directories;

        DirectoryQuery(const char directoryName[]);
        ~DirectoryQuery();
    };

    DirectoryQuery::DirectoryQuery(const char inputQuery[])
    {
            // read the contents of this directory, and fill in the "_entries" vector
        ucs2 queryString[MaxPath];
        utf8_2_ucs2((const utf8*)inputQuery, XlStringLen(inputQuery), queryString, dimof(queryString));

        ucs2 baseDirectory[MaxPath];
        XlDirname(baseDirectory, dimof(baseDirectory), queryString);

        std::vector<Entry> files;
        std::vector<Entry> directories;

            // using Win32 api interface directly
        WIN32_FIND_DATAW findData;
        XlZeroMemory(findData);
        HANDLE findHandle = FindFirstFileW((LPCWSTR)queryString, &findData);
        if (findHandle != INVALID_HANDLE_VALUE) {
            do {
                    // hide things that begin with "." -- hidden files or system directories
                if (findData.cFileName[0] != L'.') {
                    Entry result;
                    result._filename = ucs2string(baseDirectory) + ((ucs2*)findData.cFileName);
                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        directories.push_back(result);
                    } else {
                        files.push_back(result);
                    }
                }
            } while (FindNextFileW(findHandle, &findData));
            FindClose(findHandle);
        }

        _files = std::move(files);
        _directories = std::move(directories);
    }

    DirectoryQuery::~DirectoryQuery() {}
    
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class SharedBrowser::Pimpl
    {
    public:
        std::unique_ptr<DirectoryQuery> _subDirectories;
        std::unique_ptr<DirectoryQuery> _modelFiles;
        std::string     _baseDirectory;
        std::string     _currentDirectory;
        intrusive_ptr<Font> _headingFont;
        unsigned        _itemDimensions;
        std::string     _fileFilter;
        std::string     _headerName;

        Pimpl() {}
    };

    static void Copy2DTexture(
        RenderCore::Metal::DeviceContext* context, const RenderCore::Metal::ShaderResourceView& srv, Float2 screenMins, Float2 screenMaxs, float scrollAreaMin, float scrollAreaMax)
    {
        using namespace RenderCore;
        using namespace RenderCore::Metal;
        
        class Vertex
        {
        public:
            Float2  _position;
            Float2  _texCoord;
        } vertices[] = {
            { Float2(screenMins[0], screenMins[1]), Float2(0.f, 0.f) },
            { Float2(screenMins[0], screenMaxs[1]), Float2(0.f, 1.f) },
            { Float2(screenMaxs[0], screenMins[1]), Float2(1.f, 0.f) },
            { Float2(screenMaxs[0], screenMaxs[1]), Float2(1.f, 1.f) }
        };

        InputElementDesc vertexInputLayout[] = {
            InputElementDesc( "POSITION", 0, NativeFormat::R32G32_FLOAT ),
            InputElementDesc( "TEXCOORD", 0, NativeFormat::R32G32_FLOAT )
        };

        VertexBuffer vertexBuffer(vertices, sizeof(vertices));
        context->Bind(ResourceList<VertexBuffer, 1>(std::make_tuple(std::ref(vertexBuffer))), sizeof(Vertex), 0);

        ShaderProgram& shaderProgram = ::Assets::GetAssetDep<ShaderProgram>(
            "game/xleres/basic2D.vsh:P2T:" VS_DefShaderModel, 
            "game/xleres/basic.psh:copy_point_scrolllimit:" PS_DefShaderModel);
        BoundInputLayout boundVertexInputLayout(std::make_pair(vertexInputLayout, dimof(vertexInputLayout)), shaderProgram);
        context->Bind(boundVertexInputLayout);
        context->Bind(shaderProgram);

        ViewportDesc viewport(*context);
        float constants[] = { 1.f / viewport.Width, 1.f / viewport.Height, 0.f, 0.f };
        float scrollConstants[] = { scrollAreaMin, scrollAreaMax, 0.f, 0.f };
        ConstantBuffer reciprocalViewportDimensions(constants, sizeof(constants));
        ConstantBuffer scrollConstantsBuffer(scrollConstants, sizeof(scrollConstants));
        const ShaderResourceView* resources[] = { &srv };
        const ConstantBuffer* cnsts[] = { &reciprocalViewportDimensions, &scrollConstantsBuffer };
        BoundUniforms boundLayout(shaderProgram);
        boundLayout.BindConstantBuffer(Hash64("ReciprocalViewportDimensions"), 0, 1);
        boundLayout.BindConstantBuffer(Hash64("ScrollConstants"), 1, 1);
        boundLayout.BindShaderResource(Hash64("DiffuseTexture"), 0, 1);
        boundLayout.Apply(*context, UniformsStream(), UniformsStream(nullptr, cnsts, dimof(cnsts), resources, dimof(resources)));

        context->Bind(BlendState(BlendOp::Add, Blend::SrcAlpha, Blend::InvSrcAlpha));
        context->Bind(Topology::TriangleStrip);
        context->Draw(dimof(vertices));

        ID3D::ShaderResourceView* nullsrv[] = { nullptr };
        context->GetUnderlying()->PSSetShaderResources(0, dimof(nullsrv), nullsrv);
    }

    class ButtonFormatting
    {
    public:
        ColorB  _background;
        ColorB  _foreground;
        ButtonFormatting(ColorB background, ColorB foreground) : _background(background), _foreground(foreground) {}
    };

    static void DrawButtonBasic(IOverlayContext* context, const Rect& rect, const char label[], ButtonFormatting formatting)
    {
        DrawRectangle(context, rect, formatting._background);
        DrawRectangleOutline(context, rect, 0.f, formatting._foreground);
        // DrawText(context, rect, 0.f, nullptr, formatting._foreground, manipulatorName);
        context->DrawText(
            std::make_tuple(Float3(float(rect._topLeft[0]), float(rect._topLeft[1]), 0.f), Float3(float(rect._bottomRight[0]), float(rect._bottomRight[1]), 0.f)),
            1.f, nullptr, formatting._foreground, TextAlignment::Center, label, nullptr);
    }

    template<typename T> inline const T& FormatButton(InterfaceState& interfaceState, InteractableId id, const T& normalState, const T& mouseOverState, const T& pressedState)
    {
        if (interfaceState.HasMouseOver(id))
            return interfaceState.IsMouseButtonHeld(0)?pressedState:mouseOverState;
        return normalState;
    }

    static const auto Id_TotalRect = InteractableId_Make("ModelBrowser");
    static const auto Id_Directories = InteractableId_Make("BrowserDirectories");
    static const auto Id_BackDirectory = InteractableId_Make("BrowserBackDirectory");
    static const auto Id_MainSurface = InteractableId_Make("BrowserMainSurface");
    static const auto Id_MainScroller = InteractableId_Make("BrowserScroller");

    void    SharedBrowser::Render(IOverlayContext* context, Layout& layout, Interactables&interactables, InterfaceState& interfaceState)
    {
        if (!_pimpl->_subDirectories) {
            _pimpl->_subDirectories = std::make_unique<DirectoryQuery>((_pimpl->_currentDirectory + "\\*.*").c_str());
        }

        if (!_pimpl->_modelFiles) {
            _pimpl->_modelFiles = std::make_unique<DirectoryQuery>((_pimpl->_currentDirectory + "\\" + _pimpl->_fileFilter).c_str());
        }

        static float desiredWidthPercentage = 50.f/100.f;
        static ColorB backgroundRectangleColour(64, 96, 64, 127);
        static ColorB backgroundOutlineColour(192, 192, 192, 0xff);
        static ColorB headerColourNormal(192, 192, 192, 0xff);
        static ColorB headerColourHighlight(0xff, 0xff, 0xff, 0xff);

        static ButtonFormatting buttonNormalState(ColorB(127, 192, 127, 64), ColorB(164, 192, 164, 255));
        static ButtonFormatting buttonMouseOverState(ColorB(127, 192, 127, 64), ColorB(255, 255, 255, 160));
        static ButtonFormatting buttonPressedState(ColorB(127, 192, 127, 64), ColorB(255, 255, 255, 96));

        Layout toolBoxLayout(layout.AllocateFullHeightFraction(desiredWidthPercentage));

            // Header at the top
        {
            DrawRectangle(context, toolBoxLayout.GetMaximumSize(), backgroundRectangleColour);
            DrawRectangleOutline(context, Rect(toolBoxLayout.GetMaximumSize()._topLeft + Coord2(2,2), toolBoxLayout.GetMaximumSize()._bottomRight - Coord2(2,2)), 0.f, backgroundOutlineColour);
            interactables.Register(Interactables::Widget(toolBoxLayout.GetMaximumSize(), Id_TotalRect));

            const auto headingRect = toolBoxLayout.AllocateFullWidth(25);
            TextStyle font(*_pimpl->_headingFont);
            context->DrawText(
                std::make_tuple(Float3(float(headingRect._topLeft[0]), float(headingRect._topLeft[1]), 0.f), Float3(float(headingRect._bottomRight[0]), float(headingRect._bottomRight[1]), 0.f)),
                1.f, &font, interfaceState.HasMouseOver(Id_TotalRect)?headerColourHighlight:headerColourNormal, TextAlignment::Center, 
                    _pimpl->_headerName.c_str(), nullptr);
        }

            //  Write the current directory name
        unsigned textHeight = 8 + (unsigned)context->TextHeight();
        auto curDirRect = toolBoxLayout.AllocateFullWidth(textHeight);
        context->DrawText(
            std::make_tuple(Float3(float(curDirRect._topLeft[0]), float(curDirRect._topLeft[1]), 0.f), Float3(float(curDirRect._bottomRight[0]), float(curDirRect._bottomRight[1]), 0.f)),
            1.f, nullptr, headerColourNormal, TextAlignment::Center, _pimpl->_currentDirectory.c_str(), nullptr);

        {
            auto border = toolBoxLayout.AllocateFullWidth(2); // small border to reset current line
            context->DrawLine(
                ProjectionMode::P2D, 
                Float3(float(border._topLeft[0] + 2), float(border._topLeft[1]), 0.f), ColorB(0xffffffff),
                Float3(float(border._bottomRight[0] - 2), float(border._topLeft[1]), 0.f), ColorB(0xffffffff));
        }

            // Next should be a list of subdirectories to filter through
        {
            unsigned directoryIndex = 0;
            for (auto i=_pimpl->_subDirectories->_directories.cbegin(); i!=_pimpl->_subDirectories->_directories.cend(); ++i, ++directoryIndex) {
                char utf8Filename[MaxPath], baseName[MaxPath];
                ucs2_2_utf8(AsPointer(i->_filename.cbegin()), i->_filename.size(), (utf8*)utf8Filename, dimof(utf8Filename));
                XlBasename(baseName, dimof(baseName), utf8Filename);
                unsigned textWidth = 20 + (unsigned)context->StringWidth(1.f, nullptr, baseName, nullptr);
                auto directoryRect = toolBoxLayout.Allocate(Coord2(textWidth, textHeight));
                DrawButtonBasic(
                    context, directoryRect, baseName, 
                    FormatButton(interfaceState, Id_Directories+directoryIndex, buttonNormalState, buttonMouseOverState, buttonPressedState));
                interactables.Register(Interactables::Widget(directoryRect, Id_Directories+directoryIndex));
            }

            const char back[] = "<up>";
            unsigned textWidth = 20 + (unsigned)context->StringWidth(1.f, nullptr, back, nullptr);
            auto directoryRect = toolBoxLayout.Allocate(Coord2(textWidth, textHeight));
            DrawButtonBasic(
                context, directoryRect, back, 
                FormatButton(interfaceState, Id_BackDirectory, buttonNormalState, buttonMouseOverState, buttonPressedState));
            interactables.Register(Interactables::Widget(directoryRect, Id_BackDirectory));
        }

        {
            auto border = toolBoxLayout.AllocateFullWidth(2); // small border to reset current line
            context->DrawLine(
                ProjectionMode::P2D, 
                Float3(float(border._topLeft[0] + 2), float(border._topLeft[1]), 0.f), ColorB(0xffffffff),
                Float3(float(border._bottomRight[0] - 2), float(border._topLeft[1]), 0.f), ColorB(0xffffffff));
        }

            // Finally draw the models in this directory

        context->ReleaseState();        // (drawing directly to the device context -- so we must get the overlay context to release the state)

        auto* devContext = context->GetDeviceContext();
        SceneEngine::SavedTargets oldTargets(devContext);

        std::vector<std::pair<std::string, Rect>> labels;

        Layout browserLayout(toolBoxLayout.AllocateFullHeightFraction(1.f));
        interactables.Register(Interactables::Widget(browserLayout.GetMaximumSize(), Id_MainSurface));
        
        unsigned itemsPerRow = (browserLayout.GetMaximumSize().Width() - 2 * browserLayout._paddingInternalBorder + browserLayout._paddingBetweenAllocations) / (_pimpl->_itemDimensions + browserLayout._paddingBetweenAllocations);
        unsigned rowsRequired = (_pimpl->_modelFiles->_files.size() + itemsPerRow - 1) / itemsPerRow;
        unsigned surfaceMaxSize = rowsRequired * (_pimpl->_itemDimensions + browserLayout._paddingBetweenAllocations);

        auto scrollBarRect = browserLayout.GetMaximumSize();
        const unsigned thumbWidth = 32;
        scrollBarRect._topLeft[0] = std::max(scrollBarRect._topLeft[0], scrollBarRect._bottomRight[0] - Coord(thumbWidth));
        ScrollBar::Coordinates scrollCoordinates(scrollBarRect, 0.f, float(surfaceMaxSize), float(browserLayout.GetMaximumSize().Height()));
        unsigned itemScrollOffset = unsigned(_mainScrollBar.CalculateCurrentOffset(scrollCoordinates));

            // let's render each cgf file to an off-screen buffer, and then copy the results to the main output
        for (auto i=_pimpl->_modelFiles->_files.cbegin(); i!=_pimpl->_modelFiles->_files.cend(); ++i) {

                // filter out some items based on the filename
            if (!Filter(i->_filename)) {
                continue;
            }

                // Make sure have ModelScaffold, MaterialScaffold and ModelRenderer for this object
            auto outputRect = browserLayout.Allocate(Coord2(_pimpl->_itemDimensions, _pimpl->_itemDimensions));
            if (outputRect._bottomRight[1] < Coord(browserLayout.GetMaximumSize()._topLeft[1] + browserLayout._paddingInternalBorder + itemScrollOffset))
                continue;
            outputRect._topLeft[1] -= itemScrollOffset;
            outputRect._bottomRight[1] -= itemScrollOffset;
            if (!outputRect.Width() || !outputRect.Height() || outputRect._topLeft[1] > toolBoxLayout.GetMaximumSize()._bottomRight[1])
                break;

            TRY {

                utf8 utf8Filename[MaxPath];
                ucs2_2_utf8(AsPointer(i->_filename.cbegin()), i->_filename.size(), utf8Filename, dimof(utf8Filename));
                auto srv = GetSRV(devContext, i->_filename);

                    // return to the main render target viewport & viewport -- and copy the offscreen image we've just rendered
                oldTargets.ResetToOldTargets(devContext);
                Copy2DTexture(devContext, *srv.first, 
                    Float2(float(outputRect._topLeft[0]), float(outputRect._topLeft[1])), 
                    Float2(float(outputRect._bottomRight[0]), float(outputRect._bottomRight[1])),
                    float(browserLayout.GetMaximumSize()._topLeft[1]), float(browserLayout.GetMaximumSize()._bottomRight[1]));

                interactables.Register(Interactables::Widget(outputRect, srv.second));

                char baseName[MaxPath];
                XlBasename(baseName, dimof(baseName), (const char*)utf8Filename);
                labels.push_back(std::make_pair(std::string(baseName), outputRect));

            } CATCH(const ::Assets::Exceptions::InvalidResource& e) {
                labels.push_back(std::make_pair(std::string("Invalid res: ") + e.ResourceId(), outputRect));
            } CATCH(const ::Assets::Exceptions::PendingResource&) {
                labels.push_back(std::make_pair(std::string("Pending!"), outputRect));
            } CATCH(...) {
                labels.push_back(std::make_pair(std::string("Unknown exception"), outputRect));
            } CATCH_END

        }

        context->CaptureState();

        for (auto i=labels.cbegin(); i!=labels.cend(); ++i) {
            auto outputRect = i->second;
            DrawRectangleOutline(context, Rect(outputRect._topLeft + Coord2(2,2), outputRect._bottomRight - Coord2(1,1)), 0.f, backgroundOutlineColour);
            Rect labelRect(Coord2(outputRect._topLeft[0], outputRect._bottomRight[1]-20), outputRect._bottomRight);

            context->DrawText(
                std::make_tuple(Float3(float(labelRect._topLeft[0]), float(labelRect._topLeft[1]), 0.f), Float3(float(labelRect._bottomRight[0]), float(labelRect._bottomRight[1]), 0.f)),
                1.f, nullptr, ColorB(0xffffffff), TextAlignment::Center, i->first.c_str(), nullptr);
        }

            // draw the scroll bar over the top on the right size
        if (!scrollCoordinates.Collapse()) {
            auto thumbRect = scrollCoordinates.Thumb(float(itemScrollOffset));
            DrawRectangleOutline(context, thumbRect, 0.f, backgroundOutlineColour);
        }
    }

    static const std::string Slashes("/\\");

    bool    SharedBrowser::ProcessInput(InterfaceState& interfaceState, const InputSnapshot& input)
    {
        if (input._wheelDelta && interfaceState.HasMouseOver(Id_MainSurface)) {
            _mainScrollBar.ProcessDelta(float(-input._wheelDelta));
            return true;
        }

        unsigned subDirCount = _pimpl->_subDirectories ? _pimpl->_subDirectories->_directories.size() : 0;
        if (input.IsPress_LButton() || input.IsRelease_LButton()) {

                //  look through the entire mouse over stack to look for 
                //  buttons that are relevant to us
            auto stack = interfaceState.GetMouseOverStack();
            for (auto i=stack.cbegin(); i!=stack.cend(); ++i) {
                std::string newDirectory;
                bool consume = false;
                if (i->_id >= Id_Directories && i->_id < (Id_Directories + subDirCount)) {
                        // change directory to one of the subdirectories
                    if (input.IsRelease_LButton()) {
                        auto newDir = _pimpl->_subDirectories->_directories[unsigned(i->_id - Id_Directories)]._filename;
                        utf8 buffer[MaxPath];
                        ucs2_2_utf8(AsPointer(newDir.cbegin()), newDir.size(), buffer, dimof(buffer));
                        newDirectory = (const char*)buffer;
                    }
                    consume = true;
                } else if (i->_id == Id_BackDirectory) {
                        // this is "<up>... we need to go up one directory
                    if (input.IsRelease_LButton()) {
                        auto lastSlash = _pimpl->_currentDirectory.find_last_of(Slashes);
                        while (_pimpl->_currentDirectory.find_last_not_of(Slashes, lastSlash) == std::string::npos) {
                            if (lastSlash == 0 || lastSlash == std::string::npos) break;
                            lastSlash = _pimpl->_currentDirectory.find_last_of(Slashes, lastSlash-1);
                        }

                        if (lastSlash != std::string::npos) {
                            newDirectory = _pimpl->_currentDirectory.substr(0, lastSlash);
                        }
                    }
                    consume = true;
                }

                if (!newDirectory.empty()) {
                    _pimpl->_currentDirectory = newDirectory;
                    _pimpl->_subDirectories.reset();
                    _pimpl->_modelFiles.reset();
                }
                if (consume)
                    return true;
            }

        }


        return false;
    }

    SharedBrowser::SharedBrowser(const char baseDirectory[], const std::string& headerName, unsigned itemDimensions, const std::string& fileFilter)
        : _mainScrollBar(Id_MainScroller)
    {
        auto pimpl = std::make_unique<Pimpl>();
        pimpl->_baseDirectory = pimpl->_currentDirectory = baseDirectory;
        pimpl->_headingFont = GetX2Font("Raleway", 20);
        pimpl->_itemDimensions = itemDimensions;
        pimpl->_fileFilter = fileFilter;
        pimpl->_headerName = headerName;
        _pimpl = std::move(pimpl);
    }
    
    SharedBrowser::~SharedBrowser()
    {}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    using RenderCore::Assets::Simple::ModelScaffold;
    using RenderCore::Assets::Simple::MaterialScaffold;
    using RenderCore::Assets::Simple::ModelRenderer;
    using RenderCore::Assets::SharedStateSet;

    typedef std::pair<Float3, Float3> BoundingBox;

    class ModelBrowser::Pimpl
    {
    public:
        std::map<uint64, BoundingBox> _boundingBoxes;
        
        LRUCache<ModelScaffold>     _modelScaffolds;
        LRUCache<MaterialScaffold>  _materialScaffolds;
        LRUCache<ModelRenderer>     _modelRenderers;
        SharedStateSet              _sharedStateSet;

        RenderCore::Metal::RenderTargetView     _rtv;
        RenderCore::Metal::DepthStencilView     _dsv;
        RenderCore::Metal::ShaderResourceView   _srv;

        Pimpl();
    };

    ModelBrowser::Pimpl::Pimpl()
    : _modelScaffolds(2000)
    , _materialScaffolds(2000)
    , _modelRenderers(50)
    {
    }

    class ModelSceneParser : public SceneEngine::ISceneParser
    {
    public:
        RenderCore::CameraDesc  GetCameraDesc() const
        {
            const float border = 0.0f;
            float maxHalfDimension = .5f * std::max(_boundingBox.second[1] - _boundingBox.first[1], _boundingBox.second[2] - _boundingBox.first[2]);
            RenderCore::CameraDesc result;
            result._verticalFieldOfView = Deg2Rad(60.f);
            Float3 position = .5f * (_boundingBox.first + _boundingBox.second);
            position[0] = _boundingBox.first[0] - (maxHalfDimension * (1.f + border)) / XlTan(0.5f * result._verticalFieldOfView);
            result._cameraToWorld = Float4x4(
                0.f, 0.f, -1.f, position[0],
                1.f, 0.f,  0.f, position[1],
                0.f, 1.f,  0.f, position[2],
                0.f, 0.f, 0.f, 1.f);
            Combine_InPlace(result._cameraToWorld, position);
            result._nearClip = 0.01f;
            result._farClip = 1000.f;
            result._temporaryMatrix = Identity<Float4x4>();
            return result;
        }

        void ExecuteScene(  RenderCore::Metal::DeviceContext* context, 
                            SceneEngine::LightingParserContext& parserContext, 
                            const SceneEngine::SceneParseSettings& parseSettings,
                            unsigned techniqueIndex) const 
        {
            if (    parseSettings._batchFilter == SceneEngine::SceneParseSettings::BatchFilter::Depth
                ||  parseSettings._batchFilter == SceneEngine::SceneParseSettings::BatchFilter::General) {
                _model->Render(context, parserContext, techniqueIndex, *_sharedStateSet, Identity<Float4x4>(), 0);
            }
        }

        void ExecuteShadowScene(    RenderCore::Metal::DeviceContext* context, 
                                    SceneEngine::LightingParserContext& parserContext, 
                                    const SceneEngine::SceneParseSettings& parseSettings,
                                    unsigned index, unsigned techniqueIndex) const
        {
            _model->Render(context, parserContext, techniqueIndex, *_sharedStateSet, Identity<Float4x4>(), 0);
        }

        unsigned GetShadowFrustumCount() const { return 0; }
        const SceneEngine::ShadowFrustumDesc&   GetShadowFrustumDesc(unsigned index) const { return *(const SceneEngine::ShadowFrustumDesc*)nullptr; }
        

        unsigned                        GetLightCount() const { return 0; }
        const SceneEngine::LightDesc&   GetLightDesc(unsigned index) const
        {
            static SceneEngine::LightDesc light;
            light._isPointLight = light._isDynamicLight = false;
            light._lightColour = Float3(5.f, 5.f, 5.f);
            light._negativeLightDirection = Float3(0.f, 0.f, 1.f);
            light._radius = 1000.f;
            light._shadowFrustumIndex = ~unsigned(0x0);
            return light;
        }

        SceneEngine::GlobalLightingDesc GetGlobalLightingDesc() const
        {
            SceneEngine::GlobalLightingDesc result;
            result._ambientLight = Float3(0.25f, 0.25f, 0.25f);
            result._skyTexture = nullptr;
            result._doAtmosphereBlur = false;
            result._doOcean = false;
            result._doToneMap = false;
            return result;
        }

        float GetTimeValue() const { return 0.f; }

        ModelSceneParser(ModelRenderer& model, const BoundingBox& boundingBox, const SharedStateSet& sharedStateSet) 
            : _model(&model), _boundingBox(boundingBox), _sharedStateSet(&sharedStateSet) {}
        ~ModelSceneParser() {}

    protected:
        ModelRenderer * _model;
        const SharedStateSet* _sharedStateSet;
        BoundingBox _boundingBox;
    };

    static void RenderModel(RenderCore::Metal::DeviceContext* devContext, ModelRenderer& model, const BoundingBox& boundingBox, const SharedStateSet& sharedStates)
    {
            // Render the given model. Create a minimal version of the full rendering process:
            //      We need to create a LightingParserContext, and ISceneParser as well.
            //      Cameras and lights should be arranged to suit the bounding box given. Let's use 
            //      orthogonal projection to make sure the object is positioned within the output viewport well.
        RenderCore::Metal::ViewportDesc viewport(*devContext);
        SceneEngine::RenderingQualitySettings qualitySettings;
        qualitySettings._width = unsigned(viewport.Width);
        qualitySettings._height = unsigned(viewport.Height);
        qualitySettings._samplingCount = 1; qualitySettings._samplingQuality = 0;

        ModelSceneParser sceneParser(model, boundingBox, sharedStates);
        SceneEngine::TechniqueContext techniqueContext;
        techniqueContext._globalEnvironmentState.SetParameter("SKIP_MATERIAL_DIFFUSE", 1);
        SceneEngine::LightingParserContext lightingParserContext(&sceneParser, techniqueContext);
        SceneEngine::LightingParser_Execute(devContext, lightingParserContext, qualitySettings);
    }

    static const unsigned ModelBrowserItemDimensions = 196;

    std::pair<const RenderCore::Metal::ShaderResourceView*, uint64> ModelBrowser::GetSRV(
        RenderCore::Metal::DeviceContext* devContext, 
        const std::basic_string<ucs2>& filename)
    {
            // draw this item into our offscreen target, and return the SRV
        
        utf8 utf8Filename[MaxPath];
        ucs2_2_utf8(AsPointer(filename.cbegin()), filename.size(), utf8Filename, dimof(utf8Filename));

        auto& modelFormat = GetModelFormat();

        uint64 hashedName = Hash64(AsPointer(filename.cbegin()), AsPointer(filename.cend()));
        auto model = _pimpl->_modelScaffolds.Get(hashedName);
        if (!model) {
            model = modelFormat.CreateModel((const char*)utf8Filename);
            _pimpl->_modelScaffolds.Insert(hashedName, model);
        }
        auto defMatName = modelFormat.DefaultMaterialName(*model);
        uint64 hashedMaterial = Hash64(defMatName);
        auto material = _pimpl->_materialScaffolds.Get(hashedMaterial);
        if (!material) {
            material = modelFormat.CreateMaterial(defMatName.c_str());
            _pimpl->_materialScaffolds.Insert(hashedMaterial, material);
        }

        uint64 hashedModel = uint64(model.get()) | (uint64(material.get()) << 48);
        auto renderer = _pimpl->_modelRenderers.Get(hashedModel);
        if (!renderer) {
            renderer = modelFormat.CreateRenderer(std::ref(*model), std::ref(*material), std::ref(_pimpl->_sharedStateSet), 0);
            _pimpl->_modelRenderers.Insert(hashedModel, renderer);
        }

            // cache the bounding box, because it's an expensive operation to recalculate
        BoundingBox boundingBox;
        auto boundingBoxI = _pimpl->_boundingBoxes.find(hashedName);
        if (boundingBoxI== _pimpl->_boundingBoxes.end()) {
            boundingBox = model->GetBoundingBox(0);
            _pimpl->_boundingBoxes.insert(std::make_pair(hashedName, boundingBox));
        } else {
            boundingBox = boundingBoxI->second;
        }

            // draw this object to our off screen buffer
        const unsigned offscreenDims = ModelBrowserItemDimensions;
        devContext->Bind(RenderCore::MakeResourceList(_pimpl->_rtv), &_pimpl->_dsv);
        devContext->Bind(RenderCore::Metal::ViewportDesc(0, 0, float(offscreenDims), float(offscreenDims), 0.f, 1.f));
        devContext->Clear(_pimpl->_rtv, Float4(0.f, 0.f, 0.f, 1.f));
        devContext->Clear(_pimpl->_dsv, 1.f, 0);
        devContext->Bind(RenderCore::Metal::Topology::TriangleList);
        RenderModel(devContext, *renderer, boundingBox, _pimpl->_sharedStateSet);

        return std::make_pair(&_pimpl->_srv, hashedName);   // note, here, the hashedName only considered the model name, not the material name
    }

    auto ModelBrowser::SpecialProcessInput(InterfaceState& interfaceState, const InputSnapshot& input) -> ProcessInputResult
    {
        if (SharedBrowser::ProcessInput(interfaceState, input)) {
            return true;
        }

        if (input.IsRelease_LButton()) {
            auto model = _pimpl->_modelScaffolds.Get(interfaceState.TopMostWidget()._id);
            if (model) {
                return ProcessInputResult(true, model->Filename());
            }
        }

        return false;
    }

    bool ModelBrowser::Filter(const std::basic_string<ucs2>&)
    {
        return true;
    }

    ModelBrowser::ModelBrowser(const char baseDirectory[]) 
        : SharedBrowser(baseDirectory, "Model Browser", ModelBrowserItemDimensions, "*.cgf")
    {
        auto pimpl = std::make_unique<Pimpl>();

        const unsigned offscreenDims = ModelBrowserItemDimensions;
        auto& uploads = *SceneEngine::GetBufferUploads();
        using namespace BufferUploads;
        BufferDesc offscreenDesc;
        offscreenDesc._type = BufferDesc::Type::Texture;
        offscreenDesc._bindFlags = BindFlag::RenderTarget | BindFlag::ShaderResource;
        offscreenDesc._cpuAccess = 0;
        offscreenDesc._gpuAccess = GPUAccess::Read | GPUAccess::Write;
        offscreenDesc._allocationRules = 0;
        offscreenDesc._textureDesc = TextureDesc::Plain2D(offscreenDims, offscreenDims, RenderCore::Metal::NativeFormat::R8G8B8A8_UNORM);
        auto offscreenResource = uploads.Transaction_Immediate(offscreenDesc, nullptr)->AdoptUnderlying();
        offscreenDesc._bindFlags = BindFlag::DepthStencil;
        offscreenDesc._textureDesc = TextureDesc::Plain2D(offscreenDims, offscreenDims, (RenderCore::Metal::NativeFormat::Enum)DXGI_FORMAT_D24_UNORM_S8_UINT);
        auto depthResource = uploads.Transaction_Immediate(offscreenDesc, nullptr)->AdoptUnderlying();
        pimpl->_rtv =RenderCore::Metal::RenderTargetView(offscreenResource.get());
        pimpl->_dsv = RenderCore::Metal::DepthStencilView(depthResource.get());
        pimpl->_srv = RenderCore::Metal::ShaderResourceView(offscreenResource.get());

        _pimpl = std::move(pimpl);
    }

    ModelBrowser::~ModelBrowser() {}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class TextureBrowser::Pimpl
    {
    public:
        LRUCache<RenderCore::Metal::DeferredShaderResource> _resources;
        Pimpl();
    };

    TextureBrowser::Pimpl::Pimpl() : _resources(256) {}

    TextureBrowser::TextureBrowser(const char baseDirectory[]) : SharedBrowser(baseDirectory, "Texture Browser", 196, "*.dds")
    {
        auto pimpl = std::make_unique<Pimpl>();
        _pimpl = std::move(pimpl);
    }

    TextureBrowser::~TextureBrowser() {}

    std::pair<const RenderCore::Metal::ShaderResourceView*, uint64> TextureBrowser::GetSRV(RenderCore::Metal::DeviceContext* devContext, const std::basic_string<ucs2>& filename)
    {
        uint64 hashedName = Hash64(AsPointer(filename.cbegin()), AsPointer(filename.cend()));
        auto res = _pimpl->_resources.Get(hashedName);
        if (!res) {
            utf8 utf8Filename[MaxPath];
            ucs2_2_utf8(AsPointer(filename.cbegin()), filename.size(), utf8Filename, dimof(utf8Filename));
            res = std::make_shared<RenderCore::Metal::DeferredShaderResource>((const char*)utf8Filename);
            _pimpl->_resources.Insert(hashedName, res);
        }

        return std::make_pair(&res->GetShaderResource(), hashedName);
    }

    bool TextureBrowser::Filter(const std::basic_string<ucs2>& filename)
    {
            // we want to exclude items that have "_sp", "_spec" or "_ddn"
        if (filename.find((ucs2*)L"_sp") != std::string::npos) { return false; }
        if (filename.find((ucs2*)L"_spec") != std::string::npos) { return false; }
        if (filename.find((ucs2*)L"_ddn") != std::string::npos) { return false; }
        return true;
    }

}