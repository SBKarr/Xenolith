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
#include "XLDeferredManager.h"

namespace stappler::xenolith::vk {

struct RenderFontCharTextureData {
	int16_t x = 0;
	int16_t y = 0;
	uint16_t width = 0;
	uint16_t height = 0;
};

struct RenderFontCharPersistentData {
	RenderFontCharTextureData texture;
	uint32_t objectId;
	uint32_t bufferIdx;
	uint32_t offset;
};

struct RenderFontPersistentBufferUserdata : public Ref {
	Rc<DeviceMemoryPool> mempool;
	Vector<Rc<DeviceBuffer>> buffers;
	std::unordered_map<uint32_t, RenderFontCharPersistentData> chars;
};

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
	const Rc<DeviceBuffer> &getTmpBuffer() const { return _frontBuffer; }
	const Rc<DeviceBuffer> &getPersistentTargetBuffer() const { return _persistentTargetBuffer; }
	const Rc<gl::ImageAtlas> &getAtlas() const { return _atlas; }
	const Rc<RenderFontPersistentBufferUserdata> &getUserdata() const { return _userdata; }
	const Vector<VkBufferImageCopy> &getCopyFromTmpBufferData() const { return _copyFromTmpBufferData; }
	const Map<DeviceBuffer *, Vector<VkBufferImageCopy>> &getCopyFromPersistentBufferData() const { return _copyFromPersistentBufferData; }
	const Vector<VkBufferCopy> &getCopyToPersistentBufferData() const { return _copyToPersistentBufferData; }

protected:
	void doSubmitInput(FrameHandle &, Function<void(bool)> &&cb, Rc<gl::RenderFontInput> &&d);
	void writeAtlasData(FrameHandle &, bool underlinePersistent);

	uint32_t nextBufferOffset(size_t blockSize);
	uint32_t nextPersistentTransferOffset(size_t blockSize);

	bool addPersistentCopy(uint16_t fontId, char16_t c);
	void pushCopyTexture(uint32_t reqIdx, const font::CharTexture &texData);
	void pushAtlasTexture(gl::ImageAtlas *, VkBufferImageCopy &);

	Rc<gl::RenderFontInput> _input;
	Rc<RenderFontPersistentBufferUserdata> _userdata;
	uint32_t _counter = 0;
	VkDeviceSize _bufferSize = 0;
	VkDeviceSize _optimalRowAlignment = 1;
	VkDeviceSize _optimalTextureAlignment = 1;
	std::atomic<uint32_t> _bufferOffset = 0;
	std::atomic<uint32_t> _persistentOffset = 0;
	std::atomic<uint32_t> _copyFromTmpOffset = 0;
	std::atomic<uint32_t> _copyToPersistentOffset = 0;
	std::atomic<uint32_t> _textureTargetOffset = 0;
	Rc<DeviceBuffer> _frontBuffer;
	Rc<DeviceBuffer> _persistentTargetBuffer;
	Rc<gl::ImageAtlas> _atlas;
	Vector<VkBufferImageCopy> _copyFromTmpBufferData;
	Map<DeviceBuffer *, Vector<VkBufferImageCopy>> _copyFromPersistentBufferData;
	Vector<VkBufferCopy> _copyToPersistentBufferData;
	Vector<RenderFontCharPersistentData> _copyPersistentCharData;
	Vector<RenderFontCharTextureData> _textureTarget;
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
	using QueuePass::init;

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
	virtual Vector<const CommandBuffer *> doPrepareCommands(FrameHandle &) override;

	virtual void doSubmitted(FrameHandle &, Function<void(bool)> &&, bool) override;
	virtual void doComplete(FrameQueue &, Function<void(bool)> &&, bool) override;

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

static Extent2 RenderFontAttachmentHandle_buildTextureData(Vector<SpanView<VkBufferImageCopy>> requests) {
	memory::vector<VkBufferImageCopy *> layoutData; layoutData.reserve(requests.size());

	float totalSquare = 0.0f;

	for (auto &v : requests) {
		for (auto &d : v) {
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

void RenderFontAttachmentHandle::submitInput(FrameQueue &q, Rc<gl::AttachmentInputData> &&data, Function<void(bool)> &&cb) {
	auto d = data.cast<gl::RenderFontInput>();
	if (!d || q.isFinalized()) {
		cb(false);
		return;
	}

	q.getFrame()->waitForDependencies(data->waitDependencies, [this, cb = move(cb), d = move(d)] (FrameHandle &handle, bool success) mutable {
		handle.performInQueue([this, cb = move(cb), d = move(d)] (FrameHandle &handle) mutable -> bool {
			doSubmitInput(handle, move(cb), move(d));
			return true;
		}, nullptr, "RenderFontAttachmentHandle::submitInput");
	});
}

void RenderFontAttachmentHandle::doSubmitInput(FrameHandle &handle, Function<void(bool)> &&cb, Rc<gl::RenderFontInput> &&d) {
	_counter = d->requests.size();
	_input = d;
	if (auto instance = d->image->getInstance()) {
		if (auto ud = instance->userdata.cast<RenderFontPersistentBufferUserdata>()) {
			_userdata = ud;
		}
	}

	// process persistent chars
	bool underlinePersistent = false;
	uint32_t totalCount = 0;
	for (auto &it : _input->requests) {
		totalCount += it.chars.size();
	}

	_textureTarget.resize(totalCount + 1); // used in addPersistentCopy

	uint32_t extraPersistent = 0;
	uint32_t processedPersistent = 0;
	if (_userdata) {
		for (auto &it : _input->requests) {
			if (it.persistent) {
				for (auto &c : it.chars) {
					if (addPersistentCopy(it.object->getId(), c)) {
						++ processedPersistent;
						c = 0;
					} else {
						++ extraPersistent;
					}
				}
			}
		}

		if (addPersistentCopy(font::CharLayout::SourceMax, 0)) {
			underlinePersistent = true;
		}
	} else {
		for (auto &it : _input->requests) {
			if (it.persistent) {
				extraPersistent += it.chars.size();
			}
		}

		underlinePersistent = false;
	}

	_onInput = move(cb); // see RenderFontAttachmentHandle::writeAtlasData

	if (processedPersistent == totalCount && underlinePersistent) {
		// no need to transfer extra chars
		writeAtlasData(handle, underlinePersistent);
		return;
	}

	auto frame = static_cast<DeviceFrameHandle *>(&handle);
	auto &memPool = frame->getMemPool(&handle);

	_frontBuffer = memPool->spawn(AllocationUsage::HostTransitionSource, gl::BufferInfo(
		gl::ForceBufferUsage(gl::BufferUsage::TransferSrc),
		size_t(Allocator::PageSize * 2)
	));

	_copyFromTmpBufferData.resize(totalCount - processedPersistent + (underlinePersistent ? 0 : 1));

	if (extraPersistent > 0 || !underlinePersistent) {
		_copyToPersistentBufferData.resize(extraPersistent + (underlinePersistent ? 0 : 1));
		_copyPersistentCharData.resize(extraPersistent + (underlinePersistent ? 0 : 1));

		if (!_userdata) {
			_userdata = Rc<RenderFontPersistentBufferUserdata>::alloc();
			_userdata->mempool = Rc<DeviceMemoryPool>::create(memPool->getAllocator(), false);
			_userdata->buffers.emplace_back(_userdata->mempool->spawn(AllocationUsage::DeviceLocal, gl::BufferInfo(
					gl::ForceBufferUsage(gl::BufferUsage::TransferSrc | gl::BufferUsage::TransferDst),
					size_t(Allocator::PageSize * 2)
				)));
			_persistentTargetBuffer = _userdata->buffers.back();
		} else {
			auto tmp = move(_userdata);
			_userdata = Rc<RenderFontPersistentBufferUserdata>::alloc();
			_userdata->mempool = tmp->mempool;
			_userdata->chars = tmp->chars;
			_userdata->buffers = tmp->buffers;

			if (!_userdata->buffers.empty()) {
				_persistentTargetBuffer = _userdata->buffers.back();
			}
		}
	}

	auto deferred = handle.getLoop()->getApplication()->getDeferredManager();
	deferred->runFontRenderer(_input->library, _input->requests, [this] (uint32_t reqIdx, const font::CharTexture &texData) {
		pushCopyTexture(reqIdx, texData);
	}, [this, handle = Rc<FrameHandle>(&handle), underlinePersistent] {
		writeAtlasData(*handle, underlinePersistent);
		// std::cout << "RenderFontAttachmentHandle::submitInput: " << platform::device::_clock(platform::device::ClockType::Monotonic) - handle->getTimeStart() << "\n";
	});
}

void RenderFontAttachmentHandle::writeAtlasData(FrameHandle &handle, bool underlinePersistent) {
	Vector<SpanView<VkBufferImageCopy>> commands;
	if (!underlinePersistent) {
		// write single white pixel for underlines
		uint32_t offset = _frontBuffer->reserveBlock(1, _optimalTextureAlignment);
		if (offset + 1 <= Allocator::PageSize * 2) {
			uint8_t whiteColor = 255;
			_frontBuffer->setData(BytesView(&whiteColor, 1), offset);
			auto objectId = font::CharLayout::getObjectId(font::CharLayout::SourceMax, char16_t(0), font::FontAnchor::BottomLeft);
			auto texOffset = _textureTargetOffset.fetch_add(1);
			_copyFromTmpBufferData[_copyFromTmpBufferData.size() - 1] = VkBufferImageCopy({
				VkDeviceSize(offset),
				uint32_t(texOffset),
				uint32_t(objectId),
				VkImageSubresourceLayers({VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}),
				VkOffset3D({0, 0, 0}),
				VkExtent3D({1, 1, 1})
			});

			auto targetOffset = _persistentTargetBuffer->reserveBlock(1, _optimalTextureAlignment);
			_textureTarget[texOffset] = RenderFontCharTextureData{0, 0, 1, 1};
			_copyToPersistentBufferData[_copyToPersistentBufferData.size() - 1] = VkBufferCopy({
				offset, targetOffset, 1
			});
			_copyPersistentCharData[_copyPersistentCharData.size() - 1] = RenderFontCharPersistentData{
				RenderFontCharTextureData{0, 0, 1, 1},
				objectId, 0, uint32_t(targetOffset)
			};
		}
	}

	// fill new persistent chars
	for (auto &it : _copyPersistentCharData) {
		it.bufferIdx = _userdata->buffers.size() - 1;
		auto cIt = _userdata->chars.find(it.objectId);
		if (cIt == _userdata->chars.end()) {
			_userdata->chars.emplace(it.objectId, it);
		} else {
			cIt->second = it;
		}
	}

	auto pool = memory::pool::create(memory::pool::acquire());
	memory::pool::push(pool);

	// TODO - use GPU rectangle placement
	commands.emplace_back(_copyFromTmpBufferData);
	for (auto &it : _copyFromPersistentBufferData) {
		commands.emplace_back(it.second);
	}

	_imageExtent = RenderFontAttachmentHandle_buildTextureData(commands);

	auto atlas = Rc<gl::ImageAtlas>::create(_copyFromTmpBufferData.size() * 4, sizeof(font::FontAtlasValue), _imageExtent);

	for (auto &c : commands) {
		for (auto &it : c) {
			pushAtlasTexture(atlas, const_cast<VkBufferImageCopy &>(it));
		}
	}

	_atlas = atlas;

	memory::pool::pop();
	memory::pool::destroy(pool);

	handle.performOnGlThread([this] (FrameHandle &handle) {
		_onInput(true);
		_onInput = nullptr;
	}, this, false, "RenderFontAttachmentHandle::writeAtlasData");
}

uint32_t RenderFontAttachmentHandle::nextBufferOffset(size_t blockSize) {
	auto alignedSize = math::align(uint64_t(blockSize), _optimalTextureAlignment);
	return _bufferOffset.fetch_add(alignedSize);
}

uint32_t RenderFontAttachmentHandle::nextPersistentTransferOffset(size_t blockSize) {
	auto alignedSize = math::align(uint64_t(blockSize), _optimalTextureAlignment);
	return _persistentOffset.fetch_add(alignedSize);
}

bool RenderFontAttachmentHandle::addPersistentCopy(uint16_t fontId, char16_t c) {
	auto objId = font::CharLayout::getObjectId(fontId, c, font::FontAnchor::BottomLeft);
	auto it = _userdata->chars.find(objId);
	if (it != _userdata->chars.end()) {
		auto &buf = _userdata->buffers[it->second.bufferIdx];
		auto bufIt = _copyFromPersistentBufferData.find(buf.get());
		if (bufIt == _copyFromPersistentBufferData.end()) {
			bufIt = _copyFromPersistentBufferData.emplace(buf, Vector<VkBufferImageCopy>()).first;
		}

		auto texTarget = _textureTargetOffset.fetch_add(1);
		bufIt->second.emplace_back(VkBufferImageCopy{
			VkDeviceSize(it->second.offset),
			uint32_t(texTarget),
			uint32_t(objId),
			VkImageSubresourceLayers({VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}),
			VkOffset3D({0, 0, 0}),
			VkExtent3D({it->second.texture.width, it->second.texture.height, 1})
		});

		_textureTarget[texTarget] = it->second.texture;
		return true;
	}
	return false;
}

void RenderFontAttachmentHandle::pushCopyTexture(uint32_t reqIdx, const font::CharTexture &texData) {
	if (texData.width != texData.bitmapWidth || texData.height != texData.bitmapRows) {
		std::cout << "Invalid size: " << texData.width << ";" << texData.height
				<< " vs. " << texData.bitmapWidth << ";" << texData.bitmapRows << "\n";
	}

	auto size = texData.bitmapRows * std::abs(texData.pitch);
	uint32_t offset = _frontBuffer->reserveBlock(size, _optimalTextureAlignment);
	if (offset + size > Allocator::PageSize * 2) {
		return;
	}

	auto ptr = texData.bitmap;
	if (texData.pitch >= 0) {
		_frontBuffer->setData(BytesView(ptr, texData.pitch * texData.bitmapRows), offset);
	} else {
		for (size_t i = 0; i < texData.bitmapRows; ++ i) {
			_frontBuffer->setData(BytesView(ptr, -texData.pitch), offset + i * (-texData.pitch));
			ptr += texData.pitch;
		}
	}

	auto objectId = font::CharLayout::getObjectId(texData.fontID, texData.charID, font::FontAnchor::BottomLeft);
	auto texOffset = _textureTargetOffset.fetch_add(1);
	_copyFromTmpBufferData[_copyFromTmpOffset.fetch_add(1)] = VkBufferImageCopy({
		VkDeviceSize(offset),
		uint32_t(texOffset),
		uint32_t(objectId),
		VkImageSubresourceLayers({VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}),
		VkOffset3D({0, 0, 0}),
		VkExtent3D({texData.bitmapWidth, texData.bitmapRows, 1})
	});
	_textureTarget[texOffset] = RenderFontCharTextureData{texData.x, texData.y, texData.width, texData.height};

	if (_input->requests[reqIdx].persistent) {
		auto targetIdx = _copyToPersistentOffset.fetch_add(1);
		auto targetOffset = _persistentTargetBuffer->reserveBlock(size, _optimalTextureAlignment);
		_copyToPersistentBufferData[targetIdx] = VkBufferCopy({
			offset, targetOffset, size
		});
		_copyPersistentCharData[targetIdx] = RenderFontCharPersistentData{
			RenderFontCharTextureData{texData.x, texData.y, texData.width, texData.height},
			objectId, 0, uint32_t(targetOffset)
		};
	}
}

void RenderFontAttachmentHandle::pushAtlasTexture(gl::ImageAtlas *atlas, VkBufferImageCopy &d) {
	font::FontAtlasValue data[4];

	auto texOffset = d.bufferRowLength;
	auto id = d.bufferImageHeight;
	d.bufferImageHeight = 0;
	d.bufferRowLength = 0;

	auto &tex = _textureTarget[texOffset];

	const float x = float(d.imageOffset.x);
	const float y = float(d.imageOffset.y);
	const float w = float(d.imageExtent.width);
	const float h = float(d.imageExtent.height);

	data[0].pos = Vec2(tex.x, tex.y);
	data[0].tex = Vec2(x / _imageExtent.width, y / _imageExtent.height);

	data[1].pos = Vec2(tex.x, tex.y + tex.height);
	data[1].tex = Vec2(x / _imageExtent.width, (y + h) / _imageExtent.height);

	data[2].pos = Vec2(tex.x + tex.width, tex.y + tex.height);
	data[2].tex = Vec2((x + w) / _imageExtent.width, (y + h) / _imageExtent.height);

	data[3].pos = Vec2(tex.x + tex.width, tex.y);
	data[3].tex = Vec2((x + w) / _imageExtent.width, y / _imageExtent.height);

	atlas->addObject(font::CharLayout::getObjectId(id, font::FontAnchor::BottomLeft), &data[0]);
	atlas->addObject(font::CharLayout::getObjectId(id, font::FontAnchor::TopLeft), &data[1]);
	atlas->addObject(font::CharLayout::getObjectId(id, font::FontAnchor::TopRight), &data[2]);
	atlas->addObject(font::CharLayout::getObjectId(id, font::FontAnchor::BottomRight), &data[3]);
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
	for (auto &it : _data->passDescriptors) {
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
}

Vector<const CommandBuffer *> RenderFontRenderPassHandle::doPrepareCommands(FrameHandle &handle) {
	Vector<VkCommandBuffer> ret;

	auto &input = _fontAttachment->getInput();
	auto &copyFromTmp = _fontAttachment->getCopyFromTmpBufferData();
	auto &copyFromPersistent = _fontAttachment->getCopyFromPersistentBufferData();
	auto &copyToPersistent = _fontAttachment->getCopyToPersistentBufferData();

	auto &masterImage = input->image;
	auto instance = masterImage->getInstance();
	if (!instance) {
		return Vector<const CommandBuffer *>();
	}

	gl::ImageInfo info = masterImage->getInfo();

	info.format = gl::ImageFormat::R8_UNORM;
	info.extent = _fontAttachment->getImageExtent();

	_targetImage = _device->getAllocator()->spawnPersistent(AllocationUsage::DeviceLocal, info, false, instance->data.image->getIndex());

	auto buf = _pool->recordBuffer(*_device, [&] (CommandBuffer &buf) {

		Vector<BufferMemoryBarrier> persistentBarriers;
		for (auto &it : copyFromPersistent) {
			if (auto b = it.first->getPendingBarrier()) {
				persistentBarriers.emplace_back(*b);
				it.first->dropPendingBarrier();
			}
		}

		ImageMemoryBarrier inputBarrier(_targetImage,
			0, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				persistentBarriers, makeSpanView(&inputBarrier, 1));

		// copy from temporary buffer
		if (!copyFromTmp.empty()) {
			buf.cmdCopyBufferToImage(_fontAttachment->getTmpBuffer(), _targetImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copyFromTmp);
		}

		// copy from persistent buffers
		for (auto &it : copyFromPersistent) {
			buf.cmdCopyBufferToImage(it.first, _targetImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, it.second);
		}

		if (!copyToPersistent.empty()) {
			buf.cmdCopyBuffer(_fontAttachment->getTmpBuffer(), _fontAttachment->getPersistentTargetBuffer(), copyToPersistent);
			_fontAttachment->getPersistentTargetBuffer()->setPendingBarrier(BufferMemoryBarrier(_fontAttachment->getPersistentTargetBuffer(),
				VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
				QueueFamilyTransfer(), 0, _fontAttachment->getPersistentTargetBuffer()->getReservedSize()
			));
		}

		auto sourceLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		if (auto q = _device->getQueueFamily(getQueueOperations(info.type))) {
			uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			if (q->index != _pool->getFamilyIdx()) {
				srcQueueFamilyIndex = _pool->getFamilyIdx();
				dstQueueFamilyIndex = q->index;
			}

			if (input->output) {
				auto extent = _targetImage->getInfo().extent;
				auto &frame = static_cast<DeviceFrameHandle &>(handle);
				auto memPool = frame.getMemPool(&handle);

				_outBuffer = memPool->spawn(AllocationUsage::HostTransitionDestination, gl::BufferInfo(
					gl::ForceBufferUsage(gl::BufferUsage::TransferDst),
					size_t(extent.width * extent.height * extent.depth),
					gl::RenderPassType::Transfer
				));

				ImageMemoryBarrier reverseBarrier(_targetImage,
					VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
					sourceLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
				buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, makeSpanView(&reverseBarrier, 1));

				sourceLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				buf.cmdCopyImageToBuffer(_targetImage, sourceLayout, _outBuffer, 0);

				BufferMemoryBarrier bufferOutBarrier(_outBuffer,
					VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);

				buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, makeSpanView(&bufferOutBarrier, 1));
			}

			ImageMemoryBarrier outputBarrier(_targetImage,
				VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				sourceLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				QueueFamilyTransfer{srcQueueFamilyIndex, dstQueueFamilyIndex});

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

			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, targetOps, 0, makeSpanView(&outputBarrier, 1));
		}
		return true;
	});

	return Vector<const CommandBuffer *>{buf};
}

void RenderFontRenderPassHandle::doSubmitted(FrameHandle &frame, Function<void(bool)> &&func, bool success) {
	if (success) {
		auto &input = _fontAttachment->getInput();
		input->image->updateInstance(*frame.getLoop(), _targetImage, Rc<gl::ImageAtlas>(_fontAttachment->getAtlas()),
				Rc<Ref>(_fontAttachment->getUserdata()), frame.getSignalDependencies());

		if (input->output) {
			auto region = _outBuffer->map(0, _outBuffer->getSize(), true);
			input->output(_targetImage->getInfo(), BytesView(region.ptr, region.size));
			_outBuffer->unmap(region);
		}
	}

	QueuePassHandle::doSubmitted(frame, move(func), success);
	frame.signalDependencies(success);
}

void RenderFontRenderPassHandle::doComplete(FrameQueue &queue, Function<void(bool)> &&func, bool success) {
	QueuePassHandle::doComplete(queue, move(func), success);
}

}
