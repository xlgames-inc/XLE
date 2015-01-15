// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "StdAfx.h"
#include "ShaderDiagramDocument.h"

namespace ShaderDiagram
{

    Document::Document()
    {
        _previewMaterialState = gcnew Dictionary<String^, Object^>();
        _negativeLightDirection = new Float3(0.f, 0.f, 1.f);
    }

    Document::~Document()
    {
        delete _previewMaterialState;
        delete _negativeLightDirection;
    }

}