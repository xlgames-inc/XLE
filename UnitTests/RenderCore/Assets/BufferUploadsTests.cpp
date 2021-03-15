// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Metal/MetalTestHelper.h"
#include "../../../BufferUploads/IBufferUploads.h"
#include "../../../BufferUploads/DataPacket.h"
#include "../../../BufferUploads/ResourceLocator.h"
#include "../../../RenderCore/ResourceDesc.h"
#include "../../../RenderCore/Format.h"
#include "../../../RenderCore/IDevice.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"
#include <chrono>

using namespace Catch::literals;
using namespace std::chrono_literals;
namespace UnitTests
{

    TEST_CASE( "BufferUploads-TextureInitialization", "[rendercore_techniques]" )
	{
        using namespace RenderCore;
        auto metalHelper = MakeTestHelper();
        auto bu = BufferUploads::CreateManager(*metalHelper->_device);

        SECTION("Prepared data packet")
        {
            std::vector<unsigned> rawData;
            rawData.resize(256*256, 0xff7fff7f);
            auto desc = CreateDesc(
                BindFlag::ShaderResource, 0, GPUAccess::Read, TextureDesc::Plain2D(256, 256, Format::R8G8B8A8_UNORM),
                "bu-test-texture");
            auto dataPacket = BufferUploads::CreateBasicPacket(MakeIteratorRange(rawData), MakeTexturePitches(desc._textureDesc));
            auto transaction = bu->Transaction_Begin(desc, dataPacket);
            REQUIRE(transaction._transactionID != 0);
            REQUIRE(transaction._future.valid());

            auto start = std::chrono::steady_clock::now();
            for (;;) {
                bu->Update(*metalHelper->_device->GetImmediateContext(), false);
                auto status = transaction._future.wait_for(100ms);
                if (status == std::future_status::ready)
                    break;

                if ((std::chrono::steady_clock::now() - start) > 5s)
                    FAIL("Too much time has passed waiting for buffer uploads transaction to complete");
            }

            REQUIRE(bu->IsCompleted(transaction._transactionID));
            auto locator = transaction._future.get();
            REQUIRE(locator._resource != nullptr);
            auto finalResourceDesc = locator._resource->GetDesc();
            REQUIRE(finalResourceDesc._type == ResourceDesc::Type::Texture);
            REQUIRE(finalResourceDesc._textureDesc._width == desc._textureDesc._width);
            REQUIRE(finalResourceDesc._textureDesc._height == desc._textureDesc._height);
        }
    }
}

