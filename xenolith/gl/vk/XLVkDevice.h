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

#ifndef XENOLITH_GL_VK_XLVKDEVICE_H_
#define XENOLITH_GL_VK_XLVKDEVICE_H_

#include "XLVkInstance.h"
#include "XLVkDeviceQueue.h"

namespace stappler::xenolith::vk {

class Swapchain;
class Fence;
class Allocator;

class Device : public gl::Device {
public:
	using Features = DeviceInfo::Features;
	using Properties = DeviceInfo::Properties;

	static constexpr uint32_t FramesInFlight = 2;

	Device();
	virtual ~Device();

	bool init(const Rc<Instance> &instance, VkSurfaceKHR, DeviceInfo &&, const Features &);

	Instance *getInstance() const { return _vkInstance; }
	VkDevice getDevice() const { return _device; }
	VkSurfaceKHR getSurface() const { return _surface; }
	VkPhysicalDevice getPhysicalDevice() const;
	const Rc<Swapchain> & getSwapchain() const;

	bool recreateSwapChain(const Rc<gl::Loop> &, thread::TaskQueue &, bool resize);
	bool createSwapChain(const Rc<gl::Loop> &, thread::TaskQueue &);
	void cleanupSwapChain();

	virtual Rc<gl::Shader> makeShader(const gl::ProgramData &) override;
	virtual Rc<gl::Pipeline> makePipeline(const gl::RenderQueue &, const gl::RenderPassData &, const gl::PipelineData &) override;
	virtual Rc<gl::RenderPassImpl> makeRenderPass(gl::RenderPassData &) override;

	virtual void begin(Application *, thread::TaskQueue &) override;
	virtual void end(thread::TaskQueue &) override;
	virtual void reset(thread::TaskQueue &) override;
	virtual void waitIdle() override;

	virtual void incrementGeneration() override;
	virtual void invalidateFrame(gl::FrameHandle &) override;

	virtual bool isBestPresentMode() const override;

	const DeviceInfo & getInfo() const { return _info; }
	const DeviceCallTable * getTable() const { return _table; }
	const Rc<Allocator> &getAllocator() const { return _allocator; }

	virtual const Rc<gl::RenderQueue> getDefaultRenderQueue() const;

	// acquire VkQueue handle
	// - QueueOperations - one of QueueOperations flags, defining capabilities of required queue
	// - gl::FrameHandle - frame, in which queue will be used
	// - acquire - function, to call with result, can be called immediately
	// 		or when queue for specified operations become available (on GL thread)
	// - invalidate - function to call when queue query is invalidated (e.g. when frame is invalidated)
	// - ref - ref to preserve until query is completed
	// returns
	// - true is query was completed or scheduled,
	// - false if frame is not valid or no queue family with requested capabilities exists
	//
	// Acquired DeviceQueue must be released with releaseQueue
	bool acquireQueue(QueueOperations, gl::FrameHandle &, Function<void(gl::FrameHandle &, const Rc<DeviceQueue> &)> && acquire,
			Function<void(gl::FrameHandle &)> && invalidate, Rc<Ref> &&);
	void releaseQueue(Rc<DeviceQueue> &&);

	Rc<CommandPool> acquireCommandPool(QueueOperations, uint32_t = 0);
	void releaseCommandPool(Rc<CommandPool> &&);

	Rc<Fence> acquireFence(uint32_t);
	void releaseFence(Rc<Fence> &&);
	void scheduleFence(gl::Loop &, Rc<Fence> &&);

	Rc<SwapchainSync> acquireSwapchainSync(uint32_t);
	void releaseSwapchainSync(Rc<SwapchainSync> &&);

private:
	friend class DeviceQueue;

	virtual Rc<gl::FrameHandle> makeFrame(gl::Loop &, gl::RenderQueue &, bool readyForSubmit) override;

	bool setup(const Rc<Instance> &instance, VkPhysicalDevice p, const Properties &prop,
			const Vector<DeviceQueueFamily> &queueFamilies, Features &features, const Vector<const char *> &requiredExtension);

	void pushQueue(gl::Loop &, VkQueue, uint32_t);

	Rc<vk::Instance> _vkInstance;
	const DeviceCallTable *_table = nullptr;
	VkDevice _device = VK_NULL_HANDLE;

	uint32_t _currentFrame = 0;
	DeviceInfo _info;
	Features _enabledFeatures;

	VkSurfaceKHR _surface = VK_NULL_HANDLE;
	Rc<Swapchain> _swapchain;
	Rc<Allocator> _allocator;

	Vector<DeviceQueueFamily> _families;

	uint64_t _presentOrder = 0;
	bool _finished = false;

	Vector<Rc<Fence>> _fences;
	Set<Rc<Fence>> _scheduled;
	Vector<Vector<Rc<SwapchainSync>>> _sems;
};

}

#endif /* XENOLITH_GL_VK_XLVKDEVICE_H_ */
