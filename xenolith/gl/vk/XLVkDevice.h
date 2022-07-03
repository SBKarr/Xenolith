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

#ifndef XENOLITH_GL_VK_XLVKDEVICE_H_
#define XENOLITH_GL_VK_XLVKDEVICE_H_

#include "XLVkInstance.h"
#include "XLVkDeviceQueue.h"
#include "XLVkLoop.h"

namespace stappler::xenolith::vk {

class Fence;
class Allocator;
class TextureSetLayout;
class Sampler;
class Loop;
class DeviceMemoryPool;

class DeviceFrameHandle : public renderqueue::FrameHandle {
public:
	virtual ~DeviceFrameHandle();

	bool init(Loop &loop, Device &, Rc<FrameRequest> &&, uint64_t gen);

	const Rc<DeviceMemoryPool> &getMemPool() const;

protected:
	Rc<DeviceMemoryPool> _memPool;
};

class Device : public gl::Device {
public:
	using Features = DeviceInfo::Features;
	using Properties = DeviceInfo::Properties;
	using FrameHandle = renderqueue::FrameHandle;

	Device();
	virtual ~Device();

	bool init(const vk::Instance *instance, DeviceInfo &&, const Features &);

	const Instance *getInstance() const { return _vkInstance; }
	VkDevice getDevice() const { return _device; }
	VkPhysicalDevice getPhysicalDevice() const;

	void begin(Loop &loop, gl::TaskQueue &, Function<void(bool)> &&);
	virtual void end() override;

	const DeviceInfo & getInfo() const { return _info; }
	const DeviceTable * getTable() const;
	const Rc<Allocator> & getAllocator() const { return _allocator; }

	const DeviceQueueFamily *getQueueFamily(uint32_t) const;
	const DeviceQueueFamily *getQueueFamily(QueueOperations) const;
	const DeviceQueueFamily *getQueueFamily(gl::RenderPassType) const;

	const Vector<DeviceQueueFamily> &getQueueFamilies() const;

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
	Rc<DeviceQueue> tryAcquireQueueSync(QueueOperations);
	bool acquireQueue(QueueOperations, FrameHandle &, Function<void(FrameHandle &, const Rc<DeviceQueue> &)> && acquire,
			Function<void(FrameHandle &)> && invalidate, Rc<Ref> && = nullptr);
	bool acquireQueue(QueueOperations, Loop &, Function<void(Loop &, const Rc<DeviceQueue> &)> && acquire,
			Function<void(Loop &)> && invalidate, Rc<Ref> && = nullptr);
	void releaseQueue(Rc<DeviceQueue> &&);

	Rc<CommandPool> acquireCommandPool(QueueOperations, uint32_t = 0);
	Rc<CommandPool> acquireCommandPool(uint32_t familyIndex);
	void releaseCommandPool(gl::Loop &, Rc<CommandPool> &&);
	void releaseCommandPoolUnsafe(Rc<CommandPool> &&);

	const Rc<TextureSetLayout> &getTextureSetLayout() const { return _textureSetLayout; }

	BytesView emplaceConstant(renderqueue::PredefinedConstant, Bytes &) const;

	virtual bool supportsUpdateAfterBind(DescriptorType) const override;

	virtual Rc<gl::ImageObject> getEmptyImageObject() const override;
	virtual Rc<gl::ImageObject> getSolidImageObject() const override;

	virtual Rc<gl::Framebuffer> makeFramebuffer(const renderqueue::PassData *, SpanView<Rc<gl::ImageView>>, Extent2) override;
	virtual Rc<ImageStorage> makeImage(const gl::ImageInfo &) override;
	virtual Rc<gl::Semaphore> makeSemaphore() override;
	virtual Rc<gl::ImageView> makeImageView(const Rc<gl::ImageObject> &, const gl::ImageViewInfo &) override;

	template <typename Callback>
	void makeApiCall(const Callback &cb) {
		//_apiMutex.lock();
		cb(*getTable(), getDevice());
		//_apiMutex.unlock();
	}

	bool hasNonSolidFillMode() const;

private:
	friend class DeviceQueue;

	virtual void compileSamplers(gl::TaskQueue &q, bool force);

	bool setup(const Instance *instance, VkPhysicalDevice p, const Properties &prop,
			const Vector<DeviceQueueFamily> &queueFamilies, Features &features, const Vector<const char *> &requiredExtension);

	const vk::Instance *_vkInstance = nullptr;
	const DeviceTable *_table = nullptr;
#if VK_HOOK_DEBUG
	const DeviceTable *_original = nullptr;
#endif
	VkDevice _device = VK_NULL_HANDLE;

	DeviceInfo _info;
	Features _enabledFeatures;

	Rc<Allocator> _allocator;
	Rc<TextureSetLayout> _textureSetLayout;

	Vector<DeviceQueueFamily> _families;

	bool _finished = false;

	Vector<VkSampler> _immutableSamplers;
	Vector<Rc<Sampler>> _samplers;
	size_t _compiledSamplers = 0;
	std::atomic<bool> _samplersCompiled = false;

	std::unordered_map<VkFormat, VkFormatProperties> _formats;

	Mutex _resourceMutex;
	Mutex _apiMutex;
};

}

#endif /* XENOLITH_GL_VK_XLVKDEVICE_H_ */
