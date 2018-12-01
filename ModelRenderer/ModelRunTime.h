// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DelayedDrawCall.h"

namespace ModelRenderer
{
    class PreparedAnimation;

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
        MeshToModel(const PreparedAnimation& preparedAnim, const SkeletonBinding* binding = nullptr);
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
        std::unique_ptr<PreparedAnimation, void(*)(PreparedAnimation*)> CreatePreparedAnimation() const;

        void PrepareAnimation(
            IThreadContext& context, 
            PreparedAnimation& state, 
            const SkeletonBinding& skeletonBinding) const;

        static bool     CanDoPrepareAnimation(IThreadContext& context);

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
        void PrepareAnimation(  
            IThreadContext& context, 
            PreparedAnimation& state,
            const AnimationState& animState) const;
        const SkeletonBinding& GetSkeletonBinding() const;
        unsigned GetSkeletonOutputCount() const;

        void RenderSkeleton(
            IThreadContext& context, 
            Techniques::ParsingContext& parserContext, 
            const AnimationState& animState, const Float4x4& localToWorld);
        
        SkinPrepareMachine(const ModelScaffold&, const AnimationSetScaffold&, const SkeletonScaffold&);
        SkinPrepareMachine(const ModelScaffold& skinScaffold, const SkeletonMachine& skeletonScaffold);
        ~SkinPrepareMachine();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
}

