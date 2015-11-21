// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Transform.h"
#include "../../RenderCore/Metal/Format.h"
#include "../../BufferUploads/IBufferUploads.h"
#include "../../BufferUploads/DataPacket.h"
#include "../../Math/Geometry.h"
#include "../../Utility/ParameterBox.h"

extern "C"
{
    #include "../../Foreign/HosekWilkie/ArHosekSkyModel.h"
}

namespace TextureTransform
{
    static const Float3 s_verticalPanels[6][3] =
    {
	    { Float3(1,0,0), Float3(0,0,1), Float3(0,1,0) },
        { Float3(1,0,0), Float3(0,-1,0), Float3(0,0,1) },
	    { Float3(1,0,0), Float3(0,0,-1), Float3(0,-1,0) },
        { Float3(1,0,0), Float3(0,1,0), Float3(0,0,-1) },

	    { Float3(0,0,1), Float3(0,-1,0), Float3(-1,0,0) },
	    { Float3(0,0,-1), Float3(0,-1,0), Float3(1,0,0) }
    };

    static const Float2 s_verticalPanelCoords[6][2] =
    {
        { Float2(1.0f/3.0f, 0.0f),      Float2(2.0f/3.0f, 1.0f/4.0f) },
        { Float2(1.0f/3.0f, 1.0f/4.0f), Float2(2.0f/3.0f, 2.0f/4.0f) },
        { Float2(1.0f/3.0f, 2.0f/4.0f), Float2(2.0f/3.0f, 3.0f/4.0f) },
        { Float2(1.0f/3.0f, 3.0f/4.0f), Float2(2.0f/3.0f, 1.0f) },
        { Float2(0.0f, 1.0f/4.0f),      Float2(1.0f/3.0f, 2.0f/4.0f) },
        { Float2(2.0f/3.0f, 1.0f/4.0f), Float2(1.0f, 2.0f/4.0f) }
    };

    TextureResult HosekWilkieSky(const BufferUploads::TextureDesc& desc, const ParameterBox& parameters)
    {
            // The "turbidity" parameter is Linke’s turbidity factor. Hosek and Wilkie give these example parameters:
            //      T = 2 yields a very clear, Arctic-like sky
            //      T = 3 a clear sky in a temperate climate
            //      T = 6 a sky on a warm, moist day
            //      T = 10 a slightly hazy day
            //      T > 50 represent dense fog

        auto defaultSunDirection = Normalize(Float3(1.f, 1.f, 0.33f));
        Float3 sunDirection = parameters.GetParameter<Float3>(ParameterBox::ParameterNameHash("SunDirection"), defaultSunDirection);
        sunDirection = Normalize(sunDirection);

        auto turbidity = (double)parameters.GetParameter(ParameterBox::ParameterNameHash("turbidity"), 3.f);
        auto albedo = (double)parameters.GetParameter(ParameterBox::ParameterNameHash("albedo"), 0.1f);
        auto elevation = (double)Deg2Rad(parameters.GetParameter(ParameterBox::ParameterNameHash("elevation"), XlASin(sunDirection[2])));
        auto* state = arhosek_rgb_skymodelstate_alloc_init(turbidity, albedo, elevation);

        auto pixels = std::make_unique<Float4[]>(desc._width*desc._height);
        for (unsigned y=0; y<desc._height; ++y)
            for (unsigned x=0; x<desc._width; ++x) {
                auto p = y*desc._width+x;
                pixels[p] = Float4(0.f, 0.f, 0.f, 1.f);

                Float3 direction(0.f, 0.f, 0.f);
                bool hitPanel = false;

                for (unsigned c = 0; c < 6; ++c) {
                    Float2 tc(x / float(desc._width), y / float(desc._height));
                    auto tcMins = s_verticalPanelCoords[c][0];
                    auto tcMaxs = s_verticalPanelCoords[c][1];
                    if (tc[0] >= tcMins[0] && tc[1] >= tcMins[1] && tc[0] <  tcMaxs[0] && tc[1] <  tcMaxs[1]) {
                        tc[0] = 2.0f * (tc[0] - tcMins[0]) / (tcMaxs[0] - tcMins[0]) - 1.0f;
                        tc[1] = 2.0f * (tc[1] - tcMins[1]) / (tcMaxs[1] - tcMins[1]) - 1.0f;

                        hitPanel = true;
                        auto plusX = s_verticalPanels[c][0];
                        auto plusY = s_verticalPanels[c][1];
                        auto center = s_verticalPanels[c][2];
                        direction = center + plusX * tc[0] + plusY * tc[1];
                    }
                }

                if (hitPanel) {
                    auto theta = CartesianToSpherical(direction)[0];
                    theta = std::min(.4998f * gPI, theta);
                    auto gamma = XlACos(std::max(0.f, Dot(Normalize(direction), sunDirection)));

                    auto R = arhosek_tristim_skymodel_radiance(state, theta, gamma, 0);
                    auto G = arhosek_tristim_skymodel_radiance(state, theta, gamma, 1);
                    auto B = arhosek_tristim_skymodel_radiance(state, theta, gamma, 2);

                    pixels[p][0] = (float)R;
                    pixels[p][1] = (float)G;
                    pixels[p][2] = (float)B;
                }
            }

        arhosekskymodelstate_free(state);

        return TextureResult
            {
                BufferUploads::CreateBasicPacket(
                    (desc._width*desc._height)*sizeof(Float4),
                    pixels.get(),
                    BufferUploads::TexturePitches(desc._width*sizeof(Float4), desc._width*desc._height*sizeof(Float4))),
                RenderCore::Metal::NativeFormat::R32G32B32A32_FLOAT,
                UInt2(desc._width, desc._height)
            };
    }
}

