// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Metal/MetalTestHelper.h"
#include "../../../BufferUploads/IBufferUploads.h"
#include "../../../BufferUploads/DataPacket.h"
#include "../../../BufferUploads/MemoryManagement.h"
#include "../../../RenderCore/ResourceDesc.h"
#include "../../../RenderCore/Format.h"
#include "../../../RenderCore/IDevice.h"
#include "../../../Utility/HeapUtils.h"
#include "thousandeyes/futures/then.h"
#include "thousandeyes/futures/DefaultExecutor.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"
#include <chrono>
#include <future>

using namespace Catch::literals;
using namespace std::chrono_literals;
namespace UnitTests
{
    class AsyncDataSource : public BufferUploads::IAsyncDataSource
    {
    public:
        std::future<RenderCore::ResourceDesc> GetDesc () override
        {
            return std::async(
                std::launch::async,
                [capturedDesc{_desc}]() {
                    std::this_thread::sleep_for(500ms);
                    return capturedDesc;
                });
        }

        std::future<void> PrepareData(IteratorRange<const SubResource*> subResources) override
        {
            std::vector<SubResource> subResourceDst_;
            subResourceDst_.insert(subResourceDst_.end(), subResources.begin(), subResources.end());
            return std::async(
                std::launch::async,
                [capturedDesc{_desc}, capturedData{_rawData}, subResourceDst{std::move(subResourceDst_)}]() {
                    std::this_thread::sleep_for(1500ms);

                    assert(subResourceDst[0]._destination.size() == capturedData.size() * sizeof(unsigned));
                    std::copy(
                        capturedData.begin(), capturedData.end(),
                        (unsigned*)subResourceDst[0]._destination.begin());
                });
        }

        std::vector<unsigned> _rawData;
        RenderCore::ResourceDesc _desc;
    };

    TEST_CASE( "BufferUploads-TextureInitialization", "[rendercore_techniques]" )
	{
        using namespace RenderCore;
        auto metalHelper = MakeTestHelper();
        auto bu = BufferUploads::CreateManager(*metalHelper->_device);

        std::vector<unsigned> rawData;
        rawData.resize(256*256, 0xff7fff7f);
        auto desc = CreateDesc(
            BindFlag::ShaderResource, 0, GPUAccess::Read, TextureDesc::Plain2D(256, 256, Format::R8G8B8A8_UNORM),
            "bu-test-texture");

        auto executor = std::make_shared<thousandeyes::futures::DefaultExecutor>(std::chrono::milliseconds(2));
        thousandeyes::futures::Default<thousandeyes::futures::Executor>::Setter execSetter(executor);
                
        SECTION("Prepared data packet")
        {
            
            auto dataPacket = BufferUploads::CreateBasicPacket(MakeIteratorRange(rawData), MakeTexturePitches(desc._textureDesc));
            auto transaction = bu->Transaction_Begin(desc, dataPacket);
            REQUIRE(transaction._transactionID != 0);
            REQUIRE(transaction._future.valid());

            auto start = std::chrono::steady_clock::now();
            for (;;) {
                bu->Update(*metalHelper->_device->GetImmediateContext());
                auto status = transaction._future.wait_for(100ms);
                if (status == std::future_status::ready)
                    break;

                if ((std::chrono::steady_clock::now() - start) > 5s)
                    FAIL("Too much time has passed waiting for buffer uploads transaction to complete");
            }

            bu->Update(*metalHelper->_device->GetImmediateContext());

            REQUIRE(bu->IsCompleted(transaction._transactionID));
            auto finalResource = transaction._future.get().AsIndependentResource();
            REQUIRE(finalResource != nullptr);
            auto finalResourceDesc = finalResource->GetDesc();
            REQUIRE(finalResourceDesc._type == ResourceDesc::Type::Texture);
            REQUIRE(finalResourceDesc._textureDesc._width == desc._textureDesc._width);
            REQUIRE(finalResourceDesc._textureDesc._height == desc._textureDesc._height);
        }

        SECTION("Async construction")
        {
            auto dataSource = std::make_shared<AsyncDataSource>();
            dataSource->_desc = desc;
            dataSource->_rawData = rawData;

            auto transaction = bu->Transaction_Begin(dataSource);
            REQUIRE(transaction._transactionID != 0);
            REQUIRE(transaction._future.valid());

            auto start = std::chrono::steady_clock::now();
            for (;;) {
                bu->Update(*metalHelper->_device->GetImmediateContext());
                auto status = transaction._future.wait_for(100ms);
                if (status == std::future_status::ready)
                    break;

                if ((std::chrono::steady_clock::now() - start) > 5s)
                    FAIL("Too much time has passed waiting for buffer uploads transaction to complete");
            }

            bu->Update(*metalHelper->_device->GetImmediateContext());

            REQUIRE(bu->IsCompleted(transaction._transactionID));
            auto finalResource = transaction._future.get().AsIndependentResource();
            REQUIRE(finalResource != nullptr);
            auto finalResourceDesc = finalResource->GetDesc();
            REQUIRE(finalResourceDesc._type == ResourceDesc::Type::Texture);
            REQUIRE(finalResourceDesc._textureDesc._width == desc._textureDesc._width);
            REQUIRE(finalResourceDesc._textureDesc._height == desc._textureDesc._height);
        }
    }


    TEST_CASE( "BufferUploads-Heap", "[rendercore_techniques]" )
    {
        struct HeapTest_Allocation 
        {
            unsigned _start, _size;
            unsigned _begin, _end;
            signed _refCount;
        };

        SECTION("SimpleSpanningHeap")
        {
                //      Verify the heap functionality by running through many test allocations/deallocations
            SimpleSpanningHeap heap(0x8000);
            std::vector<std::pair<unsigned,unsigned>> allocations;
            for (unsigned c=0; c<10000; ++c) {
                if ((rand()%3)==0) {
                    if (allocations.size()) {
                        unsigned originalSpace = heap.CalculateAvailableSpace();
                        std::vector<std::pair<unsigned,unsigned>>::iterator i = allocations.begin() + (rand()%allocations.size());
                        bool result = heap.Deallocate(i->first, i->second);
                        REQUIRE(result);
                        if (result) {
                            unsigned newSize = heap.CalculateAvailableSpace();
                            REQUIRE(abs(int(newSize - (originalSpace+i->second)))<=16);
                        }
                        allocations.erase(i);
                    }
                } else {
                    unsigned originalSpace = heap.CalculateAvailableSpace();
                    unsigned size = rand()%0x800 + 0x1;
                    unsigned allocation = heap.Allocate(size);
                    if (allocation != ~unsigned(0x0)) {
                        allocations.push_back(std::make_pair(allocation, size));
                        unsigned newSize = heap.CalculateAvailableSpace();
                        REQUIRE(abs(int(newSize - (originalSpace-size)))<=16);
                    }
                }

                SimpleSpanningHeap dupe = heap;
                dupe.PerformDefrag(dupe.CalculateDefragSteps());
            }

            for (std::vector<std::pair<unsigned,unsigned>>::iterator i=allocations.begin(); i!=allocations.end(); ++i) {
                heap.Deallocate(i->first, i->second);
            }

                //      ...Defrag test...
            for (unsigned c=0; c<100; ++c) {
                unsigned originalSpace = heap.CalculateAvailableSpace();
                unsigned size = rand()%0x800 + 0x1;
                unsigned allocation = heap.Allocate(size);
                if (allocation != ~unsigned(0x0)) {
                    allocations.push_back(std::make_pair(allocation, size));
                    unsigned newSize = heap.CalculateAvailableSpace();
                    REQUIRE(abs(int(newSize - (originalSpace-size)))<=16);
                }
            }
            heap.PerformDefrag(heap.CalculateDefragSteps());
        }

        SECTION("ReferenceCountingLayer")
        {
            using namespace BufferUploads;
            ReferenceCountingLayer layer(0x8000);

            layer.AddRef(0x1c0, 0x10);
            layer.AddRef(0x1d0, 0x10);
            layer.AddRef(0x100, 0x100);
            layer.Release(0x100, 0x100);
            layer.Release(0x1c0, 0x10);
            layer.Release(0x1d0, 0x10);

            std::vector<HeapTest_Allocation> allocations;
            HeapTest_Allocation starterAllocation;
            starterAllocation._start = 0;
            starterAllocation._size = 0x8000;
            starterAllocation._refCount = 1;
            starterAllocation._begin = ReferenceCountingLayer::ToInternalSize(starterAllocation._start);
            starterAllocation._end = ReferenceCountingLayer::ToInternalSize(starterAllocation._start) + ReferenceCountingLayer::ToInternalSize(ReferenceCountingLayer::AlignSize(starterAllocation._size));
            layer.AddRef(starterAllocation._start, starterAllocation._size);
            allocations.push_back(starterAllocation);
            for (unsigned c=0; c<100000; ++c) {
                size_t valueStart = layer.Validate();
                if ((rand()%3)==0) {
                    HeapTest_Allocation& a = allocations[rand()%allocations.size()];
                    if (a._refCount) {
                        --a._refCount;
                        layer.Release(a._start, a._size);
                        size_t valueEnd = layer.Validate();
                        REQUIRE(valueStart == valueEnd+(a._end-a._begin));
                    }
                } else {
                    HeapTest_Allocation& a = allocations[rand()%allocations.size()];
                    if (a._size > 128) {
                        // ReferenceCountingLayer originalLayer = layer;
                        HeapTest_Allocation all;
                        all._start = rand()%(a._size-32);
                        all._size = std::min(a._size-all._start, 32+rand()%(a._size-64));
                        all._refCount = 1;
                        all._begin = ReferenceCountingLayer::ToInternalSize(all._start);
                        all._end = ReferenceCountingLayer::ToInternalSize(all._start) + ReferenceCountingLayer::ToInternalSize(ReferenceCountingLayer::AlignSize(all._size));
                        allocations.push_back(all);
                        layer.AddRef(all._start, all._size);
                        size_t valueEnd = layer.Validate();
                        REQUIRE(valueEnd == valueStart+(all._end-all._begin));
                        // if (valueEnd != valueStart+(all._end-all._begin)) {
                        //     originalLayer.AddRef(all._start, all._size);
                        // }
                    }
                }
            }

            for (std::vector<HeapTest_Allocation>::iterator i=allocations.begin(); i!=allocations.end(); ++i) {
                while (i->_refCount--) {
                    layer.Release(i->_start, i->_size);
                }
            }
        }
    }


}

