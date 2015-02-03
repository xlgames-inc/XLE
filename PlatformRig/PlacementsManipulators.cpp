// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PlacementsManipulators.h"
#include "ManipulatorsUtil.h"
#include "TerrainManipulators.h"        // needed for hit tests

#include "../RenderOverlays/DebuggingDisplay.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderOverlays/OverlayContext.h"
#include "../RenderOverlays/Overlays/Browser.h"

#include "../SceneEngine/PlacementsManager.h"
#include "../SceneEngine/Terrain.h"
#include "../SceneEngine/SceneParser.h"
#include "../SceneEngine/Techniques.h"
#include "../SceneEngine/LightingParserContext.h"
#include "../SceneEngine/CommonResources.h"

#include "../Utility/TimeUtils.h"
#include "../Utility/StringFormat.h"
#include "../Math/Transformations.h"
#include "../Math/Geometry.h"

#include "../BufferUploads/IBufferUploads.h"
#include "../SceneEngine/SceneEngineUtility.h"
#include "../SceneEngine/ResourceBox.h"
#include "../Math/ProjectionMath.h"
#include <iomanip>

#include "../Core/WinAPI/IncludeWindows.h"      // *hack* just needed for getting client rect coords!

#include "../BufferUploads/DataPacket.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/DX11/Metal/IncludeDX11.h"
#include "../RenderCore/DX11/Metal/DX11Utils.h"
#include "../SceneEngine/LightingParser.h"

namespace Sample
{
    extern std::shared_ptr<SceneEngine::ITerrainFormat> MainTerrainFormat;
    extern SceneEngine::TerrainCoordinateSystem MainTerrainCoords;
    extern SceneEngine::TerrainConfig MainTerrainConfig;
}

namespace Tools
{
    using namespace RenderOverlays::DebuggingDisplay;

///////////////////////////////////////////////////////////////////////////////////////////////////

    class IPlacementManipManagerInterface
    {
    public:
        virtual std::string GetSelectedModel() const = 0;
        virtual void EnableSelectedModelDisplay(bool newState) = 0;
        virtual void SelectModel(const char newModelName[]) = 0;

        struct Mode { enum Enum { Select, PlaceSingle }; };
        virtual void SwitchToMode(Mode::Enum newMode) = 0;
    };

    class PlacementsWidgets : public IWidget, public IPlacementManipManagerInterface
    {
    public:
        void    Render(         IOverlayContext* context, Layout& layout, 
                                Interactables& interactables, InterfaceState& interfaceState);
        void    RenderToScene(  RenderCore::Metal::DeviceContext* context, 
                                SceneEngine::LightingParserContext& parserContext);
        bool    ProcessInput(InterfaceState& interfaceState, const InputSnapshot& input);

        PlacementsWidgets(  std::shared_ptr<SceneEngine::PlacementsEditor> editor, 
                            std::shared_ptr<HitTestResolver> hitTestResolver);
        ~PlacementsWidgets();

    private:
        typedef Overlays::ModelBrowser ModelBrowser;

        std::shared_ptr<ModelBrowser>       _browser;
        std::shared_ptr<HitTestResolver>    _hitTestResolver;
        std::shared_ptr<SceneEngine::PlacementsEditor> _editor;
        bool            _browserActive;
        std::string     _selectedModel;

        std::vector<std::unique_ptr<IManipulator>> _manipulators;
        unsigned        _activeManipulatorIndex;
        bool            _drawSelectedModel;

            // "IPlacementManipManagerInterface" interface
        virtual std::string GetSelectedModel() const;
        virtual void EnableSelectedModelDisplay(bool newState);
        virtual void SelectModel(const char newModelName[]);
        virtual void SwitchToMode(Mode::Enum newMode);
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    auto HitTestResolver::DoHitTest(Int2 screenCoord) const -> Result
    {
            // currently there's no good way to get the viewport size
            //  we have to do a hack via windows...!
        RECT clientRect; GetClientRect(GetActiveWindow(), &clientRect);

        TerrainHitTestContext hitTestContext(
            *_terrainManager, *_sceneParser, *_techniqueContext,
            Int2(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top));
        auto intersection = Tools::FindTerrainIntersection(hitTestContext, screenCoord);

        Result result;
        if (intersection.second) {
            result._type = Result::Terrain;
            result._worldSpaceCollision = intersection.first;
        }

        return result;
    }

    static Float4x4 CalculateWorldToProjection(const RenderCore::CameraDesc& sceneCamera, float viewportAspect)
    {
        auto projectionMatrix = RenderCore::PerspectiveProjection(
            sceneCamera._verticalFieldOfView, viewportAspect,
            sceneCamera._nearClip, sceneCamera._farClip, RenderCore::GeometricCoordinateSpace::RightHanded, 
            #if (GFXAPI_ACTIVE == GFXAPI_DX11) || (GFXAPI_ACTIVE == GFXAPI_DX9)
                RenderCore::ClipSpaceType::Positive);
            #else
                RenderCore::ClipSpaceType::StraddlingZero);
            #endif
        return Combine(InvertOrthonormalTransform(sceneCamera._cameraToWorld), projectionMatrix);
    }

    std::pair<Float3, Float3> HitTestResolver::CalculateWorldSpaceRay(Int2 screenCoord) const
    {
        RECT clientRect; GetClientRect(GetActiveWindow(), &clientRect);
        float aspect = (clientRect.right - clientRect.left) / float(clientRect.bottom - clientRect.top);
        auto sceneCamera = _sceneParser->GetCameraDesc();
        auto worldToProjection = CalculateWorldToProjection(sceneCamera, aspect);

        Float3 frustumCorners[8];
        CalculateAbsFrustumCorners(frustumCorners, worldToProjection);
        Float3 cameraPosition = ExtractTranslation(sceneCamera._cameraToWorld);

        return RenderCore::BuildRayUnderCursor(
            screenCoord, frustumCorners, cameraPosition, 
            sceneCamera._nearClip, sceneCamera._farClip,
            std::make_pair(Float2(0.f, 0.f), Float2(float(clientRect.right - clientRect.left), float(clientRect.bottom - clientRect.top))));
    }

    Float2 HitTestResolver::ProjectToScreenSpace(const Float3& worldSpaceCoord) const
    {
        RECT clientRect; GetClientRect(GetActiveWindow(), &clientRect);
        float vWidth = float(clientRect.right - clientRect.left);
        float vHeight = float(clientRect.bottom - clientRect.top);
        auto worldToProjection = CalculateWorldToProjection(_sceneParser->GetCameraDesc(), vWidth / vHeight);
        auto projCoords = worldToProjection * Expand(worldSpaceCoord, 1.f);

        return Float2(
            (projCoords[0] / projCoords[3] * 0.5f + 0.5f) * vWidth,
            (projCoords[1] / projCoords[3] * -0.5f + 0.5f) * vHeight);
    }

    RenderCore::CameraDesc HitTestResolver::GetCameraDesc() const 
    { 
        return _sceneParser->GetCameraDesc();
    }

    HitTestResolver::HitTestResolver(
        std::shared_ptr<SceneEngine::TerrainManager> terrainManager,
        std::shared_ptr<SceneEngine::ISceneParser> sceneParser,
        std::shared_ptr<SceneEngine::TechniqueContext> techniqueContext)
    : _terrainManager(std::move(terrainManager))
    , _sceneParser(std::move(sceneParser))
    , _techniqueContext(std::move(techniqueContext))
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class SelectAndEdit : public IManipulator
    {
    public:
        bool OnInputEvent(
            const InputSnapshot& evnt,
            const HitTestResolver& hitTestContext);
        void Render(
            RenderCore::Metal::DeviceContext* context,
            SceneEngine::LightingParserContext& parserContext);

        const char* GetName() const;
        std::string GetStatusText() const;

        std::pair<FloatParameter*, size_t>  GetFloatParameters() const;
        std::pair<BoolParameter*, size_t>   GetBoolParameters() const;
        void SetActivationState(bool);

        SelectAndEdit(
            IPlacementManipManagerInterface* manInterface,
            std::shared_ptr<SceneEngine::PlacementsEditor> editor);
        ~SelectAndEdit();

    protected:
        std::shared_ptr<SceneEngine::PlacementsEditor>  _editor;
                  
        class SubOperation
        {
        public:
            enum Type { None, Translate, Scale, Rotate, MoveAcrossTerrainSurface };
            enum Axis { NoAxis, X, Y, Z };

            Type    _type;
            Float3  _parameter;
            Axis    _axisRestriction;
            Coord2  _cursorStart;
            HitTestResolver::Result _anchorTerrainIntersection;
            char    _typeInBuffer[4];

            SubOperation() : _type(None), _parameter(0.f, 0.f, 0.f), _axisRestriction(NoAxis), _cursorStart(0, 0) { _typeInBuffer[0] = '\0'; }
        };
        SubOperation _activeSubop;

        std::shared_ptr<SceneEngine::PlacementsEditor::ITransaction> _transaction;
        IPlacementManipManagerInterface* _manInterface;
        Float3 _anchorPoint;

        SceneEngine::PlacementsEditor::ObjTransDef TransformObject(
            const SceneEngine::PlacementsEditor::ObjTransDef& inputObj);
    };

    SceneEngine::PlacementsEditor::ObjTransDef SelectAndEdit::TransformObject(
        const SceneEngine::PlacementsEditor::ObjTransDef& inputObj)
    {
        Float4x4 transform;
        if (_activeSubop._type == SubOperation::Rotate) {
            transform = AsFloat4x4(-_anchorPoint);

            if (XlAbs(_activeSubop._parameter[0]) > 0.f) {
                Combine_InPlace(transform, RotationX(_activeSubop._parameter[0]));
            }

            if (XlAbs(_activeSubop._parameter[1]) > 0.f) {
                Combine_InPlace(transform, RotationY(_activeSubop._parameter[1]));
            }

            if (XlAbs(_activeSubop._parameter[2]) > 0.f) {
                Combine_InPlace(transform, RotationZ(_activeSubop._parameter[2]));
            }

            Combine_InPlace(transform, _anchorPoint);
        } else if (_activeSubop._type == SubOperation::Scale) {
            transform = AsFloat4x4(-_anchorPoint);
            Combine_InPlace(transform, ArbitraryScale(_activeSubop._parameter));
            Combine_InPlace(transform, _anchorPoint);
        } else if (_activeSubop._type == SubOperation::Translate) {
            transform = AsFloat4x4(_activeSubop._parameter);
        } else if (_activeSubop._type == SubOperation::MoveAcrossTerrainSurface) {
                //  move across terrain surface is a little different... 
                //  we have a 2d translation in XY. But then the Z values should be calculated
                //  from the terrain height.
            Float2 finalXY = Truncate(ExtractTranslation(inputObj._localToWorld)) + Truncate(_activeSubop._parameter);
            float terrainHeight = GetTerrainHeight(
                *Sample::MainTerrainFormat.get(), Sample::MainTerrainConfig, Sample::MainTerrainCoords, 
                finalXY);
            transform = AsFloat4x4(-ExtractTranslation(inputObj._localToWorld) + Expand(finalXY, terrainHeight));
        } else {
            return inputObj;
        }

        auto res = inputObj;
        res._localToWorld = AsFloat3x4(Combine(res._localToWorld, transform));
        return res;
    }

    class RayVsModelResult
    {
    public:
        unsigned _count;
        float _distance;
    };

    class RayVsModelResources
    {
    public:
        class Desc 
        {
        public:
            unsigned _elementSize;
            unsigned _elementCount;
            Desc(unsigned elementSize, unsigned elementCount) : _elementSize(elementSize), _elementCount(elementCount) {}
        };
        intrusive_ptr<ID3D::Buffer> _streamOutputBuffer;
        intrusive_ptr<ID3D::Resource> _clearedBuffer;
        intrusive_ptr<ID3D::Resource> _cpuAccessBuffer;
        RayVsModelResources(const Desc&);
    };

    RayVsModelResources::RayVsModelResources(const Desc& desc)
    {
        using namespace BufferUploads;
        using namespace RenderCore::Metal;
        auto& uploads = *SceneEngine::GetBufferUploads();

        LinearBufferDesc lbDesc;
        lbDesc._structureByteSize = desc._elementSize;
        lbDesc._sizeInBytes = desc._elementSize * desc._elementCount;

        BufferDesc bufferDesc = CreateDesc(
            BindFlag::StreamOutput, 0, GPUAccess::Read | GPUAccess::Write,
            lbDesc, "RayVsModelBuffer");
        
        auto soRes = uploads.Transaction_Immediate(bufferDesc, nullptr)->AdoptUnderlying();
        _streamOutputBuffer = QueryInterfaceCast<ID3D::Buffer>(soRes);

        _cpuAccessBuffer = uploads.Transaction_Immediate(
            CreateDesc(0, CPUAccess::Read, 0, lbDesc, "RayVsModelCopyBuffer"), nullptr)->AdoptUnderlying();

        auto pkt = CreateEmptyPacket(bufferDesc);
        XlSetMemory(pkt->GetData(), 0, pkt->GetDataSize());
        _clearedBuffer = uploads.Transaction_Immediate(
            CreateDesc(
                BindFlag::StreamOutput, 0, GPUAccess::Read | GPUAccess::Write,
                lbDesc, "RayVsModelClearingBuffer"), 
            pkt.get())->AdoptUnderlying();
    }

    static RayVsModelResult RayVsModel(
        std::pair<Float3, Float3> worldSpaceRay,
        SceneEngine::PlacementsEditor& placementsEditor, SceneEngine::PlacementGUID object,
        const SceneEngine::TechniqueContext& techniqueContext,
        const RenderCore::CameraDesc* cameraForLOD = nullptr)
    {
            // Using the GPU, look for intersections between the ray
            // and the given model. Since we're using the GPU, we need to
            // get a device context. 
            //
            // We'll have to use the immediate context
            // because we want to get the result get right. But that means the
            // immediate context can't be doing anything else in another thread.
            //
            // This will require more complex threading support in the future!
        ID3D::DeviceContext* immContextTemp = nullptr;
        RenderCore::Metal::ObjectFactory().GetUnderlying()->GetImmediateContext(&immContextTemp);
        RenderCore::Metal::DeviceContext devContext(
            intrusive_ptr<ID3D::DeviceContext>(moveptr(immContextTemp)));

            // We're doing the intersection test in the geometry shader. This means
            // we have to setup a projection transform to avoid removing any potential
            // intersection results during screen-edge clipping.
            // Also, if we want to know the triangle pts and barycentric coordinates,
            // we need to make sure that no clipping occurs.
            // The easiest way to prevent clipping would be use a projection matrix that
            // would transform all points into a single point in the center of the view
            // frustum.
        RenderCore::Metal::ViewportDesc newViewport(0.f, 0.f, float(255.f), float(255.f), 0.f, 1.f);
        devContext.Bind(newViewport);

        SceneEngine::LightingParserContext parserContext(nullptr, techniqueContext);
        SceneEngine::RenderingQualitySettings qualitySettings;
        qualitySettings._width = qualitySettings._height = 255;
        qualitySettings._samplingCount = 1;
        qualitySettings._samplingQuality = 0;

        Float4x4 specialProjMatrix = MakeFloat4x4(
            0.f, 0.f, 0.f, 0.5f,
            0.f, 0.f, 0.f, 0.5f,
            0.f, 0.f, 0.f, 0.5f,
            0.f, 0.f, 0.f, 1.f);

            // The camera settings can affect the LOD that objects a rendered with.
            // So, in some cases we need to initialise the camera to the same state
            // used in rendering. This will ensure that we get the right LOD behaviour.
        RenderCore::CameraDesc camera;
        if (cameraForLOD) { camera = *cameraForLOD; }

        SceneEngine::LightingParser_SetupScene(
            &devContext, parserContext, camera, qualitySettings);
        SceneEngine::LightingParser_SetGlobalTransform(
            &devContext, parserContext, camera, qualitySettings._width, qualitySettings._height,
            &specialProjMatrix);

        RayVsModelResult fnResult;
        fnResult._count = 0;
        fnResult._distance = FLT_MAX;

        auto oldSO = RenderCore::Metal::GeometryShader::GetDefaultStreamOutputInitializers();

        TRY {

            using namespace RenderCore::Metal;
            static const InputElementDesc eles[] = {
                InputElementDesc("INTERSECTIONDEPTH",   0, NativeFormat::R32_FLOAT),
                InputElementDesc("POINT",               0, NativeFormat::R32G32B32A32_FLOAT),
                InputElementDesc("POINT",               1, NativeFormat::R32G32B32A32_FLOAT),
                InputElementDesc("POINT",               2, NativeFormat::R32G32B32A32_FLOAT)
            };
            struct ResultEntry
            {
            public:
                union { unsigned _depthAsInt; float _intersectionDepth; };
                Float4 _pt[3];
            };

            unsigned strides[] = { sizeof(ResultEntry) };
            unsigned offsets[] = { 0 };
            GeometryShader::SetDefaultStreamOutputInitializers(
                GeometryShader::StreamOutputInitializers(eles, dimof(eles), strides, dimof(strides)));

            const unsigned maxResultCount = 256;
            auto& res = SceneEngine::FindCachedBox<RayVsModelResources>(RayVsModelResources::Desc(sizeof(ResultEntry), maxResultCount));

                // the only way to clear these things is copy from another buffer...
            devContext.GetUnderlying()->CopyResource(res._streamOutputBuffer.get(), res._clearedBuffer.get());

            ID3D::Buffer* targets[] = { res._streamOutputBuffer.get() };
            devContext.GetUnderlying()->SOSetTargets(dimof(targets), targets, offsets);

            float rayLength = Magnitude(worldSpaceRay.second - worldSpaceRay.first);
            struct RayDefinitionCBuffer
            {
                Float3 _rayStart;
                float _rayLength;
                Float3 _rayDirection;
                unsigned _dummy;
            } rayDefinitionCBuffer = 
            {
                worldSpaceRay.first, rayLength,
                (worldSpaceRay.second - worldSpaceRay.first) / rayLength, 0
            };

            devContext.BindGS(RenderCore::MakeResourceList(ConstantBuffer(&rayDefinitionCBuffer, sizeof(rayDefinitionCBuffer))));
            devContext.BindGS(RenderCore::MakeResourceList(SceneEngine::CommonResources()._defaultSampler));

                //  We need to invoke the render for the given object
                //  now. Afterwards we can query the buffers for the result
            const unsigned techniqueIndex = 6;
            placementsEditor.RenderFiltered(
                &devContext, parserContext, techniqueIndex,
                &object, &object+1);

                // We must lock the stream output buffer, and look for results within it
                // it seems that this kind of thing wasn't part of the original intentions
                // for stream output. So the results can appear anywhere within the buffer.
                // We have to search for non-zero entries. Results that haven't been written
                // to will appear zeroed out.
            devContext.GetUnderlying()->CopyResource(res._cpuAccessBuffer.get(), res._streamOutputBuffer.get());

            D3D11_MAPPED_SUBRESOURCE mappedSub;
            auto hresult = devContext.GetUnderlying()->Map(
                res._cpuAccessBuffer.get(), 0, D3D11_MAP_READ, 0, &mappedSub);
            if (SUCCEEDED(hresult)) {

                const auto* results = (const ResultEntry*)mappedSub.pData;
                for (unsigned c=0; c<maxResultCount; ++c) {
                    if (results[c]._depthAsInt) {
                        ++fnResult._count;
                        if (results[c]._intersectionDepth < fnResult._distance) {
                            fnResult._distance = results[c]._intersectionDepth;
                        }
                    }
                }

                devContext.GetUnderlying()->Unmap(res._cpuAccessBuffer.get(), 0);
            }

        } CATCH (...) {
        } CATCH_END

        devContext.GetUnderlying()->SOSetTargets(0, nullptr, nullptr);
        RenderCore::Metal::GeometryShader::SetDefaultStreamOutputInitializers(oldSO);

        return fnResult;
    }

    bool SelectAndEdit::OnInputEvent(
        const InputSnapshot& evnt,
        const HitTestResolver& hitTestContext)
    {
        bool consume = false;
        if (_transaction) {
            static const auto keyG = KeyId_Make("g");
            static const auto keyS = KeyId_Make("s");
            static const auto keyR = KeyId_Make("r");
            static const auto keyM = KeyId_Make("m");
            static const auto keyEscape = KeyId_Make("escape");

            static const auto keyX = KeyId_Make("x");
            static const auto keyY = KeyId_Make("y");
            static const auto keyZ = KeyId_Make("z");

            bool updateState = evnt._mouseDelta[0] || evnt._mouseDelta[1];

            SubOperation::Type newSubOp = (SubOperation::Type)~unsigned(0);
            if (evnt.IsPress(keyG))         { newSubOp = SubOperation::Translate; _activeSubop._typeInBuffer[0] = '\0'; updateState = true; consume = true; }
            if (evnt.IsPress(keyS))         { newSubOp = SubOperation::Scale; _activeSubop._typeInBuffer[0] = '\0'; updateState = true; consume = true; }
            if (evnt.IsPress(keyR))         { newSubOp = SubOperation::Rotate; _activeSubop._typeInBuffer[0] = '\0'; updateState = true; consume = true; }
            if (evnt.IsPress(keyM))         { newSubOp = SubOperation::MoveAcrossTerrainSurface; _activeSubop._typeInBuffer[0] = '\0'; updateState = true; consume = true; }
            if (evnt.IsPress(keyEscape))    { newSubOp = SubOperation::None; _activeSubop._typeInBuffer[0] = '\0'; consume = true; }

            if (newSubOp != _activeSubop._type && newSubOp != ~unsigned(0x0)) {
                    //  we have to "restart" the transaction. This returns everything
                    //  to it's original place
                _transaction->UndoAndRestart();
                _activeSubop = SubOperation();
                _activeSubop._type = newSubOp;
                _activeSubop._axisRestriction = SubOperation::NoAxis;
                _activeSubop._cursorStart = evnt._mousePosition;

                if (newSubOp == SubOperation::MoveAcrossTerrainSurface) {
                    _activeSubop._anchorTerrainIntersection = hitTestContext.DoHitTest(evnt._mousePosition);
                }
            }

            if (_activeSubop._type != SubOperation::None) {
                Float3 oldParameter = _activeSubop._parameter;

                if (evnt.IsPress(keyX)) { _activeSubop._axisRestriction = SubOperation::X; _activeSubop._parameter = Float3(0.f, 0.f, 0.f); _activeSubop._typeInBuffer[0] = '\0'; updateState = true; consume = true; }
                if (evnt.IsPress(keyY)) { _activeSubop._axisRestriction = SubOperation::Y; _activeSubop._parameter = Float3(0.f, 0.f, 0.f); _activeSubop._typeInBuffer[0] = '\0'; updateState = true; consume = true; }
                if (evnt.IsPress(keyZ)) { _activeSubop._axisRestriction = SubOperation::Z; _activeSubop._parameter = Float3(0.f, 0.f, 0.f); _activeSubop._typeInBuffer[0] = '\0'; updateState = true; consume = true; }

                    // allow some characters to enter into the "type in buffer"
                    //      digits & '.' & '-'
                if ((evnt._pressedChar >= (ucs2)'0' && evnt._pressedChar <= (ucs2)'9') || evnt._pressedChar == (ucs2)'.' || evnt._pressedChar == (ucs2)'-') {
                    XlCatString(_activeSubop._typeInBuffer, dimof(_activeSubop._typeInBuffer), (char)evnt._pressedChar);
                    consume = true;
                    updateState = true;
                }

                if (updateState) {
                        //  we always perform a manipulator's action in response to a mouse movement.
                    if (_activeSubop._type == SubOperation::Rotate) {

                            //  rotate by checking the angle of the mouse cursor relative to the 
                            //  anchor point (in screen space)
                        auto ssAnchor = hitTestContext.ProjectToScreenSpace(_anchorPoint);
                        float ssAngle1 = XlATan2(evnt._mousePosition[1] - ssAnchor[1], evnt._mousePosition[0] - ssAnchor[0]);
                        float ssAngle0 = XlATan2(_activeSubop._cursorStart[1] - ssAnchor[1], _activeSubop._cursorStart[0] - ssAnchor[0]);
                        float value = ssAngle0 - ssAngle1;

                        if (_activeSubop._typeInBuffer[0]) {
                            value = XlAtoF32(_activeSubop._typeInBuffer) * gPI / 180.f;
                        }

                        unsigned axisIndex = 2;
                        switch (_activeSubop._axisRestriction) {
                        case SubOperation::X: axisIndex = 0; break;
                        case SubOperation::Y: axisIndex = 1; break;
                        case SubOperation::NoAxis:
                        case SubOperation::Z: axisIndex = 2; break;
                        }

                        _activeSubop._parameter = Float3(0.f, 0.f, 0.f);
                        _activeSubop._parameter[axisIndex] = value;

                    } else if (_activeSubop._type == SubOperation::Scale) {

                            //  Scale based on the distance (in screen space) between the cursor
                            //  and the anchor point, and compare that to the distance when we
                            //  first started this operation

                        auto ssAnchor = hitTestContext.ProjectToScreenSpace(_anchorPoint);
                        float ssDist1 = Magnitude(evnt._mousePosition - ssAnchor);
                        float ssDist0 = Magnitude(_activeSubop._cursorStart - ssAnchor);
                        float scaleFactor = 1.f;
                        if (ssDist0 > 0.f) {
                            scaleFactor = ssDist1 / ssDist0;
                        }

                        if (_activeSubop._typeInBuffer[0]) {
                            scaleFactor = XlAtoF32(_activeSubop._typeInBuffer);
                        }

                        switch (_activeSubop._axisRestriction) {
                        case SubOperation::X: _activeSubop._parameter = Float3(scaleFactor, 0.f, 0.f); break;
                        case SubOperation::Y: _activeSubop._parameter = Float3(0.f, scaleFactor, 0.f); break;
                        case SubOperation::Z: _activeSubop._parameter = Float3(0.f, 0.f, scaleFactor); break;
                        case SubOperation::NoAxis: _activeSubop._parameter = Float3(scaleFactor, scaleFactor, scaleFactor); break;
                        }

                    } else if (_activeSubop._type == SubOperation::Translate) {

                            //  We always translate across a 2d plane. So we need to define a plane 
                            //  based on the camera position and the anchor position.
                            //  We will calculate an intersection between a world space ray under the
                            //  cursor and that plane. That point (in 3d) will be the basis of the 
                            //  translation we apply.
                            //
                            //  The current "up" translation axis should lie flat on the plane. We
                            //  also want the camera "right" to lie close to the plane.

                        auto currentCamera = hitTestContext.GetCameraDesc();
                        Float3 upAxis = ExtractUp_Cam(currentCamera._cameraToWorld);
                        Float3 rightAxis = ExtractRight_Cam(currentCamera._cameraToWorld);
                        assert(Equivalent(MagnitudeSquared(upAxis), 1.f, 1e-6f));
                        assert(Equivalent(MagnitudeSquared(rightAxis), 1.f, 1e-6f));

                        switch (_activeSubop._axisRestriction) {
                        case SubOperation::X: upAxis = Float3(1.f, 0.f, 0.f); break;
                        case SubOperation::Y: upAxis = Float3(0.f, 1.f, 0.f); break;
                        case SubOperation::Z: upAxis = Float3(0.f, 0.f, 1.f); break;
                        }

                        rightAxis = rightAxis - upAxis * Dot(upAxis, rightAxis);
                        if (MagnitudeSquared(rightAxis) < 1e-6f) {
                            rightAxis = ExtractUp_Cam(currentCamera._cameraToWorld);
                            rightAxis = rightAxis - upAxis * Dot(upAxis, rightAxis);
                        }

                        Float3 planeNormal = Cross(upAxis, rightAxis);
                        Float4 plane = Expand(planeNormal, -Dot(planeNormal, _anchorPoint));

                        auto ray = hitTestContext.CalculateWorldSpaceRay(evnt._mousePosition);
                        float dst = RayVsPlane(ray.first, ray.second, plane);
                        if (dst >= 0.f && dst <= 1.f) {
                            auto intersectionPt = LinearInterpolate(ray.first, ray.second, dst);
                            float transRight = Dot(intersectionPt - _anchorPoint, rightAxis);
                            float transUp = Dot(intersectionPt - _anchorPoint, upAxis);

                            if (_activeSubop._axisRestriction != SubOperation::NoAxis && _activeSubop._typeInBuffer[0]) {
                                transUp = XlAtoF32(_activeSubop._typeInBuffer);
                            }

                            switch (_activeSubop._axisRestriction) {
                            case SubOperation::X: _activeSubop._parameter = Float3(transUp, 0.f, 0.f); break;
                            case SubOperation::Y: _activeSubop._parameter = Float3(0.f, transUp, 0.f); break;
                            case SubOperation::Z: _activeSubop._parameter = Float3(0.f, 0.f, transUp); break;
                            case SubOperation::NoAxis:
                                _activeSubop._parameter = transRight * rightAxis + transUp * upAxis;
                                break;
                            }
                        }

                    } else if (_activeSubop._type == SubOperation::MoveAcrossTerrainSurface) {

                            //  We want to find an intersection point with the terrain, and then 
                            //  compare the XY coordinates of that to the anchor point

                        auto collision = hitTestContext.DoHitTest(evnt._mousePosition);
                        if (collision._type == HitTestResolver::Result::Terrain
                            && _activeSubop._anchorTerrainIntersection._type == HitTestResolver::Result::Terrain) {
                            _activeSubop._parameter = Float3(
                                collision._worldSpaceCollision[0] - _anchorPoint[0],
                                collision._worldSpaceCollision[1] - _anchorPoint[1],
                                0.f);
                        }

                    }
                }

                if (_activeSubop._parameter != oldParameter) {
                        // push these changes onto the transaction
                    unsigned count = _transaction->GetObjectCount();
                    for (unsigned c=0; c<count; ++c) {
                        auto& originalState = _transaction->GetObjectOriginalState(c);
                        auto newState = TransformObject(originalState);
                        _transaction->SetObject(c, newState);
                    }
                }
            }
        }
        

            //  On lbutton click, attempt to do hit detection
            //  select everything that intersects with the given ray
        if (evnt.IsRelease_LButton()) {

            if (_activeSubop._type != SubOperation::None) {

                if (_transaction) { 
                    _transaction->Commit();  
                    _transaction.reset();
                }
                _activeSubop = SubOperation();

            } else {

                auto worldSpaceRay = hitTestContext.CalculateWorldSpaceRay(evnt._mousePosition);
                auto intersection = _editor->Find_RayIntersection(worldSpaceRay.first, worldSpaceRay.second);

                    // we can improve the intersection by do ray-vs-triangle tests
                    // on the roughIntersection geometry
                const bool improveIntersection = true;
                if (constant_expression<improveIntersection>::result()) {
                        //  we need to create a temporary transaction to get
                        //  at the information for these objects.
                    auto trans = _editor->Transaction_Begin(
                        AsPointer(intersection.cbegin()), AsPointer(intersection.cend()));
                    
                    float bestDistance = Magnitude(worldSpaceRay.second - worldSpaceRay.first);
                    auto bestResult = std::make_pair(0ull, 0ull);

                    auto count = trans->GetObjectCount();
                    for (unsigned c=0; c<count; ++c) {
                        auto cam = hitTestContext.GetCameraDesc();
                        auto r = RayVsModel(worldSpaceRay, *_editor, trans->GetGuid(c), hitTestContext.GetTechniqueContext(), &cam);
                        if (r._count && r._distance < bestDistance) {
                            bestResult = trans->GetOriginalGuid(c);
                            bestDistance = r._distance;
                        }
                    }

                    intersection.clear();
                    if (bestResult.first && bestResult.second) {
                        intersection.push_back(bestResult);
                    }

                    trans->Cancel();
                }

                    // replace the currently active selection
                if (_transaction) {
                    _transaction->Commit();
                }
                _transaction = _editor->Transaction_Begin(
                    AsPointer(intersection.cbegin()), AsPointer(intersection.cend()));

                    //  Reset the anchor point
                    //  There are a number of different possible ways we could calculate
                    //      the anchor point... But let's find the world space bounding box
                    //      that encloses all of the objects and get the centre of that box.
                Float3 totalMins(FLT_MAX, FLT_MAX, FLT_MAX), totalMaxs(-FLT_MAX, -FLT_MAX, -FLT_MAX);
                unsigned objCount = _transaction->GetObjectCount();
                for (unsigned c=0; c<objCount; ++c) {
                    auto obj = _transaction->GetObject(c);
                    auto localBoundingBox = _transaction->GetLocalBoundingBox(c);
                    auto worldSpaceBounding = TransformBoundingBox(obj._localToWorld, localBoundingBox);
                    totalMins[0] = std::min(worldSpaceBounding.first[0], totalMins[0]);
                    totalMins[1] = std::min(worldSpaceBounding.first[1], totalMins[1]);
                    totalMins[2] = std::min(worldSpaceBounding.first[2], totalMins[2]);
                    totalMaxs[0] = std::max(worldSpaceBounding.second[0], totalMaxs[0]);
                    totalMaxs[1] = std::max(worldSpaceBounding.second[1], totalMaxs[1]);
                    totalMaxs[2] = std::max(worldSpaceBounding.second[2], totalMaxs[2]);
                }
                _anchorPoint = LinearInterpolate(totalMins, totalMaxs, 0.5f);

            }

            return true;
        }

        if (evnt.IsDblClk_LButton()) {

                //  We want to select the name model that we dbl-clked on.
                //  Actually, the model under the cursor is probably in
                //  the selection now. But let's do another ray test.
                //  Then, tell the manager to switch to placement mode

            if (_manInterface) {
                auto worldSpaceRay = hitTestContext.CalculateWorldSpaceRay(evnt._mousePosition);
                auto selected = _editor->Find_RayIntersection(worldSpaceRay.first, worldSpaceRay.second);
                if (!selected.empty()) {
                    auto tempTrans = _editor->Transaction_Begin(&selected[0], &selected[0] + 1);
                    if (tempTrans->GetObjectCount() == 1) {
                        _manInterface->SelectModel(tempTrans->GetObject(0)._model.c_str());
                        _manInterface->SwitchToMode(IPlacementManipManagerInterface::Mode::PlaceSingle);
                    }
                }
            }

            return true;
        }

        if (evnt.IsPress(KeyId_Make("delete"))) {
            if (_transaction) {
                auto count = _transaction->GetObjectCount();
                for (unsigned c=0; c<count; ++c) { _transaction->Delete(c); }
                _activeSubop = SubOperation();
            }
            return true;
        }

        return consume;
    }

    class CommonOffscreenTarget
    {
    public:
        class Desc
        {
        public:
            unsigned _width, _height;
            RenderCore::Metal::NativeFormat::Enum _format;
            Desc(unsigned width, unsigned height, RenderCore::Metal::NativeFormat::Enum format)
                : _width(width), _height(height), _format(format) {}
        };

        RenderCore::Metal::RenderTargetView _rtv;
        RenderCore::Metal::ShaderResourceView _srv;
        intrusive_ptr<BufferUploads::ResourceLocator> _resource;

        CommonOffscreenTarget(const Desc& desc);
        ~CommonOffscreenTarget();
    };

    CommonOffscreenTarget::CommonOffscreenTarget(const Desc& desc)
    {
            //  Still some work involved to just create a texture
            //  
        using namespace BufferUploads;
        auto bufferDesc = CreateDesc(
            BindFlag::ShaderResource|BindFlag::RenderTarget, 0, GPUAccess::Write,
            TextureDesc::Plain2D(desc._width, desc._height, desc._format),
            "CommonOffscreen");

        auto resource = SceneEngine::GetBufferUploads()->Transaction_Immediate(bufferDesc, nullptr);

        RenderCore::Metal::RenderTargetView rtv(resource->GetUnderlying());
        RenderCore::Metal::ShaderResourceView srv(resource->GetUnderlying());

        _rtv = std::move(rtv);
        _srv = std::move(srv);
        _resource = std::move(resource);
    }

    CommonOffscreenTarget::~CommonOffscreenTarget() {}

    class HighlightShaders
    {
    public:
        class Desc {};

        RenderCore::Metal::ShaderProgram* _drawHighlight;
        RenderCore::Metal::BoundUniforms _drawHighlightUniforms;

        const Assets::DependencyValidation& GetDependancyValidation() const   { return *_validationCallback; }

        HighlightShaders(const Desc&);
    protected:
        std::shared_ptr<Assets::DependencyValidation>  _validationCallback;
    };

    HighlightShaders::HighlightShaders(const Desc&)
    {
        auto* drawHighlight = &::Assets::GetAssetDep<RenderCore::Metal::ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen:vs_*", 
            "game/xleres/effects/outlinehighlight.psh:main:ps_*");

        RenderCore::Metal::BoundUniforms uniforms(*drawHighlight);
        SceneEngine::TechniqueContext::BindGlobalUniforms(uniforms);

        auto validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(validationCallback, &drawHighlight->GetDependancyValidation());

        _validationCallback = std::move(validationCallback);
        _drawHighlight = std::move(drawHighlight);
        _drawHighlightUniforms = std::move(uniforms);
    }

    static void RenderHighlight(
        RenderCore::Metal::DeviceContext* context,
        SceneEngine::LightingParserContext& parserContext,
        SceneEngine::PlacementsEditor* editor,
        const SceneEngine::PlacementGUID* filterBegin,
        const SceneEngine::PlacementGUID* filterEnd)
    {
        TRY {
            using namespace SceneEngine;
            using namespace RenderCore;
            SavedTargets savedTargets(context);
            const auto& viewport = savedTargets.GetViewports()[0];

            auto& offscreen = FindCachedBox<CommonOffscreenTarget>(
                CommonOffscreenTarget::Desc(unsigned(viewport.Width), unsigned(viewport.Height), 
                Metal::NativeFormat::R8G8B8A8_UNORM));

            context->Bind(MakeResourceList(offscreen._rtv), nullptr);
            context->Clear(offscreen._rtv, Float4(0.f, 0.f, 0.f, 0.f));
            context->Bind(RenderCore::Metal::Topology::TriangleList);
            editor->RenderFiltered(context, parserContext, 0, filterBegin, filterEnd);

            savedTargets.ResetToOldTargets(context);

                //  now we can render these objects over the main image, 
                //  using some filtering

            context->BindPS(MakeResourceList(offscreen._srv));

            auto& shaders = FindCachedBoxDep<HighlightShaders>(HighlightShaders::Desc());
            shaders._drawHighlightUniforms.Apply(
                *context, 
                parserContext.GetGlobalUniformsStream(), RenderCore::Metal::UniformsStream());
            context->Bind(*shaders._drawHighlight);
            context->Bind(SceneEngine::CommonResources()._blendAlphaPremultiplied);
            context->Bind(SceneEngine::CommonResources()._dssDisable);
            context->Bind(RenderCore::Metal::Topology::TriangleStrip);
            context->Draw(4);
        } 
        CATCH (const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); } 
        CATCH (const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); } 
        CATCH_END
    }

    void SelectAndEdit::Render(
        RenderCore::Metal::DeviceContext* context,
        SceneEngine::LightingParserContext& parserContext)
    {
        std::vector<std::pair<uint64, uint64>> activeSelection;

        if (_transaction) {
            activeSelection.reserve(_transaction->GetObjectCount());
            for (unsigned c=0; c<_transaction->GetObjectCount(); ++c) {
                activeSelection.push_back(_transaction->GetGuid(c));
            }
        }

        if (!activeSelection.empty()) {
                //  If we have some selection, we need to render it
                //  to an offscreen buffer, and we can perform some
                //  operation to highlight the objects in that buffer.
                //
                //  Note that we could get different results by doing
                //  this one placement at a time -- but we will most
                //  likely get the most efficient results by rendering
                //  all of objects that require highlights in one go.
            RenderHighlight(
                context, parserContext, _editor.get(),
                AsPointer(activeSelection.begin()), AsPointer(activeSelection.end()));
        }
    }

    std::string SelectAndEdit::GetStatusText() const
    {
        if (_activeSubop._type == SubOperation::None) {
            return std::string();
        }

        StringMeld<512> meld;
        meld << std::fixed << std::setprecision(2);

        switch (_activeSubop._type)
        {
        case SubOperation::Translate:
            meld << "T: (" << _activeSubop._parameter[0] << ", " << _activeSubop._parameter[1] << "," << _activeSubop._parameter[1] << "). ";
            break;

        case SubOperation::Scale:
            meld << "S: (" << _activeSubop._parameter[0] << ", " << _activeSubop._parameter[1] << "," << _activeSubop._parameter[1] << "). ";
            break;

        case SubOperation::Rotate:
            switch (_activeSubop._axisRestriction) {
            case SubOperation::X: meld << "R: (" << _activeSubop._parameter[0] * 180.f / gPI << ")"; break;
            case SubOperation::Y: meld << "R: (" << _activeSubop._parameter[1] * 180.f / gPI << ")"; break;
            case SubOperation::NoAxis:
            case SubOperation::Z: meld << "R: (" << _activeSubop._parameter[2] * 180.f / gPI << ")"; break;
            }
            break;

        case SubOperation::MoveAcrossTerrainSurface:
            meld << "T <<terrain>>: (" << _activeSubop._parameter[0] << ", " << _activeSubop._parameter[1] << "). ";
            break;
        }

        switch (_activeSubop._axisRestriction) 
        {
        case SubOperation::NoAxis: break;
        case SubOperation::X: meld << " [ Global X ] "; break;
        case SubOperation::Y: meld << " [ Global Y ] "; break;
        case SubOperation::Z: meld << " [ Global Z ] "; break;
        }

        return std::string(meld);
    }

    const char* SelectAndEdit::GetName() const                                            { return "Select And Edit"; }
    auto SelectAndEdit::GetFloatParameters() const -> std::pair<FloatParameter*, size_t>  { return std::make_pair(nullptr, 0); }
    auto SelectAndEdit::GetBoolParameters() const -> std::pair<BoolParameter*, size_t>    { return std::make_pair(nullptr, 0); }
    void SelectAndEdit::SetActivationState(bool) 
    {
        if (_transaction) {
            _transaction->Cancel();
            _transaction.reset();
        }
        _activeSubop = SubOperation();
        _anchorPoint = Float3(0.f, 0.f, 0.f);
    }

    SelectAndEdit::SelectAndEdit(
        IPlacementManipManagerInterface* manInterface,
        std::shared_ptr<SceneEngine::PlacementsEditor> editor)
    {
        _manInterface = manInterface;
        _editor = editor;
        _anchorPoint = Float3(0.f, 0.f, 0.f);
    }

    SelectAndEdit::~SelectAndEdit()
    {}

    ///////////////////////////////////////////////////////////////////////////////////////////////////

    class PlaceSingle : public IManipulator
    {
    public:
        bool OnInputEvent(
            const InputSnapshot& evnt,
            const HitTestResolver& hitTestContext);
        void Render(
            RenderCore::Metal::DeviceContext* context,
            SceneEngine::LightingParserContext& parserContext);

        const char* GetName() const;
        std::pair<FloatParameter*, size_t>  GetFloatParameters() const;
        std::pair<BoolParameter*, size_t>   GetBoolParameters() const;
        void SetActivationState(bool);
        std::string GetStatusText() const { return std::string(); }

        PlaceSingle(
            IPlacementManipManagerInterface* manInterface,
            std::shared_ptr<SceneEngine::PlacementsEditor> editor);
        ~PlaceSingle();

    protected:
        Millisecond                     _placeTimeout;
        IPlacementManipManagerInterface* _manInterface;
        std::shared_ptr<SceneEngine::PlacementsEditor> _editor;
        unsigned                        _rendersSinceHitTest;

        bool        _doRandomRotation;
        float       _placementRotation;

        std::shared_ptr<SceneEngine::PlacementsEditor::ITransaction> _transaction;

        void MoveObject(
            const Float3& newLocation,
            const char modelName[], const char materialName[]);
    };

    static void Combine_InPlace_(RotationZ rotation, Float3x4& transform)
    {
        auto temp = AsFloat4x4(transform);
        Combine_InPlace(rotation, temp);
        transform = AsFloat3x4(temp);
    }

    void PlaceSingle::MoveObject(const Float3& newLocation, 
        const char modelName[], const char materialName[])
    {
        if (!_transaction) {
            _transaction = _editor->Transaction_Begin(nullptr, nullptr);
        }

        SceneEngine::PlacementsEditor::ObjTransDef newState(
            AsFloat3x4(newLocation), _manInterface->GetSelectedModel(), materialName);
        if (_doRandomRotation) {
            Combine_InPlace_(RotationZ(_placementRotation), newState._localToWorld);
        }

        TRY {
            if (!_transaction->GetObjectCount()) {
                _placementRotation = rand() * 2.f * gPI / float(RAND_MAX);
                newState._localToWorld = AsFloat3x4(newLocation);
                if (_doRandomRotation) {
                    Combine_InPlace_(RotationZ(_placementRotation), newState._localToWorld);
                }

                _transaction->Create(newState);
                _placeTimeout = Millisecond_Now();
            } else {
                _transaction->SetObject(0, newState);
            }
        } CATCH (...) {
        } CATCH_END
    }

    bool PlaceSingle::OnInputEvent(
        const InputSnapshot& evnt,
        const HitTestResolver& hitTestContext)
    {
        //  If we get a click on the terrain, then we should perform 
            //  whatever placement operation is required (eg, creating new placements)
        auto selectedModel = _manInterface->GetSelectedModel();
        if (selectedModel.empty()) {
            if (_transaction) {
                _transaction->Cancel();
                _transaction.reset();
            }
            return false;
        }

        if (_rendersSinceHitTest > 0) {
            _rendersSinceHitTest = 0;

            auto test = hitTestContext.DoHitTest(evnt._mousePosition);
            if (test._type == HitTestResolver::Result::Terrain) {

                    //  This is a spawn event. We should add a new item of the selected model
                    //  at the point clicked.
                MoveObject(test._worldSpaceCollision, selectedModel.c_str(), "");
            }
        }

            // We add a small timeout 
        const unsigned safetyTimeout = 200;
        if (evnt.IsRelease_LButton() && (_placeTimeout - Millisecond_Now()) > safetyTimeout) {
            if (_transaction) {
                _transaction->Commit();
                _transaction.reset();
            }
        }

        if (evnt.IsRelease_RButton() || evnt.IsPress(KeyId_Make("escape"))) {
            // cancel... tell the manager to change model
            if (_manInterface) {
                _manInterface->SwitchToMode(IPlacementManipManagerInterface::Mode::Select);
            }
        }

        return false;
    }

    void PlaceSingle::Render(
        RenderCore::Metal::DeviceContext* context,
        SceneEngine::LightingParserContext& parserContext)
    {
        ++_rendersSinceHitTest;
        if (_transaction && _transaction->GetObjectCount()) {
            std::vector<SceneEngine::PlacementGUID> objects;
            objects.reserve(_transaction->GetObjectCount());
            for (unsigned c=0; c<_transaction->GetObjectCount(); ++c) {
                objects.push_back(_transaction->GetGuid(c));
            }

            RenderHighlight(
                context, parserContext, _editor.get(),
                AsPointer(objects.begin()), AsPointer(objects.end()));
        }
    }

    const char* PlaceSingle::GetName() const                                            { return "Place single"; }
    auto PlaceSingle::GetFloatParameters() const -> std::pair<FloatParameter*, size_t>  { return std::make_pair(nullptr, 0); }
    auto PlaceSingle::GetBoolParameters() const -> std::pair<BoolParameter*, size_t>    
    {
        static BoolParameter parameters[] = 
        {
            BoolParameter(ManipulatorParameterOffset(&PlaceSingle::_doRandomRotation), 0, "RandomRotation"),
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    void PlaceSingle::SetActivationState(bool newState)
    {
        if (_transaction) {
            _transaction->Cancel();
            _transaction.reset();
        }
        if (_manInterface) {
            _manInterface->EnableSelectedModelDisplay(newState);
        }
        _placeTimeout = Millisecond_Now();
    }

    PlaceSingle::PlaceSingle(
        IPlacementManipManagerInterface* manInterface,
        std::shared_ptr<SceneEngine::PlacementsEditor> editor)
    {
        _placeTimeout = 0;
        _manInterface = manInterface;
        _editor = std::move(editor);
        _rendersSinceHitTest = 0;
        _doRandomRotation = true;
        _placementRotation = 0.f;
    }
    
    PlaceSingle::~PlaceSingle() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ScatterPlacements : public IManipulator
    {
    public:
        bool OnInputEvent(
            const InputSnapshot& evnt,
            const HitTestResolver& hitTestContext);
        void Render(
            RenderCore::Metal::DeviceContext* context,
            SceneEngine::LightingParserContext& parserContext);

        const char* GetName() const;
        std::pair<FloatParameter*, size_t>  GetFloatParameters() const;
        std::pair<BoolParameter*, size_t>   GetBoolParameters() const;
        void SetActivationState(bool);
        std::string GetStatusText() const { return std::string(); }

        ScatterPlacements(
            IPlacementManipManagerInterface* manInterface,
            std::shared_ptr<SceneEngine::PlacementsEditor> editor);
        ~ScatterPlacements();

    protected:
        Millisecond                         _spawnTimer;
        IPlacementManipManagerInterface*    _manInterface;
        std::shared_ptr<SceneEngine::PlacementsEditor> _editor;

        float _radius;
        float _density;

        Float3 _hoverPoint;
        bool _hasHoverPoint;

        void PerformScatter(const Float3& centre, const char modelName[], const char materialName[]);
    };

    bool ScatterPlacements::OnInputEvent(
        const InputSnapshot& evnt,
        const HitTestResolver& hitTestContext)
    {
            //  If we get a click on the terrain, then we should perform 
            //  whatever placement operation is required (eg, creating new placements)
            //
            //  However, we need to do terrain collisions every time (because we want to
            //  move the highlight/preview position

        auto test = hitTestContext.DoHitTest(evnt._mousePosition);
        _hoverPoint = test._worldSpaceCollision;
        _hasHoverPoint = test._type == HitTestResolver::Result::Terrain;

        const Millisecond spawnTimeOut = 200;
        auto now = Millisecond_Now();
        if (evnt.IsHeld_LButton()) {
            if (test._type == HitTestResolver::Result::Terrain) {
                auto selectedModel = _manInterface->GetSelectedModel();
                if (now >= (_spawnTimer + spawnTimeOut) && !selectedModel.empty()) {
                    PerformScatter(test._worldSpaceCollision, selectedModel.c_str(), "");
                    _spawnTimer = now;
                }
                return true;
            }
        } else { _spawnTimer = 0; }

        if (evnt._wheelDelta) {
            _radius = std::max(1.f, _radius + 3.f * evnt._wheelDelta / 120.f);
        }

        if (evnt.IsRelease_RButton() || evnt.IsPress(KeyId_Make("escape"))) {
            // cancel... tell the manager to change model
            if (_manInterface) {
                _manInterface->SwitchToMode(IPlacementManipManagerInterface::Mode::Select);
            }
        }

        return false;
    }

    static float TriangleSignedArea(
        const Float2& pt1, const Float2& pt2, const Float2& pt3)
    {
        // reference:
        //  http://mathworld.wolfram.com/TriangleArea.html
        return .5f * (
            -pt2[0] * pt1[1] + pt3[0] * pt1[1] + pt1[0] * pt2[1]
            -pt3[0] * pt2[1] - pt1[0] * pt3[1] + pt2[0] * pt3[1]);
    }

    static bool PtInConvexPolygon(
        const unsigned* ptIndicesStart, const unsigned* ptIndicesEnd,
        const Float2 pts[], const Float2& pt)
    {
        unsigned posCount = 0, negCount = 0;
        for (auto* p=ptIndicesStart; (p+1)<ptIndicesEnd; ++p) {
            auto area = TriangleSignedArea(pts[*p], pts[*(p+1)], pt);
            if (area < 0.f) { ++negCount; } else { ++posCount; }
        }

        auto area = TriangleSignedArea(pts[*(ptIndicesEnd-1)], pts[*ptIndicesStart], pt);
        if (area < 0.f) { ++negCount; } else { ++posCount; }

            //  we're not making assumption about winding order. So 
            //  return true if every result is positive, or every result
            //  is negative; (in other words, one of the following must be zero)
        return posCount*negCount == 0;
    }

    static Float2 PtClosestToOrigin(const Float2& start, const Float2& end)
    {
        Float2 axis = end - start;
        float length = Magnitude(axis);
        Float2 dir = axis / length;
        float proj = Dot(-start, dir);
        return LinearInterpolate(start, end, Clamp(proj, 0.f, length));
    }

    /// <summary>Compare an axially aligned bounding box to a circle (in 2d, on the XY plane)</summary>
    /// This is a cheap alternative to box vs cylinder
    static bool AABBVsCircleInXY(
        float circleRadius, const std::pair<Float3, Float3>& boundingBox, const Float3x4& localToCircleSpace)
    {
        Float3 pts[] = 
        {
            Float3( boundingBox.first[0],  boundingBox.first[1],  boundingBox.first[2]),
            Float3(boundingBox.second[0],  boundingBox.first[1],  boundingBox.first[2]),
            Float3( boundingBox.first[0], boundingBox.second[1],  boundingBox.first[2]),
            Float3(boundingBox.second[0], boundingBox.second[1],  boundingBox.first[2]),
            Float3( boundingBox.first[0],  boundingBox.first[1], boundingBox.second[2]),
            Float3(boundingBox.second[0],  boundingBox.first[1], boundingBox.second[2]),
            Float3( boundingBox.first[0], boundingBox.second[1], boundingBox.second[2]),
            Float3(boundingBox.second[0], boundingBox.second[1], boundingBox.second[2])
        };

        Float2 testPts[dimof(pts)];
        for (unsigned c=0; c<dimof(pts); ++c) {
                // Z part is ignored completely
            testPts[c] = Truncate(TransformPoint(localToCircleSpace, pts[c]));
        }

        unsigned faces[6][4] = 
        {
            { 0, 1, 2, 3 },
            { 1, 5, 6, 2 },
            { 5, 4, 7, 6 },
            { 4, 0, 3, 7 },
            { 4, 5, 1, 0 },
            { 3, 2, 6, 7 },
        };
        
            //  We've made 6 rhomboids in 2D. We want to compare these to the circle
            //  at the origin, with the given radius.
            //  We need to find the point on the rhomboid that is closest to the origin.
            //  this will always lie on an edge (unless the origin is actually within the
            //  rhomboid). So we can just find the point on each edge is that closest to
            //  the origin, and see if that's within the radius.

        for (unsigned f=0; f<6; ++f) {
                //  first, is the origin within the rhumboid. Our pts are arranged in 
                //  winding order. So we can use a simple pt in convex polygon test.
            if (PtInConvexPolygon(faces[f], &faces[f][4], testPts, Float2(0.f, 0.f))) {
                return true; // pt is inside, so we have an interestion.
            }

            for (unsigned e=0; e<4; ++e) {
                auto edgeStart  = testPts[faces[f][e]];
                auto edgeEnd    = testPts[faces[f][(e+1)%4]];

                auto pt = PtClosestToOrigin(edgeStart, edgeEnd);
                if (MagnitudeSquared(pt) <= (circleRadius*circleRadius)) {
                    return true; // closest pt is within circle -- so return true
                }
            }
        }

        return false;
    }

    static Float2 Radial2Cart2D(float theta, float r)
    {
        auto t = XlSinCos(theta);
        return r * Float2(std::get<0>(t), std::get<1>(t));
    }

    static bool IsBlueNoiseGoodPoint(const std::vector<Float2>& existingPts, const Float2& testPt, float dRSq)
    {
        for (auto i=existingPts.begin(); i!=existingPts.end(); ++i) {
            if (MagnitudeSquared(testPt - *i) < dRSq) {
                return false;
            }
        }
        return true;
    }

    std::vector<Float2> GenerateBlueNoisePlacements(float radius, unsigned count)
    {
            //  Create new placements arranged in a equally spaced pattern
            //  around the circle.
            //  We're going to use a blue-noise pattern to calculate points
            //  in this circle. This method can be used for generate poisson
            //  disks. Here is the reference:
            //  http://www.cs.ubc.ca/~rbridson/docs/bridson-siggraph07-poissondisk.pdf

            //  We can use an expectation for the ratio of the inner circle radius to the
            //  outer radius to decide on the radius used for the poisson calculations.
            //
            //  From this page, we can see the "ideal" density values for the best circle
            //  packing results. Our method won't produce the ideal results, but so we
            //  need to use an expected density that is smaller.
            //  http://hydra.nat.uni-magdeburg.de/packing/cci/cci.html
        const float expectedDensity = 0.65f;

        const float bigCircleArea = gPI * radius * radius;
        const float littleCircleArea = bigCircleArea * expectedDensity / float(count);
        const float littleCircleRadius = sqrt(littleCircleArea / gPI);
        const float dRSq = 4*littleCircleRadius*littleCircleRadius;

        const unsigned k = 30;
        std::vector<Float2> workingSet;
        workingSet.reserve(count);
        workingSet.push_back(
            Radial2Cart2D(
                rand() * (2.f * gPI / float(RAND_MAX)),
                LinearInterpolate(.125f * radius, .25f * radius, rand() / float(RAND_MAX))));

        const unsigned iterationCount = 2 * count - 1;
        for (unsigned c=0; c<iterationCount && workingSet.size() < count; ++c) {
            assert(!workingSet.empty());
            unsigned index = rand() % unsigned(workingSet.size());

                // look for a good random connector
            bool gotGoodPt = false;
            float startAngle = rand() * (2.f * gPI / float(RAND_MAX));
            for (unsigned t=0; t<k; ++t) {
                Float2 pt = workingSet[index] + Radial2Cart2D(
                    startAngle + float(t) * (2.f * gPI / float(k)),
                    littleCircleRadius * (2.f + rand() / float(RAND_MAX)));

                if (MagnitudeSquared(pt) > radius * radius) {
                    continue;   // bad pt; outside of large radius. We need the centre to be within the large radius
                }

                    // note -- we can accelerate this test with 
                    //         a 2d lookup grid 
                if (IsBlueNoiseGoodPoint(workingSet, pt, dRSq)) {
                    gotGoodPt = true;
                    workingSet.push_back(pt);
                    break;
                }
            }

                // if we couldn't find a good connector, we have to erase the original pt 
            if (!gotGoodPt) {
                workingSet.erase(workingSet.begin() + index);

                    //  Note; there can be weird cases where the original point is remove
                    //  in these cases, we have to add back a new starter point. We can't
                    //  let the working state get empty
                if (workingSet.empty()) {
                    workingSet.push_back(
                        Radial2Cart2D(
                            rand() * (2.f * gPI / float(RAND_MAX)),
                            LinearInterpolate(.125f * radius, .25f * radius, rand() / float(RAND_MAX))));
                }
            }
        }

            // if we didn't get enough, we just have to insert randoms
        while (workingSet.size() < count) {
            auto rndpt = Radial2Cart2D(
                rand() * (2.f * gPI / float(RAND_MAX)),
                rand() * radius / float(RAND_MAX));
            workingSet.push_back(rndpt);
        }

            // todo --  this could benefit from a "relax" phase that would just shift
            //          things into a more evenly spaced arrangement

        return std::move(workingSet);
    }

    void ScatterPlacements::PerformScatter(
        const Float3& centre, const char modelName[], const char materialName[])
    {
            // Our scatter algorithm is a little unique
            //  * find the number of objects with the same model & material within 
            //      a cylinder around the given point
            //  * either add or remove one from that total
            //  * create a new random distribution of items around the centre point
            //  * clamp those items to the terrain surface
            //  * delete the old items, and add those new items to the terrain
            // This way items will usually get scattered around in a good distribution
            // But it's very random. So the artist has only limited control.

        uint64 modelGuid = Hash64(modelName);
        auto oldPlacements = _editor->Find_BoxIntersection(
            centre - Float3(_radius, _radius, _radius),
            centre + Float3(_radius, _radius, _radius),
            [=](const SceneEngine::PlacementsEditor::ObjIntersectionDef& objectDef) -> bool
            {
                if (objectDef._model == modelGuid) {
                        // Make sure the object bounding box intersects with a cylinder around "centre"
                        // box vs cylinder is a little expensive. But since the cylinder axis is just +Z
                        // perhaps we could just treat this as a 2d problem, and just do circle vs rhomboid
                        //
                        // This won't produce the same result as cylinder vs box for the caps of the cylinder
                        //      -- but we don't really care in this case.
                    auto localToCircle = Combine(objectDef._localToWorld, AsFloat3x4(-centre));
                    return AABBVsCircleInXY(_radius, objectDef._localSpaceBoundingBox, localToCircle);
                }

                return false;
            });

            //  We have a list of placements using the same model, and within the placement area.
            //  We want to either add or remove one.

        auto trans = _editor->Transaction_Begin(
            AsPointer(oldPlacements.cbegin()), AsPointer(oldPlacements.cend()));
        for (unsigned c=0; c<trans->GetObjectCount(); ++c) {
            trans->Delete(c);
        }
            
            //  Note that the blur noise method we're using will probably not work 
            //  well with very small numbers of placements. So we're going to limit 
            //  the bottom range.

        auto modelBoundingBox = _editor->GetModelBoundingBox(modelName);
        auto crossSectionArea = 
              (modelBoundingBox.second[0] - modelBoundingBox.first[0]) 
            * (modelBoundingBox.second[1] - modelBoundingBox.first[1]);

        float bigCircleArea = gPI * _radius * _radius;
        auto noisyPts = GenerateBlueNoisePlacements(_radius, unsigned(bigCircleArea*_density/crossSectionArea));

            //  Now add new placements for all of these pts.
            //  We need to clamp them to the terrain surface as we do this

        for (auto p=noisyPts.begin(); p!=noisyPts.end(); ++p) {
            Float2 pt = *p + Truncate(centre);
            float height = SceneEngine::GetTerrainHeight(
                *Sample::MainTerrainFormat.get(), Sample::MainTerrainConfig, Sample::MainTerrainCoords, 
                pt);

            auto objectToWorld = AsFloat4x4(Expand(pt, height));
            Combine_InPlace(RotationZ(rand() * 2.f * gPI / float(RAND_MAX)), objectToWorld);
            trans->Create(SceneEngine::PlacementsEditor::ObjTransDef(
                AsFloat3x4(objectToWorld), modelName, materialName));
        }

        trans->Commit();
    }

    void ScatterPlacements::Render(
        RenderCore::Metal::DeviceContext* context,
        SceneEngine::LightingParserContext& parserContext)
    {
        if (_hasHoverPoint) {
            RenderCylinderHighlight(context, parserContext, _hoverPoint, _radius);
        }
    }

    const char* ScatterPlacements::GetName() const  { return "ScatterPlace"; }

    auto ScatterPlacements::GetFloatParameters() const -> std::pair<FloatParameter*, size_t>  
    {
        static FloatParameter parameters[] = 
        {
            FloatParameter(ManipulatorParameterOffset(&ScatterPlacements::_radius), 1.f, 100.f, FloatParameter::Logarithmic, "Size"),
            FloatParameter(ManipulatorParameterOffset(&ScatterPlacements::_density), 0.01f, 1.f, FloatParameter::Linear, "Density")
        };
        return std::make_pair(parameters, dimof(parameters));
    }

    auto ScatterPlacements::GetBoolParameters() const -> std::pair<BoolParameter*, size_t>    { return std::make_pair(nullptr, 0); }

    void ScatterPlacements::SetActivationState(bool newState) 
    {
        if (_manInterface) {
            _manInterface->EnableSelectedModelDisplay(newState);
        }
    }

    ScatterPlacements::ScatterPlacements(
        IPlacementManipManagerInterface* manInterface,
        std::shared_ptr<SceneEngine::PlacementsEditor> editor)
    {
        _spawnTimer = 0;
        _manInterface = manInterface;
        _editor = std::move(editor);
        _hasHoverPoint = false;
        _hoverPoint = Float3(0.f, 0.f, 0.f);
        _radius = 20.f;
        _density = .1f;
    }
    
    ScatterPlacements::~ScatterPlacements() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    static void DrawQuadDirect(
        RenderCore::Metal::DeviceContext* context, const RenderCore::Metal::ShaderResourceView& srv, 
        Float2 screenMins, Float2 screenMaxs)
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
            "game/xleres/basic.psh:copy_bilinear:" PS_DefShaderModel);
        BoundInputLayout boundVertexInputLayout(std::make_pair(vertexInputLayout, dimof(vertexInputLayout)), shaderProgram);
        context->Bind(boundVertexInputLayout);
        context->Bind(shaderProgram);

        ViewportDesc viewport(*context);
        float constants[] = { 1.f / viewport.Width, 1.f / viewport.Height, 0.f, 0.f };
        ConstantBuffer reciprocalViewportDimensions(constants, sizeof(constants));
        const ShaderResourceView* resources[] = { &srv };
        const ConstantBuffer* cnsts[] = { &reciprocalViewportDimensions };
        BoundUniforms boundLayout(shaderProgram);
        boundLayout.BindConstantBuffer(Hash64("ReciprocalViewportDimensions"), 0, 1);
        boundLayout.BindShaderResource(Hash64("DiffuseTexture"), 0, 1);
        boundLayout.Apply(*context, UniformsStream(), UniformsStream(nullptr, cnsts, dimof(cnsts), resources, dimof(resources)));

        context->Bind(BlendState(BlendOp::Add, Blend::SrcAlpha, Blend::InvSrcAlpha));
        context->Bind(Topology::TriangleStrip);
        context->Draw(dimof(vertices));

        context->UnbindPS<ShaderResourceView>(0, 1);
    }

    static const auto Id_SelectedModel = InteractableId_Make("SelectedModel");
    static const auto Id_PlacementsSave = InteractableId_Make("PlacementsSave");

    static ButtonFormatting ButtonNormalState    ( ColorB(127, 192, 127,  64), ColorB(164, 192, 164, 255) );
    static ButtonFormatting ButtonMouseOverState ( ColorB(127, 192, 127,  64), ColorB(255, 255, 255, 160) );
    static ButtonFormatting ButtonPressedState   ( ColorB(127, 192, 127,  64), ColorB(255, 255, 255,  96) );

    void PlacementsWidgets::Render(
        IOverlayContext* context, Layout& layout, 
        Interactables& interactables, InterfaceState& interfaceState)
    {
        auto controlsRect = DrawManipulatorControls(
            context, layout, interactables, interfaceState,
            *_manipulators[_activeManipulatorIndex], "Placements tools");


            //      Our placement display is simple... 
            //
            //      Draw the name of the selected model at the bottom of
            //      the screen. If we click it, we should pop up the model browser
            //      so we can reselect.

        // const auto lineHeight   = 20u;
        // const auto horizPadding = 75u;
        // const auto vertPadding  = 50u;
        // const auto selectedRectHeight = lineHeight + 20u;
        // auto maxSize = layout.GetMaximumSize();
        // Rect selectedRect(
        //     Coord2(maxSize._topLeft[0] + horizPadding, maxSize._bottomRight[1] - vertPadding - selectedRectHeight),
        //     Coord2(std::min(maxSize._bottomRight[0], controlsRect._bottomRight[1]) - horizPadding, maxSize._bottomRight[1] - vertPadding));
        // 
        // DrawButtonBasic(
        //     context, selectedRect, _selectedModel->_modelName.c_str(),
        //     FormatButton(interfaceState, Id_SelectedModel, 
        //         ButtonNormalState, ButtonMouseOverState, ButtonPressedState));
        // interactables.Register(Interactables::Widget(selectedRect, Id_SelectedModel));
        
        auto maxSize = layout.GetMaximumSize();
        const auto previewSize = _browser ? _browser->GetPreviewSize()[1] : 196;
        const auto margin = layout._paddingInternalBorder;
        const Coord2 iconSize(93/2, 88/2);
        const auto iconPadding = layout._paddingBetweenAllocations;
        const auto buttonsRectPadding = 16;
        const auto buttonsRectVertPad = 4;

        const bool browserActive = _browser && _browserActive;
        if (!browserActive) {

                ///////////////////////   I C O N S   ///////////////////////
            const char* icons[] = 
            {
                "game/xleres/DefaultResources/icon_save.png",
                "game/xleres/DefaultResources/icon_test.png",
                "game/xleres/DefaultResources/icon_test.png",
            };
            const InteractableId iconIds[] = { Id_PlacementsSave, 0, 0 };
            const auto iconCount = dimof(icons);
            const auto toolAreaWidth = std::max(previewSize, int(iconCount * iconSize[0] + (iconCount-1) * iconPadding + 2 * buttonsRectPadding));

            const Rect buttonsArea(
                Coord2(margin, maxSize._bottomRight[1] - margin - iconSize[1] - 2 * buttonsRectVertPad),
                Coord2(margin + toolAreaWidth, maxSize._bottomRight[1] - margin));

            context->DrawQuad(
                ProjectionMode::P2D, 
                AsPixelCoords(Coord2(buttonsArea._topLeft[0], LinearInterpolate(buttonsArea._topLeft[1], buttonsArea._bottomRight[1], 0.5f))),
                AsPixelCoords(buttonsArea._bottomRight), ColorB(0, 0, 0));

            for (unsigned c=0; c<iconCount; ++c) {
                Coord2 iconTopLeft(buttonsArea._topLeft + Coord2(buttonsRectPadding + c * (iconSize[0] + iconPadding), buttonsRectVertPad));
                Rect iconRect(iconTopLeft, iconTopLeft + iconSize);

                context->DrawTexturedQuad(
                    ProjectionMode::P2D, 
                    AsPixelCoords(iconRect._topLeft),
                    AsPixelCoords(iconRect._bottomRight),
                    icons[c]);
                if (iconIds[c]) {
                    interactables.Register(Interactables::Widget(iconRect, iconIds[c]));
                }
            }
        
                ///////////////////////   S E L E C T E D    I T E M   ///////////////////////
            if (!_selectedModel.empty() && _drawSelectedModel) {
                const auto centre = buttonsArea._topLeft + Coord2(toolAreaWidth/2, -margin - previewSize/2);
                const auto temp = centre - Coord2(previewSize/2, previewSize/2);
                const Rect previewRect(temp, temp + Coord2(previewSize, previewSize));

                if (_browser) {
                    auto* devContext = context->GetDeviceContext();
                    SceneEngine::SavedTargets oldTargets(devContext);
                    const RenderCore::Metal::ShaderResourceView* srv = nullptr;
                
                    const char* errorMsg = nullptr;

                    TRY {
                        ucs2 ucs2Filename[MaxPath];
                        utf8_2_ucs2(
                            (const utf8*)AsPointer(_selectedModel.cbegin()), _selectedModel.size(), 
                            ucs2Filename, dimof(ucs2Filename));

                        auto browserSRV = _browser->GetSRV(devContext, ucs2Filename);
                        srv = browserSRV.first;
                    } 
                    CATCH(const ::Assets::Exceptions::InvalidResource&) { errorMsg = "Invalid"; } 
                    CATCH(const ::Assets::Exceptions::PendingResource&) { errorMsg = "Pending"; } 
                    CATCH_END

                    oldTargets.ResetToOldTargets(devContext);

                    if (srv) {
                        DrawQuadDirect(
                            devContext, *srv,
                            Float2(float(previewRect._topLeft[0]), float(previewRect._topLeft[1])), 
                            Float2(float(previewRect._bottomRight[0]), float(previewRect._bottomRight[1])));
                    } else if (errorMsg) {
                        DrawButtonBasic(
                            context, previewRect, errorMsg,
                            FormatButton(interfaceState, Id_SelectedModel, 
                                ButtonNormalState, ButtonMouseOverState, ButtonPressedState));
                    }
                }

                interactables.Register(Interactables::Widget(previewRect, Id_SelectedModel));
            }

        } else {

                ///////////////////////   B R O W S E R   ///////////////////////

            Rect browserRect(maxSize._topLeft, maxSize._bottomRight);
            Layout browserLayout(browserRect);
            _browser->Render(context, browserLayout, interactables, interfaceState);

        }
    }

    bool PlacementsWidgets::ProcessInput(InterfaceState& interfaceState, const InputSnapshot& input)
    {
        if (interfaceState.TopMostId() == Id_SelectedModel && input.IsRelease_LButton()) {
            _browserActive = !_browserActive;
            return true;
        }

        if (_browser && _browserActive) {
            auto result = _browser->SpecialProcessInput(interfaceState, input);
            if (!result._selectedModel.empty()) {
                _selectedModel = result._selectedModel;
                _browserActive = false; // dismiss browser on select
            }

            if (result._consumed) { return true; }
        }

        if (input.IsRelease_LButton()) {
            const auto Id_SelectedManipulatorLeft = InteractableId_Make("SelectedManipulatorLeft");
            const auto Id_SelectedManipulatorRight = InteractableId_Make("SelectedManipulatorRight");
            auto topMost = interfaceState.TopMostWidget();
            auto newManipIndex = _activeManipulatorIndex;

            if (topMost._id == Id_SelectedManipulatorLeft) {            // ---- go back one manipulator ----
                newManipIndex = (_activeManipulatorIndex + _manipulators.size() - 1) % _manipulators.size();
            } else if (topMost._id == Id_SelectedManipulatorRight) {    // ---- go forward one manipulator ----
                newManipIndex = (_activeManipulatorIndex + 1) % _manipulators.size();
            }

            if (newManipIndex != _activeManipulatorIndex) {
                _manipulators[_activeManipulatorIndex]->SetActivationState(false);
                _activeManipulatorIndex = newManipIndex;
                _manipulators[_activeManipulatorIndex]->SetActivationState(true);
                return true;
            }

            if (topMost._id == Id_PlacementsSave) {
                _editor->Save();
                return true;
            }
        }

        if (HandleManipulatorsControls(interfaceState, input, *_manipulators[_activeManipulatorIndex])) {
            return true;
        }

        if (interfaceState.GetMouseOverStack().empty()
            && _manipulators[_activeManipulatorIndex]->OnInputEvent(input, *_hitTestResolver)) {
            return true;
        }

        return false;
    }

    void PlacementsWidgets::RenderToScene(
        RenderCore::Metal::DeviceContext* context, SceneEngine::LightingParserContext& parserContext)
    {
        _manipulators[_activeManipulatorIndex]->Render(context, parserContext);
    }

    std::string PlacementsWidgets::GetSelectedModel() const { return _selectedModel; }
    void PlacementsWidgets::EnableSelectedModelDisplay(bool newState)
    {
        _drawSelectedModel = newState;
    }

    void PlacementsWidgets::SelectModel(const char newModelName[])
    {
        if (newModelName) {
            _selectedModel = newModelName;
        }
    }

    void PlacementsWidgets::SwitchToMode(Mode::Enum newMode)
    {
        auto newActiveManipIndex = _activeManipulatorIndex;
        if (newMode == Mode::PlaceSingle) { newActiveManipIndex = 1; }
        else if (newMode == Mode::Select) { newActiveManipIndex = 0; }

        if (newActiveManipIndex != _activeManipulatorIndex) {
            _manipulators[_activeManipulatorIndex]->SetActivationState(false);
            _activeManipulatorIndex = newActiveManipIndex;
            _manipulators[_activeManipulatorIndex]->SetActivationState(true);
        }
    }

    PlacementsWidgets::PlacementsWidgets(
        std::shared_ptr<SceneEngine::PlacementsEditor> editor, 
        std::shared_ptr<HitTestResolver> hitTestResolver)
    {
        auto browser = std::make_shared<ModelBrowser>("game\\objects\\Env", editor->GetModelFormat());
        _browserActive = false;
        _activeManipulatorIndex = 0;
        _drawSelectedModel = false;

        std::string selectedModel = "game\\objects\\Env\\02_harihara\\001_hdeco\\backpack_mockup.cgf";

        std::vector<std::unique_ptr<IManipulator>> manipulators;
        manipulators.push_back(std::make_unique<SelectAndEdit>(this, editor));
        manipulators.push_back(std::make_unique<PlaceSingle>(this, editor));
        manipulators.push_back(std::make_unique<ScatterPlacements>(this, editor));

        manipulators[0]->SetActivationState(true);

        _editor = std::move(editor);
        _hitTestResolver = std::move(hitTestResolver);        
        _browser = std::move(browser);
        _manipulators = std::move(manipulators);
        _selectedModel = std::move(selectedModel);
    }
    
    PlacementsWidgets::~PlacementsWidgets()
    {
        TRY { 
            _manipulators[_activeManipulatorIndex]->SetActivationState(false);
        } CATCH (...) {
        } CATCH_END
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class PlacementsManipulatorsManager::Pimpl
    {
    public:
        std::shared_ptr<SceneEngine::PlacementsManager>     _placementsManager;
        std::shared_ptr<SceneEngine::PlacementsEditor>      _editor;
        std::shared_ptr<DebugScreensSystem>     _screens;
        std::shared_ptr<PlacementsWidgets>      _placementsDispl;
        std::shared_ptr<HitTestResolver>        _hitTestResolver;
    };

    void PlacementsManipulatorsManager::RenderWidgets(RenderCore::IDevice* device, const Float4x4& viewProjTransform)
    {
        _pimpl->_screens->Render(device, viewProjTransform);
    }

    void PlacementsManipulatorsManager::RenderToScene(
        RenderCore::Metal::DeviceContext* context, SceneEngine::LightingParserContext& parserContext)
    {
        _pimpl->_placementsDispl->RenderToScene(context, parserContext);
    }

    auto PlacementsManipulatorsManager::GetInputLister() -> std::shared_ptr<RenderOverlays::DebuggingDisplay::IInputListener>
    {
        return _pimpl->_screens;
    }

    PlacementsManipulatorsManager::PlacementsManipulatorsManager(
        std::shared_ptr<SceneEngine::PlacementsManager> placementsManager,
        std::shared_ptr<SceneEngine::TerrainManager> terrainManager,
        std::shared_ptr<SceneEngine::ISceneParser> sceneParser,
        std::shared_ptr<SceneEngine::TechniqueContext> techniqueContext)
    {
        auto pimpl = std::make_unique<Pimpl>();
        pimpl->_screens = std::make_shared<DebugScreensSystem>();
        pimpl->_placementsManager = std::move(placementsManager);
        pimpl->_editor = pimpl->_placementsManager->CreateEditor();
        pimpl->_hitTestResolver = std::make_shared<HitTestResolver>(terrainManager, sceneParser, techniqueContext);
        pimpl->_placementsDispl = std::make_shared<PlacementsWidgets>(pimpl->_editor, pimpl->_hitTestResolver);
        pimpl->_screens->Register(pimpl->_placementsDispl, "Placements", DebugScreensSystem::SystemDisplay);
        _pimpl = std::move(pimpl);
    }

    PlacementsManipulatorsManager::~PlacementsManipulatorsManager()
    {}

}

