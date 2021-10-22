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
#include "XLVkFrame.h"
#include "XLGlLoop.h"
#include "XLVkTextureSet.h"
#include "XLVkRenderPassImpl.h"
#include "XLVkTransferAttachment.h"
#include "XLVkMaterialCompilationAttachment.h"
#include "XLVkRenderQueueAttachment.h"

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

Device::Device() { }

Device::~Device() {
	if (_vkInstance && _device) {
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
		it.queues.resize(it.count, VK_NULL_HANDLE);
		it.pools.reserve(it.count);
		for (size_t i = 0; i < it.count; ++ i) {
			_table->vkGetDeviceQueue(_device, it.index, i, it.queues.data() + i);
			it.pools.emplace_back(Rc<CommandPool>::create(*this, it.index, it.preferred));
		}
	}

	_allocator = Rc<Allocator>::create(*this, _info.device, _info.features, _info.properties);

	auto imageLimit = _info.properties.device10.properties.limits.maxPerStageDescriptorSampledImages;
	_textureLayoutImagesCount = imageLimit = std::min(imageLimit, config::MaxTextureSetImages);
	_textureSetLayout = Rc<TextureSetLayout>::create(*this, imageLimit);

	_renderQueueCompiler = Rc<RenderQueueCompiler>::create(*this);

	return true;
}

VkPhysicalDevice Device::getPhysicalDevice() const {
	return _info.device;
}

void Device::begin(const Application *app, thread::TaskQueue &q) {
	if (!isSamplersCompiled()) {
		compileSamplers(q, false);
	}

	gl::Device::begin(app, q);

	_materialQueue = createMaterialQueue();
	/*compileRenderQueue(q, _materialQueue, [&] (bool success) {
		_materialQueue->setCompiled(success);
	});*/
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

	_materialRenderPass->clearRequests();
	_materialQueue = nullptr;

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
	_table->vkDeviceWaitIdle(_device);
}

void Device::onLoopStarted(gl::Loop &loop) {
	gl::Device::onLoopStarted(loop);
	_textureSetLayout->initDefault(*this, loop);
}

void Device::onLoopEnded(gl::Loop &loop) {
	gl::Device::onLoopEnded(loop);
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

bool Device::acquireQueue(QueueOperations ops, gl::FrameHandle &handle, Function<void(gl::FrameHandle &, const Rc<DeviceQueue> &)> && acquire,
		Function<void(gl::FrameHandle &)> && invalidate, Rc<Ref> &&ref) {

	auto selectFamily = [&] () -> DeviceQueueFamily * {
		auto ret = getQueueFamily(ops);
		return (DeviceQueueFamily *)ret;
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

bool Device::acquireQueue(QueueOperations ops, gl::Loop &loop, Function<void(gl::Loop &, const Rc<DeviceQueue> &)> && acquire,
		Function<void(gl::Loop &)> && invalidate, Rc<Ref> &&ref) {

	auto selectFamily = [&] () -> DeviceQueueFamily * {
		auto ret = getQueueFamily(ops);
		return (DeviceQueueFamily *)ret;
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
		family->waiters.emplace_back(DeviceQueueFamily::Waiter(move(acquire), move(invalidate), &loop, move(ref)));
	}

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

	if (family->waiters.empty()) {
		family->queues.emplace_back(queue->getQueue());
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
	for (auto &it : _families) {
		auto index = it.index;
		if ((it.preferred & c) != QueueOperations::None) {
			if (!it.pools.empty()) {
				auto ret = it.pools.back();
				it.pools.pop_back();
				return ret;
			}
			return Rc<CommandPool>::create(*this, index, it.ops);
		}
	}
	return nullptr;
}

Rc<CommandPool> Device::acquireCommandPool(uint32_t familyIndex) {
	for (auto &it : _families) {
		if (it.index == familyIndex) {
			if (!it.pools.empty()) {
				auto ret = it.pools.back();
				it.pools.pop_back();
				return ret;
			}
			return Rc<CommandPool>::create(*this, it.index, it.ops);
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

void Device::compileResource(thread::TaskQueue &queue, const Rc<gl::Resource> &req, Function<void(bool)> &&complete) {
	/*if (_started) {
		auto t = Rc<TransferResource>::alloc(move(complete));
		queue.perform(Rc<Task>::create([this, req, t] (const thread::Task &) {
			if (!t->init(_allocator, req) || !t->allocate() || !t->upload()) {
				return false;
			}

			if (!t->isStagingRequired()) {
				Application::getInstance()->performOnMainThread([t] {
					t->compile();
				});
			}
			return true;
		}, [this, t] (const thread::Task &, bool success) {
			if (!t->isStagingRequired()) {
				return;
			}
		}, this));
	} else {
		// single-threaded mode
		auto t = Rc<TransferResource>::create(_allocator, req, move(complete));
		if (t && t->initialize()) {
			if (!t->isStagingRequired()) {
				t->compile();
			} else if (auto q = getQueue(QueueOperations::Transfer)) {
				auto pool = acquireCommandPool(q->getIndex());
				auto fence = acquireFence(0);

				if (t->transfer(q, pool, fence)) {
					releaseQueue(move(q));
					fence->check(false); // wait until operation is completed
					releaseCommandPool(move(pool));
					releaseFence(move(fence));
					t->compile();
				} else {
					releaseQueue(move(q));
					releaseCommandPool(move(pool));
					releaseFence(move(fence));
					t->invalidate(*this);
				}
			}
		}
	}*/
}

void Device::compileRenderQueue(gl::Loop &loop, const Rc<gl::RenderQueue> &req, Function<void(bool)> &&cb) {
	auto h = Rc<FrameHandle>::create(loop, *_renderQueueCompiler, _renderQueueOrder ++, 0);
	h->update(true);
	h->setCompleteCallback([cb] (gl::FrameHandle &handle) {
		cb(handle.isValid());
	});

	auto input = Rc<RenderQueueInput>::alloc();
	input->queue = req;

	h->submitInput(_renderQueueCompiler->getAttachment(), move(input));
}

void Device::compileSamplers(thread::TaskQueue &q, bool force) {
	_immutableSamplers.reserve(_samplersInfo.size());
	_samplers.reserve(_samplersInfo.size());
	_samplersCount = _samplersInfo.size();

	for (auto &it : _samplersInfo) {
		auto ret = new Rc<Sampler>();
		q.perform(Rc<thread::Task>::create([this, ret, v = &it] (const thread::Task &) -> bool {
			*ret = Rc<Sampler>::create(*this, *v);
			return true;
		}, [this, ret] (const thread::Task &, bool) {
			(*ret)->setIndex(_samplers.size());
			_immutableSamplers.emplace_back((*ret)->getSampler());
			_samplers.emplace_back(move(*ret));
			delete ret;
		}));
	}
	if (force) {
		q.waitForAll();
	}
	_samplersCompiled = true;
}

void Device::runMaterialCompilationFrame(gl::Loop &loop, Rc<gl::MaterialInputData> &&req) {
	auto attachment = req->attachment.get();
	auto h = Rc<FrameHandle>::create(loop, *_materialQueue, _materialRenderPass->incrementOrder(), 0);
	h->setCompleteCallback([this, attachment] (gl::FrameHandle &handle) {
		for (auto &it : handle.getOutputAttachments()) {
			if (auto r = dynamic_cast<MaterialCompilationAttachmentHandle *>(it.get())) {
				attachment->setMaterials(r->getOutputSet());
			}
		}
		if (_materialRenderPass->hasRequest(attachment)) {
			if (handle.getLoop()->isRunning()) {
				auto req = _materialRenderPass->popRequest(attachment);
				runMaterialCompilationFrame(*handle.getLoop(), move(req));
			} else {
				_materialRenderPass->clearRequests();
				_materialRenderPass->dropInProgress(attachment);
			}
		} else {
			_materialRenderPass->dropInProgress(attachment);
		}
	});
	h->submitInput(_materialRenderPass->getMaterialAttachment(), move(req));
}

void Device::compileMaterials(gl::Loop &loop, Rc<gl::MaterialInputData> &&req) {
	if (_finished) {
		return;
	}
	if (_materialRenderPass->inProgress(req->attachment)) {
		_materialRenderPass->appendRequest(req->attachment, move(req->materials));
	} else {
		auto attachment = req->attachment.get();
		_materialRenderPass->setInProgress(attachment);
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

	auto table = new DeviceCallTable();
	loadDeviceTable(instance, _device, table);

	if (!table->vkGetImageMemoryRequirements2KHR && table->vkGetImageMemoryRequirements2) {
		table->vkGetImageMemoryRequirements2KHR = table->vkGetImageMemoryRequirements2;
	}
	if (!table->vkGetBufferMemoryRequirements2KHR && table->vkGetBufferMemoryRequirements2) {
		table->vkGetBufferMemoryRequirements2KHR = table->vkGetBufferMemoryRequirements2;
	}
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

Rc<gl::RenderQueue> Device::createTransferQueue() const {
	gl::RenderQueue::Builder builder("Transfer", gl::RenderQueue::RenderOnDemand);

	auto attachment = Rc<TransferAttachment>::create("TransferAttachment");
	auto pass = Rc<TransferRenderPass>::create("TransferRenderPass");

	builder.addRenderPass(pass);
	builder.addPassInput(pass, 0, attachment);
	builder.addPassOutput(pass, 0, attachment);
	builder.addInput(attachment);
	builder.addOutput(attachment);

	return Rc<gl::RenderQueue>::create(move(builder));
}

Rc<gl::RenderQueue> Device::createMaterialQueue() {
	gl::RenderQueue::Builder builder("Material", gl::RenderQueue::RenderOnDemand);

	auto attachment = Rc<MaterialCompilationAttachment>::create("MaterialAttachment");
	auto pass = Rc<MaterialCompilationRenderPass>::create("MaterialRenderPass");

	builder.addRenderPass(pass);
	builder.addPassInput(pass, 0, attachment);
	builder.addPassOutput(pass, 0, attachment);
	builder.addInput(attachment);
	builder.addOutput(attachment);

	_materialRenderPass = pass;

	return Rc<gl::RenderQueue>::create(move(builder));
}

}
