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

#ifndef XENOLITH_GL_VK_XLVKPIPELINE_H_
#define XENOLITH_GL_VK_XLVKPIPELINE_H_

#include "XLVk.h"
#include "XLVkDevice.h"

namespace stappler::xenolith::vk {

class Shader : public gl::Shader {
public:
	bool init(Device &dev, const gl::ProgramData &);

	VkShaderModule getModule() const { return _shaderModule; }

protected:
	bool setup(Device &dev, const gl::ProgramData &, SpanView<uint32_t>);

	VkShaderModule _shaderModule = VK_NULL_HANDLE;
};

class Pipeline : public gl::Pipeline {
public:
	static bool comparePipelineOrdering(const gl::PipelineInfo &l, const gl::PipelineInfo &r);

	bool init(Device &dev, const gl::PipelineData &params, const gl::RenderSubpassData &, const gl::RenderQueue &);

	VkPipeline getPipeline() const { return _pipeline; }

protected:
	VkPipeline _pipeline = VK_NULL_HANDLE;
};

}

#endif /* XENOLITH_GL_VK_XLVKPIPELINE_H_ */
