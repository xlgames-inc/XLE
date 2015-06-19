// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ColladaCompilerInterface.h"
#include "../../ColladaConversion/NascentModel.h"
#include "../../ColladaConversion/DLLInterface.h"
#include "../../Assets/AssetUtils.h"
#include "../../ConsoleRig/AttachableLibrary.h"
#include "../../Utility/Threading/LockFree.h"
#include "../../Utility/Threading/ThreadObject.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Streams/FileUtils.h"

#define NEW_COLLADA_PATH

#pragma warning(disable:4505)       // warning C4505: 'RenderCore::Assets::SerializeToFile' : unreferenced local function has been removed

namespace RenderCore { namespace Assets 
{
    static const auto* ColladaLibraryName = "ColladaConversion.dll";

    class QueuedCompileOperation : public ::Assets::PendingCompileMarker
    {
    public:
        uint64 _typeCode;
        ::Assets::ResChar _initializer[MaxPath];

        const ::Assets::IntermediateResources::Store* _destinationStore;
    };

    class ColladaCompiler::Pimpl
    {
    public:
        ColladaConversion::ModelSerializeFunction _serializeSkinFunction;
        ColladaConversion::ModelSerializeFunction _serializeAnimationFunction;
        ColladaConversion::ModelSerializeFunction _serializeSkeletonFunction;
        ColladaConversion::ModelSerializeFunction _serializeMaterialsFunction;
        ColladaConversion::MergeAnimationDataFunction _mergeAnimationDataFunction;
        ColladaConversion::CreateModelFunction* _createModel;

        ColladaConversion::CreateColladaScaffoldFn* _createModel2;
        ColladaConversion::Model2SerializeFn* _serializeSkinFunction2;
        ColladaConversion::Model2SerializeFn* _serializeSkeletonFunction2;
        ColladaConversion::Model2SerializeFn* _serializeMaterialsFunction2;

        ColladaConversion::CreateAnimationSetFn* _createAnimationSetFn;
        ColladaConversion::ExtractAnimationsFn* _extractAnimationsFn;
        ColladaConversion::SerializeAnimationSet2Fn* _serializeAnimationSetFn;

        ConsoleRig::AttachableLibrary _library;
        bool _isAttached;
        bool _attemptedAttach;

        void PerformCompile(QueuedCompileOperation& op);
        void AttachLibrary();

        class CompilationThread;
        Threading::Mutex _threadLock;   // (used while initialising _thread for the first time)
        std::unique_ptr<CompilationThread> _thread;

        Pimpl() : _library(ColladaLibraryName)
        {
            _serializeSkinFunction = nullptr;
            _serializeAnimationFunction = nullptr;
            _serializeSkeletonFunction = nullptr;
            _serializeMaterialsFunction = nullptr;
            _mergeAnimationDataFunction = nullptr;
            _createModel = nullptr;
            _isAttached = _attemptedAttach = false;

            _createModel2 = nullptr;
            _serializeSkinFunction2 = nullptr;
            _serializeSkeletonFunction2 = nullptr;
            _serializeMaterialsFunction2 = nullptr;
            _createAnimationSetFn = nullptr;
            _extractAnimationsFn = nullptr;
            _serializeAnimationSetFn = nullptr;
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

    template<typename Type>
        static void SerializeToFile(
            const Type& model, 
            ColladaConversion::NascentChunkArray2 (*fn)(const Type&),
            const char destinationFilename[],
            const ConsoleRig::LibVersionDesc& versionInfo)
    {
        auto chunks = (*fn)(model);

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

    static void SerializeToFileJustChunk(
        RenderCore::ColladaConversion::ColladaScaffold& model, 
        RenderCore::ColladaConversion::Model2SerializeFn fn,
        const char destinationFilename[],
        const ConsoleRig::LibVersionDesc& versionInfo)
    {
        auto chunks = fn(model);

        BasicFile outputFile(destinationFilename, "wb");
        for (unsigned i=0; i<chunks.second; ++i) {
            auto& c = chunks.first[i];
            outputFile.Write(AsPointer(c._data.begin()), c._data.size(), 1);
        }
    }

    class ColladaCompiler::Pimpl::CompilationThread
    {
    public:
        void Push(std::shared_ptr<QueuedCompileOperation> op);
        void StallOnPendingOperations(bool cancelAll);

        CompilationThread(ColladaCompiler::Pimpl* pimpl);
        ~CompilationThread();
    protected:
        std::thread _thread;
        XlHandle _events[2];
        volatile bool _workerQuit;
        LockFree::FixedSizeQueue<std::weak_ptr<QueuedCompileOperation>, 256> _queue;

        ColladaCompiler::Pimpl* _pimpl; // (unprotected because it owns us)

        void ThreadFunction();
    };

    void ColladaCompiler::Pimpl::CompilationThread::StallOnPendingOperations(bool cancelAll)
    {
        if (!_workerQuit) {
            _workerQuit = true;
            XlSetEvent(_events[1]);   // trigger a manual reset event should wake all threads (and keep them awake)
            _thread.join();
        }
    }
    
    void ColladaCompiler::Pimpl::CompilationThread::Push(std::shared_ptr<QueuedCompileOperation> op)
    {
        if (!_workerQuit) {
            _queue.push_overflow(std::move(op));
            XlSetEvent(_events[0]);
        }
    }

    void ColladaCompiler::Pimpl::CompilationThread::ThreadFunction()
    {
        while (!_workerQuit) {
            std::weak_ptr<QueuedCompileOperation>* op;
            if (_queue.try_front(op)) {
                auto o = op->lock();
                if (o) _pimpl->PerformCompile(*o);
                _queue.pop();
            } else {
                XlWaitForMultipleSyncObjects(
                    2, this->_events,
                    false, XL_INFINITE, true);
            }
        }
    }

    ColladaCompiler::Pimpl::CompilationThread::CompilationThread(ColladaCompiler::Pimpl* pimpl)
    : _pimpl(pimpl)
    {
        _events[0] = XlCreateEvent(false);
        _events[1] = XlCreateEvent(true);
        _workerQuit = false;

        _thread = std::thread(std::bind(&CompilationThread::ThreadFunction, this));
    }

    ColladaCompiler::Pimpl::CompilationThread::~CompilationThread()
    {
        StallOnPendingOperations(true);
        XlCloseSyncObject(_events[0]);
        XlCloseSyncObject(_events[1]);
    }

    void ColladaCompiler::Pimpl::PerformCompile(QueuedCompileOperation& op)
    {
        TRY
        {
            char baseDir[MaxPath];
            XlDirname(baseDir, dimof(baseDir), op._initializer);

            AttachLibrary();

            ConsoleRig::LibVersionDesc libVersionDesc;
            _library.TryGetVersion(libVersionDesc);

            if (op._typeCode == Type_Model || op._typeCode == Type_Skeleton) {
                    // append an extension if it doesn't already exist
                char colladaFile[MaxPath];
                XlCopyString(colladaFile, op._initializer);
                auto* extPtr = XlExtension(colladaFile);
                if (!extPtr || !*extPtr) {
                    XlCatString(colladaFile, dimof(colladaFile), ".dae");
                }

                #if defined(NEW_COLLADA_PATH)
                    if (op._typeCode == Type_Model) {
                        auto model2 = (*_createModel2)(colladaFile);
                        SerializeToFile(*model2, _serializeSkinFunction2, op._sourceID0, libVersionDesc);

                        char matName[MaxPath];
                        op._destinationStore->MakeIntermediateName(matName, dimof(matName), op._initializer);
                        XlChopExtension(matName);
                        XlCatString(matName, dimof(matName), "-rawmat");
                        SerializeToFileJustChunk(*model2, _serializeMaterialsFunction2, matName, libVersionDesc);
                    } else {
                        auto model = (*_createModel2)(colladaFile);
                        SerializeToFile(*model, _serializeSkeletonFunction2, op._sourceID0, libVersionDesc);
                    }
                #else
                    if (op._typeCode == Type_Model) {
                        auto model = (*_createModel)(colladaFile);
                        SerializeToFile(*model, _serializeSkinFunction, op._sourceID0, libVersionDesc);

                        char matName[MaxPath];
                        op._destinationStore->MakeIntermediateName(matName, dimof(matName), op._initializer);
                        XlChopExtension(matName);
                        XlCatString(matName, dimof(matName), "-rawmat");
                        SerializeToFileJustChunk(*model, _serializeMaterialsFunction, matName, libVersionDesc);
                    } else {
                        auto model = (*_createModel)(colladaFile);
                        SerializeToFile(*model, _serializeSkeletonFunction, op._sourceID0, libVersionDesc);
                    }
                #endif

                    // write new dependencies
                std::vector<::Assets::DependentFileState> deps;
                deps.push_back(op._destinationStore->GetDependentFileState(colladaFile));
                op._dependencyValidation = op._destinationStore->WriteDependencies(op._sourceID0, baseDir, AsPointer(deps.cbegin()), AsPointer(deps.cend()));
        
                op.SetState(::Assets::AssetState::Ready);
            }

            if (op._typeCode == Type_AnimationSet) {
                    //  source for the animation set should actually be a directory name, and
                    //  we'll use all of the dae files in that directory as animation inputs
                auto sourceFiles = FindFiles(std::string(op._initializer) + "/*.dae");
                std::vector<::Assets::DependentFileState> deps;

                #if defined(NEW_COLLADA_PATH)
                    auto mergedAnimationSet = (*_createAnimationSetFn)();
                #else
                    auto mergedAnimationSet = (*_createModel)(nullptr);
                #endif
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
                        #if defined(NEW_COLLADA_PATH)
                            auto model = (*_createModel2)(i->c_str());

                                //
                                //      Now, merge the animation data into 
                            (*_extractAnimationsFn)(*mergedAnimationSet.get(), *model.get(), baseName);
                        #else
                            auto model = (*_createModel)(i->c_str());
                            (mergedAnimationSet.get()->*_mergeAnimationDataFunction)(*model.get(), baseName);
                        #endif
                    } CATCH (const std::exception& e) {
                            // on exception, ignore this animation file and move on to the next
                        LogAlwaysError << "Exception while processing animation: (" << baseName << "). Exception is: (" << e.what() << ")";
                    } CATCH_END

                    deps.push_back(op._destinationStore->GetDependentFileState(i->c_str()));
                }

                #if defined(NEW_COLLADA_PATH)
                    SerializeToFile(*mergedAnimationSet, _serializeAnimationSetFn, op._sourceID0, libVersionDesc);
                #else
                    SerializeToFile(*mergedAnimationSet, _serializeAnimationFunction, op._sourceID0, libVersionDesc);
                #endif
                op._dependencyValidation = op._destinationStore->WriteDependencies(op._sourceID0, baseDir, AsPointer(deps.cbegin()), AsPointer(deps.cend()));

                op.SetState(::Assets::AssetState::Ready);
            }
        } CATCH(...) {
            op.SetState(::Assets::AssetState::Invalid);
        } CATCH_END
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

        default:
            assert(0);
            return nullptr;   // unknown asset type!
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

            // Queue this compilation operation to occur in the background thread.
            // We can't do multiple Collada compilation at the same time. So let's just
            // use a single thread
        auto backgroundOp = std::make_shared<QueuedCompileOperation>();
        XlCopyString(backgroundOp->_initializer, initializers[0]);
        XlCopyString(backgroundOp->_sourceID0, outputName);
        backgroundOp->_destinationStore = &destinationStore;
        backgroundOp->_typeCode = typeCode;

        {
            ScopedLock(_pimpl->_threadLock);
            if (!_pimpl->_thread)
                _pimpl->_thread = std::make_unique<Pimpl::CompilationThread>(_pimpl.get());
        }
        _pimpl->_thread->Push(backgroundOp);
        
        return std::move(backgroundOp);
    }

    void ColladaCompiler::StallOnPendingOperations(bool cancelAll)
    {
        {
            ScopedLock(_pimpl->_threadLock);
            if (!_pimpl->_thread) return;
        }
        _pimpl->_thread->StallOnPendingOperations(cancelAll);
    }

    ColladaCompiler::ColladaCompiler()
    {
        _pimpl = std::make_shared<Pimpl>();
    }

    ColladaCompiler::~ColladaCompiler()
    {
    }

    void ColladaCompiler::Pimpl::AttachLibrary()
    {
        if (!_attemptedAttach && !_isAttached) {
            _attemptedAttach = true;

            _isAttached = _library.TryAttach();
            if (_isAttached) {
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

                    const char CreateModel2Name[]                = "?CreateColladaScaffold@ColladaConversion@RenderCore@@YA?AV?$unique_ptr@VColladaScaffold@ColladaConversion@RenderCore@@VCrossDLLDeletor2@Internal@23@@std@@QEBD@Z";
                    const char Model2SerializeSkinName[]         = "?SerializeSkin2@ColladaConversion@RenderCore@@YA?AU?$pair@V?$unique_ptr@$$BY0A@VNascentChunk2@ColladaConversion@RenderCore@@VCrossDLLDeletor2@Internal@23@@std@@I@std@@AEBVColladaScaffold@12@@Z";
                    const char Model2SerializeSkeletonName[]     = "?SerializeSkeleton2@ColladaConversion@RenderCore@@YA?AU?$pair@V?$unique_ptr@$$BY0A@VNascentChunk2@ColladaConversion@RenderCore@@VCrossDLLDeletor2@Internal@23@@std@@I@std@@AEBVColladaScaffold@12@@Z";
                    const char Model2SerializeMaterialsName[]    = "?SerializeMaterials2@ColladaConversion@RenderCore@@YA?AU?$pair@V?$unique_ptr@$$BY0A@VNascentChunk2@ColladaConversion@RenderCore@@VCrossDLLDeletor2@Internal@23@@std@@I@std@@AEBVColladaScaffold@12@@Z";

                    const char CreateAnimationSetName[]          = "?CreateAnimationSet@ColladaConversion@RenderCore@@YA?AV?$unique_ptr@VWorkingAnimationSet@ColladaConversion@RenderCore@@VCrossDLLDeletor2@Internal@23@@std@@QEBD@Z";
                    const char ExtractAnimationsName[]           = "?ExtractAnimations@ColladaConversion@RenderCore@@YAXAEAVWorkingAnimationSet@12@AEBVColladaScaffold@12@QEBD@Z";
                    const char SerializeAnimationSetName[]       = "?SerializeAnimationSet2@ColladaConversion@RenderCore@@YA?AU?$pair@V?$unique_ptr@$$BY0A@VNascentChunk2@ColladaConversion@RenderCore@@VCrossDLLDeletor2@Internal@23@@std@@I@std@@AEBVWorkingAnimationSet@12@@Z";
                    
                #endif
                
                auto& lib = _library;
                _createModel                = lib.GetFunction<decltype(_createModel)>(CreateModelName);
                _serializeSkinFunction      = lib.GetFunction<decltype(_serializeSkinFunction)>(ModelSerializeSkinName);
                _serializeAnimationFunction = lib.GetFunction<decltype(_serializeAnimationFunction)>(ModelSerializeAnimationName);
                _serializeSkeletonFunction  = lib.GetFunction<decltype(_serializeSkeletonFunction)>(ModelSerializeSkeletonName);
                _serializeMaterialsFunction = lib.GetFunction<decltype(_serializeMaterialsFunction)>(ModelSerializeMaterialsName);
                _mergeAnimationDataFunction = lib.GetFunction<decltype(_mergeAnimationDataFunction)>(ModelMergeAnimationDataName);

                _createModel2 = lib.GetFunction<decltype(_createModel2)>(CreateModel2Name);
                _serializeSkinFunction2 = lib.GetFunction<decltype(_serializeSkinFunction2)>(Model2SerializeSkinName);
                _serializeSkeletonFunction2 = lib.GetFunction<decltype(_serializeSkeletonFunction2)>(Model2SerializeSkeletonName);
                _serializeMaterialsFunction2 = lib.GetFunction<decltype(_serializeMaterialsFunction2)>(Model2SerializeMaterialsName);
                _createAnimationSetFn = lib.GetFunction<decltype(_createAnimationSetFn)>(CreateAnimationSetName);
                _extractAnimationsFn = lib.GetFunction<decltype(_extractAnimationsFn)>(ExtractAnimationsName);
                _serializeAnimationSetFn = lib.GetFunction<decltype(_serializeAnimationSetFn)>(SerializeAnimationSetName);
            }
        }

            // check for problems (missing functions or bad version number)
        if (!_isAttached)
            ThrowException(::Exceptions::BasicLabel("Error while linking collada conversion DLL. Could not find DLL (%s)", ColladaLibraryName));
        #if defined(NEW_COLLADA_PATH)
            if (!_createModel2 || !_serializeSkinFunction2 || !_serializeSkeletonFunction2 || !_serializeMaterialsFunction2 || !_createAnimationSetFn || !_extractAnimationsFn || !_serializeAnimationSetFn)
                ThrowException(::Exceptions::BasicLabel("Error while linking collada conversion DLL. Some interface functions are missing"));
        #else
            if (!_createModel || !_serializeSkinFunction || !_serializeAnimationFunction || !_serializeSkeletonFunction || !_mergeAnimationDataFunction)
                ThrowException(::Exceptions::BasicLabel("Error while linking collada conversion DLL. Some interface functions are missing"));
        #endif
    }

}}

