// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ColladaCompilerInterface.h"
#include "CompilationThread.h"
#include "../../ColladaConversion/NascentModel.h"
#include "../../ColladaConversion/DLLInterface.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/CompilerHelper.h"
#include "../../Assets/InvalidAssetManager.h"
#include "../../Assets/AssetServices.h"
#include "../../ConsoleRig/AttachableLibrary.h"
#include "../../Utility/Threading/LockFree.h"
#include "../../Utility/Threading/ThreadObject.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/StringFormat.h"

#define SUPPORT_OLD_PATH

#pragma warning(disable:4505)       // warning C4505: 'RenderCore::Assets::SerializeToFile' : unreferenced local function has been removed

namespace RenderCore { namespace Assets 
{
    static const auto* ColladaLibraryName = "ColladaConversion.dll";

    class ColladaCompiler::Pimpl
    {
    public:
            // ---------- interface to DLL functions ----------
        ColladaConversion::CreateCompileOperationFn* _createCompileOpFunction;

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

    static void BuildChunkFile(
        BasicFile& file,
        ColladaConversion::NascentChunkArray chunks,
        const ConsoleRig::LibVersionDesc& versionInfo,
        std::function<bool(const ColladaConversion::NascentChunk&)> predicate)
    {
        unsigned chunksForMainFile = 0;
        for (const auto& c:*chunks)
            if (predicate(c))
                ++chunksForMainFile;

        using namespace Serialization::ChunkFile;
        auto header = MakeChunkFileHeader(
            chunksForMainFile, 
            versionInfo._versionString, versionInfo._buildDateString);
        file.Write(&header, sizeof(header), 1);

        unsigned trackingOffset = unsigned(file.TellP() + sizeof(ChunkHeader) * chunksForMainFile);
        for (const auto& c:*chunks)
            if (predicate(c)) {
                auto hdr = c._hdr;
                hdr._fileOffset = trackingOffset;
                file.Write(&hdr, sizeof(c._hdr), 1);
                trackingOffset += hdr._size;
            }

        for (const auto& c:*chunks)
            if (predicate(c))
                file.Write(AsPointer(c._data.begin()), c._data.size(), 1);
    }

    static const auto ChunkType_Metrics = ConstHash64<'Metr', 'ics'>::Value;

    static void SerializeToFile(
        ColladaConversion::NascentChunkArray chunks,
        const char destinationFilename[],
        const ConsoleRig::LibVersionDesc& versionInfo)
    {
            // Create the directory if we need to...
        CreateDirectoryRecursive(MakeFileNameSplitter(destinationFilename).DriveAndPath());

            // We need to separate out chunks that will be written to
            // the main output file from chunks that will be written to
            // a metrics file.

        {
            BasicFile outputFile(destinationFilename, "wb");
            BuildChunkFile(outputFile, chunks, versionInfo,
                [](const ColladaConversion::NascentChunk& c) { return c._hdr._type != ChunkType_Metrics; });
        }

        for (const auto& c:*chunks)
            if (c._hdr._type == ChunkType_Metrics) {
                BasicFile outputFile(
                    StringMeld<MaxPath>() << destinationFilename << "-" << c._hdr._name,
                    "wb");
                outputFile.Write((const void*)AsPointer(c._data.cbegin()), 1, c._data.size());
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
        CreateDirectoryRecursive(MakeFileNameSplitter(destinationFilename).DriveAndPath());
    
        BasicFile outputFile(destinationFilename, "wb");
        BuildChunkFile(outputFile, chunks, versionInfo,
            [](const ColladaConversion::NascentChunk& c) { return c._hdr._type != ChunkType_Metrics; });
    }
///////////////////////////////////////////////////////////////////////////////////////////////////

    void ColladaCompiler::Pimpl::PerformCompile(QueuedCompileOperation& op)
    {
        TRY
        {
            auto splitName = MakeFileNameSplitter(op._initializer0);

            AttachLibrary();

            ConsoleRig::LibVersionDesc libVersionDesc;
            _library.TryGetVersion(libVersionDesc);

            if (op._typeCode != Type_AnimationSet) {

                    // We need to do some processing of the filename
                    // the filename should take this form:
                    //      [drive]path/filename[.extension][:rootnode]
                    // We will paste on .dae if the extension is missing.
                    // And when there is no root node, we just pass nullptr
                    // to the serialize functions.

                ::Assets::ResChar colladaFile[MaxPath], fileAndParameters[MaxPath];
                XlCopyString(colladaFile, splitName.AllExceptParameters());
                if (splitName.Extension().Empty())
                    XlCatString(colladaFile, dimof(colladaFile), ".dae");

				XlCopyString(fileAndParameters, colladaFile);
				if (!splitName.Parameters().Empty()) {
					XlCatString(fileAndParameters, ":");
					XlCatString(fileAndParameters, splitName.Parameters());
				}

                TRY 
                {
                    const auto* destinationFile = op.GetLocator()._sourceID0;
                    ::Assets::ResChar temp[MaxPath];
                    if (op._typeCode == Type_RawMat) {
                            // When building rawmat, a material name could be on the op._sourceID0
                            // string. But we need to remove it from the path to find the real output
                            // name.
                        XlCopyString(temp, MakeFileNameSplitter(op.GetLocator()._sourceID0).AllExceptParameters());
                        destinationFile = temp;
                    }

                    if (_newPathOk) {

                        auto model = (*_createCompileOpFunction)(fileAndParameters);
						
						// look for the first target of the correct type
						auto targetCount = model->TargetCount();
						bool foundTarget = false;
						for (unsigned t=0; t<targetCount; ++t)
							if (model->GetTarget(t)._type == op._typeCode) {
								auto chunks = model->SerializeTarget(t);
								if (op._typeCode != Type_RawMat) {
									SerializeToFile(chunks, destinationFile, libVersionDesc);
								} else 
									SerializeToFileJustChunk(chunks, destinationFile, libVersionDesc);
								foundTarget = true;
								break;
							}

						if (!foundTarget)
							Throw(::Exceptions::BasicLabel("Could not find target of the requested type in compile operation for (%s)", fileAndParameters));

                    } else {

                        if (!_oldPathOk)
                            Throw(::Exceptions::BasicLabel("Error while linking collada conversion DLL. Some interface functions are missing"));

                        #if defined(SUPPORT_OLD_PATH)
                            if (op._typeCode == Type_Model) {
                                auto model = (*_ocCreateModel)(colladaFile);
                                SerializeToFile(*model, _ocSerializeSkinFunction, destinationFile, libVersionDesc);

                                char matName[MaxPath];
                                op._destinationStore->MakeIntermediateName(matName, dimof(matName), op._initializer0);
                                XlChopExtension(matName);
                                XlCatString(matName, dimof(matName), "-rawmat");
                                SerializeToFileJustChunk(*model, _ocSerializeMaterialsFunction, matName, libVersionDesc);
                            } else {
                                auto model = (*_ocCreateModel)(colladaFile);
                                SerializeToFile(*model, _ocSerializeSkeletonFunction, destinationFile, libVersionDesc);
                            }
                        #endif

                    }

                        // write new dependencies
                    std::vector<::Assets::DependentFileState> deps;
                    deps.push_back(op._destinationStore->GetDependentFileState(colladaFile));
                    op.GetLocator()._dependencyValidation = op._destinationStore->WriteDependencies(destinationFile, splitName.DriveAndPath(), MakeIteratorRange(deps));
        
                    op.SetState(::Assets::AssetState::Ready);

                } CATCH(...) {
                    if (!op.GetLocator()._dependencyValidation) {
                        op.GetLocator()._dependencyValidation = std::make_shared<::Assets::DependencyValidation>();
                        ::Assets::RegisterFileDependency(op.GetLocator()._dependencyValidation, colladaFile);
                    }
                    throw;
                } CATCH_END

            }  else {
                    //  source for the animation set should actually be a directory name, and
                    //  we'll use all of the dae files in that directory as animation inputs
                auto sourceFiles = FindFiles(std::string(op._initializer0) + "/*.dae");
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
                            auto model = (*_createCompileOpFunction)(i->c_str());

                                //
                                //      Now, merge the animation data into 
                            (*_extractAnimationsFn)(*mergedAnimationSet.get(), *model.get(), baseName);
                        } CATCH (const std::exception& e) {
                                // on exception, ignore this animation file and move on to the next
                            LogAlwaysError << "Exception while processing animation: (" << baseName << "). Exception is: (" << e.what() << ")";
                        } CATCH_END

                        deps.push_back(op._destinationStore->GetDependentFileState(i->c_str()));
                    }

                    SerializeToFile((*_serializeAnimationSetFn)(*mergedAnimationSet), op.GetLocator()._sourceID0, libVersionDesc);
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

                        SerializeToFile(((*mergedAnimationSet).*_ocSerializeAnimationFunction)(), op.GetLocator()._sourceID0, libVersionDesc);
                    #endif
                }

                op.GetLocator()._dependencyValidation = op._destinationStore->WriteDependencies(op.GetLocator()._sourceID0, splitName.DriveAndPath(), MakeIteratorRange(deps));
                if (::Assets::Services::GetInvalidAssetMan())
                    ::Assets::Services::GetInvalidAssetMan()->MarkValid(op._initializer0);
                op.SetState(::Assets::AssetState::Ready);
            }
        } CATCH(const std::exception& e) {
            LogAlwaysError << "Caught exception while performing Collada conversion. Exception details as follows:";
            LogAlwaysError << e.what();
            if (::Assets::Services::GetInvalidAssetMan())
                ::Assets::Services::GetInvalidAssetMan()->MarkInvalid(op._initializer0, e.what());
            op.SetState(::Assets::AssetState::Invalid);
        } CATCH(...) {
            if (::Assets::Services::GetInvalidAssetMan())
                ::Assets::Services::GetInvalidAssetMan()->MarkInvalid(op._initializer0, "Unknown error");
            op.SetState(::Assets::AssetState::Invalid);
        } CATCH_END
    }
    
    class ColladaCompiler::Marker : public ::Assets::ICompileMarker
    {
    public:
        ::Assets::IntermediateAssetLocator GetExistingAsset() const;
        std::shared_ptr<::Assets::PendingCompileMarker> InvokeCompile() const;
        StringSection<::Assets::ResChar> Initializer() const;

        Marker(
            const ::Assets::ResChar requestName[], uint64 typeCode,
            const ::Assets::IntermediateAssets::Store& store,
            std::shared_ptr<ColladaCompiler> compiler);
        ~Marker();
    private:
        std::weak_ptr<ColladaCompiler> _compiler;
        ::Assets::rstring _requestName;
        uint64 _typeCode;
        const ::Assets::IntermediateAssets::Store* _store;

        void MakeIntermediateName(::Assets::ResChar destination[], size_t count) const;
    };

    void ColladaCompiler::Marker::MakeIntermediateName(::Assets::ResChar destination[], size_t count) const
    {
        bool stripParams = _typeCode == ColladaCompiler::Type_RawMat;
        if (stripParams) {
            _store->MakeIntermediateName(
                destination, (unsigned)count, MakeFileNameSplitter(_requestName).AllExceptParameters());
        } else {
            _store->MakeIntermediateName(destination, (unsigned)count, _requestName.c_str());
        }

        switch (_typeCode) {
        default:
            assert(0);
        case Type_Model: XlCatString(destination, count, "-skin"); break;
        case Type_Skeleton: XlCatString(destination, count, "-skel"); break;
        case Type_AnimationSet: XlCatString(destination, count, "-anim"); break;
        case Type_RawMat: XlCatString(destination, count, "-rawmat"); break;
        }
    }

    ::Assets::IntermediateAssetLocator ColladaCompiler::Marker::GetExistingAsset() const
    {
        ::Assets::IntermediateAssetLocator result;
        MakeIntermediateName(result._sourceID0, dimof(result._sourceID0));
        result._dependencyValidation = _store->MakeDependencyValidation(result._sourceID0);
        if (_typeCode == Type_RawMat)
            XlCatString(result._sourceID0, MakeFileNameSplitter(_requestName).ParametersWithDivider());
        return result;
    }

    std::shared_ptr<::Assets::PendingCompileMarker> ColladaCompiler::Marker::InvokeCompile() const
    {
        auto c = _compiler.lock();
        if (!c) return nullptr;

            // Queue this compilation operation to occur in the background thread.
            //
            // With the old path,  we couldn't do multiple Collada compilation at the same time. 
            // So this implementation just spawns a single dedicated threads.
            //
            // However, with the new implementation, we could use one of the global thread pools
            // and it should be ok to queue up multiple compilations at the same time.
        auto backgroundOp = std::make_shared<QueuedCompileOperation>();
        backgroundOp->SetInitializer(_requestName.c_str());
        XlCopyString(backgroundOp->_initializer0, _requestName);
        MakeIntermediateName(backgroundOp->GetLocator()._sourceID0, dimof(backgroundOp->GetLocator()._sourceID0));
        if (_typeCode == Type_RawMat)
            XlCatString(backgroundOp->GetLocator()._sourceID0, MakeFileNameSplitter(_requestName).ParametersWithDivider());
        backgroundOp->_destinationStore = _store;
        backgroundOp->_typeCode = _typeCode;

        {
            ScopedLock(c->_pimpl->_threadLock);
            if (!c->_pimpl->_thread) {
                auto* p = c->_pimpl.get();
                c->_pimpl->_thread = std::make_unique<CompilationThread>(
                    [p](QueuedCompileOperation& op) { p->PerformCompile(op); });
            }
        }
        c->_pimpl->_thread->Push(backgroundOp);
        
        return std::move(backgroundOp);
    }

    StringSection<::Assets::ResChar> ColladaCompiler::Marker::Initializer() const
    {
        return MakeStringSection(_requestName);
    }

    ColladaCompiler::Marker::Marker(
        const ::Assets::ResChar requestName[], uint64 typeCode,
        const ::Assets::IntermediateAssets::Store& store,
        std::shared_ptr<ColladaCompiler> compiler)
    : _compiler(std::move(compiler)), _requestName(requestName), _typeCode(typeCode), _store(&store)
    {}

    ColladaCompiler::Marker::~Marker() {}

    std::shared_ptr<::Assets::ICompileMarker> ColladaCompiler::PrepareAsset(
        uint64 typeCode, 
        const ::Assets::ResChar* initializers[], unsigned initializerCount, 
        const ::Assets::IntermediateAssets::Store& destinationStore)
    {
        return std::make_shared<Marker>(initializers[0], typeCode, destinationStore, shared_from_this());
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
					_createCompileOpFunction    = _library.GetFunction<decltype(_createCompileOpFunction)>("?CreateCompileOperation@ModelConversion@RenderCore@@YA?AV?$shared_ptr@VICompileOperation@ModelConversion@RenderCore@@@std@@QEBD@Z");
                    _createAnimationSetFn       = _library.GetFunction<decltype(_createAnimationSetFn)>("?CreateAnimationSet@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@VWorkingAnimationSet@ColladaConversion@RenderCore@@@std@@QBD@Z");
                    _extractAnimationsFn        = _library.GetFunction<decltype(_extractAnimationsFn)>("?ExtractAnimations@ColladaConversion@RenderCore@@YAXAEAVWorkingAnimationSet@12@AEBVICompileOperation@12@QEBD@Z");
                    _serializeAnimationSetFn    = _library.GetFunction<decltype(_serializeAnimationSetFn)>("?SerializeAnimationSet@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@V?$vector@VNascentChunk@ColladaConversion@RenderCore@@V?$allocator@VNascentChunk@ColladaConversion@RenderCore@@@std@@@std@@@std@@ABVWorkingAnimationSet@12@@Z");
                #else
                    _createCompileOpFunction    = _library.GetFunction<decltype(_createCompileOpFunction)>("?CreateCompileOperation@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@VICompileOperation@ColladaConversion@RenderCore@@@std@@QEBD@Z");
                    _createAnimationSetFn       = _library.GetFunction<decltype(_createAnimationSetFn)>("?CreateAnimationSet@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@VWorkingAnimationSet@ColladaConversion@RenderCore@@@std@@QEBD@Z");
                    _extractAnimationsFn        = _library.GetFunction<decltype(_extractAnimationsFn)>("?ExtractAnimations@ColladaConversion@RenderCore@@YAXAEAVWorkingAnimationSet@12@AEBVICompileOperation@12@QEBD@Z");
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

        _newPathOk = !!_createCompileOpFunction && !!_createAnimationSetFn && !!_extractAnimationsFn && !!_serializeAnimationSetFn;

        #if defined(SUPPORT_OLD_PATH)
            _oldPathOk = !!_ocCreateModel && !!_ocSerializeSkinFunction && !!_ocSerializeAnimationFunction && !!_ocSerializeSkeletonFunction && !!_ocMergeAnimationDataFunction;
        #endif

        if (!_newPathOk && !_oldPathOk)
            Throw(::Exceptions::BasicLabel("Error while linking collada conversion DLL. Some interface functions are missing"));
    }

}}

