/**
Copyright (c) 2020-2022 Roman Katuntsev <sbkarr@stappler.org>

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
#include "XLVk.h"

#ifdef XL_VK_DEBUG
#define XL_VK_LOG(...) log::vtext("Vk::Loop", __VA_ARGS__)
#else
#define XL_VK_LOG(...)
#endif

#include "XLVkDeviceQueue.cc"
#include "XLVkDevice.cc"
#include "XLVkAllocator.cc"
#include "XLVkBuffer.cc"
#include "XLVkFramebuffer.cc"
#include "XLVkSwapchain.cc"
#include "XLVkPipeline.cc"
#include "XLVkRenderPassImpl.cc"
#include "XLVkSync.cc"
#include "XLVkFrame.cc"
#include "XLVkObject.cc"
#include "XLVkTextureSet.cc"
#include "renderer/XLVkImageAttachment.cc"
#include "renderer/XLVkAttachment.cc"
#include "renderer/XLVkRenderPass.cc"
#include "renderer/XLVkTransferQueue.cc"
#include "renderer/XLVkMaterialCompiler.cc"
#include "renderer/XLVkMaterialRenderPass.cc"
#include "renderer/XLVkRenderQueueCompiler.cc"
#include "renderer/XLVkRenderFontQueue.cc"
