// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DelayedDrawCall.h"
#include "../Metal/Format.h"
#include "../Metal/Buffer.h"
#include "../Resource.h"

#include "../../Assets/AssetsCore.h"
#include "../../Math/Vector.h"
#include "../../Math/Matrix.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Core/Types.h"
#include "../../Core/SelectConfiguration.h"
#include <vector>

namespace RenderCore { namespace Techniques { class ParsingContext; } }
namespace Assets { class DirectorySearchRules; class PendingCompileMarker; class DependencyValidation; }

namespace RenderCore { namespace Assets
{
    class SharedStateSet;
    class ModelCommandStream;
    class ModelImmutableData;
    class MaterialImmutableData;
    class TransformationMachine;
    class ResolvedMaterial;
    class MaterialScaffold;
    class ModelRendererContext;
    class DelayedDrawCallSet;

    typedef uint64 MaterialGuid;

    /// <summary>Represents the state of animation effects on an object<summary>
    /// AnimationState is a placeholder for containing the states related to
    /// animating vertices in a model.
    class AnimationState
    {
    public:
            // only a single animation supported currently //
        float       _time;
        uint64      _animation;
        AnimationState(float time, uint64 animation) : _time(time), _animation(animation) {}
        AnimationState() {}
    };

////////////////////////////////////////////////////////////////////////////////////////////
    
    /// <summary>Structural data describing a model<summary>
    /// The "scaffold" of a model contains the structural data of a model, without the large
    /// assets and without any platform-api resources.
    ///
    /// Normally the platform api sources are instantiated in a "ModelRenderer". These two
    /// classes work closely together.
    ///
    /// However, a scaffold can be used independently from a renderer. The scaffold is a very
    /// light weight object. That means many can be loaded into memory at a time. It also
    /// means that we can load and query information from model files, without any executing
    /// any platform-specific code (for tools and for extracting metrics information).
    ///
    /// see RenderCore::Assets::Simple::ModelScaffold for more information about the scaffold
    /// concept.
    ///
    /// The ModelScaffold is loaded from a chunk-format file. See the Serialization::ChunkFile
    /// namespace for information about chunk files.
    ///
    /// <seealso cref="ModelRenderer"/>
    class ModelScaffold
    {
    public:
        const std::string&          Filename() const            { return _filename; }
        unsigned                    LargeBlocksOffset() const;
        const ModelCommandStream&   CommandStream() const;
        const ModelImmutableData&   ImmutableData() const;
        const TransformationMachine& EmbeddedSkeleton() const;
        std::pair<Float3, Float3>   GetStaticBoundingBox(unsigned lodIndex = 0) const;
        unsigned                    GetMaxLOD() const { return 0; }

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _validationCallback; }

        void Resolve() const;
        ::Assets::AssetState TryResolve();
        ::Assets::AssetState StallAndResolve();

        static const auto CompileProcessType = ConstHash64<'Mode', 'l'>::Value;

        ModelScaffold(const ::Assets::ResChar filename[]);
        ModelScaffold(std::shared_ptr<::Assets::PendingCompileMarker>&& marker);
        ModelScaffold(ModelScaffold&& moveFrom);
        ModelScaffold& operator=(ModelScaffold&& moveFrom);
        ~ModelScaffold();
    protected:
        std::unique_ptr<uint8[]>    _rawMemoryBlock;
        const ModelImmutableData*   _data;
        std::string                 _filename;
        unsigned                    _largeBlocksOffset;

        mutable std::shared_ptr<::Assets::PendingCompileMarker>     _marker;
        std::shared_ptr<::Assets::DependencyValidation>     _validationCallback;

        void CompleteFromMarker(::Assets::PendingCompileMarker& marker);
    };

    class AnimationImmutableData;
    class SkeletonBinding;
    class PreparedModelDrawCalls;

    /// <summary>Creates platform resources and renders a model<summary>
    /// ModelRenderer is used to render a model. Though the two classes work together, it is 
    /// a more heavy-weight object than ModelScaffold. When the ModelRenderer is created, it
    /// will allocate the platform api resources (including index and vertex buffers).
    ///
    /// Normally animation is applied to a model with a 2-step process.
    ///     <list>
    ///         <item> PrepareAnimation applies the vertex animation and generates a temporary vertex buffer
    ///         <item> Render will render the model into the scene (or shadows, or off-screen buffer, etc)
    ///     </list>
    ///
    /// <seealso cref="ModelScaffold" />
    class ModelRenderer
    {
    public:
        class MeshToModel
        {
        public:
            const Float4x4*         _skeletonOutput;
            unsigned                _skeletonOutputCount;
            const SkeletonBinding*  _skeletonBinding;
            Float4x4                GetMeshToModel(unsigned transformMarker) const;
            MeshToModel();
            MeshToModel(const Float4x4 skeletonOutput[], unsigned skeletonOutputCount,
                        const SkeletonBinding* binding = nullptr);
        };

            ////////////////////////////////////////////////////////////
        class PreparedAnimation;
        void Render(
            const ModelRendererContext& context,
            const SharedStateSet&   sharedStateSet,
            const Float4x4&         modelToWorld,
            const MeshToModel*      transforms = nullptr,
            PreparedAnimation*      preparedAnimation = nullptr) const;

            ////////////////////////////////////////////////////////////
        void Prepare(
            DelayedDrawCallSet& dest, 
            const SharedStateSet& sharedStateSet, 
            const Float4x4& modelToWorld,
            const MeshToModel* transforms = nullptr);

        static void RenderPrepared(
            const ModelRendererContext& context, const SharedStateSet& sharedStateSet,
            DelayedDrawCallSet& drawCalls, DelayStep delayStep);

        static void RenderPrepared(
            const ModelRendererContext& context, const SharedStateSet& sharedStateSet,
            DelayedDrawCallSet& drawCalls, DelayStep delayStep,
            const std::function<void(unsigned, unsigned, unsigned)>& callback);

            ////////////////////////////////////////////////////////////
        class PreparedAnimation
        {
        public:
            std::unique_ptr<Float4x4[]> _finalMatrices;
            Metal::VertexBuffer         _skinningBuffer;
            AnimationState              _animState;
            std::vector<unsigned>       _vbOffsets;

            PreparedAnimation();
            PreparedAnimation(PreparedAnimation&&);
            PreparedAnimation& operator=(PreparedAnimation&&);
            PreparedAnimation(const PreparedAnimation&) = delete;
            PreparedAnimation& operator=(const PreparedAnimation&) = delete;
        };

        PreparedAnimation CreatePreparedAnimation() const;

        void PrepareAnimation(
            Metal::DeviceContext* context, 
            PreparedAnimation& state, 
            const SkeletonBinding& skeletonBinding) const;

        static bool CanDoPrepareAnimation(Metal::DeviceContext* context);

        std::vector<MaterialGuid> DrawCallToMaterialBinding() const;
        MaterialGuid GetMaterialBindingForDrawCall(unsigned drawCallIndex) const;
        void LogReport() const;

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const { return _validationCallback; }

            ////////////////////////////////////////////////////////////
        ModelRenderer(
            const ModelScaffold& scaffold, const MaterialScaffold& matScaffold,
            SharedStateSet& sharedStateSet, 
            const ::Assets::DirectorySearchRules* searchRules = nullptr, unsigned levelOfDetail = 0);
        ~ModelRenderer();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;

        std::shared_ptr<::Assets::DependencyValidation>   _validationCallback;

        template<bool HasCallback>
            static void RenderPreparedInternal(
                const ModelRendererContext&, const SharedStateSet&,
                DelayedDrawCallSet&, DelayStep, 
                const std::function<void(unsigned, unsigned, unsigned)>*);
    };

////////////////////////////////////////////////////////////////////////////////////////////

    /// <summary>Structural data for a skeleton<summary>
    /// Represents the skeleton of a model.
    ///
    /// Animated models are split into 3 parts:
    ///     <list>
    ///         <item> AnimationSetScaffold
    ///         <item> SkeletonScaffold
    ///         <item> ModelScaffold (skin)
    ///     </list>
    ///
    /// Each is bound to other using interfaces of strings. The AnimationSetScaffold provides
    /// the current state of animatable parameters. The SkeletonScaffold converts those
    /// parameters into a set of low level local-to-world transformations. And finally, the 
    /// ModelScaffold knows how to render a skin over the transformations.
    ///
    /// Here, SkeletonScaffold is intentionally designed with a flattened non-hierarchical
    /// data structure. In the 3D editing tool, the skeleton will be represented by a 
    /// hierarchy of nodes. But in our run-time representation, that hierarchy has become a 
    /// a linear list of instructions, with push/pop operations. It's similar to converting
    /// a recursive method into a loop with a stack.
    ///
    /// The vertex weights are defined in the ModelScaffold. The skeleton only defines 
    /// information related to the bones, not the vertices bound to them.
    ///
    /// <seealso cref="ModelScaffold" />
    /// <seealso cref="AnimationSetScaffold" />
    class SkeletonScaffold
    {
    public:
        const std::string&              Filename() const                    { return _filename; }
        const TransformationMachine&    GetTransformationMachine() const    { return *_data; };

        static const auto CompileProcessType = ConstHash64<'Skel', 'eton'>::Value;

        SkeletonScaffold(const ::Assets::ResChar filename[]);
        SkeletonScaffold(SkeletonScaffold&& moveFrom);
        SkeletonScaffold& operator=(SkeletonScaffold&& moveFrom);
        ~SkeletonScaffold();
    protected:
        std::unique_ptr<uint8[]>        _rawMemoryBlock;
        const TransformationMachine*    _data;
        std::string                     _filename;
    };

    /// <summary>Structural data for animation<summary>
    /// Represents a set of animation that can potentially be applied to a skeleton.
    ///
    /// See SkeletonScaffold for more information.
    ///
    /// <seealso cref="ModelScaffold" />
    /// <seealso cref="SkeletonScaffold" />
    class AnimationSetScaffold
    {
    public:
        const std::string&              Filename() const        { return _filename; }
        const AnimationImmutableData&   ImmutableData() const   { return *_data; };

        static const auto CompileProcessType = ConstHash64<'Anim', 'Set'>::Value;

        AnimationSetScaffold(const ::Assets::ResChar filename[]);
        AnimationSetScaffold(AnimationSetScaffold&& moveFrom);
        AnimationSetScaffold& operator=(AnimationSetScaffold&& moveFrom);
        ~AnimationSetScaffold();
    protected:
        std::unique_ptr<uint8[]>        _rawMemoryBlock;
        const AnimationImmutableData*   _data;
        std::string                     _filename;
    };

    /// <summary>Bind together a model, animation set and skeleton for rendering<summary>
    /// Before we can apply animation to a model, we need to first bind together the
    /// model, animation set and skeleton.
    ///
    /// We do this at load-time, because we sometimes want to share skeletons or 
    /// animations between multiple models. By binding at load time, we can allow
    /// the run-time code to mix and match different resources.
    ///
    /// However, the model, animation set and skeleton are all bound using interfaces
    /// of string names. Binding is quick, but we don't want to do it every frame. This
    /// object allows us to prepare the binding once.
    ///
    /// The SkinPrepareMachine is similar in concept to the ModelRenderer. It takes
    /// the scaffold resources and prepares some extra information in the constructor.
    ///
    /// So, for a fully rendered character, we need 3 scaffolds:
    ///     <list>
    ///         <item> AnimationSetScaffold
    ///         <item> SkeletonScaffold
    ///         <item> ModelScaffold (skin)
    ///     </list>
    ///
    /// And we also need our 2 "run-time" classes:
    ///     <list>
    ///         <item> ModelRenderer
    ///         <item> SkinPrepareMachine
    ///     </list>
    ///
    /// So final rendering of a skinned model will look a little like this:
    /// <code>
    ///     auto preparedAnim = _modelRenderer->PreallocateState();
    ///     preparedAnim._animState = RenderCore::Assets::AnimationState(animTime, animIndex);
    ///
    ///         // SkinPrepareMachine::PrepareAnimation applies the given 
    ///         // animation state to the skeleton.
    ///     _skinPrepareMachine->PrepareAnimation(context, preparedAnim);
    ///
    ///         // ModelRenderer::PrepareAnimation applies the given skeleton
    ///         // state to the vertices.
    ///     _modelRenderer->PrepareAnimation(context, preparedAnim, _skinPrepareMachine->GetSkeletonBinding());
    ///
    ///         // Finally ModelRenderer::Render draws the object
    ///     _modelRenderer->Render(ModelRenderer::Context(...), Identity<Float4x4>(), nullptr, &preparedAnim);
    /// </code>
    ///
    /// Of course, this structure allows flexibility for caching and scheduling as
    /// needed.
    class SkinPrepareMachine
    {
    public:
        void PrepareAnimation(  Metal::DeviceContext* context, 
                                ModelRenderer::PreparedAnimation& state) const;
        const SkeletonBinding& GetSkeletonBinding() const;
        unsigned GetSkeletonOutputCount() const;

        void RenderSkeleton(
                Metal::DeviceContext* context, 
                Techniques::ParsingContext& parserContext, 
                const AnimationState& animState, const Float4x4& localToWorld);
        
        SkinPrepareMachine(const ModelScaffold&, const AnimationSetScaffold&, const SkeletonScaffold&);
        ~SkinPrepareMachine();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

}}

