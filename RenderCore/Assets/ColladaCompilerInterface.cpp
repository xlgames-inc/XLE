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

#define SUPPORT_OLD_PATH

#pragma warning(disable:4505)       // warning C4505: 'RenderCore::Assets::SerializeToFile' : unreferenced local function has been removed

namespace RenderCore { namespace Assets 
{
    static const auto* ColladaLibraryName = "ColladaConversion.dll";

    class QueuedCompileOperation : public ::Assets::PendingCompileMarker
    {
    public:
        uint64 _typeCode;
        ::Assets::ResChar _initializer[MaxPath];

        const ::Assets::IntermediateAssets::Store* _destinationStore;
    };

    class ColladaCompiler::Pimpl
    {
    public:
            // ---------- interface to DLL functions ----------
        ColladaConversion::CreateColladaScaffoldFn* _createColladaScaffold;
        ColladaConversion::ModelSerializeFn*        _serializeSkinFunction;
        ColladaConversion::ModelSerializeFn*        _serializeSkeletonFunction;
        ColladaConversion::ModelSerializeFn*        _serializeMaterialsFunction;

        ColladaConversion::CreateAnimationSetFn*    _createAnimationSetFn;
        ColladaConversion::ExtractAnimationsFn*     _extractAnimationsFn;
        ColladaConversion::SerializeAnimationSetFn* _serializeAnimationSetFn;

        #if defined(SUPPORT_OLD_PATH)
                // ---------- old "Open Collada" based interface ----------
            ColladaConversion::OCModelSerializeFunction     _ocSerializeSkinFunction;
            ColladaConversion::OCModelSerializeFunction     _ocSerializeAnimationFunction;
            ColladaConversion::OCModelSerializeFunction     _ocSerializeSkeletonFunction;
            ColladaConversion::OCModelSerializeFunction     _ocSerializeMaterialsFunction;
            ColladaConversion::OCMergeAnimationDataFunction _ocMergeAnimationDataFunction;
            ColladaConversion::OCCreateModelFunction*       _ocCreateModel;
        #endif

            // --------------------------------------------------------
        ConsoleRig::AttachableLibrary _library;
        bool _isAttached;
        bool _attemptedAttach;
        bool _newPathOk, _oldPathOk;

        void PerformCompile(QueuedCompileOperation& op);
        void AttachLibrary();

        class CompilationThread;
        Threading::Mutex _threadLock;   // (used while initialising _thread for the first time)
        std::unique_ptr<CompilationThread> _thread;

        Pimpl() : _library(ColladaLibraryName)
        {
            #if defined(NEW_COLLADA_PATH)

                _createColladaScaffold = nullptr;
                _serializeSkinFunction = nullptr;
                _serializeSkeletonFunction = nullptr;
                _serializeMaterialsFunction = nullptr;
                _createAnimationSetFn = nullptr;
                _extractAnimationsFn = nullptr;
                _serializeAnimationSetFn = nullptr;

            #else

                _ocSerializeSkinFunction = nullptr;
                _ocSerializeAnimationFunction = nullptr;
                _ocSerializeSkeletonFunction = nullptr;
                _ocSerializeMaterialsFunction = nullptr;
                _ocMergeAnimationDataFunction = nullptr;
                _ocCreateModel = nullptr;

            #endif

            _isAttached = _attemptedAttach = false;
            _newPathOk = _oldPathOk = false;
        }
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    static void SerializeToFile(
        ColladaConversion::NascentChunkArray chunks,
        const char destinationFilename[],
        const ConsoleRig::LibVersionDesc& versionInfo)
    {
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
        header._chunkCount = (unsigned)chunks->size();
            
        BasicFile outputFile(destinationFilename, "wb");
        outputFile.Write(&header, sizeof(header), 1);

        unsigned trackingOffset = unsigned(outputFile.TellP() + sizeof(ChunkHeader) * chunks->size());
        for (unsigned i=0; i<(unsigned)chunks->size(); ++i) {
            auto& c = (*chunks)[i];
            auto hdr = c._hdr;
            hdr._fileOffset = trackingOffset;
            outputFile.Write(&hdr, sizeof(c._hdr), 1);
            trackingOffset += hdr._size;
        }

        for (unsigned i=0; i<(unsigned)chunks->size(); ++i) {
            auto& c = (*chunks)[i];
            outputFile.Write(AsPointer(c._data.begin()), c._data.size(), 1);
        }
    }

    static void SerializeToFileJustChunk(
        ColladaConversion::NascentChunkArray chunks,
        const char destinationFilename[],
        const ConsoleRig::LibVersionDesc& versionInfo)
    {
        BasicFile outputFile(destinationFilename, "wb");
        for (unsigned i=0; i<(unsigned)chunks->size(); ++i) {
            auto& c = (*chunks)[i];
            outputFile.Write(AsPointer(c._data.begin()), c._data.size(), 1);
        }
    }

    static void SerializeToFileJustChunk(
        RenderCore::ColladaConversion::NascentModel& model, 
        RenderCore::ColladaConversion::OCModelSerializeFunction fn,
        const char destinationFilename[],
        const ConsoleRig::LibVersionDesc& versionInfo)
    {
        auto chunks = (model.*fn)();
    
        BasicFile outputFile(destinationFilename, "wb");
        for (unsigned i=0; i<(unsigned)chunks->size(); ++i) {
            auto& c = (*chunks)[i];
            outputFile.Write(AsPointer(c._data.begin()), c._data.size(), 1);
        }
    }

    static void SerializeToFile(
        RenderCore::ColladaConversion::NascentModel& model, 
        RenderCore::ColladaConversion::OCModelSerializeFunction fn,
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
        header._chunkCount = (unsigned)chunks->size();
            
        BasicFile outputFile(destinationFilename, "wb");
        outputFile.Write(&header, sizeof(header), 1);
    
        unsigned trackingOffset = unsigned(outputFile.TellP() + sizeof(ChunkHeader) * chunks->size());
        for (unsigned i=0; i<(unsigned)chunks->size(); ++i) {
            auto& c = (*chunks)[i];
            auto hdr = c._hdr;
            hdr._fileOffset = trackingOffset;
            outputFile.Write(&hdr, sizeof(c._hdr), 1);
            trackingOffset += hdr._size;
        }
    
        for (unsigned i=0; i<(unsigned)chunks->size(); ++i) {
            auto& c = (*chunks)[i];
            outputFile.Write(AsPointer(c._data.begin()), c._data.size(), 1);
        }
    }
///////////////////////////////////////////////////////////////////////////////////////////////////

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

                FileNameSplitter<::Assets::ResChar> splitName(op._initializer);

                    // We need to do some processing of the filename
                    // the filename should take this form:
                    //      [drive]path/filename[.extension][:rootnode]
                    // We will paste on .dae if the extension is missing.
                    // And when there is no root node, we just pass nullptr
                    // to the serialize functions.

                ::Assets::ResChar colladaFile[MaxPath], rootNode[MaxPath];
                XlCopyString(colladaFile, splitName.AllExceptParameters());
                if (splitName.Extension().Empty())
                    XlCatString(colladaFile, dimof(colladaFile), ".dae");
                XlCopyString(rootNode, splitName.Parameters());

                if (_newPathOk) {

                    if (op._typeCode == Type_Model) {
                        auto model2 = (*_createColladaScaffold)(colladaFile);
                        SerializeToFile(
                            (*_serializeSkinFunction)(*model2, rootNode),
                            op._sourceID0, libVersionDesc);

                        char matName[MaxPath];
                        op._destinationStore->MakeIntermediateName(matName, dimof(matName), op._initializer);
                        XlChopExtension(matName);
                        XlCatString(matName, dimof(matName), "-rawmat");
                        SerializeToFileJustChunk(
                            (*_serializeMaterialsFunction)(*model2, rootNode), 
                            matName, libVersionDesc);
                    } else {
                        auto model = (*_createColladaScaffold)(colladaFile);
                        SerializeToFile(
                            (*_serializeSkeletonFunction)(*model, rootNode),
                            op._sourceID0, libVersionDesc);
                    }

                } else {

                    if (!_oldPathOk)
                        Throw(::Exceptions::BasicLabel("Error while linking collada conversion DLL. Some interface functions are missing"));

                    #if defined(SUPPORT_OLD_PATH)
                        if (op._typeCode == Type_Model) {
                            auto model = (*_ocCreateModel)(colladaFile);
                            SerializeToFile(*model, _ocSerializeSkinFunction, op._sourceID0, libVersionDesc);

                            char matName[MaxPath];
                            op._destinationStore->MakeIntermediateName(matName, dimof(matName), op._initializer);
                            XlChopExtension(matName);
                            XlCatString(matName, dimof(matName), "-rawmat");
                            SerializeToFileJustChunk(*model, _ocSerializeMaterialsFunction, matName, libVersionDesc);
                        } else {
                            auto model = (*_ocCreateModel)(colladaFile);
                            SerializeToFile(*model, _ocSerializeSkeletonFunction, op._sourceID0, libVersionDesc);
                        }
                    #endif

                }

                    // write new dependencies
                std::vector<::Assets::DependentFileState> deps;
                deps.push_back(op._destinationStore->GetDependentFileState(colladaFile));
                op._dependencyValidation = op._destinationStore->WriteDependencies(op._sourceID0, baseDir, MakeIteratorRange(deps));
        
                op.SetState(::Assets::AssetState::Ready);
            }

            if (op._typeCode == Type_AnimationSet) {
                    //  source for the animation set should actually be a directory name, and
                    //  we'll use all of the dae files in that directory as animation inputs
                auto sourceFiles = FindFiles(std::string(op._initializer) + "/*.dae");
                std::vector<::Assets::DependentFileState> deps;

                if (_newPathOk) {
                    auto mergedAnimationSet = (*_createAnimationSetFn)("mergedanim");
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
                            auto model = (*_createColladaScaffold)(i->c_str());

                                //
                                //      Now, merge the animation data into 
                            (*_extractAnimationsFn)(*mergedAnimationSet.get(), *model.get(), baseName);
                        } CATCH (const std::exception& e) {
                                // on exception, ignore this animation file and move on to the next
                            LogAlwaysError << "Exception while processing animation: (" << baseName << "). Exception is: (" << e.what() << ")";
                        } CATCH_END

                        deps.push_back(op._destinationStore->GetDependentFileState(i->c_str()));
                    }

                    SerializeToFile((*_serializeAnimationSetFn)(*mergedAnimationSet), op._sourceID0, libVersionDesc);
                } else {
                    if (!_oldPathOk)
                        Throw(::Exceptions::BasicLabel("Error while linking collada conversion DLL. Some interface functions are missing"));
                        
                    #if defined(SUPPORT_OLD_PATH)
                        auto mergedAnimationSet = (*_ocCreateModel)(nullptr);
                        for (auto i=sourceFiles.begin(); i!=sourceFiles.end(); ++i) {
                            char baseName[MaxPath]; // get the base name of the file (without the extension)
                            XlBasename(baseName, dimof(baseName), i->c_str());
                            XlChopExtension(baseName);

                            TRY {
                                auto model = (*_ocCreateModel)(i->c_str());
                                (mergedAnimationSet.get()->*_ocMergeAnimationDataFunction)(*model.get(), baseName);
                            } CATCH (const std::exception& e) {
                                LogAlwaysError << "Exception while processing animation: (" << baseName << "). Exception is: (" << e.what() << ")";
                            } CATCH_END

                            deps.push_back(op._destinationStore->GetDependentFileState(i->c_str()));
                        }

                        SerializeToFile(((*mergedAnimationSet).*_ocSerializeAnimationFunction)(), op._sourceID0, libVersionDesc);
                    #endif
                }

                op._dependencyValidation = op._destinationStore->WriteDependencies(op._sourceID0, baseDir, MakeIteratorRange(deps));
                op.SetState(::Assets::AssetState::Ready);
            }
        } CATCH(...) {
            op.SetState(::Assets::AssetState::Invalid);
        } CATCH_END
    }
    
    std::shared_ptr<::Assets::PendingCompileMarker> ColladaCompiler::PrepareAsset(
        uint64 typeCode, 
        const ::Assets::ResChar* initializers[], unsigned initializerCount, 
        const ::Assets::IntermediateAssets::Store& destinationStore)
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
             if (depVal && depVal->GetValidationIndex() == 0) {
                    // already exists -- just return "ready"
                return std::make_unique<::Assets::PendingCompileMarker>(
                    ::Assets::AssetState::Ready, outputName, ~0ull, std::move(depVal));
             }
        }

            // Queue this compilation operation to occur in the background thread.
            //
            // With the old path,  we couldn't do multiple Collada compilation at the same time. 
            // So this implementation just spawns a single dedicated threads.
            //
            // However, with the new implementation, we could use one of the global thread pools
            // and it should be ok to queue up multiple compilations at the same time.
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

                #if !TARGET_64BIT
                    _createColladaScaffold      = _library.GetFunction<decltype(_createColladaScaffold)>("?CreateColladaScaffold@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@VColladaScaffold@ColladaConversion@RenderCore@@@std@@QBD@Z");
                    _serializeSkinFunction      = _library.GetFunction<decltype(_serializeSkinFunction)>("?SerializeSkin@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@V?$vector@VNascentChunk@ColladaConversion@RenderCore@@V?$allocator@VNascentChunk@ColladaConversion@RenderCore@@@std@@@std@@@std@@ABVColladaScaffold@12@QBD@Z");
                    _serializeSkeletonFunction  = _library.GetFunction<decltype(_serializeSkeletonFunction)>("?SerializeSkeleton@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@V?$vector@VNascentChunk@ColladaConversion@RenderCore@@V?$allocator@VNascentChunk@ColladaConversion@RenderCore@@@std@@@std@@@std@@ABVColladaScaffold@12@QBD@Z");
                    _serializeMaterialsFunction = _library.GetFunction<decltype(_serializeMaterialsFunction)>("?SerializeMaterials@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@V?$vector@VNascentChunk@ColladaConversion@RenderCore@@V?$allocator@VNascentChunk@ColladaConversion@RenderCore@@@std@@@std@@@std@@ABVColladaScaffold@12@QBD@Z");
                    _createAnimationSetFn       = _library.GetFunction<decltype(_createAnimationSetFn)>("?CreateAnimationSet@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@VWorkingAnimationSet@ColladaConversion@RenderCore@@@std@@QBD@Z");
                    _extractAnimationsFn        = _library.GetFunction<decltype(_extractAnimationsFn)>("?ExtractAnimations@ColladaConversion@RenderCore@@YAXAAVWorkingAnimationSet@12@ABVColladaScaffold@12@QBD@Z");
                    _serializeAnimationSetFn    = _library.GetFunction<decltype(_serializeAnimationSetFn)>("?SerializeAnimationSet@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@V?$vector@VNascentChunk@ColladaConversion@RenderCore@@V?$allocator@VNascentChunk@ColladaConversion@RenderCore@@@std@@@std@@@std@@ABVWorkingAnimationSet@12@@Z");
                #else
                    _createColladaScaffold      = _library.GetFunction<decltype(_createColladaScaffold)>("?CreateColladaScaffold@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@VColladaScaffold@ColladaConversion@RenderCore@@@std@@QEBD@Z");
                    _serializeSkinFunction      = _library.GetFunction<decltype(_serializeSkinFunction)>("?SerializeSkin@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@V?$vector@VNascentChunk@ColladaConversion@RenderCore@@V?$allocator@VNascentChunk@ColladaConversion@RenderCore@@@std@@@std@@@std@@AEBVColladaScaffold@12@QEBD@Z");
                    _serializeSkeletonFunction  = _library.GetFunction<decltype(_serializeSkeletonFunction)>("?SerializeSkeleton@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@V?$vector@VNascentChunk@ColladaConversion@RenderCore@@V?$allocator@VNascentChunk@ColladaConversion@RenderCore@@@std@@@std@@@std@@AEBVColladaScaffold@12@QEBD@Z");
                    _serializeMaterialsFunction = _library.GetFunction<decltype(_serializeMaterialsFunction)>("?SerializeMaterials@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@V?$vector@VNascentChunk@ColladaConversion@RenderCore@@V?$allocator@VNascentChunk@ColladaConversion@RenderCore@@@std@@@std@@@std@@AEBVColladaScaffold@12@QEBD@Z");
                    _createAnimationSetFn       = _library.GetFunction<decltype(_createAnimationSetFn)>("?CreateAnimationSet@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@VWorkingAnimationSet@ColladaConversion@RenderCore@@@std@@QEBD@Z");
                    _extractAnimationsFn        = _library.GetFunction<decltype(_extractAnimationsFn)>("?ExtractAnimations@ColladaConversion@RenderCore@@YAXAEAVWorkingAnimationSet@12@AEBVColladaScaffold@12@QEBD@Z");
                    _serializeAnimationSetFn    = _library.GetFunction<decltype(_serializeAnimationSetFn)>("?SerializeAnimationSet@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@V?$vector@VNascentChunk@ColladaConversion@RenderCore@@V?$allocator@VNascentChunk@ColladaConversion@RenderCore@@@std@@@std@@@std@@AEBVWorkingAnimationSet@12@@Z");
                #endif

                #if defined(SUPPORT_OLD_PATH)
                    #if !TARGET_64BIT
                            // old "OpenCollada" path
                        _ocCreateModel                = _library.GetFunction<decltype(_ocCreateModel)>("?OCCreateModel@ColladaConversion@RenderCore@@YA?AV?$unique_ptr@VNascentModel@ColladaConversion@RenderCore@@VCrossDLLDeletor@Internal@23@@std@@QBD@Z");
                        _ocSerializeSkinFunction      = _library.GetFunction<decltype(_ocSerializeSkinFunction)>("?SerializeSkin@NascentModel@ColladaConversion@RenderCore@@QBE?AV?$shared_ptr@V?$vector@VNascentChunk@ColladaConversion@RenderCore@@V?$allocator@VNascentChunk@ColladaConversion@RenderCore@@@std@@@std@@@std@@XZ");
                        _ocSerializeAnimationFunction = _library.GetFunction<decltype(_ocSerializeAnimationFunction)>("?SerializeAnimationSet@NascentModel@ColladaConversion@RenderCore@@QBE?AV?$shared_ptr@V?$vector@VNascentChunk@ColladaConversion@RenderCore@@V?$allocator@VNascentChunk@ColladaConversion@RenderCore@@@std@@@std@@@std@@XZ");
                        _ocSerializeSkeletonFunction  = _library.GetFunction<decltype(_ocSerializeSkeletonFunction)>("?SerializeSkeleton@NascentModel@ColladaConversion@RenderCore@@QBE?AV?$shared_ptr@V?$vector@VNascentChunk@ColladaConversion@RenderCore@@V?$allocator@VNascentChunk@ColladaConversion@RenderCore@@@std@@@std@@@std@@XZ");
                        _ocSerializeMaterialsFunction = _library.GetFunction<decltype(_ocSerializeMaterialsFunction)>("?SerializeMaterials@NascentModel@ColladaConversion@RenderCore@@QBE?AV?$shared_ptr@V?$vector@VNascentChunk@ColladaConversion@RenderCore@@V?$allocator@VNascentChunk@ColladaConversion@RenderCore@@@std@@@std@@@std@@XZ");
                        _ocMergeAnimationDataFunction = _library.GetFunction<decltype(_ocMergeAnimationDataFunction)>("?MergeAnimationData@NascentModel@ColladaConversion@RenderCore@@QAEXABV123@QBD@Z");
                    #else
                            // old "OpenCollada" path
                        _ocCreateModel                = _library.GetFunction<decltype(_ocCreateModel)>("?OCCreateModel@ColladaConversion@RenderCore@@YA?AV?$unique_ptr@VNascentModel@ColladaConversion@RenderCore@@VCrossDLLDeletor@Internal@23@@std@@QEBD@Z");
                        _ocSerializeSkinFunction      = _library.GetFunction<decltype(_ocSerializeSkinFunction)>("?SerializeSkin@NascentModel@ColladaConversion@RenderCore@@QEBA?AV?$shared_ptr@V?$vector@VNascentChunk@ColladaConversion@RenderCore@@V?$allocator@VNascentChunk@ColladaConversion@RenderCore@@@std@@@std@@@std@@XZ");
                        _ocSerializeAnimationFunction = _library.GetFunction<decltype(_ocSerializeAnimationFunction)>("?SerializeAnimationSet@NascentModel@ColladaConversion@RenderCore@@QEBA?AV?$shared_ptr@V?$vector@VNascentChunk@ColladaConversion@RenderCore@@V?$allocator@VNascentChunk@ColladaConversion@RenderCore@@@std@@@std@@@std@@XZ");
                        _ocSerializeSkeletonFunction  = _library.GetFunction<decltype(_ocSerializeSkeletonFunction)>("?SerializeSkeleton@NascentModel@ColladaConversion@RenderCore@@QEBA?AV?$shared_ptr@V?$vector@VNascentChunk@ColladaConversion@RenderCore@@V?$allocator@VNascentChunk@ColladaConversion@RenderCore@@@std@@@std@@@std@@XZ");
                        _ocSerializeMaterialsFunction = _library.GetFunction<decltype(_ocSerializeMaterialsFunction)>("?SerializeMaterials@NascentModel@ColladaConversion@RenderCore@@QEBA?AV?$shared_ptr@V?$vector@VNascentChunk@ColladaConversion@RenderCore@@V?$allocator@VNascentChunk@ColladaConversion@RenderCore@@@std@@@std@@@std@@XZ");
                        _ocMergeAnimationDataFunction = _library.GetFunction<decltype(_ocMergeAnimationDataFunction)>("?MergeAnimationData@NascentModel@ColladaConversion@RenderCore@@QEAAXAEBV123@QEBD@Z");
                    #endif
                #endif
            }
        }

            // check for problems (missing functions or bad version number)
        if (!_isAttached)
            Throw(::Exceptions::BasicLabel("Error while linking collada conversion DLL. Could not find DLL (%s)", ColladaLibraryName));

        _newPathOk = !!_createColladaScaffold && !!_serializeSkinFunction && !!_serializeSkeletonFunction && !!_serializeMaterialsFunction && !!_createAnimationSetFn && !!_extractAnimationsFn && !!_serializeAnimationSetFn;

        #if defined(SUPPORT_OLD_PATH)
            _oldPathOk = !!_ocCreateModel && !!_ocSerializeSkinFunction && !!_ocSerializeAnimationFunction && !!_ocSerializeSkeletonFunction && !!_ocMergeAnimationDataFunction;
        #endif

        if (!_newPathOk && !_oldPathOk)
            Throw(::Exceptions::BasicLabel("Error while linking collada conversion DLL. Some interface functions are missing"));
    }

}}

