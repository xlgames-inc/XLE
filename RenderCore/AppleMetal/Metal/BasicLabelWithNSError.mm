//
//  BasicLabelWithNSError.mm
//  RenderCore_AppleMetal
//
//  Created by Andrew Barnert on 12/5/19.
//

#import "BasicLabelWithNSError.h"
#import "../../../../Externals/Misc/OCPtr.h"

namespace RenderCore { namespace Metal_AppleMetal
{
using ::Exceptions::BasicLabel;

struct BasicLabelWithNSError::_Pimpl {
public:
    _Pimpl(NSError *error) : _error(error) {}
    TBC::OCPtr<NSError> _error;
};

BasicLabelWithNSError::BasicLabelWithNSError(NSError *error, const char format[], ...) never_throws : BasicLabel(), _pimpl(std::make_shared<_Pimpl>(error)) {
    va_list args;
    va_start(args, format);
    std::vsnprintf(_buffer, dimof(_buffer), format, args);
    va_end(args);
}

BasicLabelWithNSError::BasicLabelWithNSError(NSError *error, const char format[], va_list args) never_throws : BasicLabel(format, args), _pimpl(std::make_shared<_Pimpl>(error)) {
}

BasicLabelWithNSError::BasicLabelWithNSError(const BasicLabelWithNSError& copyFrom) never_throws : BasicLabel(copyFrom), _pimpl(copyFrom._pimpl) {
}

BasicLabelWithNSError& BasicLabelWithNSError::operator=(const BasicLabelWithNSError& copyFrom) never_throws {
    BasicLabel::operator=(copyFrom);
    _pimpl = copyFrom._pimpl;
    return *this;
}

BasicLabelWithNSError::BasicLabelWithNSError() never_throws : BasicLabel() {
}

BasicLabelWithNSError::~BasicLabelWithNSError() {
}

NSError *BasicLabelWithNSError::nsError() const never_throws {
    return _pimpl->_error.get();
}

}}

