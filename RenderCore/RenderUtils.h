// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Utility/PtrUtils.h"
#include "../Utility/MiniHeap.h"
#include "../Math/Matrix.h"
#include "../Core/Exceptions.h"
#include "../Core/Types.h"

namespace RenderCore
{
    namespace Exceptions
    {
        /// <summary>Some unspecific failure during rendering</summary>
        /// This exception should be used in cases that relate to an underlying
        /// graphics API. It could mean there is a compatibility issue, or a
        /// failure of the hardware.
        class GenericFailure : public ::Exceptions::BasicLabel
        {
        public: 
            GenericFailure(const char what[]);
        };

        /// <summary>Failure creating object</summary>
        /// Failure while attempting to create an object. This can sometimes be
        /// due to insufficient resources (ie, out of memory).
        /// But it could also happen if the IDevice is in an invalid state.
        /// This could potentially happen if (for example):
        ///     <list>
        ///         <item>Hardware fault or driver crash</item>
        ///         <item>Plug-and-play graphics card disconnected</item>
        ///         <item>Driver is unavailable (because during a driver install or similar operation)</item>
        ///     </list>
        /// These cases result in a "lost device" type state in DirectX11. While
        /// this is much more rare in DirectX11 than DirectX9, it can still happen.
        class AllocationFailure : public GenericFailure
        {
        public: 
            AllocationFailure(const char what[]);
        };
    }

    class CameraDesc
    {
    public:
        Float4x4    _cameraToWorld;
        float       _verticalFieldOfView;
        float       _nearClip, _farClip;

        Float4x4    _temporaryMatrix;       // (temporary placeholder for a hack)

        CameraDesc();
    };

    class GlobalTransform
    {
    public:
        Float4x4    _worldToClip;
        Float4      _frustumCorners[4];
        Float3      _worldSpaceView;
        float       _farClip;
        Float4      _minimalProjection;
        Float4x4    _viewToWorld;
    };

    struct LocalTransform
    {
    public:
        Float3x4    _localToWorld;
        Float3      _localSpaceView;
        float       _dummy;
        Float3      _localNegativeLightDirection;
        float       _dummy2;
    };

    namespace GeometricCoordinateSpace      { enum Enum { LeftHanded,       RightHanded };  }
    namespace ClipSpaceType                 { enum Enum { StraddlingZero,   Positive };     }
    Float4x4 PerspectiveProjection(
        float verticalFOV, float aspectRatio,
        float nearClipPlane, float farClipPlane,
        GeometricCoordinateSpace::Enum coordinateSpace,
        ClipSpaceType::Enum clipSpaceType);

    Float4x4 PerspectiveProjection(
        float l, float t, float r, float b,
        float nearClipPlane, float farClipPlane,
        ClipSpaceType::Enum clipSpaceType);

    Float4x4 PerspectiveProjection(
        const RenderCore::CameraDesc& sceneCamera, float viewportAspect);

    Float4x4 OrthogonalProjection(
        float l, float t, float r, float b,
        float nearClipPlane, float farClipPlane,
        GeometricCoordinateSpace::Enum coordinateSpace,
        ClipSpaceType::Enum clipSpaceType);

    ClipSpaceType::Enum GetDefaultClipSpaceType();

    std::pair<float, float> CalculateNearAndFarPlane(
        const Float4& minimalProjection, ClipSpaceType::Enum clipSpaceType);
    Float2 CalculateDepthProjRatio_Ortho(
        const Float4& minimalProjection, ClipSpaceType::Enum clipSpaceType);

    std::pair<Float3, Float3> BuildRayUnderCursor(
        Int2 mousePosition, RenderCore::CameraDesc& sceneCamera, 
        const std::pair<Float2, Float2>& viewport);

    std::pair<Float3, Float3> BuildRayUnderCursor(
        Int2 mousePosition, 
        Float3 absFrustumCorners[], 
        const Float3& cameraPosition,
        float nearClip, float farClip,
        const std::pair<Float2, Float2>& viewport);

    /// <summary>A reference counted packet for small allocations</summary>
    /// This object manage a small packet of reference counted memory in
    /// a way that reduces fragmentation and contention for the main heap.
    ///
    /// Often we need to create small packets for constant buffer uploads.
    /// We want those packets to be variable sized (but typically multiples
    /// of 8), and have reference counting attached. Most of the time the
    /// allocations might be created and destroyed during a single frame 
    /// (but sometimes the allocations can last longer).
    ///
    /// This object uses a dedicated heap that is designed to deal with many
    /// small constant-buffer sized packets. It performs thread safe reference
    /// counting, so packets can be passed between threads safely.
    ///
    /// By using a separate heap, we avoid polluting the main heap with these
    /// smaller (often short-lived) allocations.
    ///
    /// SharedPkt has proper value semantics (it works like a smart pointer, 
    /// but with a size attached). Prefer to use the "move" type methods to
    /// avoid unnecessary AddRef/Releases.
    class SharedPkt : public MiniHeap::Allocation
    {
    public:
        void* get() const never_throws { return _allocation; }
        operator bool() const never_throws { return _allocation != nullptr; }

        void* begin() { return _allocation; }
        void* end() { return PtrAdd(_allocation, _size); }
        size_t size() { return _size; }

        SharedPkt() never_throws;
        SharedPkt(const SharedPkt& cloneFrom);
        SharedPkt& operator=(const SharedPkt& cloneFrom);
        SharedPkt(SharedPkt&& moveFrom) never_throws;
        SharedPkt& operator=(SharedPkt&& moveFrom) never_throws;
        ~SharedPkt();

        friend SharedPkt MakeSharedPkt(size_t size);
        friend SharedPkt MakeSharedPkt(const void* begin, const void* end);

        void swap(SharedPkt& other) never_throws;
    private:
        SharedPkt(MiniHeap::Allocation alloc, size_t size);
        size_t _size;
        static MiniHeap& GetHeap();
    };

    SharedPkt MakeLocalTransformPacket(const Float4x4& localToWorld, const CameraDesc& camera);
    SharedPkt MakeLocalTransformPacket(const Float4x4& localToWorld, const Float3& worldSpaceCameraPosition);
    LocalTransform MakeLocalTransform(const Float4x4& localToWorld, const Float3& worldSpaceCameraPosition);

    template<typename T> SharedPkt MakeSharedPkt(const T& input)
    {
        return MakeSharedPkt(&input, PtrAdd(&input, sizeof(T)));
    }

    inline SharedPkt::SharedPkt(SharedPkt&& moveFrom) never_throws
    {
        _allocation = nullptr;
        _marker = ~uint32(0x0);
        _size = 0;
        swap(moveFrom);
    }

    inline SharedPkt::SharedPkt() never_throws
    {
        _allocation = nullptr;
        _marker = ~uint32(0x0);
        _size = 0;
    }

    inline SharedPkt& SharedPkt::operator=(const SharedPkt& cloneFrom)
    {
        SharedPkt(cloneFrom).swap(*this);
        return *this;
    }

    inline SharedPkt& SharedPkt::operator=(SharedPkt&& moveFrom) never_throws
    {
        SharedPkt(moveFrom).swap(*this);
        return *this;
    }

    inline void SharedPkt::swap(SharedPkt& other) never_throws
    {
        std::swap(other._allocation, _allocation);
        std::swap(other._marker, _marker);
        std::swap(other._size, _size);
    }
}


