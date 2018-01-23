// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CompilationThread.h"
#include "../ConsoleRig/Log.h"
#include "../ConsoleRig/GlobalServices.h"
#include "../Utility/Threading/CompletionThreadPool.h"

namespace Assets 
{
    void CompilationThread::StallOnPendingOperations(bool cancelAll)
    {
        if (!_workerQuit) {
            _workerQuit = true;
            XlSetEvent(_events[1]);   // trigger a manual reset event should wake all threads (and keep them awake)
            _thread.join();
        }
    }
    
    void CompilationThread::Push(
		std::shared_ptr<::Assets::CompileFuture> future,
		std::function<void(::Assets::CompileFuture&)> operation)
    {
        if (!_workerQuit) {
			_queue.push_overflow(Element{future, std::move(operation)});
            XlSetEvent(_events[0]);
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
                XlWaitForMultipleSyncObjects(
                    2, this->_events,
                    false, XL_INFINITE, true);
            }
        }
    }

    CompilationThread::CompilationThread()
    {
        _events[0] = XlCreateEvent(false);
        _events[1] = XlCreateEvent(true);
        _workerQuit = false;

        _thread = std::thread(std::bind(&CompilationThread::ThreadFunction, this));
    }

    CompilationThread::~CompilationThread()
    {
        StallOnPendingOperations(true);
        XlCloseSyncObject(_events[0]);
        XlCloseSyncObject(_events[1]);
    }

	void QueueCompileOperation(
		const std::shared_ptr<::Assets::CompileFuture>& future,
		std::function<void(::Assets::CompileFuture&)>&& operation)
	{
		auto fn = std::move(operation);
		ConsoleRig::GlobalServices::GetLongTaskThreadPool().EnqueueBasic(
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
		});
	}

}

