// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CompilationThread.h"
#include "../OSServices/Log.h"
#include "../OSServices/WinAPI/System_WinAPI.h"
#include "../ConsoleRig/GlobalServices.h"
#include "../Utility/Threading/CompletionThreadPool.h"

namespace Assets 
{
    void CompilationThread::StallOnPendingOperations(bool cancelAll)
    {
        if (!_workerQuit) {
            _workerQuit = true;
            OSServices::XlSetEvent(_events[1]);   // trigger a manual reset event should wake all threads (and keep them awake)
            _thread.join();
        }
    }
    
    void CompilationThread::Push(
		std::shared_ptr<::Assets::ArtifactFuture> future,
		std::function<void(::Assets::ArtifactFuture&)> operation)
    {
        if (!_workerQuit) {
			_queue.push_overflow(Element{future, std::move(operation)});
            OSServices::XlSetEvent(_events[0]);
        }
    }

    void CompilationThread::ThreadFunction()
    {
        while (!_workerQuit) {
			Element* op = nullptr;
            if (_queue.try_front(op)) {
                auto future = op->_future.lock();
				auto fn = std::move(op->_operation);
				_queue.pop();

                TRY
                {
                    fn(*future);
                }
                CATCH (const ::Assets::Exceptions::PendingAsset&)
                {
                    // We need to stall on a pending asset while compiling
                    // All we can do is delay the request, and try again later.
                    // Let's move the request into a separate queue, so that
                    // new request get processed first.
                    _delayedQueue.push(Element{future, std::move(fn)});
                }
				CATCH (const ::Assets::Exceptions::ConstructionError& e)
				{
					auto artifact = std::make_shared<::Assets::CompilerExceptionArtifact>(e.GetActualizationLog(), e.GetDependencyValidation());
					future->AddArtifact("exception", artifact);
					future->SetState(::Assets::AssetState::Invalid);
				}
                CATCH (const std::exception& e)
                {
					auto artifact = std::make_shared<::Assets::CompilerExceptionArtifact>(::Assets::AsBlob(e), nullptr);
					future->AddArtifact("exception", artifact);
					future->SetState(::Assets::AssetState::Invalid);
                }
				CATCH (...)
				{
					future->SetState(::Assets::AssetState::Invalid);
				}
                CATCH_END

            } else if (_delayedQueue.try_front(op)) {
                
                    // do a short sleep first, do avoid too much
                    // trashing while processing delayed items. Note
                    // that if any new request comes in during this Sleep,
                    // then we won't handle that request in a prompt manner.
                Threading::Sleep(1);
				auto future = op->_future.lock();
				auto fn = std::move(op->_operation);
				_delayedQueue.pop();

                TRY
                {
					fn(*future);
                }
                CATCH (const ::Assets::Exceptions::PendingAsset&)
                {
                    // We need to stall on a pending asset while compiling
                    // All we can do is delay the request, and try again later.
                    // Let's move the request into a separate queue, so that
                    // new request get processed first.
					_delayedQueue.push(Element{ future, std::move(fn) });
                }
				CATCH (const ::Assets::Exceptions::ConstructionError& e)
				{
					auto artifact = std::make_shared<::Assets::CompilerExceptionArtifact>(e.GetActualizationLog(), e.GetDependencyValidation());
					future->AddArtifact("exception", artifact);
					future->SetState(::Assets::AssetState::Invalid);
				}
				CATCH (const std::exception& e)
				{
					auto artifact = std::make_shared<::Assets::CompilerExceptionArtifact>(::Assets::AsBlob(e), nullptr);
					future->AddArtifact("exception", artifact);
					future->SetState(::Assets::AssetState::Invalid);
				}
				CATCH (...)
				{
					future->SetState(::Assets::AssetState::Invalid);
				}
                CATCH_END

            } else {
                OSServices::XlWaitForMultipleSyncObjects(
                    2, this->_events,
                    false, OSServices::XL_INFINITE, true);
            }
        }
    }

    CompilationThread::CompilationThread()
    {
        _events[0] = OSServices::XlCreateEvent(false);
        _events[1] = OSServices::XlCreateEvent(true);
        _workerQuit = false;

        _thread = std::thread(std::bind(&CompilationThread::ThreadFunction, this));
    }

    CompilationThread::~CompilationThread()
    {
        StallOnPendingOperations(true);
        OSServices::XlCloseSyncObject(_events[0]);
        OSServices::XlCloseSyncObject(_events[1]);
    }

	void QueueCompileOperation(
		const std::shared_ptr<::Assets::ArtifactFuture>& future,
		std::function<void(::Assets::ArtifactFuture&)>&& operation)
	{
        if (!ConsoleRig::GlobalServices::GetInstance().GetLongTaskThreadPool().IsGood()) {
            operation(*future);
            return;
        }

		auto fn = std::move(operation);
		ConsoleRig::GlobalServices::GetInstance().GetLongTaskThreadPool().EnqueueBasic(
			[future, fn]() {
				TRY
				{
					fn(*future);
				}
				CATCH(const ::Assets::Exceptions::ConstructionError& e)
				{
					auto artifact = std::make_shared<::Assets::CompilerExceptionArtifact>(e.GetActualizationLog(), e.GetDependencyValidation());
					future->AddArtifact("exception", artifact);
					future->SetState(::Assets::AssetState::Invalid);
				}
				CATCH(const std::exception& e)
				{
					auto artifact = std::make_shared<::Assets::CompilerExceptionArtifact>(::Assets::AsBlob(e), nullptr);
					future->AddArtifact("exception", artifact);
					future->SetState(::Assets::AssetState::Invalid);
				}
				CATCH(...)
				{
					future->SetState(::Assets::AssetState::Invalid);
				}
				CATCH_END
				assert(future->GetAssetState() != ::Assets::AssetState::Pending);	// if it is still marked "pending" at this stage, it will never change state
		});
	}

}

