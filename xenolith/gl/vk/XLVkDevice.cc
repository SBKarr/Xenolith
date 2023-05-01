/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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
#include "XLVkPipeline.h"
#include "XLGlLoop.h"
#include "XLVkTextureSet.h"
#include "XLVkRenderPassImpl.h"
#include "XLVkLoop.h"
#include "XLVkFramebuffer.h"
#include "XLVkView.h"
#include "XLVkAllocator.h"

namespace stappler::xenolith::vk {

DeviceFrameHandle::~DeviceFrameHandle() {
	if (!_valid) {
		//auto dev = (Device *)_device;
		//dev->getTable()->vkDeviceWaitIdle(dev->getDevice());
	}
	_memPools.clear();
}

bool DeviceFrameHandle::init(Loop &loop, Device &device, Rc<FrameRequest> &&req, uint64_t gen) {
	if (!renderqueue::FrameHandle::init(loop, device, move(req), gen)) {
		return false;
	}

	_allocator = device.getAllocator();
	return true;
}

const Rc<DeviceMemoryPool> &DeviceFrameHandle::getMemPool(void *key) {
	std::unique_lock<Mutex> lock(_mutex);
	auto v = _memPools.find(key);
	if (v == _memPools.end()) {
		v = _memPools.emplace(key, Rc<DeviceMemoryPool>::create(_allocator, _request->isPersistentMapping())).first;
	}
	return v->second;
}

Device::Device() { }

Device::~Device() {
	if (_vkInstance && _device) {
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
	XL_VKDEVICE_LOG("~Device");
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

	_presentMask = info.presentFamily.presentSurfaceMask;

	info.graphicsFamily.count = 1;
	info.presentFamily.count = 1;

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
		Application::getInstance()->perform([info = _info] (const Task &) {
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

	auto maxDescriptors = _info.properties.device10.properties.limits.maxPerStageDescriptorSampledImages;
	auto maxResources = _info.properties.device10.properties.limits.maxPerStageResources - 16;

	auto imageLimit = std::min(maxResources, maxDescriptors);
	auto bufferLimit = std::min(info.properties.device10.properties.limits.maxPerStageDescriptorStorageBuffers, maxDescriptors);
	if (!_info.features.device10.features.shaderSampledImageArrayDynamicIndexing) {
		imageLimit = 1;
	}
	_textureLayoutImagesCount = imageLimit = std::min(imageLimit, config::MaxTextureSetImages);
	_textureLayoutBuffersCount = bufferLimit = std::min(bufferLimit, config::MaxBufferArrayObjects);
	_textureSetLayout = Rc<TextureSetLayout>::create(*this, imageLimit, bufferLimit);

	do {
		VkFormatProperties properties;

		auto addDepthFormat = [&] (VkFormat fmt) {
			_vkInstance->vkGetPhysicalDeviceFormatProperties(_info.device, fmt, &properties);
			_formats.emplace(fmt, properties);
			if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
				_depthFormats.emplace_back(gl::ImageFormat(fmt));
			}
		};

		auto addColorFormat = [&] (VkFormat fmt) {
			_vkInstance->vkGetPhysicalDeviceFormatProperties(_info.device, fmt, &properties);
			_formats.emplace(fmt, properties);
			if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0 && (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) != 0) {
				_colorFormats.emplace_back(gl::ImageFormat(fmt));
			}
		};

		addDepthFormat(VK_FORMAT_D16_UNORM);
		addDepthFormat(VK_FORMAT_X8_D24_UNORM_PACK32);
		addDepthFormat(VK_FORMAT_D32_SFLOAT);
		addDepthFormat(VK_FORMAT_S8_UINT);
		addDepthFormat(VK_FORMAT_D16_UNORM_S8_UINT);
		addDepthFormat(VK_FORMAT_D24_UNORM_S8_UINT);
		addDepthFormat(VK_FORMAT_D32_SFLOAT_S8_UINT);

		addColorFormat(VK_FORMAT_R8_UNORM);
		addColorFormat(VK_FORMAT_R8G8_UNORM);
		addColorFormat(VK_FORMAT_R8G8B8_UNORM);
		addColorFormat(VK_FORMAT_R8G8B8A8_UNORM);
	} while (0);

	return true;
}

VkPhysicalDevice Device::getPhysicalDevice() const {
	return _info.device;
}

void Device::begin(Loop &loop, gl::TaskQueue &q, Function<void(bool)> &&cb) {
	compileSamplers(q, true);
	_textureSetLayout->compile(*this, _immutableSamplers);
	_textureSetLayout->initDefault(*this, loop, move(cb));
	_loopThreadId = std::this_thread::get_id();
}

void Device::end() {
	for (auto &it : _families) {
		for (auto &b : it.pools) {
			b->invalidate(*this);
		}
		it.pools.clear();
	}

	_finished = true;

	for (auto &it : _samplers) {
		it->invalidate();
	}
	_samplers.clear();
}

#if VK_HOOK_DEBUG
static thread_local uint64_t s_vkFnCallStart = 0;
#endif

const DeviceTable * Device::getTable() const {
#if VK_HOOK_DEBUG
	setDeviceHookThreadContext([] (void *ctx, const char *name, PFN_vkVoidFunction fn) {
		/*auto dev = (Device *)ctx;
		if (std::this_thread::get_id() == dev->_loopThreadId) {
			if (fn != (PFN_vkVoidFunction)dev->_original->vkCreateFence
					&& fn != (PFN_vkVoidFunction)dev->_original->vkGetFenceStatus) {
				log::text("Vk-Call-Loop", name);
			}
		}
		if (fn == (PFN_vkVoidFunction)dev->_original->vkQueueSubmit
				|| fn == (PFN_vkVoidFunction)dev->_original->vkQueuePresentKHR
				|| fn == (PFN_vkVoidFunction)dev->_original->vkAcquireNextImageKHR
				|| fn == (PFN_vkVoidFunction)dev->_original->vkCreateSwapchainKHR
				|| fn == (PFN_vkVoidFunction)dev->_original->vkDestroySwapchainKHR) {
			//log::text("Vk-Call", name);
		}*/
		s_vkFnCallStart = platform::device::_clock();
	}, [] (void *ctx, const char *name, PFN_vkVoidFunction fn) {
		auto dt = platform::device::_clock() - s_vkFnCallStart;
		/*auto dev = (Device *)ctx;
		if (std::this_thread::get_id() == dev->_loopThreadId) {
			log::vtext("Vk-Call-Timeout", name, ": ", dt);
		}*/
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

Rc<DeviceQueue> Device::tryAcquireQueueSync(QueueOperations ops, bool lockThread) {
	auto family = (DeviceQueueFamily *)getQueueFamily(ops);
	if (!family) {
		return nullptr;
	}

	std::unique_lock<Mutex> lock(_resourceMutex);
	if (lockThread) {
		++ _resourceQueueWaiters;
		while (family->queues.empty()) {
			_resourceQueueCond.wait(lock);
		}
		-- _resourceQueueWaiters;
	}
	if (!family->queues.empty()) {
		XL_VKDEVICE_LOG("tryAcquireQueueSync ", family->index, " (", family->count, ") ", getQueueOperationsDesc(family->ops));
		auto queue = move(family->queues.back());
		family->queues.pop_back();
		return queue;
	}
	return nullptr;
}

bool Device::acquireQueue(QueueOperations ops, FrameHandle &handle, Function<void(FrameHandle &, const Rc<DeviceQueue> &)> && acquire,
		Function<void(FrameHandle &)> && invalidate, Rc<Ref> &&ref) {

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
		XL_VKDEVICE_LOG("acquireQueue-wait ", family->index, " (", family->count, ") ", getQueueOperationsDesc(family->ops));
		family->waiters.emplace_back(DeviceQueueFamily::Waiter(move(acquire), move(invalidate), &handle, move(ref)));
	}

	if (queue) {
		queue->setOwner(handle);
		XL_VKDEVICE_LOG("acquireQueue ", family->index, " (", family->count, ") ", getQueueOperationsDesc(family->ops));
		acquire(handle, queue);
	}
	return true;
}

bool Device::acquireQueue(QueueOperations ops, Loop &loop, Function<void(Loop &, const Rc<DeviceQueue> &)> && acquire,
		Function<void(Loop &)> && invalidate, Rc<Ref> &&ref) {

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
		XL_VKDEVICE_LOG("acquireQueue-wait ", family->index, " (", family->count, ") ", getQueueOperationsDesc(family->ops));
		family->waiters.emplace_back(DeviceQueueFamily::Waiter(move(acquire), move(invalidate), &loop, move(ref)));
	}

	lock.unlock();

	if (queue) {
		XL_VKDEVICE_LOG("acquireQueue ", family->index, " (", family->count, ") ", getQueueOperationsDesc(family->ops));
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

	queue->reset();

	std::unique_lock<Mutex> lock(_resourceMutex);
	// Проверяем, есть ли синхронные ожидающие
	if (_resourceQueueWaiters > 0) {
		family->queues.emplace_back(move(queue));
		_resourceQueueCond.notify_one();
		return;
	}

	// Проверяем, есть ли асинхронные ожидающие
	if (family->waiters.empty()) {
		XL_VKDEVICE_LOG("releaseQueue ", family->index, " (", family->count, ") ", getQueueOperationsDesc(family->ops));
		family->queues.emplace_back(move(queue));
	} else {
		if (family->waiters.front().handle) {
			Rc<FrameHandle> handle;
			Rc<Ref> ref;
			Function<void(FrameHandle &, const Rc<DeviceQueue> &)> cb;
			Function<void(FrameHandle &)> invalidate;

			cb = move(family->waiters.front().acquireForFrame);
			invalidate = move(family->waiters.front().releaseForFrame);
			ref = move(family->waiters.front().ref);
			handle = move(family->waiters.front().handle);
			family->waiters.erase(family->waiters.begin());

			if (handle && handle->isValid()) {
				if (cb) {
					queue->setOwner(*handle);
					XL_VKDEVICE_LOG("release-acquireQueue ", family->index, " (", family->count, ") ", getQueueOperationsDesc(family->ops));
					cb(*handle, queue);
				}
			} else if (invalidate) {
				XL_VKDEVICE_LOG("invalidate ", family->index, " (", family->count, ") ", getQueueOperationsDesc(family->ops));
				invalidate(*handle);
			}

			handle = nullptr;
		} else if (family->waiters.front().loop) {
			Rc<Loop> loop;
			Rc<Ref> ref;
			Function<void(Loop &, const Rc<DeviceQueue> &)> cb;
			Function<void(Loop &)> invalidate;

			cb = move(family->waiters.front().acquireForLoop);
			invalidate = move(family->waiters.front().releaseForLoop);
			ref = move(family->waiters.front().ref);
			loop = move(family->waiters.front().loop);
			family->waiters.erase(family->waiters.begin());

			if (loop && loop->isRunning()) {
				if (cb) {
					XL_VKDEVICE_LOG("release-acquireQueue ", family->index, " (", family->count, ") ", getQueueOperationsDesc(family->ops));
					cb(*loop, queue);
				}
			} else if (invalidate) {
				XL_VKDEVICE_LOG("invalidate ", family->index, " (", family->count, ") ", getQueueOperationsDesc(family->ops));
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
	/*pool->reset(*this, true);

	std::unique_lock<Mutex> lock(_resourceMutex);
	_families[pool->getFamilyIdx()].pools.emplace_back(Rc<CommandPool>(pool));*/

	auto refId = retain();
	loop.performInQueue(Rc<Task>::create([this, pool] (const Task &) -> bool {
		pool->reset(*this);
		return true;
	}, [this, pool, refId] (const Task &, bool success) {
		if (success) {
			std::unique_lock<Mutex> lock(_resourceMutex);
			_families[pool->getFamilyIdx()].pools.emplace_back(Rc<CommandPool>(pool));
		}
		release(refId);
	}));
}

void Device::releaseCommandPoolUnsafe(Rc<CommandPool> &&pool) {
	pool->reset(*this);

	std::unique_lock<Mutex> lock(_resourceMutex);
	_families[pool->getFamilyIdx()].pools.emplace_back(Rc<CommandPool>(pool));
}

static BytesView Device_emplaceConstant(Bytes &data, BytesView constant) {
	auto originalSize = data.size();
	auto constantSize = constant.size();
	data.resize(originalSize + constantSize);
	memcpy(data.data() + originalSize, constant.data(), constantSize);
	return BytesView(data.data() + originalSize, constantSize);
}

BytesView Device::emplaceConstant(renderqueue::PredefinedConstant c, Bytes &data) const {
	uint32_t intData = 0;
	switch (c) {
	case renderqueue::PredefinedConstant::SamplersArraySize:
		return Device_emplaceConstant(data, BytesView((const uint8_t *)&_samplersCount, sizeof(uint32_t)));
		break;
	case renderqueue::PredefinedConstant::SamplersDescriptorIdx:
		intData = 0;
		return Device_emplaceConstant(data, BytesView((const uint8_t *)&intData, sizeof(uint32_t)));
		break;
	case renderqueue::PredefinedConstant::TexturesArraySize:
		return Device_emplaceConstant(data, BytesView((const uint8_t *)&_textureSetLayout->getImageCount(), sizeof(uint32_t)));
		break;
	case renderqueue::PredefinedConstant::TexturesDescriptorIdx:
		intData = 1;
		return Device_emplaceConstant(data, BytesView((const uint8_t *)&intData, sizeof(uint32_t)));
		break;
	case renderqueue::PredefinedConstant::BuffersArraySize:
		return Device_emplaceConstant(data, BytesView((const uint8_t *)&_textureSetLayout->getBuffersCount(), sizeof(uint32_t)));
		break;
	case renderqueue::PredefinedConstant::BuffersDescriptorIdx:
		intData = 2;
		return Device_emplaceConstant(data, BytesView((const uint8_t *)&intData, sizeof(uint32_t)));
		break;
	case renderqueue::PredefinedConstant::CurrentSamplerIdx:
		break;
	}
	return BytesView();
}

bool Device::supportsUpdateAfterBind(DescriptorType type) const {
	switch (type) {
	case DescriptorType::Sampler:
		return true; // Samplers are immutable engine-wide
		break;
	case DescriptorType::CombinedImageSampler:
		return _info.features.deviceDescriptorIndexing.descriptorBindingSampledImageUpdateAfterBind;
		break;
	case DescriptorType::SampledImage:
		return _info.features.deviceDescriptorIndexing.descriptorBindingSampledImageUpdateAfterBind;
		break;
	case DescriptorType::StorageImage:
		return _info.features.deviceDescriptorIndexing.descriptorBindingStorageImageUpdateAfterBind;
		break;
	case DescriptorType::UniformTexelBuffer:
		return _info.features.deviceDescriptorIndexing.descriptorBindingUniformTexelBufferUpdateAfterBind;
		break;
	case DescriptorType::StorageTexelBuffer:
		return _info.features.deviceDescriptorIndexing.descriptorBindingStorageTexelBufferUpdateAfterBind;
		break;
	case DescriptorType::UniformBuffer:
	case DescriptorType::UniformBufferDynamic:
		return _info.features.deviceDescriptorIndexing.descriptorBindingUniformBufferUpdateAfterBind;
		break;
	case DescriptorType::StorageBuffer:
	case DescriptorType::StorageBufferDynamic:
		return _info.features.deviceDescriptorIndexing.descriptorBindingStorageBufferUpdateAfterBind;
		break;
	case DescriptorType::InputAttachment:
	case DescriptorType::Attachment:
	case DescriptorType::Unknown:
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

Rc<gl::Framebuffer> Device::makeFramebuffer(const renderqueue::PassData *pass, SpanView<Rc<gl::ImageView>> views, Extent2 extent) {
	return Rc<Framebuffer>::create(*this, (RenderPassImpl *)pass->impl.get(), views, extent);
}

auto Device::makeImage(const gl::ImageInfo &imageInfo) -> Rc<ImageStorage> {

	bool isTransient = (imageInfo.usage & gl::ImageUsage::TransientAttachment) != gl::ImageUsage::None;

	auto img = _allocator->spawnPersistent(
			isTransient ? AllocationUsage::DeviceLocalLazilyAllocated : AllocationUsage::DeviceLocal,
			imageInfo, false);

	return Rc<ImageStorage>::create(move(img));
}

Rc<gl::Semaphore> Device::makeSemaphore() {
	auto ret = Rc<Semaphore>::create(*this);
	return ret;
}

Rc<gl::ImageView> Device::makeImageView(const Rc<gl::ImageObject> &img, const gl::ImageViewInfo &info) {
	auto ret = Rc<ImageView>::create(*this, (Image *)img.get(), info);
	return ret;
}

bool Device::hasNonSolidFillMode() const {
	return _info.features.device10.features.fillModeNonSolid;
}

bool Device::hasDynamicIndexedBuffers() const {
	return _info.features.device10.features.shaderStorageBufferArrayDynamicIndexing;
}

void Device::waitIdle() const {
	_table->vkDeviceWaitIdle(_device);
}

void Device::compileSamplers(gl::TaskQueue &q, bool force) {
	_immutableSamplers.resize(_samplersInfo.size(), VK_NULL_HANDLE);
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
	void *next = nullptr;
#ifdef VK_ENABLE_BETA_EXTENSIONS
	if ((features.flags & ExtensionFlags::Portability) != ExtensionFlags::None) {
		features.devicePortability.pNext = next;
		next = &features.devicePortability;
	}
#endif
	if (prop.device10.properties.apiVersion >= VK_API_VERSION_1_3) {
		features.device13.pNext = next;
		features.device12.pNext = &features.device13;
		features.device11.pNext = &features.device12;
		features.device10.pNext = &features.device11;
		deviceCreateInfo.pNext = &features.device11;
	} else if (prop.device10.properties.apiVersion >= VK_API_VERSION_1_2) {
		features.device12.pNext = next;
		features.device11.pNext = &features.device12;
		features.device10.pNext = &features.device11;
		deviceCreateInfo.pNext = &features.device11;
	} else {
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
		if (Application::getInstance()->getData().validation) {
			deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(sizeof(s_validationLayers) / sizeof(const char *));
			deviceCreateInfo.ppEnabledLayerNames = s_validationLayers;
		}
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
