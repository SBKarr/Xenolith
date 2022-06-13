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

#include "XLVkRenderFontQueue.h"
#include "XLFontFace.h"

namespace stappler::xenolith::vk {

class RenderFontAttachment : public renderqueue::GenericAttachment {
public:
	virtual ~RenderFontAttachment();

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};

class RenderFontAttachmentHandle : public renderqueue::AttachmentHandle {
public:
	virtual ~RenderFontAttachmentHandle();

	virtual bool setup(FrameQueue &, Function<void(bool)> &&) override;
	virtual void submitInput(FrameQueue &, Rc<gl::AttachmentInputData> &&, Function<void(bool)> &&) override;

	Extent2 getImageExtent() const { return _imageExtent; }
	const Rc<gl::RenderFontInput> &getInput() const { return _input; }
	const Rc<DeviceBuffer> &getBuffer() const { return _frontBuffer; }
	const Rc<gl::ImageAtlas> &getAtlas() const { return _atlas; }
	const Vector<VkBufferImageCopy> &getBufferData() const { return _bufferData; }

protected:
	void writeBufferData(gl::RenderFontInput::FontRequest &, VkBufferImageCopy *target);
	void writeAtlasData(FrameHandle &);

	uint32_t nextBufferOffset(size_t blockSize);

	Rc<gl::RenderFontInput> _input;
	uint32_t _counter = 0;
	VkDeviceSize _bufferSize = 0;
	VkDeviceSize _optimalRowAlignment = 1;
	VkDeviceSize _optimalTextureAlignment = 1;
	std::atomic<size_t> _bufferOffset = 0;
	Rc<DeviceBuffer> _frontBuffer;
	Rc<gl::ImageAtlas> _atlas;
	Vector<VkBufferImageCopy> _bufferData;
	Extent2 _imageExtent;
	Mutex _mutex;
	Function<void(bool)> _onInput;
};

class RenderFontRenderPass : public QueuePass {
public:
	virtual ~RenderFontRenderPass();

	virtual bool init(StringView);

	virtual Rc<PassHandle> makeFrameHandle(const FrameQueue &) override;

	const RenderFontAttachment *getRenderFontAttachment() const {
		return _fontAttachment;
	}

protected:
	virtual void prepare(gl::Device &) override;

	const RenderFontAttachment *_fontAttachment;
};

class RenderFontRenderPassHandle : public QueuePassHandle {
public:
	virtual ~RenderFontRenderPassHandle();

	virtual bool init(Pass &, const FrameQueue &) override;

	virtual QueueOperations getQueueOps() const override;

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;
	virtual void finalize(FrameQueue &, bool successful)  override;

protected:
	virtual Vector<VkCommandBuffer> doPrepareCommands(FrameHandle &) override;

	RenderFontAttachmentHandle *_fontAttachment = nullptr;
	QueueOperations _queueOps = QueueOperations::None;
	Rc<Image> _targetImage;
	Rc<DeviceBuffer> _outBuffer;
};

RenderFontQueue::~RenderFontQueue() { }

bool RenderFontQueue::init(StringView name, Function<void(FrameQueue &, const Rc<AttachmentHandle> &, Function<void(bool)> &&)> &&input) {
	Queue::Builder builder(name);

	auto attachment = Rc<RenderFontAttachment>::create("FontAttachment");
	auto pass = Rc<RenderFontRenderPass>::create("FontRenderPass");

	if (input) {
		attachment->setInputCallback(move(input));
	}

	builder.addRenderPass(pass);
	builder.addPassInput(pass, 0, attachment, renderqueue::AttachmentDependencyInfo());
	builder.addPassOutput(pass, 0, attachment, renderqueue::AttachmentDependencyInfo());
	builder.addInput(attachment);
	builder.addOutput(attachment);

	if (Queue::init(move(builder))) {
		_attachment = attachment.get();
		return true;
	}
	return false;
}

RenderFontAttachment::~RenderFontAttachment() { }

auto RenderFontAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<RenderFontAttachmentHandle>::create(this, handle);
}

RenderFontAttachmentHandle::~RenderFontAttachmentHandle() { }

bool RenderFontAttachmentHandle::setup(FrameQueue &handle, Function<void(bool)> &&) {
	auto dev = (Device *)handle.getFrame()->getDevice();
	_optimalTextureAlignment = std::max(dev->getInfo().properties.device10.properties.limits.optimalBufferCopyOffsetAlignment, VkDeviceSize(4));
	_optimalRowAlignment = std::max(dev->getInfo().properties.device10.properties.limits.optimalBufferCopyRowPitchAlignment, VkDeviceSize(4));
	return true;
}

static Extent2 RenderFontAttachmentHandle_buildTextureData(SpanView<VkBufferImageCopy> requests) {
	memory::vector<VkBufferImageCopy *> layoutData; layoutData.reserve(requests.size());

	float totalSquare = 0.0f;

	for (auto &d : requests) {
		auto it = std::lower_bound(layoutData.begin(), layoutData.end(), &d,
				[] (const VkBufferImageCopy * l, const VkBufferImageCopy * r) -> bool {
			if (l->imageExtent.height == r->imageExtent.height && l->imageExtent.width == r->imageExtent.width) {
				return l->bufferImageHeight < r->bufferImageHeight;
			} else if (l->imageExtent.height == r->imageExtent.height) {
				return l->imageExtent.width > r->imageExtent.width;
			} else {
				return l->imageExtent.height > r->imageExtent.height;
			}
		});
		layoutData.emplace(it, (VkBufferImageCopy *)&d);
		totalSquare += d.imageExtent.width * d.imageExtent.height;
	}

	font::EmplaceCharInterface iface({
		[] (void *ptr) -> uint16_t { return ((VkBufferImageCopy *)ptr)->imageOffset.x; }, // x
		[] (void *ptr) -> uint16_t { return ((VkBufferImageCopy *)ptr)->imageOffset.y; }, // y
		[] (void *ptr) -> uint16_t { return ((VkBufferImageCopy *)ptr)->imageExtent.width; }, // width
		[] (void *ptr) -> uint16_t { return ((VkBufferImageCopy *)ptr)->imageExtent.height; }, // height
		[] (void *ptr, uint16_t value) { ((VkBufferImageCopy *)ptr)->imageOffset.x = value; }, // x
		[] (void *ptr, uint16_t value) { ((VkBufferImageCopy *)ptr)->imageOffset.y = value; }, // y
		[] (void *ptr, uint16_t value) { }, // tex
	});

	auto span = makeSpanView((void **)layoutData.data(), layoutData.size());

	return font::emplaceChars(iface, span, totalSquare);
}

void RenderFontAttachmentHandle::submitInput(FrameQueue &handle, Rc<gl::AttachmentInputData> &&data, Function<void(bool)> &&cb) {
	if (auto d = data.cast<gl::RenderFontInput>()) {
		_counter = d->requests.size();
		_input = d;

		auto frame = static_cast<DeviceFrameHandle *>(handle.getFrame().get());
		auto &memPool =  frame->getMemPool();

		_frontBuffer = memPool->spawn(AllocationUsage::HostTransitionSource, gl::BufferInfo(
			gl::ForceBufferUsage(gl::BufferUsage::TransferSrc),
			size_t(Allocator::PageSize * 2)
		));

		size_t totalCount = 0;
		for (auto &it : _input->requests) {
			totalCount += it.second.size();
		}

		_bufferData.resize(totalCount + 1);

		size_t offset = 0;
		_onInput = move(cb);
		for (auto &it : _input->requests) {
			handle.getFrame()->performInQueue([this, req = &it, target = _bufferData.data() + offset] (FrameHandle &) -> bool {
				writeBufferData(*req, target);
				return true;
			}, [this] (FrameHandle &handle, bool success) {
				if (success) {
					-- _counter;
					if (_counter == 0) {
						writeAtlasData(handle);
					}
				} else {
					handle.invalidate();
				}
			}, nullptr, "RenderFontAttachmentHandle::submitInput");
			offset += it.second.size();
		}
	} else {
		cb(false);
	}
}

void RenderFontAttachmentHandle::writeBufferData(gl::RenderFontInput::FontRequest &req, VkBufferImageCopy *target) {
	for (size_t i = 0; i < req.second.size(); ++ i) {
		req.first->acquireTexture(req.second[i], [&] (uint8_t *ptr, uint32_t width, uint32_t rows, int pitch) {
			auto size = rows * std::abs(pitch);
			uint32_t offset = nextBufferOffset(rows * std::abs(pitch));
			if (offset + size > Allocator::PageSize * 2) {
				return;
			}

			if (pitch == 0) {
				pitch = width;
			}

			if (pitch >= 0) {
				_frontBuffer->setData(BytesView(ptr, pitch * rows), offset);
			} else {
				for (size_t i = 0; i < rows; ++ i) {
					_frontBuffer->setData(BytesView(ptr, -pitch), offset + i * (-pitch));
					ptr += pitch;
				}
			}

			*target = VkBufferImageCopy({
				VkDeviceSize(offset),
				uint32_t(/*pitch*/0),
				uint32_t(font::CharLayout::getObjectId(req.first->getId(), req.second[i], font::FontAnchor::BottomLeft)),
				VkImageSubresourceLayers({VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}),
				VkOffset3D({0, 0, 0}),
				VkExtent3D({width, rows, 1})
			});
			++ target;
		});
	}
}

void RenderFontAttachmentHandle::writeAtlasData(FrameHandle &handle) {
	handle.performInQueue([this] (FrameHandle &) -> bool {
		// write single white pixel for underlines
		uint32_t offset = nextBufferOffset(1);
		if (offset + 1 <= Allocator::PageSize * 2) {
			uint8_t whiteColor = 255;
			_frontBuffer->setData(BytesView(&whiteColor, 1), offset);
			_bufferData[_bufferData.size() - 1] = VkBufferImageCopy({
				VkDeviceSize(offset),
				uint32_t(0),
				uint32_t( font::CharLayout::getObjectId(0, char16_t(0), font::FontAnchor::BottomLeft)),
				VkImageSubresourceLayers({VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}),
				VkOffset3D({0, 0, 0}),
				VkExtent3D({1, 1, 1})
			});
		}

		auto pool = memory::pool::create(memory::pool::acquire());
		memory::pool::push(pool);

		// TODO - use GPU rectangle placement
		_imageExtent = RenderFontAttachmentHandle_buildTextureData(SpanView<VkBufferImageCopy>(_bufferData.data(), _bufferData.size() - 1));

		_bufferData[_bufferData.size() - 1].imageOffset =
				VkOffset3D({int32_t(_imageExtent.width - 1), int32_t(_imageExtent.height - 1), 0});

		auto atlas = Rc<gl::ImageAtlas>::create(_bufferData.size() * 4);

		for (auto &d : _bufferData) {
			auto id = d.bufferImageHeight;
			d.bufferImageHeight = 0;

			const float x = float(d.imageOffset.x);
			const float y = float(d.imageOffset.y);
			const float w = float(d.imageExtent.width);
			const float h = float(d.imageExtent.height);

			atlas->addObject(font::CharLayout::getObjectId(id, font::FontAnchor::BottomLeft),
					Vec2(x / _imageExtent.width, y / _imageExtent.height));
			atlas->addObject(font::CharLayout::getObjectId(id, font::FontAnchor::TopLeft),
					Vec2(x / _imageExtent.width, (y + h) / _imageExtent.height));
			atlas->addObject(font::CharLayout::getObjectId(id, font::FontAnchor::TopRight),
					Vec2((x + w) / _imageExtent.width, (y + h) / _imageExtent.height));
			atlas->addObject(font::CharLayout::getObjectId(id, font::FontAnchor::BottomRight),
					Vec2((x + w) / _imageExtent.width, y / _imageExtent.height));
		}

		_atlas = atlas;

		memory::pool::pop();
		memory::pool::destroy(pool);

		return true;
	}, [this] (FrameHandle &handle, bool success) {
		_onInput(success);
		_onInput = nullptr;
	}, nullptr, "RenderFontAttachmentHandle::writeAtlasData");
}

uint32_t RenderFontAttachmentHandle::nextBufferOffset(size_t blockSize) {
	auto alignedSize = math::align(blockSize, _optimalTextureAlignment);
	return _bufferOffset.fetch_add(alignedSize);
}

RenderFontRenderPass::~RenderFontRenderPass() { }

bool RenderFontRenderPass::init(StringView name) {
	if (QueuePass::init(name, PassType::Generic, renderqueue::RenderOrderingHighest, 1)) {
		_queueOps = QueueOperations::Transfer;
		return true;
	}
	return false;
}

auto RenderFontRenderPass::makeFrameHandle(const FrameQueue &handle) -> Rc<PassHandle> {
	return Rc<RenderFontRenderPassHandle>::create(*this, handle);
}

void RenderFontRenderPass::prepare(gl::Device &) {
	for (auto &it : _data->descriptors) {
		if (auto a = dynamic_cast<RenderFontAttachment *>(it->getAttachment())) {
			_fontAttachment = a;
		}
	}
}

RenderFontRenderPassHandle::~RenderFontRenderPassHandle() { }

bool RenderFontRenderPassHandle::init(Pass &pass, const FrameQueue &handle) {
	if (!QueuePassHandle::init(pass, handle)) {
		return false;
	}

	_queueOps = ((QueuePass *)_renderPass.get())->getQueueOps();

	auto dev = (Device *)handle.getFrame()->getDevice();
	auto q = dev->getQueueFamily(_queueOps);
	if (q->transferGranularity.width > 1 || q->transferGranularity.height > 1) {
		_queueOps = QueueOperations::Graphics;
		for (auto &it : dev->getQueueFamilies()) {
			if (it.index != q->index) {
				switch (it.preferred) {
				case QueueOperations::Compute:
				case QueueOperations::Transfer:
				case QueueOperations::Graphics:
					if ((it.transferGranularity.width == 1 || it.transferGranularity.height == 1) && toInt(_queueOps) < toInt(it.preferred)) {
						_queueOps = it.preferred;
					}
					break;
				default:
					break;
				}
			}
		}
	}

	return true;
}

QueueOperations RenderFontRenderPassHandle::getQueueOps() const {
	return _queueOps;
}


bool RenderFontRenderPassHandle::prepare(FrameQueue &handle, Function<void(bool)> &&cb) {
	if (auto a = handle.getAttachment(((RenderFontRenderPass *)_renderPass.get())->getRenderFontAttachment())) {
		_fontAttachment = (RenderFontAttachmentHandle *)a->handle.get();
	}
	return QueuePassHandle::prepare(handle, move(cb));
}

void RenderFontRenderPassHandle::finalize(FrameQueue &handle, bool successful) {
	QueuePassHandle::finalize(handle, successful);

	if (!successful) {
		return;
	}

	auto &input = _fontAttachment->getInput();
	input->image->updateInstance(*handle.getLoop(), _targetImage, Rc<gl::ImageAtlas>(_fontAttachment->getAtlas()));

	if (input->output) {
		auto region = _outBuffer->map(0, _outBuffer->getSize(), true);
		input->output(_targetImage->getInfo(), BytesView(region.ptr, region.size));
		_outBuffer->unmap(region);
	}
}

Vector<VkCommandBuffer> RenderFontRenderPassHandle::doPrepareCommands(FrameHandle &handle) {
	Vector<VkCommandBuffer> ret;

	auto buf = _pool->allocBuffer(*_device);
	auto table = _device->getTable();
	auto dev = (Device *)handle.getDevice();

	auto &input = _fontAttachment->getInput();
	auto &bufferData = _fontAttachment->getBufferData();

	auto &masterImage = input->image;
	auto instance = masterImage->getInstance();
	if (!instance) {
		return Vector<VkCommandBuffer>();
	}

	gl::ImageInfo info = masterImage->getInfo();

	info.format = gl::ImageFormat::R8_UNORM;
	info.extent = _fontAttachment->getImageExtent();

	_targetImage = dev->getAllocator()->spawnPersistent(AllocationUsage::DeviceLocal, info, false, instance->data.image->getIndex());

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	table->vkBeginCommandBuffer(buf, &beginInfo);

	VkImageMemoryBarrier inputBarrier({
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
		0, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		_targetImage->getImage(), VkImageSubresourceRange({
			VK_IMAGE_ASPECT_COLOR_BIT,
			0, 1, 0, 1
		})
	});

	table->vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &inputBarrier);

	table->vkCmdCopyBufferToImage(buf, _fontAttachment->getBuffer()->getBuffer(), _targetImage->getImage(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, bufferData.size(), bufferData.data());

	auto sourceLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	if (auto q = dev->getQueueFamily(getQueueOperations(info.type))) {
		uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		if (q->index != _pool->getFamilyIdx()) {
			srcQueueFamilyIndex = _pool->getFamilyIdx();
			dstQueueFamilyIndex = q->index;
		}

		if (input->output) {
			auto extent = _targetImage->getInfo().extent;
			auto &frame = static_cast<DeviceFrameHandle &>(handle);
			auto &memPool =  frame.getMemPool();

			_outBuffer = memPool->spawn(AllocationUsage::HostTransitionDestination, gl::BufferInfo(
				gl::ForceBufferUsage(gl::BufferUsage::TransferDst),
				size_t(extent.width * extent.height * extent.depth),
				gl::RenderPassType::Transfer
			));

			VkImageMemoryBarrier reverseBarrier({
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
				VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
				sourceLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				_targetImage->getImage(), VkImageSubresourceRange({
					VK_IMAGE_ASPECT_COLOR_BIT,
					0, 1, 0, 1
				})
			});

			table->vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
					0, nullptr,
					0, nullptr,
					1, &reverseBarrier);

			sourceLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

			VkBufferImageCopy reverseRegion({
				VkDeviceSize(0), uint32_t(0), uint32_t(0),
				VkImageSubresourceLayers({
					VK_IMAGE_ASPECT_COLOR_BIT,
					0, 0, 1
				}),
				VkOffset3D({0, 0, 0}),
				VkExtent3D({extent.width, extent.height, extent.depth})
			});

			table->vkCmdCopyImageToBuffer(buf, _targetImage->getImage(), sourceLayout, _outBuffer->getBuffer(), 1, &reverseRegion);

			VkBufferMemoryBarrier bufferOutBarrier({
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
				VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				_outBuffer->getBuffer(), 0, VK_WHOLE_SIZE
			});

			table->vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
					0, nullptr,
					1, &bufferOutBarrier,
					0, nullptr);
		}

		VkImageMemoryBarrier outputBarrier({
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			sourceLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			srcQueueFamilyIndex, dstQueueFamilyIndex,
			_targetImage->getImage(), VkImageSubresourceRange({
				VK_IMAGE_ASPECT_COLOR_BIT,
				0, 1, 0, 1
			})
		});

		if (q->index != _pool->getFamilyIdx()) {
			_targetImage->setPendingBarrier(outputBarrier);
		}

		VkPipelineStageFlags targetOps = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
				| VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

		auto ops = getQueueOps();
		switch (ops) {
		case QueueOperations::Transfer:
			targetOps = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			break;
		case QueueOperations::Compute:
			targetOps = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			break;
		default:
			break;
		}

		table->vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_TRANSFER_BIT,
			targetOps, 0,
			0, nullptr,
			0, nullptr,
			1, &outputBarrier);
	}

	if (table->vkEndCommandBuffer(buf) != VK_SUCCESS) {
		return Vector<VkCommandBuffer>();
	}

	ret.emplace_back(buf);
	return ret;
}

}
