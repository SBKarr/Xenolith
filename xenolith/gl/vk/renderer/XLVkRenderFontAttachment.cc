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

#include "XLVkRenderFontAttachment.h"
#include "XLVkFrame.h"
#include "XLFontFace.h"
#include "SLFontLibrary.h"

namespace stappler::xenolith::vk {

RenderFontAttachment::~RenderFontAttachment() { }

Rc<gl::AttachmentHandle> RenderFontAttachment::makeFrameHandle(const gl::FrameHandle &handle) {
	return Rc<RenderFontAttachmentHandle>::create(this, handle);
}

RenderFontAttachmentHandle::~RenderFontAttachmentHandle() { }

bool RenderFontAttachmentHandle::setup(gl::FrameHandle &handle) {
	auto dev = (Device *)handle.getDevice();
	_optimalTextureAlignment = dev->getInfo().properties.device10.properties.limits.optimalBufferCopyOffsetAlignment;
	_optimalRowAlignment = dev->getInfo().properties.device10.properties.limits.optimalBufferCopyRowPitchAlignment;
	return true;
}

static Size RenderFontAttachmentHandle_buildTextureData(Vector<VkBufferImageCopy> &requests) {
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
		layoutData.emplace(it, &d);
		totalSquare += d.imageExtent.width * d.imageExtent.height;
	}

	layout::EmplaceCharInterface iface({
		[] (void *ptr) -> uint16_t { return ((VkBufferImageCopy *)ptr)->imageOffset.x; }, // x
		[] (void *ptr) -> uint16_t { return ((VkBufferImageCopy *)ptr)->imageOffset.y; }, // y
		[] (void *ptr) -> uint16_t { return ((VkBufferImageCopy *)ptr)->imageExtent.width; }, // width
		[] (void *ptr) -> uint16_t { return ((VkBufferImageCopy *)ptr)->imageExtent.height; }, // height
		[] (void *ptr, uint16_t value) { ((VkBufferImageCopy *)ptr)->imageOffset.x = value; }, // x
		[] (void *ptr, uint16_t value) { ((VkBufferImageCopy *)ptr)->imageOffset.y = value; }, // y
		[] (void *ptr, uint16_t value) { }, // tex
	});

	auto span = makeSpanView((void **)layoutData.data(), layoutData.size());

	return layout::emplaceChars(iface, span, totalSquare);
}

bool RenderFontAttachmentHandle::submitInput(gl::FrameHandle &handle, Rc<gl::AttachmentInputData> &&data) {
	if (auto d = data.cast<gl::RenderFontInput>()) {
		_counter = d->requests.size();
		_input = d;

		auto &frame = static_cast<FrameHandle &>(handle);
		auto &memPool =  frame.getMemPool();

		_frontBuffer = memPool->spawn(AllocationUsage::HostTransitionSource, gl::BufferInfo(
			gl::ForceBufferUsage(gl::BufferUsage::TransferSrc),
			size_t(Allocator::PageSize * 2)
		));

		size_t totalCount = 0;
		for (auto &it : _input->requests) {
			totalCount += it.chars.size();
		}

		_bufferData.resize(totalCount);

		size_t offset = 0;
		for (auto &it : _input->requests) {
			handle.performInQueue([this, req = &it, target = _bufferData.data() + offset] (gl::FrameHandle &) -> bool {
				writeBufferData(*req, target);
				return true;
			}, [this] (gl::FrameHandle &handle, bool success) {
				if (success) {
					-- _counter;
					if (_counter == 0) {
						writeAtlasData(handle);
					}
				} else {
					handle.invalidate();
				}
			});
			offset += it.chars.size();
		}
		return true;
	}
	return false;
}

void RenderFontAttachmentHandle::writeBufferData(gl::RenderFontInput::FontRequest &req, VkBufferImageCopy *target) {
	for (size_t i = 0; i < req.chars.size(); ++ i) {
		req.face->acquireTexture(req.chars[i], [&] (uint8_t *ptr, uint32_t width, uint32_t rows, int pitch) {
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
				uint32_t(pitch),
				uint32_t(gl::RenderFontInput::getObjectId(req.sourceId, req.chars[i], gl::RenderFontInput::BottomLeft)),
				VkImageSubresourceLayers({VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}),
				VkOffset3D({0, 0, 0}),
				VkExtent3D({width, rows, 1})
			});
			++ target;
		});
	}
}

void RenderFontAttachmentHandle::addBufferData(Vector<VkBufferImageCopy> &&data) {
	_bufferData.insert(std::end(_bufferData), std::begin(data), std::end(data));
}

void RenderFontAttachmentHandle::writeAtlasData(gl::FrameHandle &handle) {
	handle.performInQueue([this] (gl::FrameHandle &) -> bool {

		auto pool = memory::pool::create(memory::pool::acquire());
		memory::pool::push(pool);

		auto size = RenderFontAttachmentHandle_buildTextureData(_bufferData);

		auto atlas = Rc<gl::ImageAtlas>::create(_bufferData.size() * 4);

		for (auto &d : _bufferData) {
			auto id = d.bufferImageHeight;
			d.bufferImageHeight = 0;
			atlas->addObject(gl::RenderFontInput::getObjectId(id, gl::RenderFontInput::BottomLeft),
					Vec2(d.imageOffset.x / size.width, d.imageOffset.y / size.height));
			atlas->addObject(gl::RenderFontInput::getObjectId(id, gl::RenderFontInput::TopLeft),
					Vec2(d.imageOffset.x / size.width, (d.imageOffset.y + d.imageExtent.height) / size.height));
			atlas->addObject(gl::RenderFontInput::getObjectId(id, gl::RenderFontInput::TopRight),
					Vec2((d.imageOffset.x + d.imageExtent.width) / size.width, (d.imageOffset.y + d.imageExtent.height) / size.height));
			atlas->addObject(gl::RenderFontInput::getObjectId(id, gl::RenderFontInput::BottomRight),
					Vec2((d.imageOffset.x + d.imageExtent.width) / size.width, d.imageOffset.y / size.height));
		}

		_image.atlas = atlas;

		memory::pool::pop();
		memory::pool::destroy(pool);

		return true;
	}, [this] (gl::FrameHandle &handle, bool success) {
		if (success) {
			handle.setInputSubmitted(this);
		} else {
			handle.invalidate();
		}
	});
}

uint32_t RenderFontAttachmentHandle::nextBufferOffset(size_t blockSize) {
	auto alignedSize = math::align(blockSize, _optimalTextureAlignment);
	return _bufferOffset.fetch_add(alignedSize);
}

RenderFontRenderPass::~RenderFontRenderPass() { }

bool RenderFontRenderPass::init(StringView name) {
	if (RenderPass::init(name, gl::RenderPassType::Generic, gl::RenderOrderingHighest, 1)) {
		_queueOps = QueueOperations::Transfer;
		return true;
	}
	return false;
}

Rc<gl::RenderPassHandle> RenderFontRenderPass::makeFrameHandle(gl::RenderPassData *data, const gl::FrameHandle &handle) {
	return Rc<RenderFontRenderPassHandle>::create(*this, data, handle);
}

void RenderFontRenderPass::prepare(gl::Device &) {
	for (auto &it : _data->descriptors) {
		if (auto a = dynamic_cast<RenderFontAttachment *>(it->getAttachment())) {
			_fontAttachment = a;
		}
	}
}

RenderFontRenderPassHandle::~RenderFontRenderPassHandle() { }

bool RenderFontRenderPassHandle::init(gl::RenderPass &pass, gl::RenderPassData *data, const gl::FrameHandle &handle) {
	if (!RenderPassHandle::init(pass, data, handle)) {
		return false;
	}

	_queueOps = ((RenderPass *)_renderPass.get())->getQueueOps();

	auto dev = (Device *)handle.getDevice();
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

bool RenderFontRenderPassHandle::prepare(gl::FrameHandle &handle) {
	return false;
}

void RenderFontRenderPassHandle::addRequiredAttachment(const gl::Attachment *a, const Rc<gl::AttachmentHandle> &h) {
	RenderPassHandle::addRequiredAttachment(a, h);
	if (a == ((RenderFontRenderPass *)_renderPass.get())->getRenderFontAttachment()) {
		_fontAttachment = (RenderFontAttachmentHandle *)h.get();
	}
}

Vector<VkCommandBuffer> RenderFontRenderPassHandle::doPrepareCommands(gl::FrameHandle &handle, uint32_t index) {
	return Vector<VkCommandBuffer>();
}

}
