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

#include "XLVkDevice.h"
#include "XLApplication.h"
#include "XLGlObject.h"
#include "XLVkSwapchain.h"
#include "XLVkPipeline.h"
#include "XLGlFrame.h"
#include "XLGlLoop.h"
#include "XLVkRenderPassImpl.h"

namespace stappler::xenolith::vk {

#ifdef XL_VK_HOOK_DEBUG
static std::mutex hook_callLock;
static DeviceCallTable *hook_origTable = nullptr;

enum VkResult hook_vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence) {
	log::text("Vk-Hook", "vkQueueSubmit");
	return hook_origTable->vkQueueSubmit(queue, submitCount, pSubmits, fence);
}

enum VkResult hook_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
	log::text("Vk-Hook", "vkQueuePresentKHR");
	return hook_origTable->vkQueuePresentKHR(queue, pPresentInfo);
}

enum VkResult hook_vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex) {
	log::text("Vk-Hook", "vkAcquireNextImageKHR");
	return hook_origTable->vkAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
}

enum VkResult hook_vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo) {
	log::text("Vk-Hook", "vkBeginCommandBuffer");
	return hook_origTable->vkBeginCommandBuffer(commandBuffer, pBeginInfo);
}

enum VkResult hook_vkEndCommandBuffer(VkCommandBuffer commandBuffer) {
	log::text("Vk-Hook", "vkEndCommandBuffer");
	return hook_origTable->vkEndCommandBuffer(commandBuffer);
}

enum VkResult hook_vkAllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers) {
	log::text("Vk-Hook", "vkAllocateCommandBuffers");
	return hook_origTable->vkAllocateCommandBuffers(device, pAllocateInfo, pCommandBuffers);
}

enum VkResult hook_vkResetCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags) {
	log::text("Vk-Hook", "vkResetCommandPool");
	return hook_origTable->vkResetCommandPool(device, commandPool, flags);
}
#endif

SwapchainSync::~SwapchainSync() { }

bool SwapchainSync::init(Device &dev, uint32_t idx) {
	_index = idx;
	_imageReady = Rc<Semaphore>::create(dev);
	_renderFinished = Rc<Semaphore>::create(dev);
	return true;
}

void SwapchainSync::reset() {
	_imageReady->reset();
	_renderFinished->reset();
}

void SwapchainSync::invalidate() {
	_imageReady->invalidate();
	_imageReady = nullptr;
	_renderFinished->invalidate();
	_renderFinished = nullptr;
}

DeviceQueue::~DeviceQueue() { }

bool DeviceQueue::init(Device &device, VkQueue queue, uint32_t index, QueueOperations ops) {
	_device = &device;
	_queue = queue;
	_index = index;
	_ops = ops;
	return true;
}

Device::Device() { }

Device::~Device() {
	if (_vkInstance && _device) {
		_swapchain->invalidate(*this);
		_swapchain = nullptr;

		clearShaders();
		invalidateObjects();

		_table->vkDestroyDevice(_device, nullptr);
		delete _table;

		_device = nullptr;
		_table = nullptr;
	}
}

bool Device::init(const Rc<Instance> &inst, VkSurfaceKHR surface, DeviceInfo && info,
		const Features &features) {
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
		_families.emplace_back(DeviceQueueFamily({ info.index, count, preferred, info.ops}));
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

	if (!gl::Device::init(inst.get())) {
		return false;
	}

	_vkInstance = inst;
	_surface = surface;
	_info =move(info);

	if constexpr (s_printVkInfo) {
		Application::getInstance()->perform([this, info = _info, instance = _vkInstance] (const Task &) {
			log::vtext("Vk-Info", "Device info:", info.description());
			return true;
		}, nullptr, this);
	}

	for (auto &it : _families) {
		it.queues.resize(it.count, VK_NULL_HANDLE);
		it.pools.reserve(it.count);
		for (size_t i = 0; i < it.count; ++ i) {
			_table->vkGetDeviceQueue(_device, it.index, i, it.queues.data() + i);
			it.pools.emplace_back(Rc<CommandPool>::create(*this, it.index, it.preferred));
		}
	}

	_sems.resize(2);
	_swapchain = Rc<Swapchain>::create(*this);

	return true;
}

VkPhysicalDevice Device::getPhysicalDevice() const {
	return _info.device;
}

const Rc<Swapchain> &Device::getSwapchain() const {
	return _swapchain;
}

bool Device::recreateSwapChain(const Rc<gl::Loop> &loop, thread::TaskQueue &queue, bool resize) {
	return _swapchain->recreateSwapChain(*this, loop, queue, resize);
}

bool Device::createSwapChain(const Rc<gl::Loop> &loop, thread::TaskQueue &queue) {
	auto info = _vkInstance->getSurfaceOptions(_surface, _info.device);
	return _swapchain->createSwapChain(*this, loop, queue, move(info), _swapchain->getPresentMode());
}

void Device::cleanupSwapChain() {
	_swapchain->cleanupSwapChain(*this);
}


Rc<gl::Shader> Device::makeShader(const gl::ProgramData &data) {
	return Rc<Shader>::create(*this, data);
}

Rc<gl::Pipeline> Device::makePipeline(const gl::RenderQueue &queue, const gl::RenderPassData &pass, const gl::PipelineData &params) {
	return Rc<Pipeline>::create(*this, params, pass, queue);
}

Rc<gl::RenderPassImpl> Device::makeRenderPass(gl::RenderPassData &data) {
	return Rc<RenderPassImpl>::create(*this, data);
}

Rc<gl::PipelineLayout> Device::makePipelineLayout(const gl::PipelineLayoutData &data) {
	return Rc<PipelineLayout>::create(*this, data);
}

void Device::begin(Application *app, thread::TaskQueue &q) {

}

void Device::end(thread::TaskQueue &) {
	waitIdle();

	for (auto &it : _families) {
		for (auto &b : it.pools) {
			b->invalidate(*this);
		}
		it.pools.clear();
	}
	cleanupSwapChain();
	_finished = true;

	for (auto &it : _fences) {
		it->invalidate();
	}
	_fences.clear();

	for (auto &it : _sems) {
		for (auto &iit : it) {
			iit->invalidate();
		}
	}
	_sems.clear();
}

void Device::reset(thread::TaskQueue &q) {

}

void Device::waitIdle() {
	for (auto &it : _scheduled) {
		it->check(false);
	}
	_scheduled.clear();
	_table->vkDeviceWaitIdle(_device);
}

void Device::incrementGeneration() {
	gl::Device::incrementGeneration();
	for (auto &f : _families) {
		auto it = f.waiters.begin();
		while (it != f.waiters.end()) {
			auto h = it->handle;
			it->release(*h);
			it = f.waiters.erase(it);
		}
	}
}

void Device::invalidateFrame(gl::FrameHandle &frame) {
	gl::Device::invalidateFrame(frame);
	for (auto &f : _families) {
		auto it = f.waiters.begin();
		while (it != f.waiters.end()) {
			if (it->handle == &frame) {
				it->release(frame);
				it = f.waiters.erase(it);
			} else {
				++ it;
			}
		}
	}
}

bool Device::isBestPresentMode() const {
	return _swapchain->isBestPresentMode();
}

const Rc<gl::RenderQueue> Device::getDefaultRenderQueue() const {
	return _swapchain->getDefaultRenderQueue();
}

bool Device::acquireQueue(QueueOperations ops, gl::FrameHandle &handle, Function<void(gl::FrameHandle &, const Rc<DeviceQueue> &)> && acquire,
		Function<void(gl::FrameHandle &)> && invalidate, Rc<Ref> &&ref) {

	if (std::find(_frames.begin(), _frames.end(), &handle) == _frames.end()) {
		return false;
	}

	auto selectFamily = [&] () -> DeviceQueueFamily * {
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
	};

	auto family = selectFamily();
	if (!family) {
		return false;
	}

	Rc<DeviceQueue> queue;
	if (!family->queues.empty()) {
		queue = Rc<DeviceQueue>::create(*this, family->queues.back(), family->index, family->ops);
		family->queues.pop_back();
	} else {
		family->waiters.emplace_back(DeviceQueueFamily::Waiter(move(acquire), move(invalidate), &handle, move(ref)));
	}

	if (queue) {
		acquire(handle, queue);
	}
	return true;
}

void Device::releaseQueue(Rc<DeviceQueue> &&queue) {
	Rc<gl::FrameHandle> handle;
	Rc<Ref> ref;
	Function<void(gl::FrameHandle &, const Rc<DeviceQueue> &)> cb;
	Function<void(gl::FrameHandle &)> invalidate;

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

	if (family) {
		if (family->waiters.empty()) {
			family->queues.emplace_back(queue->getQueue());
		} else {
			cb = move(family->waiters.front().acquire);
			invalidate = move(family->waiters.front().release);
			ref = move(family->waiters.front().ref);
			handle = move(family->waiters.front().handle);
			family->waiters.erase(family->waiters.begin());
		}
	}

	if (handle && handle->isValid()) {
		if (cb) {
			cb(*handle, queue);
		}
	} else if (invalidate) {
		invalidate(*handle);
	}

	queue = nullptr;
	handle = nullptr;
}

Rc<CommandPool> Device::acquireCommandPool(QueueOperations c, uint32_t) {
	for (auto &it : _families) {
		auto index = it.index;
		if ((it.preferred & c) != QueueOperations::None) {
			if (!it.pools.empty()) {
				auto ret = it.pools.back();
				it.pools.pop_back();
				return ret;
			}
			return Rc<CommandPool>::create(*this, index, c);
		}
	}
	return nullptr;
}

void Device::releaseCommandPool(Rc<CommandPool> &&pool) {
	pool->reset(*this);

	for (auto &it : _families) {
		if ((it.preferred & pool->getClass()) != QueueOperations::None) {
			it.pools.emplace_back(move(pool));
			return;
		}
	}
}

Rc<Fence> Device::acquireFence(uint32_t v) {
	if (!_fences.empty()) {
		auto ret = _fences.back();
		_fences.pop_back();
		ret->setFrame(v);
		return ret;
	}
	auto f = Rc<Fence>::create(*this);
	f->setFrame(v);
	return f;
}

void Device::releaseFence(Rc<Fence> &&ref) {
	ref->reset();
	_fences.emplace_back(move(ref));
}

void Device::scheduleFence(gl::Loop &loop, Rc<Fence> &&fence) {
	if (fence->check()) {
		releaseFence(move(fence));
		return;
	}

	_scheduled.emplace(fence);
	loop.schedule([this, fence = move(fence)] (gl::Loop::Context &) {
		if (_scheduled.find(fence) == _scheduled.end()) {
			return true;
		}
		if (fence->check()) {
			_scheduled.erase(fence);
			releaseFence(Rc<Fence>(fence));
			return true;
		}
		return false;
	});
}

Rc<SwapchainSync> Device::acquireSwapchainSync(uint32_t idx) {
	idx = idx % FramesInFlight;
	auto &v = _sems.at(idx);
	if (!v.empty()) {
		auto ret = v.back();
		v.pop_back();
		return ret;
	}
	return Rc<SwapchainSync>::create(*this, idx);
}

void Device::releaseSwapchainSync(Rc<SwapchainSync> &&ref) {
	if (!_sems.empty()) {
		_sems.at(ref->getIndex() % FramesInFlight).emplace_back(move(ref));
	}
}

Rc<gl::FrameHandle> Device::makeFrame(gl::Loop &loop, gl::RenderQueue &queue, bool readyForSubmit) {
	return Rc<gl::FrameHandle>::create(loop, queue, _order ++, _gen, readyForSubmit);
}

bool Device::setup(const Rc<Instance> &instance, VkPhysicalDevice p, const Properties &prop,
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

	auto table = new DeviceCallTable();
	loadDeviceTable(instance.get(), _device, table);
	_table = table;

#ifdef XL_VK_HOOK_DEBUG
	hook_origTable = new DeviceCallTable(*_table);
	table->vkQueueSubmit = hook_vkQueueSubmit;
	table->vkQueuePresentKHR = hook_vkQueuePresentKHR;
	table->vkAcquireNextImageKHR = hook_vkAcquireNextImageKHR;
	table->vkBeginCommandBuffer = hook_vkBeginCommandBuffer;
	table->vkEndCommandBuffer = hook_vkEndCommandBuffer;
	table->vkAllocateCommandBuffers = hook_vkAllocateCommandBuffers;
	table->vkResetCommandPool = hook_vkResetCommandPool;
#endif

	return true;
}

CommandPool::~CommandPool() {
	if (_commandPool) {
		log::vtext("VK-Error", "CommandPool was not destroyed");
	}
}

bool CommandPool::init(Device &dev, uint32_t familyIdx, QueueOperations c, bool transient) {
	_class = c;
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = familyIdx;
	poolInfo.flags = transient ? VK_COMMAND_POOL_CREATE_TRANSIENT_BIT : 0;

	return dev.getTable()->vkCreateCommandPool(dev.getDevice(), &poolInfo, nullptr, &_commandPool) == VK_SUCCESS;
}

void CommandPool::invalidate(Device &dev) {
	if (_commandPool) {
		dev.getTable()->vkDestroyCommandPool(dev.getDevice(), _commandPool, nullptr);
		_commandPool = VK_NULL_HANDLE;
	} else {
		log::vtext("VK-Error", "CommandPool is not defined");
	}
}

VkCommandBuffer CommandPool::allocBuffer(Device &dev, Level level) {
	if (_commandPool) {
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = _commandPool;
		allocInfo.level = VkCommandBufferLevel(level);
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer ret;
		if (dev.getTable()->vkAllocateCommandBuffers(dev.getDevice(), &allocInfo, &ret) == VK_SUCCESS) {
			return ret;
		}
	}
	return nullptr;
}

Vector<VkCommandBuffer> CommandPool::allocBuffers(Device &dev, uint32_t count, Level level) {
	Vector<VkCommandBuffer> vec;
	if (_commandPool) {
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = _commandPool;
		allocInfo.level = VkCommandBufferLevel(level);
		allocInfo.commandBufferCount = count;

		vec.resize(count);
		if (dev.getTable()->vkAllocateCommandBuffers(dev.getDevice(), &allocInfo, vec.data()) == VK_SUCCESS) {
			return vec;
		}
		vec.clear();
	}
	return vec;
}

void CommandPool::freeDefaultBuffers(Device &dev, Vector<VkCommandBuffer> &vec) {
	if (_commandPool) {
		dev.getTable()->vkFreeCommandBuffers(dev.getDevice(), _commandPool, static_cast<uint32_t>(vec.size()), vec.data());
	}
	vec.clear();
}

void CommandPool::reset(Device &dev, bool release) {
	if (_commandPool) {
		dev.getTable()->vkResetCommandPool(dev.getDevice(), _commandPool, release ? VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT : 0);
	}
}

}
