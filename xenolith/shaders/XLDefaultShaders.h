/**
Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

#ifndef XENOLITH_SHADERS_XLDEFAULTSHADERS_H_
#define XENOLITH_SHADERS_XLDEFAULTSHADERS_H_

#include "XLDefine.h"

namespace stappler::xenolith::shaders {

extern SpanView<uint32_t> MaterialFrag;
extern SpanView<uint32_t> MaterialVert;
extern SpanView<uint32_t> SdfTrianglesComp;
extern SpanView<uint32_t> SdfImageComp;
extern SpanView<uint32_t> ShadowMergeFrag;
extern SpanView<uint32_t> ShadowMergeNullFrag;
extern SpanView<uint32_t> ShadowMergeVert;

}

#endif /* XENOLITH_SHADERS_XLDEFAULTSHADERS_H_ */
