//
//  BasicLabelWithNSError.h
//  RenderCore_AppleMetal
//
//  Created by Andrew Barnert on 12/5/19.
//

#pragma once

#import <Foundation/Foundation.h>
#import "../../Core/Exceptions.h"
#include <memory>

namespace RenderCore { namespace Metal_AppleMetal
{
using ::Exceptions::BasicLabel;

class BasicLabelWithNSError : public BasicLabel
{
public:
    BasicLabelWithNSError(NSError *error, const char format[], ...) never_throws;
    BasicLabelWithNSError(NSError *error, const char format[], va_list args) never_throws;
    BasicLabelWithNSError(const BasicLabelWithNSError& copyFrom) never_throws;
    BasicLabelWithNSError& operator=(const BasicLabelWithNSError& copyFrom) never_throws;
    virtual ~BasicLabelWithNSError();
    NSError *nsError() const never_throws;
protected:
    BasicLabelWithNSError() never_throws;
    struct _Pimpl;
    std::shared_ptr<_Pimpl> _pimpl;
};

}}
