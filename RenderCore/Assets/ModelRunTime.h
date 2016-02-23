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
#include "../../Assets/ChunkFileAsset.h"
#include "../../Math/Vector.h"
#include "../../Math/Matrix.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/IteratorUtils.h"    // for IteratorRange
#include "../../Core/Types.h"
#include "../../Core/SelectConfiguration.h"
#include <vector>

namespace RenderCore { namespace Techniques { class ParsingContext; } }
namespace Assets { class DirectorySearchRules; class ICompileMarker; class DependencyValidation; }

namespace RenderCore { namespace Assets
{
    class SharedStateSet;
    class ModelCommandStream;
    class TransformationMachine;
    class ModelRendererContext;
    class SkeletonBinding;
    
    class MaterialScaffold;
    class ModelSupplementScaffold;

    class AnimationImmutableData;
    class ModelImmutableData;
    class ModelSupplementImmutableData;

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

    /// <summary>Structural data describing a model</summary>
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
    class ModelScaffold : public ::Assets::ChunkFileAsset
    {
    public:
        unsigned                        LargeBlocksOffset() const;
        const ModelCommandStream&       CommandStream() const;
        const ModelImmutableData&       ImmutableData() const;
        const TransformationMachine&    EmbeddedSkeleton() const;
        std::pair<Float3, Float3>       GetStaticBoundingBox(unsigned lodIndex = 0) const;
        unsigned                        GetMaxLOD() const;

        static const auto CompileProcessType = ConstHash64<'Mode', 'l'>::Value;

        ModelScaffold(const ::Assets::ResChar filename[]);
        ModelScaffold(std::shared_ptr<::Assets::ICompileMarker>&& marker);
        ModelScaffold(ModelScaffold&& moveFrom) never_throws;
        ModelScaffold& operator=(ModelScaffold&& moveFrom) never_throws;
        ~ModelScaffold();

    private:
        std::unique_ptr<uint8[]>    _rawMemoryBlock;
        unsigned                    _largeBlocksOffset;

        static void Resolver(void*, IteratorRange<::Assets::AssetChunkResult*>);
        const ModelImmutableData*   TryImmutableData() const;
    };
    
    class MeshToModel
    {
    public:
        const Float4x4*         _skeletonOutput;
        unsigned                _skeletonOutputCount;
        const SkeletonBinding*  _skeletonBinding;

        Float4x4    GetMeshToModel(unsigned transformMarker) const;
        bool        IsGood() const { return _skeletonOutput != nullptr; }

        MeshToModel();
        MeshToModel(const Float4x4 skeletonOutput[], unsigned skeletonOutputCount,
                    const SkeletonBinding* binding = nullptr);
        MeshToModel(const ModelScaffold&);
    };

    /// <summary>Creates platform resources and renders a model</summary>
    /// ModelRenderer is used to render a model. Though the two classes work together, it is 
    /// a more heavy-weight object than ModelScaffold. When the ModelRenderer is created, it
    /// will allocate the platform api resources (including index and vertex buffers).
    ///
    /// Normally animation is applied to a model with a 2-step process.
    ///     <list>
    ///         <item> PrepareAnimation applies the vertex animation and generates a temporary vertex buffer </item>
    ///         <item> Render will render the model into the scene (or shadows, or off-screen buffer, etc) </item>
    ///     </list>
    ///
    /// <seealso cref="ModelScaffold" />
    class ModelRenderer
    {
    public:
            ////////////////////////////////////////////////////////////
        class PreparedAnimation;
        void Render(
            const ModelRendererContext& context,
            const SharedStateSet&   sharedStateSet,
            const Float4x4&         modelToWorld,
            const MeshToModel&      transforms = MeshToModel(),
            PreparedAnimation*      preparedAnimation = nullptr) const;

            ////////////////////////////////////////////////////////////
        void Prepare(
            DelayedDrawCallSet& dest, 
            const SharedStateSet& sharedStateSet, 
            const Float4x4& modelToWorld,
            const MeshToModel& transforms = MeshToModel()) const;

        static void RenderPrepared(
            const ModelRendererContext& context, const SharedStateSet& sharedStateSet,
            const DelayedDrawCallSet& drawCalls, DelayStep delayStep);

        class DrawCallEvent
        {
        public:
            unsigned _indexCount, _firstIndex;
            unsigned _firstVertex, _drawCallIndex;
        };

        static void RenderPrepared(
            const ModelRendererContext& context, const SharedStateSet& sharedStateSet,
            const DelayedDrawCallSet& drawCalls, DelayStep delayStep,
            const std::function<void(DrawCallEvent)>& callback);

        static void Sort(DelayedDrawCallSet& drawCalls);

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

        static bool     CanDoPrepareAnimation(Metal::DeviceContext* context);

        auto            DrawCallToMaterialBinding() const -> std::vector<MaterialGuid>;
        MaterialGuid    GetMaterialBindingForDrawCall(unsigned drawCallIndex) const;
        void            LogReport() const;

        ::Assets::AssetState GetAssetState() const;
        ::Assets::AssetState TryResolve() const;

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const
            { return _validationCallback; }

        using Supplements = IteratorRange<const ModelSupplementScaffold**>;

            ////////////////////////////////////////////////////////////
        ModelRenderer(
            const ModelScaffold& scaffold, const MaterialScaffold& matScaffold,
            Supplements supplements,
            SharedStateSet& sharedStateSet, 
            const ::Assets::DirectorySearchRules* searchRules = nullptr, unsigned levelOfDetail = 0);
        ~ModelRenderer();

        ModelRenderer(const ModelRenderer&) = delete;
        ModelRenderer& operator=(const ModelRenderer&) = delete;
    protected:
        class Pimpl;
        class PimplWithSkinning;
        std::unique_ptr<PimplWithSkinning> _pimpl;

        std::shared_ptr<::Assets::DependencyValidation>   _validationCallback;

        template<bool HasCallback>
            static void RenderPreparedInternal(
                const ModelRendererContext&, const SharedStateSet&,
                const DelayedDrawCallSet&, DelayStep, 
                const std::function<void(DrawCallEvent)>*);
    };

////////////////////////////////////////////////////////////////////////////////////////////
    
    /// <summary>Supplemental vertex data associated with a model</summary>
    /// Some techniques require adding extra vertex data onto a model.
    /// For example, internal model static ambient occlusion might add another
    /// vertex element for each vertex.
    ///
    /// A ModelSupplement is a separate file that contains extra vertex 
    /// streams associated with some separate model file.
    ///
    /// This is especially useful for vertex elements that are only required
    /// in some quality modes. In the example mode, low quality mode might 
    /// disable the internal ambient occlusion -- and in this case we might
    /// skip loading the model supplement.
    ///
    /// The supplement can only add extra vertex elements to vertices that 
    /// already exist in the main model. It can't add new vertices.
    ///
    /// <seealso cref="ModelScaffold"/>
    /// <seealso cref="ModelRenderer"/>
    class ModelSupplementScaffold : public ::Assets::ChunkFileAsset
    {
    public:
        unsigned LargeBlocksOffset() const;
        const ModelSupplementImmutableData& ImmutableData() const;

        ModelSupplementScaffold(const ::Assets::ResChar filename[]);
        ModelSupplementScaffold(std::shared_ptr<::Assets::ICompileMarker>&& marker);
        ModelSupplementScaffold(ModelSupplementScaffold&& moveFrom) never_throws;
        ModelSupplementScaffold& operator=(ModelSupplementScaffold&& moveFrom) never_throws;
        ~ModelSupplementScaffold();

    private:
        std::unique_ptr<uint8[]>    _rawMemoryBlock;
        unsigned                    _largeBlocksOffset;

        static void Resolver(void*, IteratorRange<::Assets::AssetChunkResult*>);
        const ModelSupplementImmutableData*   TryImmutableData() const;
    };

////////////////////////////////////////////////////////////////////////////////////////////

    /// <summary>Structural data for a skeleton</summary>
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
    class SkeletonScaffold : public ::Assets::ChunkFileAsset
    {
    public:
        const TransformationMachine&    GetTransformationMachine() const;

        static const auto CompileProcessType = ConstHash64<'Skel', 'eton'>::Value;

        SkeletonScaffold(const ::Assets::ResChar filename[]);
        SkeletonScaffold(std::shared_ptr<::Assets::ICompileMarker>&& marker);
        SkeletonScaffold(SkeletonScaffold&& moveFrom) never_throws;
        SkeletonScaffold& operator=(SkeletonScaffold&& moveFrom) never_throws;
        ~SkeletonScaffold();
    private:
        std::unique_ptr<uint8[]>    _rawMemoryBlock;
        static void Resolver(void*, IteratorRange<::Assets::AssetChunkResult*>);
        const TransformationMachine*   TryImmutableData() const;
    };

    /// <summary>Structural data for animation</summary>
    /// Represents a set of animation that can potentially be applied to a skeleton.
    ///
    /// See SkeletonScaffold for more information.
    ///
    /// <seealso cref="ModelScaffold" />
    /// <seealso cref="SkeletonScaffold" />
    class AnimationSetScaffold : public ::Assets::ChunkFileAsset
    {
    public:
        const AnimationImmutableData&   ImmutableData() const;

        static const auto CompileProcessType = ConstHash64<'Anim', 'Set'>::Value;

        AnimationSetScaffold(const ::Assets::ResChar filename[]);
        AnimationSetScaffold(std::shared_ptr<::Assets::ICompileMarker>&& marker);
        AnimationSetScaffold(AnimationSetScaffold&& moveFrom) never_throws;
        AnimationSetScaffold& operator=(AnimationSetScaffold&& moveFrom) never_throws;
        ~AnimationSetScaffold();
    private:
        std::unique_ptr<uint8[]>    _rawMemoryBlock;
        static void Resolver(void*, IteratorRange<::Assets::AssetChunkResult*>);
        const AnimationImmutableData*   TryImmutableData() const;
    };

    /// <summary>Bind together a model, animation set and skeleton for rendering</summary>
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
    ///         <item> AnimationSetScaffold </item>
    ///         <item> SkeletonScaffold </item>
    ///         <item> ModelScaffold (skin) </item>
    ///     </list>
    ///
    /// And we also need our 2 "run-time" classes:
    ///     <list>
    ///         <item> ModelRenderer </item>
    ///         <item> SkinPrepareMachine </item>
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
        SkinPrepareMachine(const ModelScaffold& skinScaffold, const TransformationMachine& skeletonScaffold);
        ~SkinPrepareMachine();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };


}}

