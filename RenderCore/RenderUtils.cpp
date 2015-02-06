// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RenderUtils.h"
#include "Resource.h"
#include "Metal/Shader.h"

#include "../../Utility/StringFormat.h"
#include "../../Math/Transformations.h"
#include "../../Math/ProjectionMath.h"

namespace RenderCore
{
    Float3 NegativeLightDirection    = Normalize(Float3(0.f, 1.0f, 1.f));

    namespace Exceptions
    {
        GenericFailure::GenericFailure(const char what[]) : ::Exceptions::BasicLabel(what) {}

        AllocationFailure::AllocationFailure(const char what[]) 
        : GenericFailure(what) 
        {}
    }

    CameraDesc::CameraDesc()
    {
        _cameraToWorld          = Identity<Float4x4>();
        _nearClip               = 0.1f;
        _farClip                = 100000.f;
        _verticalFieldOfView    = Deg2Rad(34.8246f);
    }


    Float4x4 PerspectiveProjection(
        float verticalFOV, float aspectRatio,
        float nearClipPlane, float farClipPlane,
        GeometricCoordinateSpace::Enum coordinateSpace,
        ClipSpaceType::Enum clipSpaceType )
    {

            //
            //      Generate a perspective projection matrix with the given
            //      parameters.
            //
            //      Note that we have a few things to consider:
            //
            //          Depth range for homogeneous clip space:
            //              OpenGL defines valid clip space depths as -w<z<w
            //              But in DirectX, we need to use 0<z<w
            //              (in other words, OpenGL straddles 0, while DirectX doesn't)
            //          It's a bit odd, but we're kind of stuck with it.
            //
            //      We're assuming the "camera forward" direction as -Z in camera
            //      space. This is the Collada standard... I'm sure how common that
            //      is.
            //
            //      After transformation, +Z will be away from the viewer.
            //      (ie, increasing Z values mean greater depth)
            //
            //      The caller can choose a left handed or right handed coordinate system
            //      (this will just flip the image horizontally).
            //  
            //      We always use "verticalFOV" and an aspect ratio to define the
            //      viewing angles. This tends to make the most sense to the viewer when
            //      they are (for example) resizing a window. In that case, normally
            //      verticalFOV should stay static, while the aspect ratio will change
            //      (ie horizontal viewing angle will adapt to the dimensions of the window)
            //
            //      BTW, verticalFOV should be in radians, and is the half angle
            //      (ie, it's the angle between the centre ray and the edge of the screen, not
            //      from one edge of the screen to the other)
            //
            //      This code doesn't support skewed or off centre projections for multi-screen
            //      output.
            //      See this link for a generalised transform: 
            //              http://csc.lsu.edu/~kooima/pdfs/gen-perspective.pdf
            //

        const float n = nearClipPlane;
        const float h = n * XlTan(.5f * verticalFOV);
        const float w = h * aspectRatio;
        float l, r;
        const float t = h, b = -h;

        if (coordinateSpace == GeometricCoordinateSpace::LeftHanded) {
            l = w; r = -w;
        } else {
            l = -w; r = w;
        }

        return PerspectiveProjection(l, t, r, b, nearClipPlane, farClipPlane, clipSpaceType);
    }

    Float4x4 PerspectiveProjection(
        float l, float t, float r, float b,
        float nearClipPlane, float farClipPlane,
        ClipSpaceType::Enum clipSpaceType )
    {
        const float n = nearClipPlane;
        const float f = farClipPlane;

        Float4x4 result = Identity<Float4x4>();
        result(0,0) =  (2.f * n) / (r-l);
        result(0,2) =  (r+l) / (r-l);

        result(1,1) =  (2.f * n) / (t-b);
        result(1,2) =  (t+b) / (t-b);

        if (clipSpaceType == ClipSpaceType::Positive) {
                //  This is the D3D view of clip space
                //      0<z/w<1
            result(2,2) =    -(f) / (f-n);            // (note z direction flip here as well as below)
            result(2,3) =  -(f*n) / (f-n);
        } else {
                //  This is the OpenGL view of clip space
                //      -1<w/z<1
            result(2,2) =        (f+n) / (f-n);
            result(2,3) =  -(-2.f*f*n) / (f-n);
        }

        result(3,2) =   -1.f;    // (-1 required to flip space around from -Z camera forward to (z/w) increasing with distance)
        result(3,3) =   0.f;

            //
            //      Both OpenGL & DirectX expect a left-handed coordinate system post-projection
            //          +X is right
            //          +Y is up (ie, coordinates are bottom-up)
            //          +Z is into the screen (increasing values are increasing depth, negative depth values are behind the camera)
            //

        return result;
    }
    
    ClipSpaceType::Enum GetDefaultClipSpaceType()
    {
            // (todo -- this condition could be a runtime test)
        #if (GFXAPI_ACTIVE == GFXAPI_DX11) || (GFXAPI_ACTIVE == GFXAPI_DX9)         
            return ClipSpaceType::Positive;
        #else
            return ClipSpaceType::StraddlingZero;
        #endif
    }

    Float4x4 PerspectiveProjection(
        const RenderCore::CameraDesc& sceneCamera, float viewportAspect)
    {
        return PerspectiveProjection(
            sceneCamera._verticalFieldOfView, viewportAspect,
            sceneCamera._nearClip, sceneCamera._farClip, GeometricCoordinateSpace::RightHanded, 
            GetDefaultClipSpaceType());
    }

    Float4x4 OrthogonalProjection(
        float l, float t, float r, float b,
        float nearClipPlane, float farClipPlane,
        GeometricCoordinateSpace::Enum coordinateSpace,
        ClipSpaceType::Enum clipSpaceType)
    {
        const float n = nearClipPlane;
        const float f = farClipPlane;

        Float4x4 result = Identity<Float4x4>();
        result(0,0) =  2.f / (r-l);
        result(0,3) =  .5f * (r+l);

        result(1,1) =  2.f / (t-b);
        result(1,3) =  .5f * (t+b);

        if (clipSpaceType == ClipSpaceType::Positive) {
                //  This is the D3D view of clip space
                //      0<z/w<1
            result(2,2) =  -1.f / (f-n);            // (note z direction flip here)
            result(2,3) =   -n / (f-n);
        } else {
            assert(0);
        }

            //
            //      Both OpenGL & DirectX expect a left-handed coordinate system post-projection
            //          +X is right
            //          +Y is up (ie, coordinates are bottom-up)
            //          +Z is into the screen (increasing values are increasing depth, negative depth values are behind the camera)
            //

        return result;
    }

    std::pair<float, float> CalculateNearAndFarPlane(
        const Float4& minimalProjection, ClipSpaceType::Enum clipSpaceType)
    {
            // Given a "minimal projection", figure out the near and far plane
            // that was used to create this projection matrix (assuming it was a 
            // perspective projection created with the function 
            // "PerspectiveProjection"
            //
            // Note that the "minimal projection" can be got from a projection
            // matrix using the "ExtractMinimalProjection" function.
            //
            // We just need to do some algebra to reverse the calculations we
            // used to build the perspective transform matrix.
            //
            // For ClipSpaceType::Positive:
            //      miniProj[2] = A = -f / (f-n)
            //      miniProj[3] = B = -(f*n) / (f-n)
            //      C = B / A = n
            //      A * (f-n) = -f
            //      Af - An = -f
            //      Af + f = An
            //      (A + 1) * f = An
            //      f = An / (A+1)
            //        = B / (A+1)
            //
            // For ClipSpaceType::StraddlingZero
            //      miniProj[2] = A = (f+n) / (f-n)
            //      miniProj[3] = B = (2fn) / (f-n)
            //      n = B / (A + 1)
            //      f = B / (A - 1)

        const float A = minimalProjection[2];
        const float B = minimalProjection[3];
        if (clipSpaceType == ClipSpaceType::Positive) {
            return std::make_pair(B / A, B / (A + 1.f));
        } else {
            return std::make_pair(B / (A + 1.f), B / (A - 1.f));
        }
    }

    std::pair<float, float> CalculateNearAndFarPlane_Ortho(
        const Float4& minimalProjection, ClipSpaceType::Enum clipSpaceType)
    {
            // For ClipSpaceType::Positive:
            //      miniProj[2] = A = -1 / (f-n)
            //      miniProj[3] = B = -n / (f-n)
            //      C = B / A = n

        const float A = minimalProjection[2];
        const float B = minimalProjection[3];
        if (clipSpaceType == ClipSpaceType::Positive) {
            return std::make_pair(B / A, (B - 1.f) / A);
        } else {
            assert(0);
            return std::make_pair(0.f, 0.f);
        }
    }

    Float2 CalculateDepthProjRatio_Ortho(
        const Float4& minimalProjection, ClipSpaceType::Enum clipSpaceType)
    {
        auto nearAndFar = CalculateNearAndFarPlane_Ortho(minimalProjection, clipSpaceType);
        return Float2(    1.f / (nearAndFar.second - nearAndFar.first),
            -nearAndFar.first / (nearAndFar.second - nearAndFar.first));
    }

    std::pair<Float3, Float3> BuildRayUnderCursor(
        Int2 mousePosition, 
        Float3 absFrustumCorners[], 
        const Float3& cameraPosition,
        float nearClip, float farClip,
        const std::pair<Float2, Float2>& viewport)
    {
        float u = (float(mousePosition[0]) - viewport.first[0]) / (viewport.second[0] - viewport.first[0]);
        float v = (float(mousePosition[1]) - viewport.first[1]) / (viewport.second[1] - viewport.first[1]);
        float w0 = (1.0f - u) * (1.0f - v);
        float w1 = (1.0f - u) * v;
        float w2 = u * (1.0f - v);
        float w3 = u * v;

        Float3 direction = 
              w0 * (absFrustumCorners[0] - cameraPosition)
            + w1 * (absFrustumCorners[1] - cameraPosition)
            + w2 * (absFrustumCorners[2] - cameraPosition)
            + w3 * (absFrustumCorners[3] - cameraPosition)
            ;
        direction = Normalize(direction);

        return std::make_pair(
            cameraPosition + nearClip * direction,
            cameraPosition + farClip * direction);
    }

    std::pair<Float3, Float3> BuildRayUnderCursor(
        Int2 mousePosition, RenderCore::CameraDesc& sceneCamera, 
        const std::pair<Float2, Float2>& viewport)
    {
            // calculate proper worldToProjection for this cameraDesc and viewport
            //      -- then get the frustum corners. We can use these to find the
            //          correct direction from the view position under the given 
            //          mouse position
        Float3 frustumCorners[8];
        const float viewportAspect = (viewport.second[0] - viewport.first[0]) / float(viewport.second[1] - viewport.first[1]);
        auto projectionMatrix = PerspectiveProjection(sceneCamera, viewportAspect);

        auto worldToProjection = Combine(InvertOrthonormalTransform(sceneCamera._cameraToWorld), projectionMatrix);
        CalculateAbsFrustumCorners(frustumCorners, worldToProjection);

        Float3 cameraPosition = ExtractTranslation(sceneCamera._cameraToWorld);
        return BuildRayUnderCursor(
            mousePosition, frustumCorners, cameraPosition, 
            sceneCamera._nearClip, sceneCamera._farClip,
            viewport);
    }

    SharedPkt MakeLocalTransformPacket(const Float4x4& localToWorld, const CameraDesc& camera)
    {
        return MakeLocalTransformPacket(localToWorld, ExtractTranslation(camera._cameraToWorld));
    }

    LocalTransform MakeLocalTransform(const Float4x4& localToWorld, const Float3& worldSpaceCameraPosition)
    {
        LocalTransform localTransform;
        CopyTransform(localTransform._localToWorld, localToWorld);
        auto worldToLocal = InvertOrthonormalTransform(localToWorld);
        localTransform._localSpaceView = TransformPoint(worldToLocal, worldSpaceCameraPosition);
        localTransform._localNegativeLightDirection = TransformDirectionVector(worldToLocal, NegativeLightDirection);
        return localTransform;
    }

    SharedPkt MakeLocalTransformPacket(const Float4x4& localToWorld, const Float3& worldSpaceCameraPosition)
    {
        return MakeSharedPkt(MakeLocalTransform(localToWorld, worldSpaceCameraPosition));
    }


    SharedPkt::SharedPkt(MiniHeap::Allocation alloc, size_t size)
    : Allocation(alloc), _size(size)
    {
            // Careful --   first initialization never addrefs!
            //              this is because allocations will return an 
            //              object with reference count of 1
    }

    SharedPkt::SharedPkt(const SharedPkt& cloneFrom)
    : Allocation(cloneFrom), _size(cloneFrom._size)
    {
        GetHeap().AddRef(*this);
    }

    SharedPkt::~SharedPkt()
    {
        if (_allocation != nullptr) {
            GetHeap().Release(*this);
        }
    }

    SharedPkt MakeSharedPkt(size_t size)
    {
        auto& heap = SharedPkt::GetHeap();
        return SharedPkt(heap.Allocate(size), size);
    }

    SharedPkt MakeSharedPkt(const void* begin, const void* end)
    {
        auto& heap = SharedPkt::GetHeap();
        auto size = size_t(ptrdiff_t(end) - ptrdiff_t(begin));
        SharedPkt pkt(heap.Allocate(size), size);
        if (pkt.begin()) {
            XlCopyMemory(pkt.begin(), begin, size);
        }
        return std::move(pkt);
    }

    MiniHeap& SharedPkt::GetHeap()
    {
            // \todo -- this has to be done in a DLL safe way.
            //          There should be a single heap like this
            //          that is used by the whole system
        static MiniHeap MainHeap;
        return MainHeap;
    }

}


