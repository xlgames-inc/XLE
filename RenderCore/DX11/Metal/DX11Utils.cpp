// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DX11Utils.h"
#include "../../../Utility/ParameterBox.h"

#include <d3d11shader.h>        // D3D11_SHADER_TYPE_DESC

namespace RenderCore { namespace Metal_DX11
{
    ImpliedTyping::TypeDesc GetType(D3D11_SHADER_TYPE_DESC typeDesc)
    {
        using namespace Utility::ImpliedTyping;
        TypeDesc result;
        switch (typeDesc.Type) {
        case D3D_SVT_BOOL:  result._type = TypeCat::Bool;   break;
        case D3D_SVT_INT:   result._type = TypeCat::Int32;  break;
        case D3D_SVT_FLOAT: result._type = TypeCat::Float;  break;
        case D3D_SVT_UINT:
        case D3D_SVT_UINT8: result._type = TypeCat::UInt32; break;

        default:
        case D3D_SVT_VOID:  result._type = TypeCat::Void;   break;
        }

        if (typeDesc.Elements > 0) {
            result._arrayCount = uint16(typeDesc.Elements);
        } else if (typeDesc.Class == D3D_SVC_VECTOR) {
            result._arrayCount = uint16(typeDesc.Columns);
        } else if (typeDesc.Class == D3D_SVC_MATRIX_ROWS || typeDesc.Class == D3D_SVC_MATRIX_COLUMNS) {
            result._arrayCount = uint16(typeDesc.Columns * typeDesc.Rows);
        }

        return result;
    }
}}

    // instantiate this ptr type
template Utility::intrusive_ptr<ID3D::Resource>;
