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
		it.queues.resize(it.count, VK_NULL_HANDLE);
		it.pools.reserve(it.count);
		for (size_t i = 0; i < it.count; ++ i) {
			getTable()->vkGetDeviceQueue(_device, it.index, i, it.queues.data() + i);
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

const DeviceTable * Device::getTable() const {
#if VK_HOOK_DEBUG
	setDeviceHookThreadContext([] (void *, const char *name, PFN_vkVoidFunction) {
		log::text("Vk-Call", name);
	}, nullptr, _original, nullptr, (void *)this);
#endif
	return _table;
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

void Device::releaseCommandPool(gl::Loop &loop, Rc<CommandPool> &&pool) {
	auto refId = retain();
	loop.getQueue()->perform(Rc<Task>::create([this, pool] (const Task &) -> bool {
		pool->reset(*this);
		return true;
	}, [this, pool, refId] (const Task &, bool success) {
		if (success) {
			_families[pool->getFamilyIdx()].pools.emplace_back(Rc<CommandPool>(pool));
		}
		release(refId);
	}));
}

void Device::releaseCommandPoolUnsafe(Rc<CommandPool> &&pool) {
	pool->reset(*this);

	_families[pool->getFamilyIdx()].pools.emplace_back(Rc<CommandPool>(pool));
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

const Vector<gl::ImageFormat> &Device::getSupportedDepthStencilFormat() const {
	return _depthFormats;
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
	ret->extent = imageInfo.extent = extent;

	auto img = _allocator->spawnPersistent(AllocationUsage::DeviceLocal, imageInfo, false);

	ret->image = img.get();

	for (auto &desc : attachment->getDescriptors()) {
		auto v = Rc<ImageView>::create(*this, *(gl::ImageAttachmentDescriptor *)desc.get(), img);
		ret->views.emplace(desc->getRenderPass(), move(v));
	}

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
			if (_samplers.size() == _samplersCount) {
				_samplersCompiled = true;
			}
			delete ret;
		}));
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
