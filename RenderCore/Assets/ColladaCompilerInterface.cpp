// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ColladaCompilerInterface.h"
#include "../../ColladaConversion/NascentModel.h"
#include "../../Assets/AssetUtils.h"
#include "../../ConsoleRig/AttachableLibrary.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Streams/FileUtils.h"

namespace RenderCore { namespace Assets 
{
    static const auto* ColladaLibraryName = "ColladaConversion.dll";

    class ColladaCompiler::Pimpl
    {
    public:
        ColladaConversion::ModelSerializeFunction _serializeSkinFunction;
        ColladaConversion::ModelSerializeFunction _serializeAnimationFunction;
        ColladaConversion::ModelSerializeFunction _serializeSkeletonFunction;
        ColladaConversion::ModelSerializeFunction _serializeMaterialsFunction;
        ColladaConversion::MergeAnimationDataFunction _mergeAnimationDataFunction;
        ColladaConversion::CreateModelFunction* _createModel;

        ConsoleRig::AttachableLibrary _library;
        bool _isAttached;
        bool _attemptedAttach;

        Pimpl() : _library(ColladaLibraryName)
        {
            _serializeSkinFunction = nullptr;
            _serializeAnimationFunction = nullptr;
            _serializeSkeletonFunction = nullptr;
            _serializeMaterialsFunction = nullptr;
            _mergeAnimationDataFunction = nullptr;
            _createModel = nullptr;
            _isAttached = _attemptedAttach = false;
        }
    };

    static void SerializeToFile(
        RenderCore::ColladaConversion::NascentModel& model, 
        RenderCore::ColladaConversion::ModelSerializeFunction fn,
        const char destinationFilename[],
        const ConsoleRig::LibVersionDesc& versionInfo)
    {
        auto chunks = (model.*fn)();

            // (create the directory if we need to)
        char dirName[MaxPath];
        XlDirname(dirName, dimof(dirName), destinationFilename);
        CreateDirectoryRecursive(dirName);

        using namespace Serialization::ChunkFile;
        ChunkFileHeader header;

        XlZeroMemory(header);
        header._magic = MagicHeader;
        header._fileVersionNumber = 0;
        XlCopyString(header._buildVersion, dimof(header._buildVersion), versionInfo._versionString);
        XlCopyString(header._buildDate, dimof(header._buildDate), versionInfo._buildDateString);
        header._chunkCount = chunks.second;
            
        BasicFile outputFile(destinationFilename, "wb");
        outputFile.Write(&header, sizeof(header), 1);

        unsigned trackingOffset = unsigned(outputFile.TellP() + sizeof(ChunkHeader) * chunks.second);
        for (unsigned i=0; i<chunks.second; ++i) {
            auto& c = chunks.first[i];
            auto hdr = c._hdr;
            hdr._fileOffset = trackingOffset;
            outputFile.Write(&hdr, sizeof(c._hdr), 1);
            trackingOffset += hdr._size;
        }

        for (unsigned i=0; i<chunks.second; ++i) {
            auto& c = chunks.first[i];
            outputFile.Write(AsPointer(c._data.begin()), c._data.size(), 1);
        }
    }

    static void SerializeToFileJustChunk(
        RenderCore::ColladaConversion::NascentModel& model, 
        RenderCore::ColladaConversion::ModelSerializeFunction fn,
        const char destinationFilename[],
        const ConsoleRig::LibVersionDesc& versionInfo)
    {
        auto chunks = (model.*fn)();

        BasicFile outputFile(destinationFilename, "wb");
        for (unsigned i=0; i<chunks.second; ++i) {
            auto& c = chunks.first[i];
            outputFile.Write(AsPointer(c._data.begin()), c._data.size(), 1);
        }
    }

    std::shared_ptr<::Assets::PendingCompileMarker> ColladaCompiler::PrepareResource(
        uint64 typeCode, 
        const ::Assets::ResChar* initializers[], unsigned initializerCount, 
        const ::Assets::IntermediateResources::Store& destinationStore)
    {
        char outputName[MaxPath];
        destinationStore.MakeIntermediateName(outputName, dimof(outputName), initializers[0]);
        switch (typeCode) {
        case Type_Model: XlCatString(outputName, dimof(outputName), "-skin"); break;
        case Type_Skeleton: XlCatString(outputName, dimof(outputName), "-skel"); break;
        case Type_AnimationSet: XlCatString(outputName, dimof(outputName), "-anim"); break;
        }
        if (DoesFileExist(outputName)) {
                // MakeDependencyValidation returns an object only if dependencies are currently good
             auto depVal = destinationStore.MakeDependencyValidation(outputName);
             if (depVal) {
                    // already exists -- just return "ready"
                return std::make_unique<::Assets::PendingCompileMarker>(
                    ::Assets::AssetState::Ready, outputName, ~0ull, std::move(depVal));
             }
        }

        char baseDir[MaxPath];
        XlDirname(baseDir, dimof(baseDir), initializers[0]);

        AttachLibrary();

        ConsoleRig::LibVersionDesc libVersionDesc;
        _pimpl->_library.TryGetVersion(libVersionDesc);

        if (typeCode == Type_Model || typeCode == Type_Skeleton) {
                // append an extension if it doesn't already exist
            char colladaFile[MaxPath];
            XlCopyString(colladaFile, initializers[0]);
            auto* extPtr = XlExtension(colladaFile);
            if (!extPtr || !*extPtr) {
                XlCatString(colladaFile, dimof(colladaFile), ".dae");
            }

            auto model = (*_pimpl->_createModel)(colladaFile);
            if (typeCode == Type_Model) {
                SerializeToFile(*model, _pimpl->_serializeSkinFunction, outputName, libVersionDesc);

                char matName[MaxPath];
                destinationStore.MakeIntermediateName(matName, dimof(matName), initializers[0]);
                XlChopExtension(matName);
                XlCatString(matName, dimof(matName), "-rawmat");
                SerializeToFileJustChunk(*model, _pimpl->_serializeMaterialsFunction, matName, libVersionDesc);
            } else {
                SerializeToFile(*model, _pimpl->_serializeSkeletonFunction, outputName, libVersionDesc);
            }

                // write new dependencies
            std::vector<::Assets::DependentFileState> deps;
            deps.push_back(destinationStore.GetDependentFileState(colladaFile));
            auto newDepVal = destinationStore.WriteDependencies(outputName, baseDir, AsPointer(deps.cbegin()), AsPointer(deps.cend()));
        
                // we can return a "ready" resource
            return std::make_unique<::Assets::PendingCompileMarker>(
                ::Assets::AssetState::Ready, outputName, ~uint64(0x0), std::move(newDepVal));
        }

        if (typeCode == Type_AnimationSet) {
                //  source for the animation set should actually be a directory name, and
                //  we'll use all of the dae files in that directory as animation inputs
            auto sourceFiles = FindFiles(std::string(initializers[0]) + "/*.dae");
            std::vector<::Assets::DependentFileState> deps;

            auto mergedAnimationSet = (*_pimpl->_createModel)(nullptr);
            for (auto i=sourceFiles.begin(); i!=sourceFiles.end(); ++i) {
                char baseName[MaxPath]; // get the base name of the file (without the extension)
                XlBasename(baseName, dimof(baseName), i->c_str());
                XlChopExtension(baseName);

                TRY {
                        //
                        //      First; load the animation file as a model
                        //          note that this will do geometry processing; etc -- but all that geometry
                        //          information will be ignored.
                        //
                    auto model = (*_pimpl->_createModel)(i->c_str());

                        //
                        //      Now, merge the animation data into 
                    (mergedAnimationSet.get()->*_pimpl->_mergeAnimationDataFunction)(*model.get(), baseName);
                } CATCH (const std::exception& e) {
                        // on exception, ignore this animation file and move on to the next
                    LogAlwaysError << "Exception while processing animation: (" << baseName << "). Exception is: (" << e.what() << ")";
                } CATCH_END

                deps.push_back(destinationStore.GetDependentFileState(i->c_str()));
            }

            SerializeToFile(*mergedAnimationSet, _pimpl->_serializeAnimationFunction, outputName, libVersionDesc);
            auto newDepVal = destinationStore.WriteDependencies(outputName, baseDir, AsPointer(deps.cbegin()), AsPointer(deps.cend()));

            return std::make_unique<::Assets::PendingCompileMarker>(
                ::Assets::AssetState::Ready, outputName, ~uint64(0x0), std::move(newDepVal));
        }

        assert(0);
        return false;   // unknown asset type!
    }

    ColladaCompiler::ColladaCompiler()
    {
        _pimpl = std::make_unique<Pimpl>();
    }

    ColladaCompiler::~ColladaCompiler()
    {
    }

    void ColladaCompiler::AttachLibrary()
    {
        if (!_pimpl->_attemptedAttach && !_pimpl->_isAttached) {
            _pimpl->_attemptedAttach = true;

            _pimpl->_isAttached = _pimpl->_library.TryAttach();
            if (_pimpl->_isAttached) {
                using namespace RenderCore::ColladaConversion;

                    //  Find and set the function pointers we need
                    //  Note the function names have been decorated by the compiler
                    //      ... we could consider a better method for doing this... Maybe the DLL should just
                    //          export a single method, and just query function addresses with some fixed guids...? 
                #if !TARGET_64BIT
                    const char CreateModelName[]                = "?CreateModel@ColladaConversion@RenderCore@@YA?AV?$unique_ptr@VNascentModel@ColladaConversion@RenderCore@@VCrossDLLDeletor@Internal@23@@std@@QBD@Z";
                    const char ModelSerializeSkinName[]         = "?SerializeSkin@NascentModel@ColladaConversion@RenderCore@@QBE?AU?$pair@V?$unique_ptr@$$BY0A@VNascentChunk@ColladaConversion@RenderCore@@VCrossDLLDeletor@Internal@23@@std@@I@std@@XZ";
                    const char ModelSerializeAnimationName[]    = "?SerializeAnimationSet@NascentModel@ColladaConversion@RenderCore@@QBE?AU?$pair@V?$unique_ptr@$$BY0A@VNascentChunk@ColladaConversion@RenderCore@@VCrossDLLDeletor@Internal@23@@std@@I@std@@XZ";
                    const char ModelSerializeSkeletonName[]     = "?SerializeSkeleton@NascentModel@ColladaConversion@RenderCore@@QBE?AU?$pair@V?$unique_ptr@$$BY0A@VNascentChunk@ColladaConversion@RenderCore@@VCrossDLLDeletor@Internal@23@@std@@I@std@@XZ";
                    const char ModelSerializeMaterialsName[]    = "?SerializeMaterials@NascentModel@ColladaConversion@RenderCore@@QBE?AU?$pair@V?$unique_ptr@$$BY0A@VNascentChunk@ColladaConversion@RenderCore@@VCrossDLLDeletor@Internal@23@@std@@I@std@@XZ";
                    const char ModelMergeAnimationDataName[]    = "?MergeAnimationData@NascentModel@ColladaConversion@RenderCore@@QAEXABV123@QBD@Z";
                #else
                    const char CreateModelName[]                = "?CreateModel@ColladaConversion@RenderCore@@YA?AV?$unique_ptr@VNascentModel@ColladaConversion@RenderCore@@VCrossDLLDeletor@Internal@23@@std@@QEBD@Z";
                    const char ModelSerializeSkinName[]         = "?SerializeSkin@NascentModel@ColladaConversion@RenderCore@@QEBA?AU?$pair@V?$unique_ptr@$$BY0A@VNascentChunk@ColladaConversion@RenderCore@@VCrossDLLDeletor@Internal@23@@std@@I@std@@XZ";
                    const char ModelSerializeAnimationName[]    = "?SerializeAnimationSet@NascentModel@ColladaConversion@RenderCore@@QEBA?AU?$pair@V?$unique_ptr@$$BY0A@VNascentChunk@ColladaConversion@RenderCore@@VCrossDLLDeletor@Internal@23@@std@@I@std@@XZ";
                    const char ModelSerializeSkeletonName[]     = "?SerializeSkeleton@NascentModel@ColladaConversion@RenderCore@@QEBA?AU?$pair@V?$unique_ptr@$$BY0A@VNascentChunk@ColladaConversion@RenderCore@@VCrossDLLDeletor@Internal@23@@std@@I@std@@XZ";
                    const char ModelSerializeMaterialsName[]    = "?SerializeMaterials@NascentModel@ColladaConversion@RenderCore@@QEBA?AU?$pair@V?$unique_ptr@$$BY0A@VNascentChunk@ColladaConversion@RenderCore@@VCrossDLLDeletor@Internal@23@@std@@I@std@@XZ";
                    const char ModelMergeAnimationDataName[]    = "?MergeAnimationData@NascentModel@ColladaConversion@RenderCore@@QEAAXAEBV123@QEBD@Z";
                #endif
                
                auto& lib = _pimpl->_library;
                _pimpl->_createModel                = lib.GetFunction<decltype(_pimpl->_createModel)>(CreateModelName);
                _pimpl->_serializeSkinFunction      = lib.GetFunction<decltype(_pimpl->_serializeSkinFunction)>(ModelSerializeSkinName);
                _pimpl->_serializeAnimationFunction = lib.GetFunction<decltype(_pimpl->_serializeAnimationFunction)>(ModelSerializeAnimationName);
                _pimpl->_serializeSkeletonFunction  = lib.GetFunction<decltype(_pimpl->_serializeSkeletonFunction)>(ModelSerializeSkeletonName);
                _pimpl->_serializeMaterialsFunction = lib.GetFunction<decltype(_pimpl->_serializeMaterialsFunction)>(ModelSerializeMaterialsName);
                _pimpl->_mergeAnimationDataFunction = lib.GetFunction<decltype(_pimpl->_mergeAnimationDataFunction)>(ModelMergeAnimationDataName);
            }
        }

            // check for problems (missing functions or bad version number)
        if (!_pimpl->_isAttached)
            ThrowException(::Exceptions::BasicLabel("Error while linking collada conversion DLL. Could not find DLL (%s)", ColladaLibraryName));
        if (!_pimpl->_createModel || !_pimpl->_serializeSkinFunction || !_pimpl->_serializeAnimationFunction || !_pimpl->_serializeSkeletonFunction || !_pimpl->_mergeAnimationDataFunction)
            ThrowException(::Exceptions::BasicLabel("Error while linking collada conversion DLL. Some interface functions are missing"));
    }

}}

