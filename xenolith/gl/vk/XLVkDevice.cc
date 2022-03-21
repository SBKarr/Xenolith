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

#include "XLVkDevice.h"
#include "XLApplication.h"
#include "XLGlObject.h"
#include "XLVkSwapchain.h"
#include "XLVkPipeline.h"
#include "XLVkFrame.h"
#include "XLGlLoop.h"
#include "XLVkTextureSet.h"
#include "XLVkRenderPassImpl.h"
#include "XLVkTransferQueue.h"
#include "XLVkMaterialCompiler.h"
#include "XLVkRenderQueueCompiler.h"

namespace stappler::xenolith::vk {

Device::Device() { }

Device::~Device() {
	if (_vkInstance && _device) {
		if (_renderQueueCompiler) {
			_renderQueueCompiler = nullptr;
		}

		if (_materialQueue) {
			_materialQueue = nullptr;
		}

		if (_allocator) {
			_allocator->invalidate(*this);
			_allocator = nullptr;
		}

		if (_textureSetLayout) {
			_textureSetLayout->invalidate(*this);
			_textureSetLayout = nullptr;
		}

		clearShaders();
		invalidateObjects();

		_table->vkDestroyDevice(_device, nullptr);
		delete _table;

		_device = nullptr;
		_table = nullptr;
	}
}

bool Device::init(const vk::Instance *inst, DeviceInfo && info, const Features &features) {
	Set<uint32_t> uniqueQueueFamilies = { info.graphicsFamily.index, info.presentFamily.index, info.transferFamily.index, info.computeFamily.index };

	auto emplaceQueueFamily = [&] (DeviceInfo::QueueFamilyInfo &info, uint32_t count, QueueOperations preferred) {
		for (auto &it : _families) {
			if (it.index == info.index) {
				it.preferred |= preferred;
				it.count = std::min(it.count + count, std::min(info.count, uint32_t(std::thread::hardware_concurrency())));
				return;
			}
		}
		count = std::min(count, std::min(info.count, uint32_t(std::thread::hardware_concurrency())));
		_families.emplace_back(DeviceQueueFamily({ info.index, count, preferred, info.ops, info.minImageTransferGranularity}));
	};

	emplaceQueueFamily(info.graphicsFamily, std::thread::hardware_concurrency(), QueueOperations::Graphics);
	emplaceQueueFamily(info.presentFamily, 1, QueueOperations::Present);
	emplaceQueueFamily(info.transferFamily, 2, QueueOperations::Transfer);
	emplaceQueueFamily(info.computeFamily, std::thread::hardware_concurrency(), QueueOperations::Compute);

	Vector<const char *> extensions;
	for (auto &it : s_requiredDeviceExtensions) {
		if (it && !isPromotedExtension(inst->getVersion(), StringView(it))) {
			extensions.emplace_back(it);
		}
	}

	for (auto &it : info.optionalExtensions) {
		extensions.emplace_back(it.data());
	}

	for (auto &it : info.promotedExtensions) {
		if (!isPromotedExtension(inst->getVersion(), it)) {
			extensions.emplace_back(it.data());
		}
	}

	_enabledFeatures = features;
	if (!setup(inst, info.device, info.properties, _families, _enabledFeatures, extensions)) {
		return false;
	}

	if (!gl::Device::init(inst)) {
		return false;
	}

	_vkInstance = inst;
	_info = move(info);

	if constexpr (s_printVkInfo) {
		Application::getInstance()->perform([this, info = _info, instance = _vkInstance] (const Task &) {
			log::vtext("Vk-Info", "Device info:\n", info.description());
			return true;
		}, nullptr, this);
	}

	for (auto &it : _families) {
		it.queues.reserve(it.count);
		it.pools.reserve(it.count);
		for (size_t i = 0; i < it.count; ++ i) {
			VkQueue queue = VK_NULL_HANDLE;
			getTable()->vkGetDeviceQueue(_device, it.index, i, &queue);

			it.queues.emplace_back(Rc<DeviceQueue>::create(*this, queue, it.index, it.ops));
			it.pools.emplace_back(Rc<CommandPool>::create(*this, it.index, it.preferred));
		}
	}

	_allocator = Rc<Allocator>::create(*this, _info.device, _info.features, _info.properties);

	auto imageLimit = _info.properties.device10.properties.limits.maxPerStageDescriptorSampledImages;
	_textureLayoutImagesCount = imageLimit = std::min(imageLimit, config::MaxTextureSetImages);
	_textureSetLayout = Rc<TextureSetLayout>::create(*this, imageLimit);

	_renderQueueCompiler = Rc<RenderQueueCompiler>::create(*this);

	do {
		VkFormatProperties properties;
		_vkInstance->vkGetPhysicalDeviceFormatProperties(_info.device, VK_FORMAT_D16_UNORM, &properties);
		_formats.emplace(VK_FORMAT_D16_UNORM, properties);
		if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
			_depthFormats.emplace_back(gl::ImageFormat(VK_FORMAT_D16_UNORM));
		}

		_vkInstance->vkGetPhysicalDeviceFormatProperties(_info.device, VK_FORMAT_X8_D24_UNORM_PACK32, &properties);
		_formats.emplace(VK_FORMAT_X8_D24_UNORM_PACK32, properties);
		if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
			_depthFormats.emplace_back(gl::ImageFormat(VK_FORMAT_X8_D24_UNORM_PACK32));
		}

		_vkInstance->vkGetPhysicalDeviceFormatProperties(_info.device, VK_FORMAT_D32_SFLOAT, &properties);
		_formats.emplace(VK_FORMAT_D32_SFLOAT, properties);
		if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
			_depthFormats.emplace_back(gl::ImageFormat(VK_FORMAT_D32_SFLOAT));
		}

		_vkInstance->vkGetPhysicalDeviceFormatProperties(_info.device, VK_FORMAT_S8_UINT, &properties);
		_formats.emplace(VK_FORMAT_S8_UINT, properties);
		if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
			_depthFormats.emplace_back(gl::ImageFormat(VK_FORMAT_S8_UINT));
		}

		_vkInstance->vkGetPhysicalDeviceFormatProperties(_info.device, VK_FORMAT_D16_UNORM_S8_UINT, &properties);
		_formats.emplace(VK_FORMAT_D16_UNORM_S8_UINT, properties);
		if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
			_depthFormats.emplace_back(gl::ImageFormat(VK_FORMAT_D16_UNORM_S8_UINT));
		}

		_vkInstance->vkGetPhysicalDeviceFormatProperties(_info.device, VK_FORMAT_D24_UNORM_S8_UINT, &properties);
		_formats.emplace(VK_FORMAT_D24_UNORM_S8_UINT, properties);
		if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
			_depthFormats.emplace_back(gl::ImageFormat(VK_FORMAT_D24_UNORM_S8_UINT));
		}
	} while (0);

	return true;
}

VkPhysicalDevice Device::getPhysicalDevice() const {
	return _info.device;
}

void Device::begin(const Application *app, thread::TaskQueue &q) {
	if (!isSamplersCompiled()) {
		compileSamplers(*app->getQueue(), true);
	}

	gl::Device::begin(app, q);

	_materialQueue = Rc<MaterialCompiler>::create();
	_transferQueue = Rc<TransferQueue>::create();
}

void Device::end(thread::TaskQueue &q) {
	waitIdle();

	for (auto &it : _families) {
		for (auto &b : it.pools) {
			b->invalidate(*this);
		}
		it.pools.clear();
	}

	_finished = true;

	_materialQueue->clearRequests();
	_materialQueue = nullptr;
	_transferQueue = nullptr;

	for (auto &it : _fences) {
		it->invalidate();
	}
	_fences.clear();

	for (auto &it : _samplers) {
		it->invalidate();
	}
	_samplers.clear();

	gl::Device::end(q);
}

void Device::waitIdle() {
	for (auto &it : _scheduled) {
		it->check(false);
	}
	_scheduled.clear();
	getTable()->vkDeviceWaitIdle(_device);
}

void Device::onLoopStarted(gl::Loop &loop) {
	gl::Device::onLoopStarted(loop);
	_textureSetLayout->initDefault(*this, loop);

	loop.getQueue()->waitForAll();

	compileRenderQueue(loop, _materialQueue);
	compileRenderQueue(loop, _transferQueue);
}

void Device::onLoopEnded(gl::Loop &loop) {
	gl::Device::onLoopEnded(loop);
}

#if VK_HOOK_DEBUG
static thread_local uint64_t s_vkFnCallStart = 0;
#endif

const DeviceTable * Device::getTable() const {
#if VK_HOOK_DEBUG
	setDeviceHookThreadContext([] (void *ctx, const char *name, PFN_vkVoidFunction fn) {
		auto dev = (Device *)ctx;
		if (std::this_thread::get_id() == dev->_loopThreadId) {
			if (fn != (PFN_vkVoidFunction)dev->_original->vkCreateFence
					&& fn != (PFN_vkVoidFunction)dev->_original->vkGetFenceStatus) {
				//log::text("Vk-Call", name);
			}
		}
		if (fn == (PFN_vkVoidFunction)dev->_original->vkQueueSubmit
				|| fn == (PFN_vkVoidFunction)dev->_original->vkQueuePresentKHR
				|| fn == (PFN_vkVoidFunction)dev->_original->vkAcquireNextImageKHR
				|| fn == (PFN_vkVoidFunction)dev->_original->vkCreateSwapchainKHR
				|| fn == (PFN_vkVoidFunction)dev->_original->vkDestroySwapchainKHR) {
			log::text("Vk-Call", name);
		}
		s_vkFnCallStart = platform::device::_clock();
	}, [] (void *ctx, const char *name, PFN_vkVoidFunction fn) {
		auto dt = platform::device::_clock() - s_vkFnCallStart;
		if (dt > 200000) {
			log::vtext("Vk-Call-Timeout", name, ": ", dt);
		}
	}, _original, nullptr, (void *)this);
#endif

	return _table;
}

const DeviceQueueFamily *Device::getQueueFamily(uint32_t familyIdx) const {
	for (auto &it : _families) {
		if (it.index == familyIdx) {
			return &it;
		}
	}
	return nullptr;
}

const DeviceQueueFamily *Device::getQueueFamily(QueueOperations ops) const {
	for (auto &it : _families) {
		if (it.preferred == ops) {
			return &it;
		}
	}
	for (auto &it : _families) {
		if ((it.ops & ops) != QueueOperations::None) {
			return &it;
		}
	}
	return nullptr;
}

const DeviceQueueFamily *Device::getQueueFamily(gl::RenderPassType type) const {
	switch (type) {
	case gl::RenderPassType::Graphics:
		return getQueueFamily(QueueOperations::Graphics);
		break;
	case gl::RenderPassType::Compute:
		return getQueueFamily(QueueOperations::Compute);
		break;
	case gl::RenderPassType::Transfer:
		return getQueueFamily(QueueOperations::Transfer);
		break;
	case gl::RenderPassType::Generic:
		return nullptr;
		break;
	}
	return nullptr;
}

const Vector<DeviceQueueFamily> &Device::getQueueFamilies() const {
	return _families;
}

Rc<DeviceQueue> Device::tryAcquireQueueSync(QueueOperations ops) {
	auto family = (DeviceQueueFamily *)getQueueFamily(ops);
	if (!family) {
		return nullptr;
	}

	std::unique_lock<Mutex> lock(_resourceMutex);
	if (!family->queues.empty()) {
		auto queue = move(family->queues.back());
		family->queues.pop_back();
		return queue;
	}
	return nullptr;
}

bool Device::acquireQueue(QueueOperations ops, gl::FrameHandle &handle, Function<void(gl::FrameHandle &, const Rc<DeviceQueue> &)> && acquire,
		Function<void(gl::FrameHandle &)> && invalidate, Rc<Ref> &&ref) {

	auto family = (DeviceQueueFamily *)getQueueFamily(ops);
	if (!family) {
		return false;
	}

	std::unique_lock<Mutex> lock(_resourceMutex);
	Rc<DeviceQueue> queue;
	if (!family->queues.empty()) {
		queue = move(family->queues.back());
		family->queues.pop_back();
	} else {
		family->waiters.emplace_back(DeviceQueueFamily::Waiter(move(acquire), move(invalidate), &handle, move(ref)));
	}

	if (queue) {
		acquire(handle, queue);
	}
	return true;
}

bool Device::acquireQueue(QueueOperations ops, gl::Loop &loop, Function<void(gl::Loop &, const Rc<DeviceQueue> &)> && acquire,
		Function<void(gl::Loop &)> && invalidate, Rc<Ref> &&ref) {

	auto family = (DeviceQueueFamily *)getQueueFamily(ops);
	if (!family) {
		return false;
	}

	std::unique_lock<Mutex> lock(_resourceMutex);
	Rc<DeviceQueue> queue;
	if (!family->queues.empty()) {
		queue = move(family->queues.back());
		family->queues.pop_back();
	} else {
		family->waiters.emplace_back(DeviceQueueFamily::Waiter(move(acquire), move(invalidate), &loop, move(ref)));
	}

	lock.unlock();
	if (queue) {
		acquire(loop, queue);
	}
	return true;
}

void Device::releaseQueue(Rc<DeviceQueue> &&queue) {
	DeviceQueueFamily *family = nullptr;
	for (auto &it : _families) {
		if (it.index == queue->getIndex()) {
			family = &it;
			break;
		}
	}

	if (!family) {
		return;
	}

	std::unique_lock<Mutex> lock(_resourceMutex);
	if (family->waiters.empty()) {
		family->queues.emplace_back(move(queue));
	} else {
		if (family->waiters.front().handle) {
			Rc<gl::FrameHandle> handle;
			Rc<Ref> ref;
			Function<void(gl::FrameHandle &, const Rc<DeviceQueue> &)> cb;
			Function<void(gl::FrameHandle &)> invalidate;

			cb = move(family->waiters.front().acquireForFrame);
			invalidate = move(family->waiters.front().releaseForFrame);
			ref = move(family->waiters.front().ref);
			handle = move(family->waiters.front().handle);
			family->waiters.erase(family->waiters.begin());

			if (handle && handle->isValid()) {
				if (cb) {
					cb(*handle, queue);
				}
			} else if (invalidate) {
				invalidate(*handle);
			}

			handle = nullptr;
		} else if (family->waiters.front().loop) {
			Rc<gl::Loop> loop;
			Rc<Ref> ref;
			Function<void(gl::Loop &, const Rc<DeviceQueue> &)> cb;
			Function<void(gl::Loop &)> invalidate;

			cb = move(family->waiters.front().acquireForLoop);
			invalidate = move(family->waiters.front().releaseForLoop);
			ref = move(family->waiters.front().ref);
			loop = move(family->waiters.front().loop);
			family->waiters.erase(family->waiters.begin());

			if (loop && loop->isRunning()) {
				if (cb) {
					cb(*loop, queue);
				}
			} else if (invalidate) {
				invalidate(*loop);
			}

			loop = nullptr;
		}
	}

	queue = nullptr;
}

Rc<CommandPool> Device::acquireCommandPool(QueueOperations c, uint32_t) {
	auto family = (DeviceQueueFamily *)getQueueFamily(c);
	if (!family) {
		return nullptr;
	}

	std::unique_lock<Mutex> lock(_resourceMutex);
	if (!family->pools.empty()) {
		auto ret = family->pools.back();
		family->pools.pop_back();
		return ret;
	}
	lock.unlock();
	return Rc<CommandPool>::create(*this, family->index, family->ops);
}

Rc<CommandPool> Device::acquireCommandPool(uint32_t familyIndex) {
	auto family = (DeviceQueueFamily *)getQueueFamily(familyIndex);
	if (!family) {
		return nullptr;
	}

	std::unique_lock<Mutex> lock(_resourceMutex);
	if (!family->pools.empty()) {
		auto ret = family->pools.back();
		family->pools.pop_back();
		return ret;
	}
	lock.unlock();
	return Rc<CommandPool>::create(*this, family->index, family->ops);
}

void Device::releaseCommandPool(gl::Loop &loop, Rc<CommandPool> &&pool) {
	pool->reset(*this);

	std::unique_lock<Mutex> lock(_resourceMutex);
	_families[pool->getFamilyIdx()].pools.emplace_back(Rc<CommandPool>(pool));

	/*auto refId = retain();
	loop.getQueue()->perform(Rc<Task>::create([this, pool] (const Task &) -> bool {
		pool->reset(*this);
		return true;
	}, [this, pool, refId] (const Task &, bool success) {
		if (success) {
			std::unique_lock<Mutex> lock(_resourceMutex);
			_families[pool->getFamilyIdx()].pools.emplace_back(Rc<CommandPool>(pool));
		}
		release(refId);
	}));*/
}

void Device::releaseCommandPoolUnsafe(Rc<CommandPool> &&pool) {
	pool->reset(*this);

	std::unique_lock<Mutex> lock(_resourceMutex);
	_families[pool->getFamilyIdx()].pools.emplace_back(Rc<CommandPool>(pool));
}

Rc<Fence> Device::acquireFence(uint32_t v) {
	std::unique_lock<Mutex> lock(_resourceMutex);
	if (!_fences.empty()) {
		auto ret = _fences.back();
		_fences.pop_back();
		ret->setFrame(v);
		return ret;
	}
	lock.unlock();
	auto f = Rc<Fence>::create(*this);
	f->setFrame(v);
	return f;
}

void Device::releaseFence(gl::Loop &loop, Rc<Fence> &&ref) {
	ref->reset(loop, [this] (Rc<Fence> &&ref) {
		std::unique_lock<Mutex> lock(_resourceMutex);
		_fences.emplace_back(move(ref));
	});
}

void Device::releaseFenceUnsafe(Rc<Fence> &&ref) {
	ref->resetUnsafe();
	std::unique_lock<Mutex> lock(_resourceMutex);
	_fences.emplace_back(move(ref));
}

void Device::scheduleFence(gl::Loop &loop, Rc<Fence> &&fence) {
	if (!fence->isArmed() || fence->check()) {
		releaseFence(loop, move(fence));
		return;
	}

	_scheduled.emplace(fence);
	loop.schedule([this, fence = move(fence)] (gl::Loop::Context &ctx) {
		if (_scheduled.find(fence) == _scheduled.end()) {
			return true;
		}
		if (fence->check()) {
			_scheduled.erase(fence);
			releaseFence(*ctx.loop, Rc<Fence>(fence));
			return true;
		}
		return false;
	}, "Device::scheduleFence");
}

void Device::scheduleFence(gl::Loop &loop, Rc<Fence> &&fence, Function<bool(Fence &)> &&cb) {
	if (cb(*fence)) {
		releaseFence(loop, move(fence));
		return;
	}

	_scheduled.emplace(fence);
	loop.schedule([this, fence = move(fence), cb = move(cb)] (gl::Loop::Context &ctx) mutable {
		if (_scheduled.find(fence) == _scheduled.end()) {
			return true;
		}
		if (cb(*fence)) {
			_scheduled.erase(fence);
			releaseFence(*ctx.loop, move(fence));
			return true;
		}
		return false;
	}, "Device::scheduleFence");
}

static BytesView Device_emplaceConstant(Bytes &data, BytesView constant) {
	data.resize(data.size() + constant.size());
	memcpy(data.data() + data.size() - constant.size(), constant.data(), constant.size());
	return BytesView(data.data() + data.size() - constant.size(), constant.size());
}

BytesView Device::emplaceConstant(gl::PredefinedConstant c, Bytes &data) const {
	switch (c) {
	case gl::PredefinedConstant::SamplersArraySize:
		return Device_emplaceConstant(data, BytesView((const uint8_t *)&_samplersCount, sizeof(uint32_t)));
		break;
	case gl::PredefinedConstant::TexturesArraySize:
		return Device_emplaceConstant(data, BytesView((const uint8_t *)&_textureSetLayout->getImageCount(), sizeof(uint32_t)));
		break;
	}
	return BytesView();
}

bool Device::supportsUpdateAfterBind(gl::DescriptorType type) const {
	switch (type) {
	case gl::DescriptorType::Sampler:
		return true; // Samplers are immutable engine-wide
		break;
	case gl::DescriptorType::CombinedImageSampler:
		return _info.features.deviceDescriptorIndexing.descriptorBindingSampledImageUpdateAfterBind;
		break;
	case gl::DescriptorType::SampledImage:
		return _info.features.deviceDescriptorIndexing.descriptorBindingSampledImageUpdateAfterBind;
		break;
	case gl::DescriptorType::StorageImage:
		return _info.features.deviceDescriptorIndexing.descriptorBindingStorageImageUpdateAfterBind;
		break;
	case gl::DescriptorType::UniformTexelBuffer:
		return _info.features.deviceDescriptorIndexing.descriptorBindingUniformTexelBufferUpdateAfterBind;
		break;
	case gl::DescriptorType::StorageTexelBuffer:
		return _info.features.deviceDescriptorIndexing.descriptorBindingStorageTexelBufferUpdateAfterBind;
		break;
	case gl::DescriptorType::UniformBuffer:
		return _info.features.deviceDescriptorIndexing.descriptorBindingUniformBufferUpdateAfterBind;
		break;
	case gl::DescriptorType::StorageBuffer:
		return _info.features.deviceDescriptorIndexing.descriptorBindingStorageBufferUpdateAfterBind;
		break;
	default:
		return false;
		break;
	}
	return false;
}

Rc<gl::ImageObject> Device::getEmptyImageObject() const {
	return _textureSetLayout->getEmptyImageObject();
}

Rc<gl::ImageObject> Device::getSolidImageObject() const {
	return _textureSetLayout->getSolidImageObject();
}

Rc<gl::FrameHandle> Device::makeFrame(gl::Loop &loop, Rc<gl::FrameRequest> &&req, uint64_t gen) {
	return Rc<FrameHandle>::create(loop, move(req), gen);
}

Rc<gl::Framebuffer> Device::makeFramebuffer(const gl::RenderPassData *pass, SpanView<Rc<gl::ImageView>> views, Extent2 extent) {
	return Rc<Framebuffer>::create(*this, ((RenderPassImpl *)pass->impl.get())->getRenderPass(), views, extent);
}

Rc<gl::ImageAttachmentObject> Device::makeImage(const gl::ImageAttachment *attachment, Extent3 extent) {
	auto ret = Rc<gl::ImageAttachmentObject>::alloc();

	auto imageInfo = attachment->getInfo();
	if (attachment->isTransient()) {
		imageInfo.usage |= gl::ImageUsage::TransientAttachment;
	}
	ret->extent = imageInfo.extent = extent;

	auto img = _allocator->spawnPersistent(
			attachment->isTransient() ? AllocationUsage::DeviceLocalLazilyAllocated : AllocationUsage::DeviceLocal,
			imageInfo, false);

	ret->image = img.get();
	ret->makeViews(*this, *attachment);

	return ret;
}

Rc<gl::Semaphore> Device::makeSemaphore() {
	auto ret = Rc<Semaphore>::create(*this);
	return ret;
}

Rc<gl::ImageView> Device::makeImageView(const Rc<gl::ImageObject> &img, const gl::ImageViewInfo &info) {
	auto ret = Rc<ImageView>::create(*this, (Image *)img.get(), info);
	return ret;
}

void Device::compileResource(gl::Loop &loop, const Rc<gl::Resource> &req, Function<void(bool)> &&complete) {
	auto h = makeFrame(loop, _transferQueue->makeRequest(Rc<TransferResource>::create(getAllocator(), req, move(complete))), 0);
	h->update(true);
}

void Device::compileRenderQueue(gl::Loop &loop, const Rc<gl::RenderQueue> &req, Function<void(bool)> &&cb) {
	if (req->usesSamplers() && !_samplersCompiled.load()) {
		loop.getQueue()->waitForAll();
	}

	auto input = Rc<RenderQueueInput>::alloc();
	input->queue = req;

	auto h = makeFrame(loop, _renderQueueCompiler->makeRequest(move(input)), 0);
	if (cb) {
		h->setCompleteCallback([cb] (gl::FrameHandle &handle) {
			cb(handle.isValid());
		});
	}

	h->update(true);
}

void Device::compileImage(gl::Loop &loop, const Rc<gl::DynamicImage> &image, Function<void(bool)> &&cb) {
	_textureSetLayout->compileImage(*this, loop, image, move(cb));
}

void Device::compileSamplers(thread::TaskQueue &q, bool force) {
	_immutableSamplers.resize(_samplersInfo.size(), nullptr);
	_samplers.resize(_samplersInfo.size(), nullptr);
	_samplersCount = _samplersInfo.size();

	size_t i = 0;
	for (auto &it : _samplersInfo) {
		auto objIt = _samplers.data() + i;
		auto glIt = _immutableSamplers.data() + i;
		q.perform(Rc<thread::Task>::create([this, objIt, glIt, v = &it] (const thread::Task &) -> bool {
			*objIt = Rc<Sampler>::create(*this, *v);
			*glIt = (*objIt)->getSampler();
			return true;
		}, [this] (const thread::Task &, bool) {
			++ _compiledSamplers;
			if (_compiledSamplers == _samplersCount) {
				_samplersCompiled = true;
			}
		}));

		++ i;
	}
	if (force) {
		q.waitForAll();
	}
}

void Device::runMaterialCompilationFrame(gl::Loop &loop, Rc<gl::MaterialInputData> &&req) {
	auto targetAttachment = req->attachment;

	auto h = makeFrame(loop, _materialQueue->makeRequest(move(req)), 0);
	h->setCompleteCallback([this, targetAttachment] (gl::FrameHandle &handle) {
		if (_materialQueue->hasRequest(targetAttachment)) {
			if (handle.getLoop()->isRunning()) {
				auto req = _materialQueue->popRequest(targetAttachment);
				runMaterialCompilationFrame(*handle.getLoop(), move(req));
			} else {
				_materialQueue->clearRequests();
				_materialQueue->dropInProgress(targetAttachment);
			}
		} else {
			_materialQueue->dropInProgress(targetAttachment);
		}
	});
	h->update(true);
}

void Device::compileMaterials(gl::Loop &loop, Rc<gl::MaterialInputData> &&req) {
	if (_finished) {
		return;
	}
	if (_materialQueue->inProgress(req->attachment)) {
		_materialQueue->appendRequest(req->attachment, move(req));
	} else {
		auto attachment = req->attachment;
		_materialQueue->setInProgress(attachment);
		runMaterialCompilationFrame(loop, move(req));
	}
}

bool Device::setup(const Instance *instance, VkPhysicalDevice p, const Properties &prop,
		const Vector<DeviceQueueFamily> &queueFamilies, Features &features, const Vector<const char *> &requiredExtension) {
	Vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	uint32_t maxQueues = 0;
	for (auto &it : queueFamilies) {
		maxQueues = std::max(it.count, maxQueues);
	}

	Vector<float> queuePriority;
	queuePriority.resize(maxQueues, 1.0f);

	for (auto & queueFamily : queueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo = { };
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily.index;
		queueCreateInfo.queueCount = queueFamily.count;
		queueCreateInfo.pQueuePriorities = queuePriority.data();
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkDeviceCreateInfo deviceCreateInfo = { };
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	if (prop.device10.properties.apiVersion >= VK_VERSION_1_2) {
		features.device12.pNext = nullptr;
		features.device11.pNext = &features.device12;
		features.device10.pNext = &features.device11;
		deviceCreateInfo.pNext = &features.device11;
	} else {
		void *next = nullptr;
		if ((features.flags & ExtensionFlags::Storage16Bit) != ExtensionFlags::None) {
			features.device16bitStorage.pNext = next;
			next = &features.device16bitStorage;
		}
		if ((features.flags & ExtensionFlags::Storage8Bit) != ExtensionFlags::None) {
			features.device8bitStorage.pNext = next;
			next = &features.device8bitStorage;
		}
		if ((features.flags & ExtensionFlags::ShaderFloat16) != ExtensionFlags::None || (features.flags & ExtensionFlags::ShaderInt8) != ExtensionFlags::None) {
			features.deviceShaderFloat16Int8.pNext = next;
			next = &features.deviceShaderFloat16Int8;
		}
		if ((features.flags & ExtensionFlags::DescriptorIndexing) != ExtensionFlags::None) {
			features.deviceDescriptorIndexing.pNext = next;
			next = &features.deviceDescriptorIndexing;
		}
		if ((features.flags & ExtensionFlags::DeviceAddress) != ExtensionFlags::None) {
			features.deviceBufferDeviceAddress.pNext = next;
			next = &features.deviceBufferDeviceAddress;
		}
		deviceCreateInfo.pNext = next;
	}
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.pEnabledFeatures = &features.device10.features;
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtension.size());
	deviceCreateInfo.ppEnabledExtensionNames = requiredExtension.data();

	if constexpr (s_enableValidationLayers) {
		deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(sizeof(s_validationLayers) / sizeof(const char *));
		deviceCreateInfo.ppEnabledLayerNames = s_validationLayers;
	} else {
		deviceCreateInfo.enabledLayerCount = 0;
	}

	if (instance->vkCreateDevice(p, &deviceCreateInfo, nullptr, &_device) != VK_SUCCESS) {
		return false;
	}

#if VK_HOOK_DEBUG
	auto hookTable = new DeviceTable(DeviceTable::makeHooks());
	_original = new DeviceTable(instance->vkGetDeviceProcAddr, _device);
	_table = hookTable;
#else
	_table = new DeviceTable(instance->vkGetDeviceProcAddr, _device);
#endif

	return true;
}

}
