// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ModelRendererInternal.h"
#include "SkeletonScaffoldInternal.h"
#include "ModelImmutableData.h"
#include "RawAnimationCurve.h"
#include "SharedStateSet.h"
#include "AssetUtils.h"     // actually just needed for chunk id
#include "DeferredShaderResource.h"
#include "../RenderUtils.h"

#include "../IDevice.h"
#include "../Metal/Shader.h"
#include "../Metal/InputLayout.h"
#include "../Metal/DeviceContext.h"
#include "../Metal/ShaderResource.h"
#include "../Metal/Resource.h"
#include "../Metal/ObjectFactory.h"

#include "../Techniques/ResourceBox.h"
#include "../Techniques/Techniques.h"
#include "../Techniques/TechniqueUtils.h"
#include "../Techniques/ParsingContext.h"
#include "../Techniques/CommonResources.h"
#include "../../ConsoleRig/Console.h"
#include "../../ConsoleRig/Log.h"
#include "../../Assets/ChunkFile.h"
#include "../../Utility/Streams/FileUtils.h"

#if GFXAPI_ACTIVE == GFXAPI_DX11
	#include "../DX11/Metal/IncludeDX11.h"
#endif

#pragma warning(disable:4127)       // conditional expression is constant
#pragma warning(disable:4505)		// unreferenced local function has been removed

namespace RenderCore { namespace Assets
{
    class HashedInputAssemblies
    {
    public:
        class Desc
        {
        public:
            uint64 _hash;
            Desc(uint64 hash) : _hash(hash) {}
        };

        std::vector<Metal::InputElementDesc> _elements;
        HashedInputAssemblies(const Desc& ) {}
    };

    class SkinningBindingBox
    {
    public:
        struct BindingType { enum Enum { tbuffer, cbuffer }; };
        class Desc
        {
        public:
            BindingType::Enum _bindingType;
            uint64 _iaHash;
            unsigned _vertexStride;
            Desc(BindingType::Enum bindingType, uint64 iaHash, unsigned vertexStride) : _bindingType(bindingType), _iaHash(iaHash), _vertexStride(vertexStride) {}
        };

        SkinningBindingBox(const Desc& desc);

        Metal::GeometryShader       _geometryShader;
        Metal::BoundUniforms        _boundUniforms;
        Metal::BoundInputLayout     _boundInputLayout;
        BindingType::Enum           _bindingType;

        Metal::VertexShader       _skinningVertexShaderP4;
        Metal::VertexShader       _skinningVertexShaderP2;
        Metal::VertexShader       _skinningVertexShaderP1;
        Metal::VertexShader       _skinningVertexShaderP0;

        const ::Assets::DepValPtr& GetDependencyValidation() const   { return _validationCallback; }
        ::Assets::DepValPtr _validationCallback;
    };

    SkinningBindingBox::SkinningBindingBox(const Desc& desc)
    {
        auto& ai = Techniques::FindCachedBox<HashedInputAssemblies>(HashedInputAssemblies::Desc(desc._iaHash));
        const auto& skinningInputLayout = ai._elements;

        std::vector<Metal::InputElementDesc> skinningOutputLayout;
        for (auto i=skinningInputLayout.cbegin(); i!=skinningInputLayout.cend(); ++i) {
            if (i->_inputSlot == 0) skinningOutputLayout.push_back(*i);
        }

            ///////////////////////////////////////////////

        const char* skinningVertexShaderSourceP4    = (desc._bindingType==BindingType::cbuffer) ? ("game/xleres/animation/skinning.vsh:P4:" VS_DefShaderModel) : ("game/xleres/animation/skinning_viatbuffer.vsh:P4:" VS_DefShaderModel);
        const char* skinningVertexShaderSourceP2    = (desc._bindingType==BindingType::cbuffer) ? ("game/xleres/animation/skinning.vsh:P2:" VS_DefShaderModel) : ("game/xleres/animation/skinning_viatbuffer.vsh:P2:" VS_DefShaderModel);
        const char* skinningVertexShaderSourceP1    = (desc._bindingType==BindingType::cbuffer) ? ("game/xleres/animation/skinning.vsh:P1:" VS_DefShaderModel) : ("game/xleres/animation/skinning_viatbuffer.vsh:P1:" VS_DefShaderModel);
        const char* skinningVertexShaderSourceP0    = (desc._bindingType==BindingType::cbuffer) ? ("game/xleres/animation/skinning.vsh:P0:" VS_DefShaderModel) : ("game/xleres/animation/skinning_viatbuffer.vsh:P0:" VS_DefShaderModel);
        const char* geometryShaderSourceP           = "game/xleres/animation/skinning.gsh:P:" GS_DefShaderModel;

        const char* skinningVertexShaderSourcePN4   = (desc._bindingType==BindingType::cbuffer) ? ("game/xleres/animation/skinning.vsh:PN4:" VS_DefShaderModel) : ("game/xleres/animation/skinning_viatbuffer.vsh:PN4:" VS_DefShaderModel);
        const char* skinningVertexShaderSourcePN2   = (desc._bindingType==BindingType::cbuffer) ? ("game/xleres/animation/skinning.vsh:PN2:" VS_DefShaderModel) : ("game/xleres/animation/skinning_viatbuffer.vsh:PN2:" VS_DefShaderModel);
        const char* skinningVertexShaderSourcePN1   = (desc._bindingType==BindingType::cbuffer) ? ("game/xleres/animation/skinning.vsh:PN1:" VS_DefShaderModel) : ("game/xleres/animation/skinning_viatbuffer.vsh:PN1:" VS_DefShaderModel);
        const char* skinningVertexShaderSourcePN0   = (desc._bindingType==BindingType::cbuffer) ? ("game/xleres/animation/skinning.vsh:PN0:" VS_DefShaderModel) : ("game/xleres/animation/skinning_viatbuffer.vsh:PN0:" VS_DefShaderModel);
        const char* geometryShaderSourcePN          = "game/xleres/animation/skinning.gsh:PN:" GS_DefShaderModel;

        const bool hasNormals = !!HasElement(AsPointer(skinningOutputLayout.cbegin()), AsPointer(skinningOutputLayout.cend()), "NORMAL");

            //  outputs from skinning are always float3's currently. So, we can get the vertex stride
            //  just from the outputs count
        unsigned outputVertexStride = unsigned(skinningOutputLayout.size() * 3 * sizeof(float));

        using namespace Metal;
        _geometryShader = GeometryShader(
            hasNormals ? geometryShaderSourcePN : geometryShaderSourceP, 
            GeometryShader::StreamOutputInitializers(
                AsPointer(skinningOutputLayout.begin()), unsigned(skinningOutputLayout.size()),
                &outputVertexStride, 1));

        auto& vsByteCodeP4 = ::Assets::GetAssetDep<CompiledShaderByteCode>(hasNormals ? skinningVertexShaderSourcePN4 : skinningVertexShaderSourceP4);
        auto& vsByteCodeP2 = ::Assets::GetAssetDep<CompiledShaderByteCode>(hasNormals ? skinningVertexShaderSourcePN2 : skinningVertexShaderSourceP2);
        auto& vsByteCodeP1 = ::Assets::GetAssetDep<CompiledShaderByteCode>(hasNormals ? skinningVertexShaderSourcePN1 : skinningVertexShaderSourceP1);
        auto& vsByteCodeP0 = ::Assets::GetAssetDep<CompiledShaderByteCode>(hasNormals ? skinningVertexShaderSourcePN0 : skinningVertexShaderSourceP0);

        _skinningVertexShaderP4 = Metal::VertexShader(vsByteCodeP4);
        _skinningVertexShaderP2 = Metal::VertexShader(vsByteCodeP2);
        _skinningVertexShaderP1 = Metal::VertexShader(vsByteCodeP1);
        _skinningVertexShaderP0 = Metal::VertexShader(vsByteCodeP0);

        BoundUniforms boundUniforms(vsByteCodeP4);
        const auto jointTransformsHash = Hash64("JointTransforms");
        if (desc._bindingType == BindingType::cbuffer) {
            boundUniforms.BindConstantBuffer(jointTransformsHash, 0, 1);
        } else {
            boundUniforms.BindShaderResource(jointTransformsHash, 0, 1);
        }

        _boundInputLayout = BoundInputLayout(
            std::make_pair(AsPointer(skinningInputLayout.cbegin()), skinningInputLayout.size()),
            vsByteCodeP4);

        /////////////////////////////////////////////////////////////////////////////

        auto validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(validationCallback, vsByteCodeP4.GetDependencyValidation());
        ::Assets::RegisterAssetDependency(validationCallback, vsByteCodeP2.GetDependencyValidation());
        ::Assets::RegisterAssetDependency(validationCallback, vsByteCodeP1.GetDependencyValidation());
        ::Assets::RegisterAssetDependency(validationCallback, vsByteCodeP0.GetDependencyValidation());

        _validationCallback = std::move(validationCallback);
        _boundUniforms = std::move(boundUniforms);
    }


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    AnimationSetBinding::AnimationSetBinding(
            const AnimationSet::OutputInterface&            output,
            const TransformationMachine::InputInterface&    input)
    {
            //
            //      for each animation set output value, match it with a 
            //      value in the transformation machine interface
            //      The interfaces are not sorted, we we just have to 
            //      do brute force searches. But there shouldn't be many
            //      parameters (so it should be fairly quick)
            //
        std::vector<unsigned> result;
        result.resize(output._parameterInterfaceCount);
        for (size_t c=0; c<output._parameterInterfaceCount; ++c) {
            uint64 parameterName = output._parameterInterfaceDefinition[c];
            result[c] = ~unsigned(0x0);

            for (size_t c2=0; c2<input._parameterCount; ++c2) {
                if (input._parameters[c2]._name == parameterName) {
                    result[c] = unsigned(c2);
                    break;
                }
            }

            #if defined(_DEBUG)
                if (result[c] == ~unsigned(0x0)) {
                    LogWarning << "Animation driver output cannot be bound to transformation machine input";
                }
            #endif
        }

        _animDriverToMachineParameter = std::move(result);
    }

    AnimationSetBinding::AnimationSetBinding() {}
    AnimationSetBinding::AnimationSetBinding(AnimationSetBinding&& moveFrom) never_throws
    : _animDriverToMachineParameter(std::move(moveFrom._animDriverToMachineParameter))
    {}
    AnimationSetBinding& AnimationSetBinding::operator=(AnimationSetBinding&& moveFrom) never_throws
    {
        _animDriverToMachineParameter = std::move(moveFrom._animDriverToMachineParameter);
        return *this;
    }
    AnimationSetBinding::~AnimationSetBinding() {}

    SkeletonBinding::SkeletonBinding(   const TransformationMachine::OutputInterface&   output,
                                        const ModelCommandStream::InputInterface&       input)
    {
        std::vector<unsigned> result;
        std::vector<Float4x4> inverseBindMatrices;
        result.resize(input._jointCount);
        inverseBindMatrices.resize(input._jointCount);

        for (size_t c=0; c<input._jointCount; ++c) {
            uint64 name = input._jointNames[c];
            result[c] = ~unsigned(0x0);
            inverseBindMatrices[c] = Identity<Float4x4>();

            for (size_t c2=0; c2<output._outputMatrixNameCount; ++c2) {
                if (output._outputMatrixNames[c2] == name) {
                    result[c] = unsigned(c2);
                    inverseBindMatrices[c] = output._skeletonInverseBindMatrices[c2];
                    break;
                }
            }

            #if defined(_DEBUG)
                // if (result[c] == ~unsigned(0x0)) {
                //     LogWarning << "Couldn't bind skin matrix to transformation machine output.";
                // }
            #endif
        }
            
        _modelJointIndexToMachineOutput = std::move(result);
        _modelJointIndexToInverseBindMatrix = std::move(inverseBindMatrices);
    }

    SkeletonBinding::SkeletonBinding() {}
    SkeletonBinding::SkeletonBinding(SkeletonBinding&& moveFrom) never_throws
    : _modelJointIndexToMachineOutput(std::move(moveFrom._modelJointIndexToMachineOutput))
    , _modelJointIndexToInverseBindMatrix(std::move(moveFrom._modelJointIndexToInverseBindMatrix))
    {}
    SkeletonBinding& SkeletonBinding::operator=(SkeletonBinding&& moveFrom) never_throws
    {
        _modelJointIndexToMachineOutput = std::move(moveFrom._modelJointIndexToMachineOutput);
        _modelJointIndexToInverseBindMatrix = std::move(moveFrom._modelJointIndexToInverseBindMatrix);
        return *this;
    }
    SkeletonBinding::~SkeletonBinding() {}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void TransformationMachine::GenerateOutputTransforms(   
        Float4x4 output[], unsigned outputCount,
        const TransformationParameterSet*   parameterSet) const
    {
        if (outputCount < _outputMatrixCount)
            Throw(::Exceptions::BasicLabel("Output buffer to TransformationMachine::GenerateOutputTransforms is too small"));
        GenerateOutputTransformsFree(
            output, outputCount, parameterSet, 
            MakeIteratorRange(_commandStream, _commandStream + _commandStreamSize));
    }

    void TransformationMachine::GenerateOutputTransforms(   
        Float4x4 output[], unsigned outputCount,
        const TransformationParameterSet*   parameterSet,
        const DebugIterator& debugIterator) const
    {
        if (outputCount < _outputMatrixCount)
            Throw(::Exceptions::BasicLabel("Output buffer to TransformationMachine::GenerateOutputTransforms is too small"));
        GenerateOutputTransformsFree(
            output, outputCount, parameterSet, 
            MakeIteratorRange(_commandStream, _commandStream + _commandStreamSize), 
            debugIterator);
    }

    TransformationMachine::TransformationMachine()
    {
        _commandStream = nullptr;
        _commandStreamSize = 0;
        _outputMatrixCount = 0;
    }

    TransformationMachine::~TransformationMachine()
    {
    }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void ModelRenderer::PimplWithSkinning::StartBuildingSkinning(
        Metal::DeviceContext& context, SkinningBindingBox& bindingBox) const
    {
        context.Bind(bindingBox._boundInputLayout);
        context.Bind(bindingBox._geometryShader);

        context.Unbind<Metal::PixelShader>();
        context.Bind(Metal::Topology::PointList);
    }

    void ModelRenderer::PimplWithSkinning::EndBuildingSkinning(Metal::DeviceContext& context) const
    {
		#if GFXAPI_ACTIVE == GFXAPI_DX11
			context.GetUnderlying()->SOSetTargets(0, nullptr, nullptr);
		#endif
        context.Unbind<Metal::GeometryShader>();
    }

    static void SetSkinningShader(Metal::DeviceContext& context, SkinningBindingBox& bindingBox, unsigned materialIndexValue)
    {
        if (materialIndexValue == 4)         context.Bind(bindingBox._skinningVertexShaderP4);
        else if (materialIndexValue == 2)    context.Bind(bindingBox._skinningVertexShaderP2);
        else if (materialIndexValue == 1)    context.Bind(bindingBox._skinningVertexShaderP1);
        else {
            assert(materialIndexValue == 0);
            context.Bind(bindingBox._skinningVertexShaderP0);
        }
    }

    static void WriteJointTransforms(   Float3x4 destination[], size_t destinationCount,
                                        const BoundSkinnedGeometry& controller,
                                        const Float4x4              transformationMachineResult[],
                                        const SkeletonBinding&      skeletonBinding)
    {
        for (unsigned c=0; c<std::min(controller._jointMatrixCount, destinationCount); ++c) {
            auto jointMatrixIndex = controller._jointMatrices[c];
            auto transMachineOutput = skeletonBinding.ModelJointToMachineOutput(jointMatrixIndex);
            if (transMachineOutput != ~unsigned(0x0)) {
                Float4x4 inverseBindByBindShapeMatrix = controller._inverseBindByBindShapeMatrices[c];
                Float4x4 finalMatrix = 
                    Combine(    inverseBindByBindShapeMatrix, 
                                transformationMachineResult[transMachineOutput]);
                destination[c] = Truncate(finalMatrix);
            } else {
                destination[c] = Identity<Float3x4>();
            }
        }
    }

        //
        //      Simple method for managing tbuffer textures...
        //      Keep multiple textures for each frame. Use a single
        //      texture for each
        //
        //      Also consider using a single large circular buffer
        //      and using the discard/no-overwrite pattern
        //
    class TBufferTemporaryTexture
    {
    public:
        RenderCore::ResourcePtr			_resource;
        RenderCore::ResourcePtr			_stagingResource;
        Metal::ShaderResourceView       _view;
        size_t                          _size;
        unsigned                        _lastAllocatedFrame;
        unsigned                        _shaderOffsetValue;

        TBufferTemporaryTexture();
        TBufferTemporaryTexture(const void* sourceData, size_t bufferSize, bool dynamicResource=false);
        TBufferTemporaryTexture(const TBufferTemporaryTexture& cloneFrom);
        TBufferTemporaryTexture(TBufferTemporaryTexture&& moveFrom);
        TBufferTemporaryTexture& operator=(const TBufferTemporaryTexture& cloneFrom);
        TBufferTemporaryTexture& operator=(TBufferTemporaryTexture&& moveFrom);
    };

    static unsigned globalCircularBuffer_Size = 200 * sizeof(Float3x4);
    static unsigned globalCircularBuffer_WritingPosition = 0;
    TBufferTemporaryTexture globalCircularBuffer;

    static TBufferTemporaryTexture AllocateExistingTBufferTemporaryTexture(size_t bufferSize);
    static TBufferTemporaryTexture AllocateNewTBufferTemporaryTexture(const void* bufferData, size_t bufferSize);
    static void PushTBufferTemporaryTexture(Metal::DeviceContext* context, TBufferTemporaryTexture& tex);

    void ModelRenderer::PimplWithSkinning::InitialiseSkinningVertexAssembly(
        uint64 inputAssemblyHash,
        const BoundSkinnedGeometry& scaffoldGeo)
    {
        auto& iaBox = Techniques::FindCachedBox<HashedInputAssemblies>(HashedInputAssemblies::Desc(inputAssemblyHash));
        if (iaBox._elements.empty()) {
                //  This hashed input assembly will contain both the full input assembly 
                //  for preparing skinning (with the animated elements in slot 0, and 
                //  the skeleton binding info in slot 1)
            Metal::InputElementDesc inputDesc[12];
            unsigned vertexElementCount = BuildLowLevelInputAssembly(
                inputDesc, dimof(inputDesc),
                scaffoldGeo._animatedVertexElements._ia._elements);

            vertexElementCount += BuildLowLevelInputAssembly(
                &inputDesc[vertexElementCount], dimof(inputDesc) - vertexElementCount,
                scaffoldGeo._skeletonBinding._ia._elements, 1);

            iaBox._elements.insert(iaBox._elements.begin(), inputDesc, &inputDesc[vertexElementCount]);
        }
    }

    static void ApplyConversionFromStreamOutput(VertexElement dst[], const VertexElement src[], unsigned count)
    {
        using namespace Metal;
        for (unsigned c=0; c<count; ++c) {
            XlCopyMemory(&dst[c], &src[c], sizeof(VertexElement));
            dst[c]._alignedByteOffset = ~unsigned(0x0);   // have to reset all of the offsets (because elements might change size)

                // change 16 bit precision formats into 32 bit
                //  (also change 4 dimensional vectors into 3 dimension vectors. Since there
                //  isn't a 4D float16 format, most of the time the 4D format is intended to
                //  be a 3D format. This will break if the format is intended to truly be
                //  4D).
            if (    GetComponentType(Metal::NativeFormat::Enum(dst[c]._nativeFormat)) == FormatComponentType::Float
                &&  GetComponentPrecision(Metal::NativeFormat::Enum(dst[c]._nativeFormat)) == 16) {

                auto components = GetComponents(Metal::NativeFormat::Enum(dst[c]._nativeFormat));
                if (components == FormatComponents::RGBAlpha) {
                    components = FormatComponents::RGB;
                }
                auto recastFormat = FindFormat(
                    FormatCompressionType::None, 
                    components, FormatComponentType::Float,
                    32);
                if (recastFormat != NativeFormat::Unknown) {
                    dst[c]._nativeFormat = recastFormat;
                }
            }
        }
    }

    unsigned ModelRenderer::PimplWithSkinning::BuildPostSkinInputAssembly(
        Metal::InputElementDesc dst[], unsigned dstCount,
        const BoundSkinnedGeometry& scaffoldGeo)
    {
        // build the native input assembly that should be used when using
        // prepared animation

        VertexElement convertedElements[16];
        unsigned convertedCount = 
            std::min( dstCount, 
                (unsigned)std::min(dimof(convertedElements), 
                    scaffoldGeo._animatedVertexElements._ia._elements.size()));
        ApplyConversionFromStreamOutput(
            convertedElements, 
            AsPointer(scaffoldGeo._animatedVertexElements._ia._elements.cbegin()), convertedCount);

        unsigned eleCount = BuildLowLevelInputAssembly(
            dst, dstCount, convertedElements, convertedCount);

            // (add the unanimated part)
        eleCount += BuildLowLevelInputAssembly(
            &dst[eleCount], dstCount - eleCount, 
            scaffoldGeo._vb._ia._elements, 1);

        return eleCount;
    }

    auto ModelRenderer::PimplWithSkinning::BuildAnimBinding(
        const ModelCommandStream::GeoCall& geoInst,
        const BoundSkinnedGeometry& geo,
        SharedStateSet& sharedStateSet,
        const uint64 textureBindPoints[], unsigned textureBindPointsCnt) -> SkinnedMeshAnimBinding
    {
            //  Build technique interfaces and binding information for the prepared
            //  animation case. This is extra information attached to the skinned mesh
            //  object that is used when we render the mesh using prepared animation

        SkinnedMeshAnimBinding result;
        result._iaAnimationHash = 0;
        result._scaffold = &geo;
        result._techniqueInterface = ~unsigned(0x0);
        result._vertexStride = 0;

        Metal::InputElementDesc inputDescForRender[12];
        auto vertexElementForRenderCount = 
            PimplWithSkinning::BuildPostSkinInputAssembly(
                inputDescForRender, dimof(inputDescForRender), geo);

        result._techniqueInterface = 
            sharedStateSet.InsertTechniqueInterface(
                inputDescForRender, vertexElementForRenderCount, 
                textureBindPoints, textureBindPointsCnt);

        result._vertexStride = 
            CalculateVertexStride(inputDescForRender, &inputDescForRender[vertexElementForRenderCount], 0);

        result._iaAnimationHash = geo._animatedVertexElements._ia.BuildHash() ^ geo._skeletonBinding._ia.BuildHash();
        InitialiseSkinningVertexAssembly(result._iaAnimationHash, geo);

        return result;
    }

    void ModelRenderer::PimplWithSkinning::BuildSkinnedBuffer(  
                Metal::DeviceContext*       context,
                const SkinnedMesh&          mesh,
                const SkinnedMeshAnimBinding& preparedAnimBinding, 
                const Float4x4              transformationMachineResult[],
                const SkeletonBinding&      skeletonBinding,
                Metal::VertexBuffer&        outputResult,
                unsigned                    outputOffset) const
    {
#if GFXAPI_ACTIVE == GFXAPI_DX11
        using namespace Metal;
        const auto bindingType =
            Tweakable("SkeletonUpload_ViaTBuffer", false)
                ? SkinningBindingBox::BindingType::tbuffer
                : SkinningBindingBox::BindingType::cbuffer;

            // fill in the "HashedInputAssemblies" box if necessary
        const auto& scaffold = *preparedAnimBinding._scaffold;
        auto& bindingBox = Techniques::FindCachedBoxDep2<SkinningBindingBox>(
            bindingType, preparedAnimBinding._iaAnimationHash, mesh._extraVbStride[SkinnedMesh::VertexStreams::AnimatedGeo]);

            ///////////////////////////////////////////////

        #if defined(_DEBUG) ///////////////////////////////////////////////////////////////////
                    //
                    //  we get warning messages from D3D11 if the device debug flag is on, 
                    //  and the buffer size is not right
                    //
            const size_t packetCount = (bindingType==SkinningBindingBox::BindingType::cbuffer)?200:scaffold._jointMatrixCount;     
        #else /////////////////////////////////////////////////////////////////////////////////
            const size_t packetCount = scaffold._jointMatrixCount;
        #endif ////////////////////////////////////////////////////////////////////////////////

        ConstantBufferPacket constantBufferPackets[1];

        TBufferTemporaryTexture tbufferTexture;
        if (bindingType == SkinningBindingBox::BindingType::cbuffer) {

            constantBufferPackets[0] = RenderCore::MakeSharedPktSize(sizeof(Float3x4) * packetCount);
            Float3x4* jointTransforms = (Float3x4*)constantBufferPackets[0].begin();
            WriteJointTransforms(jointTransforms, packetCount, scaffold, transformationMachineResult, skeletonBinding);

        } else {

                //
                //      Write via tbuffer. We need to allocate a texture,
                //      write in the new transform data, and bind it to
                //      the pipeline.
                //
                //      The way in which we create and upload to the texture
                //      may have some effect on this... We probably want to avoid 
                //      copying via the command buffer -- and instead just write
                //      to reserved GPU space.
                //

            const bool useCircularBuffer = true;
            if (useCircularBuffer) {

                    //
                    //  Use a single buffer with many discard maps... This
                    //  seems to be producing more efficient GPU side results!
                    //
                if (!globalCircularBuffer._size) {
                    globalCircularBuffer = TBufferTemporaryTexture(nullptr, globalCircularBuffer_Size, true);
                }

                unsigned currentWritingPosition = globalCircularBuffer_WritingPosition;
                D3D11_MAP mapType = D3D11_MAP_WRITE_DISCARD; // D3D11_MAP_WRITE_NO_OVERWRITE;
                globalCircularBuffer_WritingPosition += unsigned(packetCount * sizeof(Float3x4));
                if (1) { // globalCircularBuffer_WritingPosition > globalCircularBuffer_Size) {
                        // Wrap around moment... discard and start writing from top
                    currentWritingPosition = 0;
                    mapType = D3D11_MAP_WRITE_DISCARD;
                    globalCircularBuffer_WritingPosition = unsigned(packetCount * sizeof(Float3x4));
                }

                    //
                    //      Note we always have to map the entire buffer here... But most of the time
                    //      we're only writing to a small subset of the buffer. The means extra redundant
                    //      dirty data will be copied to the GPU... Is there any way to do this better?
                    //      We can't use NO_OVERWRITE maps here, because that is only allows for
                    //      index and vertex buffers.
                    //
                D3D11_MAPPED_SUBRESOURCE mapping;
                HRESULT hresult = context->GetUnderlying()->Map(globalCircularBuffer._resource.get(), 0, mapType, 0, &mapping);
                if (SUCCEEDED(hresult) && mapping.pData) {
                    WriteJointTransforms(
                        (Float3x4*)PtrAdd(mapping.pData, currentWritingPosition), packetCount, 
                        scaffold, transformationMachineResult, skeletonBinding);
                    context->GetUnderlying()->Unmap(globalCircularBuffer._resource.get(), 0);
                }

                tbufferTexture = globalCircularBuffer;
                tbufferTexture._shaderOffsetValue = currentWritingPosition / sizeof(Float3x4);

                // unsigned* shaderOffsetValue = (unsigned*)AsPointer(constantBufferPackets[0]->begin());
                // *shaderOffsetValue = tbufferTexture._shaderOffsetValue;

            } else {
                tbufferTexture = AllocateExistingTBufferTemporaryTexture(packetCount * sizeof(Float3x4));
                if (tbufferTexture._stagingResource) {

                    D3D11_MAPPED_SUBRESOURCE mapping;
                        //
                        //      Also consider "no-overwrite" map access. This can only be used for
                        //      vertex buffers, however... which means binding a vertex buffer to a tbuffer
                        //      shader input.
                        //
                    HRESULT hresult = context->GetUnderlying()->Map(tbufferTexture._stagingResource.get(), 0, D3D11_MAP_WRITE, 0, &mapping);
                    if (SUCCEEDED(hresult) && mapping.pData) {
                        WriteJointTransforms((Float3x4*)mapping.pData, packetCount, scaffold, transformationMachineResult, skeletonBinding);
                        context->GetUnderlying()->Unmap(tbufferTexture._stagingResource.get(), 0);
                    }
                    PushTBufferTemporaryTexture(context, tbufferTexture);

                } else {

                    auto buffer = std::make_unique<Float3x4[]>(packetCount);
                    WriteJointTransforms(buffer.get(), packetCount, scaffold, transformationMachineResult, skeletonBinding);
                    tbufferTexture = AllocateNewTBufferTemporaryTexture(buffer.get(), packetCount * sizeof(Float3x4));

                }
            }

        }

            ///////////////////////////////////////////////

        ID3D::Buffer* soOutputBuffer = outputResult.GetUnderlying();
        context->GetUnderlying()->SOSetTargets(1, &soOutputBuffer, &outputOffset);
        {
            const Metal::ShaderResourceView* temp[] = { tbufferTexture._view.GetUnderlying() ? &tbufferTexture._view : nullptr };
            bindingBox._boundUniforms.Apply(
                *context, 
                Metal::UniformsStream(),
                Metal::UniformsStream(constantBufferPackets, nullptr, dimof(constantBufferPackets), temp, 1));
        }
        StartBuildingSkinning(*context, bindingBox);

            // bind the mesh streams -- we need animated geometry and skeleton binding
        auto animGeo = SkinnedMesh::VertexStreams::AnimatedGeo;
        auto skelBind = SkinnedMesh::VertexStreams::SkeletonBinding;

        const VertexBuffer* vbs [2] = { &_vertexBuffer, &_vertexBuffer };
        const unsigned strides  [2] = { mesh._extraVbStride[animGeo], mesh._extraVbStride[skelBind] };
        unsigned offsets        [2] = { mesh._extraVbOffset[animGeo], mesh._extraVbOffset[skelBind] };
        context->Bind(0, 2, vbs, strides, offsets);


            //
            //      (   vertex shader signatures are all the same -- so we can keep the 
            //          same input layout and constant buffer assignments   )
            //

        for (unsigned di=0; di<scaffold._preskinningDrawCallCount; ++di) {
            auto& d = scaffold._preskinningDrawCalls[di];
            SetSkinningShader(*context, bindingBox, d._subMaterialIndex);
            context->Draw(d._indexCount, d._firstVertex);
        }
#endif
    }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    auto ModelRenderer::CreatePreparedAnimation() const -> PreparedAnimation
    {
            //  We need to allocate a vertex buffer that can contain the animated vertex data
            //  for this whole mesh. 
        unsigned vbSize = 0;
        std::vector<unsigned> offsets;
        offsets.reserve(_pimpl->_skinnedMeshes.size());

        auto b=_pimpl->_skinnedBindings.cbegin();
        for (auto m=_pimpl->_skinnedMeshes.cbegin(); m!=_pimpl->_skinnedMeshes.cend(); ++m, ++b) {
            offsets.push_back(vbSize);

            const auto stream = PimplWithSkinning::SkinnedMesh::VertexStreams::AnimatedGeo;
            unsigned size =     // (size post conversion might not be the same as the input data)
                m->_vertexCount[stream] * b->_vertexStride;
            vbSize += size;
        }

        PreparedAnimation result;
        result._skinningBuffer = Metal::VertexBuffer(nullptr, vbSize);
        result._vbOffsets = std::move(offsets);
        return result;
    }

    void ModelRenderer::PrepareAnimation(
        Metal::DeviceContext* context, PreparedAnimation& result, 
        const SkeletonBinding& skeletonBinding) const
    {
        for (size_t i=0; i<_pimpl->_skinnedMeshes.size(); ++i) {
            _pimpl->BuildSkinnedBuffer(
                context, 
                _pimpl->_skinnedMeshes[i], 
                _pimpl->_skinnedBindings[i],
                result._finalMatrices.get(), skeletonBinding, 
                result._skinningBuffer, result._vbOffsets[i]);
        }

        _pimpl->EndBuildingSkinning(*context);
    }

	#if GFXAPI_ACTIVE == GFXAPI_DX11
		static intrusive_ptr<ID3D::Device> ExtractDevice(RenderCore::Metal::DeviceContext* context)
		{
			ID3D::Device* tempPtr;
			context->GetUnderlying()->GetDevice(&tempPtr);
			return moveptr(tempPtr);
		}
	#endif

    bool ModelRenderer::CanDoPrepareAnimation(Metal::DeviceContext* context)
    {
		#if GFXAPI_ACTIVE == GFXAPI_DX11
			auto featureLevel = ExtractDevice(context)->GetFeatureLevel();
			return (featureLevel >= D3D_FEATURE_LEVEL_10_0);
		#else
			return false;
		#endif
    }

    ModelRenderer::PreparedAnimation::PreparedAnimation() 
    {
    }

    ModelRenderer::PreparedAnimation::PreparedAnimation(PreparedAnimation&& moveFrom)
    : _finalMatrices(std::move(moveFrom._finalMatrices))
    , _skinningBuffer(std::move(moveFrom._skinningBuffer))
    , _vbOffsets(std::move(moveFrom._vbOffsets))
    , _animState(moveFrom._animState) {}

    ModelRenderer::PreparedAnimation& ModelRenderer::PreparedAnimation::operator=(PreparedAnimation&& moveFrom)
    {
        _finalMatrices = std::move(moveFrom._finalMatrices);
        _skinningBuffer = std::move(moveFrom._skinningBuffer);
        _vbOffsets = std::move(moveFrom._vbOffsets);
        _animState = moveFrom._animState;
        return *this;
    }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    struct CompareAnimationName
    {
        bool operator()(const AnimationSet::Animation& lhs, const AnimationSet::Animation& rhs) const { return lhs._name < rhs._name; }
        bool operator()(const AnimationSet::Animation& lhs, uint64 rhs) const { return lhs._name < rhs; }
        bool operator()(uint64 lhs, const AnimationSet::Animation& rhs) const { return lhs < rhs._name; }
    };

    TransformationParameterSet      AnimationSet::BuildTransformationParameterSet(
        const AnimationState&           animState__,
        const TransformationMachine&    transformationMachine,
        const AnimationSetBinding&      binding,
        const RawAnimationCurve*        curves,
        size_t                          curvesCount) const
    {
        TransformationParameterSet result(transformationMachine.GetDefaultParameters());
        float* float1s      = result.GetFloat1Parameters();
        Float3* float3s     = result.GetFloat3Parameters();
        Float4* float4s     = result.GetFloat4Parameters();
        Float4x4* float4x4s = result.GetFloat4x4Parameters();

        AnimationState animState = animState__;

        size_t driverStart = 0, driverEnd = GetAnimationDriverCount();
        size_t constantDriverStartIndex = 0, constantDriverEndIndex = _constantDriverCount;
        if (animState._animation!=0x0) {
            auto end = &_animations[_animationCount];
            auto i = std::lower_bound(_animations, end, animState._animation, CompareAnimationName());
            if (i!=end && i->_name == animState._animation) {
                driverStart = i->_beginDriver;
                driverEnd = i->_endDriver;
                constantDriverStartIndex = i->_beginConstantDriver;
                constantDriverEndIndex = i->_endConstantDriver;
                animState._time += i->_beginTime;
            }
        }

        const TransformationMachine::InputInterface& inputInterface 
            = transformationMachine.GetInputInterface();
        for (size_t c=driverStart; c<driverEnd; ++c) {
            const AnimationDriver& driver = _animationDrivers[c];
            unsigned transInputIndex = binding.AnimDriverToMachineParameter(driver._parameterIndex);
            if (transInputIndex == ~unsigned(0x0)) {
                continue;   // (unbound output)
            }

            assert(transInputIndex < inputInterface._parameterCount);
            const TransformationMachine::InputInterface::Parameter& p 
                = inputInterface._parameters[transInputIndex];

            if (driver._samplerType == TransformationParameterSet::Type::Float4x4) {
                if (driver._curveId < curvesCount) {
                    const RawAnimationCurve& curve = curves[driver._curveId];
                    assert(p._type == TransformationParameterSet::Type::Float4x4);
                    // assert(i->_index < float4x4s.size());
                    float4x4s[p._index] = curve.Calculate<Float4x4>(animState._time);
                }
            } else if (driver._samplerType == TransformationParameterSet::Type::Float4) {
                if (driver._curveId < curvesCount) {
                    const RawAnimationCurve& curve = curves[driver._curveId];
                    if (p._type == TransformationParameterSet::Type::Float4) {
                        float4s[p._index] = curve.Calculate<Float4>(animState._time);
                    } else if (p._type == TransformationParameterSet::Type::Float3) {
                        float3s[p._index] = Truncate(curve.Calculate<Float4>(animState._time));
                    } else {
                        assert(p._type == TransformationParameterSet::Type::Float1);
                        float1s[p._index] = curve.Calculate<Float4>(animState._time)[0];
                    }
                }
            } else if (driver._samplerType == TransformationParameterSet::Type::Float3) {
                if (driver._curveId < curvesCount) {
                    const RawAnimationCurve& curve = curves[driver._curveId];
                    if (p._type == TransformationParameterSet::Type::Float3) {
                        float3s[p._index] = curve.Calculate<Float3>(animState._time);
                    } else {
                        assert(p._type == TransformationParameterSet::Type::Float1);
                        float1s[p._index] = curve.Calculate<Float3>(animState._time)[0];
                    }
                }
            } else if (driver._samplerType == TransformationParameterSet::Type::Float1) {
                if (driver._curveId < curvesCount) {
                    const RawAnimationCurve& curve = curves[driver._curveId];
                    float result = curve.Calculate<float>(animState._time);
                    if (p._type == TransformationParameterSet::Type::Float1) {
                        float1s[p._index] = result;
                    } else if (p._type == TransformationParameterSet::Type::Float3) {
                        assert(driver._samplerOffset < 3);
                        float3s[p._index][driver._samplerOffset] = result;
                    } else if (p._type == TransformationParameterSet::Type::Float4) {
                        assert(driver._samplerOffset < 4);
                        float4s[p._index][driver._samplerOffset] = result;
                    }
                }
            }
        }

        for (   size_t c=constantDriverStartIndex; c<constantDriverEndIndex; ++c) {
            const ConstantDriver& driver = _constantDrivers[c];
            unsigned transInputIndex = binding.AnimDriverToMachineParameter(driver._parameterIndex);
            if (transInputIndex == ~unsigned(0x0)) {
                continue;   // (unbound output)
            }

            assert(transInputIndex < inputInterface._parameterCount);
            const TransformationMachine::InputInterface::Parameter& p 
                = inputInterface._parameters[transInputIndex];

            const void* data    = PtrAdd(_constantData, driver._dataOffset);
            if (driver._samplerType == TransformationParameterSet::Type::Float4x4) {
                assert(p._type == TransformationParameterSet::Type::Float4x4);
                float4x4s[p._index] = *(const Float4x4*)data;
            } else if (driver._samplerType == TransformationParameterSet::Type::Float4) {
                if (p._type == TransformationParameterSet::Type::Float4) {
                    float4s[p._index] = *(const Float4*)data;
                } else if (p._type == TransformationParameterSet::Type::Float3) {
                    float3s[p._index] = Truncate(*(const Float4*)data);
                }
            } else if (driver._samplerType == TransformationParameterSet::Type::Float3) {
                assert(p._type == TransformationParameterSet::Type::Float3);
                float3s[p._index] = *(Float3*)data;
            } else if (driver._samplerType == TransformationParameterSet::Type::Float1) {
                if (p._type == TransformationParameterSet::Type::Float1) {
                    float1s[p._index] = *(float*)data;
                } else if (p._type == TransformationParameterSet::Type::Float3) {
                    assert(driver._samplerOffset < 3);
                    float3s[p._index][driver._samplerOffset] = *(const float*)data;
                } else if (p._type == TransformationParameterSet::Type::Float4) {
                    assert(driver._samplerOffset < 4);
                    float4s[p._index][driver._samplerOffset] = *(const float*)data;
                }
            }
        }

        return result;
    }

    AnimationSet::Animation AnimationSet::FindAnimation(uint64 animation) const
    {
        for (size_t c=0; c<_animationCount; ++c) {
            if (_animations[c]._name == animation) {
                return _animations[c];
            }
        }
        Animation result;
        result._name = 0ull;
        result._beginDriver = result._endDriver = 0;
        result._beginTime = result._endTime = 0.f;
        return result;
    }

    unsigned                AnimationSet::FindParameter(uint64 parameterName) const
    {
        for (size_t c=0; c<_outputInterface._parameterInterfaceCount; ++c) {
            if (_outputInterface._parameterInterfaceDefinition[c] == parameterName) {
                return unsigned(c);
            }
        }
        return ~unsigned(0x0);
    }

    AnimationSet::AnimationSet() {}
    AnimationSet::~AnimationSet()
    {
        DestroyArray(_animationDrivers,         &_animationDrivers[_animationDriverCount]);
    }

    AnimationImmutableData::AnimationImmutableData() {}
    AnimationImmutableData::~AnimationImmutableData()
    {
        DestroyArray(_curves, &_curves[_curvesCount]);
    }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class SkinPrepareMachine::Pimpl
    {
    public:
        std::unique_ptr<AnimationSetBinding> _animationSetBinding;
        std::unique_ptr<SkeletonBinding> _skeletonBinding;
        const AnimationSetScaffold* _animationSetScaffold;
        const TransformationMachine* _transMachine;
    };

    void SkinPrepareMachine::PrepareAnimation(   
            Metal::DeviceContext* context, 
            ModelRenderer::PreparedAnimation& state) const
    {
        auto& skeleton = *_pimpl->_transMachine;
            
        auto finalMatCount = skeleton.GetOutputMatrixCount();
        state._finalMatrices = std::make_unique<Float4x4[]>(finalMatCount);
        if (_pimpl->_animationSetScaffold && !Tweakable("AnimBasePose", false)) {
            auto& animSet = _pimpl->_animationSetScaffold->ImmutableData();
            auto params = animSet._animationSet.BuildTransformationParameterSet(
                state._animState, 
                skeleton, *_pimpl->_animationSetBinding, 
                animSet._curves, animSet._curvesCount);
            
            skeleton.GenerateOutputTransforms(state._finalMatrices.get(), finalMatCount, &params);
        } else {
            skeleton.GenerateOutputTransforms(state._finalMatrices.get(), finalMatCount, &skeleton.GetDefaultParameters());
        }
    }

    const SkeletonBinding& SkinPrepareMachine::GetSkeletonBinding() const
    {
        return *_pimpl->_skeletonBinding;
    }

    unsigned SkinPrepareMachine::GetSkeletonOutputCount() const
    {
        return _pimpl->_transMachine->GetOutputMatrixCount();
    }

    SkinPrepareMachine::SkinPrepareMachine(const ModelScaffold& skinScaffold, const AnimationSetScaffold& animationScaffold, const SkeletonScaffold& skeletonScaffold)
    {
        auto pimpl = std::make_unique<Pimpl>();
        pimpl->_animationSetBinding = std::make_unique<AnimationSetBinding>(
            animationScaffold.ImmutableData()._animationSet.GetOutputInterface(), 
            skeletonScaffold.GetTransformationMachine().GetInputInterface());
        pimpl->_skeletonBinding = std::make_unique<SkeletonBinding>(
            skeletonScaffold.GetTransformationMachine().GetOutputInterface(), 
            skinScaffold.CommandStream().GetInputInterface());
        pimpl->_animationSetScaffold = &animationScaffold;
        pimpl->_transMachine = &skeletonScaffold.GetTransformationMachine();
        _pimpl = std::move(pimpl);
    }

    SkinPrepareMachine::SkinPrepareMachine(const ModelScaffold& skinScaffold, const TransformationMachine& transMachine)
    {
        auto pimpl = std::make_unique<Pimpl>();
        pimpl->_skeletonBinding = std::make_unique<SkeletonBinding>(
            transMachine.GetOutputInterface(), 
            skinScaffold.CommandStream().GetInputInterface());
        pimpl->_animationSetScaffold = nullptr;
        pimpl->_transMachine = &transMachine;
        _pimpl = std::move(pimpl);
    }

    SkinPrepareMachine::~SkinPrepareMachine()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    const TransformationMachine&   SkeletonScaffold::GetTransformationMachine() const                
    {
        Resolve(); 
        return *(const TransformationMachine*)Serialization::Block_GetFirstObject(_rawMemoryBlock.get());
    }

    const TransformationMachine*   SkeletonScaffold::TryImmutableData() const
    {
        if (!_rawMemoryBlock) return nullptr;
        return (const TransformationMachine*)Serialization::Block_GetFirstObject(_rawMemoryBlock.get());
    }

    static const ::Assets::AssetChunkRequest SkeletonScaffoldChunkRequests[]
    {
        ::Assets::AssetChunkRequest { "Scaffold", ChunkType_Skeleton, 0, ::Assets::AssetChunkRequest::DataType::BlockSerializer },
    };
    
    SkeletonScaffold::SkeletonScaffold(const ::Assets::ResChar filename[])
    : ChunkFileAsset("SkeletonScaffold")
    {
        Prepare(filename, ResolveOp{MakeIteratorRange(SkeletonScaffoldChunkRequests), &Resolver});
    }

    SkeletonScaffold::SkeletonScaffold(std::shared_ptr<::Assets::ICompileMarker>&& marker)
    : ChunkFileAsset("SkeletonScaffold")
    {
        Prepare(*marker, ResolveOp{MakeIteratorRange(SkeletonScaffoldChunkRequests), &Resolver});
    }

    SkeletonScaffold::SkeletonScaffold(SkeletonScaffold&& moveFrom)
    : ::Assets::ChunkFileAsset(std::move(moveFrom)) 
    , _rawMemoryBlock(std::move(moveFrom._rawMemoryBlock))
    {}

    SkeletonScaffold& SkeletonScaffold::operator=(SkeletonScaffold&& moveFrom)
    {
        ::Assets::ChunkFileAsset::operator=(std::move(moveFrom));
        _rawMemoryBlock = std::move(moveFrom._rawMemoryBlock);
        return *this;
    }

    SkeletonScaffold::~SkeletonScaffold()
    {
        auto* data = TryImmutableData();
        if (data)
            data->~TransformationMachine();
    }

    void SkeletonScaffold::Resolver(void* obj, IteratorRange<::Assets::AssetChunkResult*> chunks)
    {
        auto* scaffold = (SkeletonScaffold*)obj;
        if (scaffold) {
            scaffold->_rawMemoryBlock = std::move(chunks[0]._buffer);
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    const AnimationImmutableData&   AnimationSetScaffold::ImmutableData() const                
    {
        Resolve(); 
        return *(const AnimationImmutableData*)Serialization::Block_GetFirstObject(_rawMemoryBlock.get());
    }

    const AnimationImmutableData*   AnimationSetScaffold::TryImmutableData() const
    {
        if (!_rawMemoryBlock) return nullptr;
        return (const AnimationImmutableData*)Serialization::Block_GetFirstObject(_rawMemoryBlock.get());
    }

    static const ::Assets::AssetChunkRequest AnimationSetScaffoldChunkRequests[]
    {
        ::Assets::AssetChunkRequest { "Scaffold", ChunkType_AnimationSet, 0, ::Assets::AssetChunkRequest::DataType::BlockSerializer },
    };
    
    AnimationSetScaffold::AnimationSetScaffold(const ::Assets::ResChar filename[])
    : ChunkFileAsset("AnimationSetScaffold")
    {
        Prepare(filename, ResolveOp{MakeIteratorRange(AnimationSetScaffoldChunkRequests), &Resolver});
    }

    AnimationSetScaffold::AnimationSetScaffold(std::shared_ptr<::Assets::ICompileMarker>&& marker)
    : ChunkFileAsset("AnimationSetScaffold")
    {
        Prepare(*marker, ResolveOp{MakeIteratorRange(AnimationSetScaffoldChunkRequests), &Resolver});
    }

    AnimationSetScaffold::AnimationSetScaffold(AnimationSetScaffold&& moveFrom)
    : ::Assets::ChunkFileAsset(std::move(moveFrom)) 
    , _rawMemoryBlock(std::move(moveFrom._rawMemoryBlock))
    {}

    AnimationSetScaffold& AnimationSetScaffold::operator=(AnimationSetScaffold&& moveFrom)
    {
        ::Assets::ChunkFileAsset::operator=(std::move(moveFrom));
        _rawMemoryBlock = std::move(moveFrom._rawMemoryBlock);
        return *this;
    }

    AnimationSetScaffold::~AnimationSetScaffold()
    {
        auto* data = TryImmutableData();
        if (data)
            data->~AnimationImmutableData();
    }

    void AnimationSetScaffold::Resolver(void* obj, IteratorRange<::Assets::AssetChunkResult*> chunks)
    {
        auto* scaffold = (AnimationSetScaffold*)obj;
        if (scaffold) {
            scaffold->_rawMemoryBlock = std::move(chunks[0]._buffer);
        }
    }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class Vertex_PC
    {
    public:
        Float3      _position;
        unsigned    _color;
    };

    static void RenderSkeleton_DebugIterator(const Float4x4& parent, const Float4x4& child, const void* userData)
    {
        Vertex_PC start;   start._position   = ExtractTranslation(parent);   start._color = 0xffff7f7f;
        Vertex_PC end;       end._position   = ExtractTranslation(child);      end._color = 0xffffffff;
        ((std::vector<Vertex_PC>*)userData)->push_back(start);
        ((std::vector<Vertex_PC>*)userData)->push_back(end);
    }

    void    SkinPrepareMachine::RenderSkeleton( 
                Metal::DeviceContext* context, 
                Techniques::ParsingContext& parserContext, 
                const AnimationState& animState, const Float4x4& localToWorld)
    {
        using namespace RenderCore::Metal;

        InputElementDesc vertexInputLayout[] = {
            InputElementDesc( "POSITION",   0, NativeFormat::R32G32B32_FLOAT ),
            InputElementDesc( "COLOR",      0, NativeFormat::R8G8B8A8_UNORM )
        };

            //      Setup basic state --- blah, blah, blah..., 

        ConstantBufferPacket constantBufferPackets[1];
        constantBufferPackets[0] = Techniques::MakeLocalTransformPacket(
            localToWorld, 
            ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld));

        const auto& shaderProgram = ::Assets::GetAsset<ShaderProgram>(  
            "game/xleres/forward/illum.vsh:main:" VS_DefShaderModel, 
            "game/xleres/forward/illum.psh:main", "GEO_HAS_COLOUR=1");
        BoundInputLayout boundVertexInputLayout(std::make_pair(vertexInputLayout, dimof(vertexInputLayout)), shaderProgram);
        context->Bind(boundVertexInputLayout);
        context->Bind(shaderProgram);

        BoundUniforms boundLayout(shaderProgram);
        static const auto HashLocalTransform = Hash64("LocalTransform");
        boundLayout.BindConstantBuffer(HashLocalTransform, 0, 1);
        Techniques::TechniqueContext::BindGlobalUniforms(boundLayout);
        boundLayout.Apply(*context, 
            parserContext.GetGlobalUniformsStream(),
            UniformsStream(constantBufferPackets, nullptr, dimof(constantBufferPackets)));

            //      Construct the lines for while iterating through the transformation machine

        std::vector<Vertex_PC> workingVertices;
        auto& skeleton = *_pimpl->_transMachine;
        auto temp = std::make_unique<Float4x4[]>(skeleton.GetOutputMatrixCount());
        if (_pimpl->_animationSetScaffold) {
            auto& animSet = _pimpl->_animationSetScaffold->ImmutableData();
            auto params = animSet._animationSet.BuildTransformationParameterSet(
                    animState, skeleton, *_pimpl->_animationSetBinding.get(), 
                    animSet._curves, animSet._curvesCount);
            using namespace std::placeholders;
            skeleton.GenerateOutputTransforms(
                temp.get(), skeleton.GetOutputMatrixCount(),
                &params, 
                std::bind(&RenderSkeleton_DebugIterator, _1, _2, &workingVertices));
        } else {
            using namespace std::placeholders;
            skeleton.GenerateOutputTransforms(
                temp.get(), skeleton.GetOutputMatrixCount(), &skeleton.GetDefaultParameters(),
                std::bind(&RenderSkeleton_DebugIterator, _1, _2, &workingVertices));
        }

        VertexBuffer vertexBuffer(AsPointer(workingVertices.begin()), workingVertices.size()*sizeof(Vertex_PC));
        context->Bind(MakeResourceList(vertexBuffer), sizeof(Vertex_PC), 0);
        context->Bind(Techniques::CommonResources()._dssDisable);
        context->Bind(Techniques::CommonResources()._blendOpaque);
        context->Bind(Topology::LineList);
        context->Draw((unsigned)workingVertices.size());
    }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    static unsigned currentFrameIndex = 0;
    void IncreaseTBufferTemporaryTextureFrameIndex() { ++currentFrameIndex; }       // should be more or less synced with Present()

    TBufferTemporaryTexture::TBufferTemporaryTexture() { _size = 0; }
    TBufferTemporaryTexture::TBufferTemporaryTexture(const void* sourceData, size_t bufferSize, bool dynamicResource)
    {
        _lastAllocatedFrame = currentFrameIndex;
        _size = bufferSize;
        _shaderOffsetValue = 0;

		#if GFXAPI_ACTIVE == GFXAPI_DX11
			using namespace Metal;
			D3D11_BUFFER_DESC bufferDesc;
			bufferDesc.ByteWidth = (UINT)bufferSize;
			bufferDesc.Usage = dynamicResource?D3D11_USAGE_DYNAMIC:D3D11_USAGE_DEFAULT;
			bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			bufferDesc.CPUAccessFlags = dynamicResource?D3D11_CPU_ACCESS_WRITE:0;
			bufferDesc.MiscFlags = 0;
			bufferDesc.StructureByteStride = 0;

			auto* objFactory = GetObjectFactory();

			D3D11_SUBRESOURCE_DATA subData;
			subData.pSysMem = sourceData;
			subData.SysMemPitch = subData.SysMemSlicePitch = (UINT)bufferSize;
			_resource = objFactory->CreateBuffer(&bufferDesc, sourceData?(&subData):nullptr);

			bufferDesc.Usage = D3D11_USAGE_STAGING;
			bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			bufferDesc.BindFlags = 0;
			_stagingResource = objFactory->CreateBuffer(&bufferDesc);

			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
			srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER ;
			srvDesc.Buffer.ElementOffset = 0;
			srvDesc.Buffer.ElementWidth = (UINT)(bufferSize / (4*sizeof(float)));
			auto srv = objFactory->CreateShaderResourceView(_resource.get(), &srvDesc);
			_view = ShaderResourceView(srv.get());
		#endif
    }

    TBufferTemporaryTexture::TBufferTemporaryTexture(const TBufferTemporaryTexture& cloneFrom)
    :       _resource(cloneFrom._resource)
    ,       _stagingResource(cloneFrom._stagingResource)
    ,       _view(cloneFrom._view)
    ,       _size(cloneFrom._size)
    ,       _shaderOffsetValue(cloneFrom._shaderOffsetValue)
    ,       _lastAllocatedFrame(cloneFrom._lastAllocatedFrame)
    {}

    TBufferTemporaryTexture::TBufferTemporaryTexture(TBufferTemporaryTexture&& moveFrom)
    :       _resource(std::move(moveFrom._resource))
    ,       _stagingResource(std::move(moveFrom._stagingResource))
    ,       _view(std::move(moveFrom._view))
    ,       _size(moveFrom._size)
    ,       _shaderOffsetValue(moveFrom._shaderOffsetValue)
    ,       _lastAllocatedFrame(moveFrom._lastAllocatedFrame)
    {}

    TBufferTemporaryTexture& TBufferTemporaryTexture::operator=(const TBufferTemporaryTexture& cloneFrom)
    {
        _resource = cloneFrom._resource;
        _stagingResource = cloneFrom._stagingResource;
        _view = cloneFrom._view;
        _size = cloneFrom._size;
        _shaderOffsetValue = cloneFrom._shaderOffsetValue;
        _lastAllocatedFrame = cloneFrom._lastAllocatedFrame;
        return *this;
    }

    TBufferTemporaryTexture& TBufferTemporaryTexture::operator=(TBufferTemporaryTexture&& moveFrom)
    {
        _resource = std::move(moveFrom._resource);
        _stagingResource = std::move(moveFrom._stagingResource);
        _view = std::move(moveFrom._view);
        _size = std::move(moveFrom._size);
        _shaderOffsetValue = std::move(moveFrom._shaderOffsetValue);
        _lastAllocatedFrame = std::move(moveFrom._lastAllocatedFrame);
        return *this;
    }

    static const unsigned FrameBufferCount = 4;
    static std::vector<TBufferTemporaryTexture> textures[FrameBufferCount];

    static TBufferTemporaryTexture AllocateExistingTBufferTemporaryTexture(size_t bufferSize)
    {
        auto& thisFrameTextures = textures[currentFrameIndex%FrameBufferCount];
        for (auto i=thisFrameTextures.begin(); i!=thisFrameTextures.end(); ++i) {
            if (i->_size == bufferSize && i->_lastAllocatedFrame < currentFrameIndex) {
                i->_lastAllocatedFrame = currentFrameIndex;
                return *i;
            }
        }

        return TBufferTemporaryTexture();
    }

    static TBufferTemporaryTexture AllocateNewTBufferTemporaryTexture(const void* bufferData, size_t bufferSize)
    {
        auto& thisFrameTextures = textures[currentFrameIndex%FrameBufferCount];
        thisFrameTextures.push_back(std::move(TBufferTemporaryTexture(bufferData, bufferSize)));
        return thisFrameTextures[thisFrameTextures.size()-1];
    }

    static void PushTBufferTemporaryTexture(Metal::DeviceContext* context, TBufferTemporaryTexture& tex)
    {
        Metal::Copy(*context, *tex._resource.get(), *tex._stagingResource.get());
    }
}}

