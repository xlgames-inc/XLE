// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CommonResources.h"
#include "ResourceBox.h"
#include "TechniqueUtils.h" // just for sizeof(LocalTransformConstants)

namespace RenderCore { namespace Techniques
{
    namespace Internal
    {
        std::vector<std::unique_ptr<IBoxTable>> BoxTables;
        IBoxTable::~IBoxTable() {}
    }

    CommonResourceBox::CommonResourceBox(const Desc&)
    {
        using namespace RenderCore::Metal;
        _dssReadWrite = DepthStencilState();
        _dssReadOnly = DepthStencilState(true, false);
        _dssDisable = DepthStencilState(false, false);
        _dssReadWriteWriteStencil = DepthStencilState(true, true, 0xff, 0xff, StencilMode::AlwaysWrite);

        _blendStraightAlpha = BlendState(BlendOp::Add, Blend::SrcAlpha, Blend::InvSrcAlpha);
        _blendAlphaPremultiplied = BlendState(BlendOp::Add, Blend::One, Blend::InvSrcAlpha);
        _blendOneSrcAlpha = BlendState(BlendOp::Add, Blend::One, Blend::SrcAlpha);
        _blendOpaque = BlendOp::NoBlending;

        _defaultRasterizer = CullMode::Back;
        _cullDisable = CullMode::None;
        _cullReverse = RasterizerState(CullMode::Back, false);

        _localTransformBuffer = ConstantBuffer(nullptr, sizeof(LocalTransformConstants));
    }

    CommonResourceBox& CommonResources()
    {
        return FindCachedBox<CommonResourceBox>(CommonResourceBox::Desc());
    }


    void ResourceBoxes_Shutdown()
    {
        Internal::BoxTables = std::vector<std::unique_ptr<Internal::IBoxTable>>();
    }

}}
