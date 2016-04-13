// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Noise.h"
#include "SceneEngineUtils.h"
#include "../RenderCore/Format.h"
#include "../BufferUploads/DataPacket.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../Math/Vector.h"

namespace SceneEngine
{
    using namespace RenderCore;
    using namespace RenderCore::Metal;

    PerlinNoiseResources::PerlinNoiseResources(const Desc& desc)
    {
        const Float4 g[] = {
            Float4(1,1,0,0),    Float4(-1,1,0,0),    Float4(1,-1,0,0),    Float4(-1,-1,0,0),
            Float4(1,0,1,0),    Float4(-1,0,1,0),    Float4(1,0,-1,0),    Float4(-1,0,-1,0),
            Float4(0,1,1,0),    Float4(0,-1,1,0),    Float4(0,1,-1,0),    Float4(0,-1,-1,0),
            Float4(1,1,0,0),    Float4(0,-1,1,0),    Float4(-1,1,0,0),    Float4(0,-1,-1,0),
        };

        const uint8 perm[256]= {
            151,160,137,91,90,15,
            131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
            190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
            88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
            77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
            102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
            135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
            5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
            223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
            129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
            251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
            49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
            138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
        };

        auto& uploads = GetBufferUploads();
        auto gradDesc = BuildRenderTargetDesc(BufferUploads::BindFlag::ShaderResource, BufferUploads::TextureDesc::Plain1D(dimof(g), Format::R32G32B32_TYPELESS), "NoiseGrad");
        auto permDesc = BuildRenderTargetDesc(BufferUploads::BindFlag::ShaderResource, BufferUploads::TextureDesc::Plain1D(dimof(perm), Format::R8_TYPELESS), "NoisePerm");
        auto gradPkt = BufferUploads::CreateEmptyPacket(gradDesc);
        XlCopyMemory(gradPkt->GetData(), g, std::min(sizeof(g), gradPkt->GetDataSize()));
        auto permPkt = BufferUploads::CreateEmptyPacket(permDesc);
        XlCopyMemory(permPkt->GetData(), perm, std::min(sizeof(perm), permPkt->GetDataSize()));
        auto gradTexture = uploads.Transaction_Immediate(gradDesc, gradPkt.get());
        auto permTexture = uploads.Transaction_Immediate(permDesc, permPkt.get());

        /*{
            auto device  = MainBridge::GetInstance()->GetDevice();
            auto context = RenderCore::Metal::DeviceContext::GetImmediateContext(device);
            D3DX11SaveTextureToFile(context->GetUnderlying(), gradTexture.get(), D3DX11_IFF_DDS, "perlin_grad.dds");
            D3DX11SaveTextureToFile(context->GetUnderlying(), permTexture.get(), D3DX11_IFF_DDS, "perlin_perm.dds");
        }*/

        _gradShaderResource = ShaderResourceView(gradTexture->ShareUnderlying(), Format::R32G32B32_FLOAT);
        _permShaderResource = ShaderResourceView(permTexture->ShareUnderlying(), Format::R8_UNORM);
    }

    PerlinNoiseResources::~PerlinNoiseResources()
    {}
}


