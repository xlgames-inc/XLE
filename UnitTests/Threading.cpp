// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "../Assets/AsyncLoadOperation.h"
#include "../Utility/Threading/CompletionThreadPool.h"
#include <CppUnitTest.h>

namespace UnitTests
{
    class AsyncLoadTest : public ::Assets::AsyncLoadOperation
    {
    public:
    protected:
        virtual ::Assets::AssetState Complete(const void* buffer, size_t bufferSize)
        {
            (void)buffer; (void)bufferSize;
            return ::Assets::AssetState::Ready;
        }
    };

    TEST_CLASS(Threading)
	{
	public:
		TEST_METHOD(CompletionThreadPoolTest)
		{
            UnitTest_SetWorkingDirectory();

            {
                CompletionThreadPool pool(4);
            }

            {
                CompletionThreadPool pool(4);
                volatile unsigned temp = 0;
                for (unsigned c=0; c<128; ++c)
                    pool.Enqueue([&temp]() {++temp;});
                while (temp < 100) {}
            }

            {
                CompletionThreadPool pool(4);
                std::vector<std::shared_ptr<AsyncLoadTest>> tests;
                for (unsigned c=0; c<128; ++c) {
                    auto t = std::make_shared<AsyncLoadTest>();
                    t->Enqueue("log.cfg", pool);
                    tests.push_back(t);
                }
            }
        }
    };
}

