// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4512)

#include "UITypesBinding.h"
#include <msclr\auto_gcroot.h>

namespace GUILayer
{

///////////////////////////////////////////////////////////////////////////////////////////////////

    class InvalidatePropertyGrid : public OnChangeCallback
    {
    public:
        void    OnChange();

        InvalidatePropertyGrid(PropertyGrid^ linked);
        ~InvalidatePropertyGrid();
    protected:
        msclr::auto_gcroot<PropertyGrid^> _linked;
    };

    void    InvalidatePropertyGrid::OnChange()
    {
        if (_linked.get()) {
            _linked->Invalidate();
        }
    }

    InvalidatePropertyGrid::InvalidatePropertyGrid(PropertyGrid^ linked) : _linked(linked) {}
    InvalidatePropertyGrid::~InvalidatePropertyGrid() {}

    void ModelVisSettings::AttachCallback(PropertyGrid^ callback)
    {
        (*_object)->_changeEvent._callbacks.push_back(
            std::shared_ptr<OnChangeCallback>(new InvalidatePropertyGrid(callback)));
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

}

