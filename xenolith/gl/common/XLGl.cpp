/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>

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

#include "XLDefine.h"

#define XL_FRAME_EMITTER_DEBUG 1
//#define XL_FRAME_DEBUG 1

#ifdef XL_LOOP_DEBUG
#define XL_LOOP_LOG(...) log::vtext("gl::Loop", __VA_ARGS__)
#else
#define XL_LOOP_LOG(...)
#endif

#ifdef XL_FRAME_DEBUG
#define XL_FRAME_LOG(...) log::vtext("gl::Frame", __VA_ARGS__)
#else
#define XL_FRAME_LOG(...)
#endif

#ifdef XL_FRAME_EMITTER_DEBUG
#define XL_FRAME_EMITTER_LOG(...) log::vtext("gl::FrameEmitter", __VA_ARGS__)
#else
#define XL_FRAME_EMITTER_LOG(...)
#endif

#define XL_FRAME_PROFILE(fn, tag, max) \
	do { XL_PROFILE_BEGIN(frame, "gl::FrameHandle", tag, max); fn; XL_PROFILE_END(frame); } while (0);

#include "XLGlDevice.cc"
#include "XLGlCommandList.cc"
#include "XLGlLoop.cc"
#include "XLGlView.cc"
#include "XLGlObject.cc"
#include "XLGlResource.cc"
#include "XLGlAttachment.cc"
#include "XLGlRenderQueue.cc"
#include "XLGlMaterial.cc"
#include "XLGlInstance.cc"
#include "XLGlFrameCache.cc"
#include "XLGlFrameEmitter.cc"
#include "XLGlFrameHandle.cc"
#include "XLGlFrameQueue.cc"
#include "XLGlRenderPass.cc"
#include "XLGlUtils.cc"
#include "XLGlSwapchain.cc"
#include "XLGlDynamicImage.cc"
