// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(BINDING_H)
#define BINDING_H

#define SAMPLER_GLOBAL(index) register(s[index])

#define TEXTURE_BOUND(index) register(t[index])
#define TEXTURE_GLOBAL(index) register(t[12+index])
#define TEXTURE_DYNAMIC(index) register(t[8+index])

#define CB_BOUND(index) register(b[index])
#define CB_GLOBAL(index) register(b[8+index])
#define CB_DYNAMIC(index) register(b[4+index])

#endif
