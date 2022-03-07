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

#include "XLGlAttachment.h"

namespace stappler::xenolith::gl {

bool Attachment::init(StringView name, AttachmentType type) {
	_name = name.str();
	_type = type;
	return true;
}

void Attachment::clear() {

}

void Attachment::addUsage(AttachmentUsage usage, AttachmentOps ops) {
	_usage |= usage;
	_ops |= ops;
}

void Attachment::setInputCallback(Function<void(FrameQueue &, const Rc<AttachmentHandle> &, Function<void(bool)> &&)> &&input) {
	_inputCallback = move(input);
}

void Attachment::acquireInput(FrameQueue &frame, const Rc<AttachmentHandle> &a, Function<void(bool)> &&cb) {
	if (_inputCallback) {
		_inputCallback(frame, a, move(cb));
	} else {
		log::vtext("Attachment", "Input callback for attachment is not defined: ", getName());
	}
}

AttachmentDescriptor *Attachment::addDescriptor(RenderPassData *data) {
	for (auto &it : _descriptors) {
		if (it->getRenderPass() == data) {
			return it;
		}
	}

	if (auto d = makeDescriptor(data)) {
		return _descriptors.emplace_back(d);
	}

	return nullptr;
}

void Attachment::sortDescriptors(RenderQueue &queue, Device &dev) {
	Set<uint32_t> priorities;

	for (auto &it : _descriptors) {
		auto pass = it->getRenderPass();
		auto iit = priorities.find(pass->ordering.get());
		if (iit == priorities.end()) {
			priorities.emplace(pass->ordering.get());
		} else {
			log::vtext("Gl-Error", "Duplicate render pass priority '", pass->ordering.get(),
				"' for attachment '", getName(), "', render ordering can be invalid");
		}
	}

	std::sort(_descriptors.begin(), _descriptors.end(),
			[&] (const Rc<AttachmentDescriptor> &l, const Rc<AttachmentDescriptor> &r) {
		return l->getRenderPass()->ordering < r->getRenderPass()->ordering;
	});

	for (auto &it : _descriptors) {
		it->sortRefs(queue, dev);
	}
}

Rc<AttachmentHandle> Attachment::makeFrameHandle(const FrameQueue &) {
	return nullptr;
}

Vector<RenderPassData *> Attachment::getRenderPasses() const {
	Vector<RenderPassData *> ret;
	for (auto &it : _descriptors) {
		ret.emplace_back(it->getRenderPass());
	}
	return ret;
}

RenderPassData *Attachment::getFirstRenderPass() const {
	if (_descriptors.empty()) {
		return nullptr;
	}

	return _descriptors.front()->getRenderPass();
}

RenderPassData *Attachment::getLastRenderPass() const {
	if (_descriptors.empty()) {
		return nullptr;
	}

	return _descriptors.back()->getRenderPass();
}

RenderPassData *Attachment::getNextRenderPass(RenderPassData *pass) const {
	size_t idx = 0;
	for (auto &it : _descriptors) {
		if (it->getRenderPass() == pass) {
			break;
		}
		++idx;
	}
	if (idx + 1 >= _descriptors.size()) {
		return nullptr;
	}
	return _descriptors.at(idx + 1)->getRenderPass();
}

RenderPassData *Attachment::getPrevRenderPass(RenderPassData *pass) const {
	size_t idx = 0;
	for (auto &it : _descriptors) {
		if (it->getRenderPass() == pass) {
			break;
		}
		++ idx;
	}
	if (idx == 0) {
		return nullptr;
	}
	return _descriptors.at(idx - 1)->getRenderPass();
}

bool AttachmentDescriptor::init(RenderPassData *pass, Attachment *attachment) {
	_renderPass = pass;
	_descriptor.attachment = attachment;
	_descriptor.descriptor = this;
	return true;
}

void AttachmentDescriptor::clear() {

}

void AttachmentDescriptor::reset() {

}

void AttachmentDescriptor::setIndex(uint32_t idx) {
	_index = idx;

	if (_descriptor.type == DescriptorType::Unknown) {
		return;
	}

	for (auto &subpass : _renderPass->subpasses) {
		for (auto &pipeline : subpass.pipelines) {
			for (auto &it : pipeline->shaders) {
				for (auto &binding : it.data->bindings) {
					if (binding.set == 0 && binding.descriptor == _index) {
						_descriptor.stages |= it.data->stage;
					}
				}
			}
		}
	}

	std::cout << "[" << getName() << ":" << _index << "] usage:" << getProgramStageDescription(_descriptor.stages) << "\n";
}

AttachmentRef *AttachmentDescriptor::addRef(uint32_t idx, AttachmentUsage usage, AttachmentDependencyInfo info) {
	for (auto &it : _refs) {
		if (it->getSubpass() == idx) {
			if ((it->getUsage() & usage) != AttachmentUsage::None) {
				return nullptr;
			} else {
				it->addUsage(usage);
				it->addDependency(info);
				return it;
			}
		}
	}

	if (auto ref = makeRef(idx, usage, info)) {
		return _refs.emplace_back(ref);
	}

	return nullptr;
}

void AttachmentDescriptor::sortRefs(RenderQueue &queue, Device &dev) {
	std::sort(_refs.begin(), _refs.end(), [&] (const Rc<AttachmentRef> &l, const Rc<AttachmentRef> &r) {
		return l->getSubpass() < r->getSubpass();
	});

	for (auto &it : _refs) {
		it->updateLayout();

		_dependency.requiredRenderPassState = FrameRenderPassState(std::max(toInt(_dependency.requiredRenderPassState),
				toInt(it->getDependency().requiredRenderPassState)));
	}

	_dependency.initialUsageStage = _refs.front()->getDependency().initialUsageStage;
	_dependency.initialAccessMask = _refs.front()->getDependency().initialAccessMask;
	_dependency.finalUsageStage = _refs.back()->getDependency().finalUsageStage;
	_dependency.finalAccessMask = _refs.back()->getDependency().finalAccessMask;

	if (_descriptor.type != DescriptorType::Unknown) {
		return;
	}

	auto type = _descriptor.attachment->getType();
	if (type == AttachmentType::Buffer) {
		auto buffer = (BufferAttachment *)_descriptor.attachment;
		if (_descriptor.attachment->getDescriptorType() != DescriptorType::Unknown) {
			_descriptor.type = _descriptor.attachment->getDescriptorType();
		} else {
			DescriptorType descriptor = DescriptorType::Unknown;
			if ((buffer->getInfo().usage & BufferUsage::UniformTexelBuffer) != BufferUsage::None) {
				if (descriptor == DescriptorType::Unknown) {
					descriptor = DescriptorType::UniformTexelBuffer;
				} else {
					log::vtext("Gl-Error", "Fail to deduce DescriptorType from attachment '", _descriptor.attachment->getName(), "'");
				}
			}
			if ((buffer->getInfo().usage & BufferUsage::StorageTexelBuffer) != BufferUsage::None) {
				if (descriptor == DescriptorType::Unknown) {
					descriptor = DescriptorType::StorageTexelBuffer;
				} else {
					log::vtext("Gl-Error", "Fail to deduce DescriptorType from attachment '", _descriptor.attachment->getName(), "'");
				}
			}
			if ((buffer->getInfo().usage & BufferUsage::UniformBuffer) != BufferUsage::None) {
				if (descriptor == DescriptorType::Unknown) {
					descriptor = DescriptorType::UniformBuffer;
				} else {
					log::vtext("Gl-Error", "Fail to deduce DescriptorType from attachment '", _descriptor.attachment->getName(), "'");
				}
			}
			if ((buffer->getInfo().usage & BufferUsage::StorageBuffer) != BufferUsage::None) {
				if (descriptor == DescriptorType::Unknown) {
					descriptor = DescriptorType::StorageBuffer;
				} else {
					log::vtext("Gl-Error", "Fail to deduce DescriptorType from attachment '", _descriptor.attachment->getName(), "'");
				}
			}

			if (descriptor != DescriptorType::Unknown) {
				_descriptor.type = descriptor;
			}
		}
	} else if (type == AttachmentType::Image) {
		bool isInputAttachment = false;
		for (auto &usage : _refs) {
			if ((usage->getUsage() & AttachmentUsage::Input) != AttachmentUsage::None) {
				isInputAttachment = true;
				break;
			}
		}
		if (isInputAttachment) {
			_descriptor.type = DescriptorType::InputAttachment;
		}
	}
}

bool AttachmentRef::init(AttachmentDescriptor *desc, uint32_t idx, AttachmentUsage usage, AttachmentDependencyInfo dep) {
	_descriptor = desc;
	_subpass = idx;
	_usage = usage;
	_dependency = dep;
	return true;
}

void AttachmentRef::addDependency(AttachmentDependencyInfo info) {
	if (info.initialUsageStage != PipelineStage::None) {
		_dependency.initialUsageStage |= info.initialUsageStage;
		_dependency.initialAccessMask |= info.initialAccessMask;
	}

	if (info.finalUsageStage != PipelineStage::None) {
		_dependency.finalUsageStage |= info.finalUsageStage;
		_dependency.finalAccessMask |= info.finalAccessMask;
	}
}

void AttachmentRef::updateLayout() {

}

bool BufferAttachment::init(StringView str, const BufferInfo &info) {
	_info = info;
	if (Attachment::init(str, AttachmentType::Buffer)) {
		_info.key = _name;
		return true;
	}
	return false;
}

void BufferAttachment::clear() {
	Attachment::clear();
}

BufferAttachmentDescriptor *BufferAttachment::addBufferDescriptor(RenderPassData *pass) {
	return (BufferAttachmentDescriptor *)addDescriptor(pass);
}

Rc<AttachmentDescriptor> BufferAttachment::makeDescriptor(RenderPassData *pass) {
	return Rc<BufferAttachmentDescriptor>::create(pass, this);
}

BufferAttachmentRef *BufferAttachmentDescriptor::addBufferRef(uint32_t idx, AttachmentUsage usage, AttachmentDependencyInfo info) {
	return (BufferAttachmentRef *)addRef(idx, usage, info);
}

Rc<AttachmentRef> BufferAttachmentDescriptor::makeRef(uint32_t idx, AttachmentUsage usage, AttachmentDependencyInfo info) {
	return Rc<BufferAttachmentRef>::create(this, idx, usage, info);
}

ImageAttachmentObject::~ImageAttachmentObject() { }

void ImageAttachmentObject::rearmSemaphores(Device &dev) {
	if (waitSem && waitSem->isWaited()) {
		// successfully waited on this sem
		auto tmp = move(waitSem);
		waitSem = nullptr;

		if (signalSem && signalSem->isSignaled()) {
			// successfully signaled
			if (!signalSem->isWaited()) {
				waitSem = move(signalSem);
			}
		}

		signalSem = move(tmp);
		if (!signalSem->reset()) {
			signalSem = nullptr;
		}
	} else if (!waitSem) {
		if (signalSem && signalSem->isSignaled()) {
			if (!signalSem->isWaited()) {
				// successfully signaled
				waitSem = move(signalSem);
			}
		}
		signalSem = nullptr;
	} else {
		// next frame should wait on this waitSem
		// signalSem should be unsignaled due frame processing logic
		signalSem = nullptr;
	}

	if (!signalSem) {
		signalSem = dev.makeSemaphore();
	}
}

void ImageAttachmentObject::makeViews(Device &dev, const ImageAttachment &attachment) {
	for (auto &desc : attachment.getDescriptors()) {
		if (desc->getAttachment()->getType() == gl::AttachmentType::Image) {
			auto imgDesc = (gl::ImageAttachmentDescriptor *)desc.get();
			gl::ImageViewInfo info(*imgDesc);

			auto it = views.find(info);
			if (it == views.end()) {
				views.emplace(info, dev.makeImageView(image, info));
			}
		}
	}
}

bool ImageAttachment::init(StringView str, const ImageInfo &info, AttachmentInfo &&a) {
	if (Attachment::init(str, AttachmentType::Image)) {
		_imageInfo = info;
		_imageInfo.key = _name;
		_attachmentInfo = move(a);
		return true;
	}
	return false;
}

void ImageAttachment::addImageUsage(gl::ImageUsage usage) {
	_imageInfo.usage |= usage;
}

ImageAttachmentDescriptor *ImageAttachment::addImageDescriptor(RenderPassData *data) {
	return (ImageAttachmentDescriptor *)addDescriptor(data);
}

bool ImageAttachment::isCompatible(const ImageInfo &image) const {
	return _imageInfo.isCompatible(image);
}

Extent3 ImageAttachment::getSizeForFrame(const FrameQueue &frame) const {
	if (_attachmentInfo.frameSizeCallback) {
		return _attachmentInfo.frameSizeCallback(frame);
	} else {
		return _imageInfo.extent;
	}
}

Rc<AttachmentDescriptor> ImageAttachment::makeDescriptor(RenderPassData *pass) {
	return Rc<ImageAttachmentDescriptor>::create(pass, this);
}

bool ImageAttachmentDescriptor::init(RenderPassData *pass, ImageAttachment *attachment) {
	if (AttachmentDescriptor::init(pass, attachment)) {
		_colorMode = attachment->getColorMode();
		return true;
	}
	return false;
}

ImageAttachmentRef *ImageAttachmentDescriptor::addImageRef(uint32_t idx, AttachmentUsage usage, AttachmentLayout layout,
		AttachmentDependencyInfo info) {
	for (auto &it : _refs) {
		if (it->getSubpass() == idx) {
			if ((it->getUsage() & usage) != AttachmentUsage::None) {
				return nullptr;
			} else {
				auto ref = (ImageAttachmentRef *)it.get();
				if (ref->getLayout() != layout) {
					log::vtext("Gl-Error", "Multiple layouts defined for attachment '", _descriptor.attachment->getName(),
							"' within renderpass ", _renderPass->key, ":", idx);
					return nullptr;
				}

				it->addUsage(usage);
				it->addDependency(info);
				return (ImageAttachmentRef *)it.get();
			}
		}
	}

	if (auto ref = makeImageRef(idx, usage, layout, info)) {
		_refs.emplace_back(ref);
		return ref;
	}

	return nullptr;
}

ImageAttachment *ImageAttachmentDescriptor::getImageAttachment() const {
	return (ImageAttachment *)getAttachment();
}

Rc<ImageAttachmentRef> ImageAttachmentDescriptor::makeImageRef(uint32_t idx, AttachmentUsage usage, AttachmentLayout layout,
		AttachmentDependencyInfo info) {
	return Rc<ImageAttachmentRef>::create(this, idx, usage, layout, info);
}

bool ImageAttachmentRef::init(ImageAttachmentDescriptor *desc, uint32_t subpass, AttachmentUsage usage, AttachmentLayout layout,
		AttachmentDependencyInfo info) {
	if (AttachmentRef::init(desc, subpass, usage, info)) {
		_layout = layout;
		return true;
	}
	return false;
}

void ImageAttachmentRef::setLayout(AttachmentLayout layout) {
	_layout = layout;
}

void ImageAttachmentRef::updateLayout() {
	AttachmentRef::updateLayout();

	auto fmt = ((ImageAttachment *)_descriptor->getAttachment())->getInfo().format;

	bool separateDepthStencil = false;
	bool hasColor = false;
	bool hasDepth = false;
	bool hasStencil = false;

	switch (fmt) {
	case ImageFormat::D16_UNORM:
	case ImageFormat::X8_D24_UNORM_PACK32:
	case ImageFormat::D32_SFLOAT:
		hasDepth = true;
		break;
	case ImageFormat::S8_UINT:
		hasStencil = true;
		break;
	case ImageFormat::D16_UNORM_S8_UINT:
	case ImageFormat::D24_UNORM_S8_UINT:
	case ImageFormat::D32_SFLOAT_S8_UINT:
		hasDepth = true;
		hasStencil = true;
		break;
	default:
		hasColor = true;
		break;
	}

	switch (_usage) {
	case AttachmentUsage::Input:
		switch (_layout) {
		case AttachmentLayout::DepthReadOnlyStencilAttachmentOptimal:
		case AttachmentLayout::DepthAttachmentStencilReadOnlyOptimal:
		case AttachmentLayout::DepthReadOnlyOptimal:
		case AttachmentLayout::StencilReadOnlyOptimal:
		case AttachmentLayout::DepthStencilReadOnlyOptimal:
		case AttachmentLayout::ShaderReadOnlyOptimal:
		case AttachmentLayout::General:
			break;
		case AttachmentLayout::Ignored:
			if (hasColor) {
				_layout = AttachmentLayout::ShaderReadOnlyOptimal;
			} else if ((!separateDepthStencil && (hasDepth || hasStencil)) || (hasDepth && hasStencil)) {
				_layout = AttachmentLayout::DepthStencilReadOnlyOptimal;
			} else if (hasDepth) {
				_layout = AttachmentLayout::DepthReadOnlyOptimal;
			} else if (hasStencil) {
				_layout = AttachmentLayout::StencilReadOnlyOptimal;
			} else {
				_layout = AttachmentLayout::General;
			}
			break;
		default:
			log::vtext("Gl-Error", "Invalid layout for attachment '", _descriptor->getAttachment()->getName(),
					"' in renderpass ", _descriptor->getRenderPass()->key, ":", _subpass);
			break;
		}
		break;
	case AttachmentUsage::Output:
	case AttachmentUsage::Resolve:
		switch (_layout) {
		case AttachmentLayout::ColorAttachmentOptimal:
		case AttachmentLayout::General:
			break;
		case AttachmentLayout::Ignored:
			_layout = AttachmentLayout::ColorAttachmentOptimal;
			break;
		default:
			log::vtext("Gl-Error", "Invalid layout for attachment '", _descriptor->getAttachment()->getName(),
					"' in renderpass ", _descriptor->getRenderPass()->key, ":", _subpass);
			break;
		}
		break;
	case AttachmentUsage::InputOutput:
		switch (_layout) {
		case AttachmentLayout::General:
			break;
		case AttachmentLayout::Ignored:
			_layout = AttachmentLayout::General;
			break;
		default:
			log::vtext("Gl-Error", "Invalid layout for attachment '", _descriptor->getAttachment()->getName(),
					"' in renderpass ", _descriptor->getRenderPass()->key, ":", _subpass);
			break;
		}
		break;
	case AttachmentUsage::DepthStencil:
		switch (_layout) {
		case AttachmentLayout::DepthStencilAttachmentOptimal:
		case AttachmentLayout::DepthAttachmentOptimal:
		case AttachmentLayout::DepthReadOnlyOptimal:
		case AttachmentLayout::StencilAttachmentOptimal:
		case AttachmentLayout::StencilReadOnlyOptimal:
		case AttachmentLayout::DepthReadOnlyStencilAttachmentOptimal:
		case AttachmentLayout::DepthAttachmentStencilReadOnlyOptimal:
		case AttachmentLayout::DepthStencilReadOnlyOptimal:
		case AttachmentLayout::General:
			break;
		case AttachmentLayout::Ignored:
			if ((!separateDepthStencil && (hasDepth || hasStencil)) || (hasDepth && hasStencil)) {
				_layout = AttachmentLayout::DepthStencilAttachmentOptimal;
			} else if (hasDepth) {
				_layout = AttachmentLayout::DepthAttachmentOptimal;
			} else if (hasStencil) {
				_layout = AttachmentLayout::StencilAttachmentOptimal;
			} else {
				_layout = AttachmentLayout::General;
			}
			break;
		default:
			log::vtext("Gl-Error", "Invalid layout for attachment '", _descriptor->getAttachment()->getName(),
					"' in renderpass ", _descriptor->getRenderPass()->key, ":", _subpass);
			break;
		}
		break;
	case AttachmentUsage::Input | AttachmentUsage::DepthStencil:
		switch (_layout) {
		case AttachmentLayout::DepthReadOnlyStencilAttachmentOptimal:
		case AttachmentLayout::DepthAttachmentStencilReadOnlyOptimal:
		case AttachmentLayout::DepthStencilReadOnlyOptimal:
		case AttachmentLayout::General:
			break;
		case AttachmentLayout::Ignored:
			_layout = AttachmentLayout::General;
			break;
		default:
			log::vtext("Gl-Error", "Invalid layout for attachment '", _descriptor->getAttachment()->getName(),
					"' in renderpass ", _descriptor->getRenderPass()->key, ":", _subpass);
			break;
		}
		break;
	default:
		log::vtext("Gl-Error", "Invalid usage for attachment '", _descriptor->getAttachment()->getName(),
				"' in renderpass ", _descriptor->getRenderPass()->key, ":", _subpass);
		break;
	}
}

bool GenericAttachment::init(StringView name) {
	return Attachment::init(name, AttachmentType::Generic);
}

Rc<AttachmentHandle> GenericAttachment::makeFrameHandle(const FrameQueue &h) {
	return Rc<AttachmentHandle>::create(this, h);
}

Rc<AttachmentDescriptor> GenericAttachment::makeDescriptor(RenderPassData *data) {
	return Rc<GenericAttachmentDescriptor>::create(data, this);
}

Rc<AttachmentRef> GenericAttachmentDescriptor::makeRef(uint32_t idx, AttachmentUsage usage, AttachmentDependencyInfo info) {
	return Rc<GenericAttachmentRef>::create(this, idx, usage, info);
}

bool AttachmentHandle::init(const Rc<Attachment> &attachment, const FrameQueue &frame) {
	_attachment = attachment;
	return true;
}

void AttachmentHandle::setQueueData(FrameQueueAttachmentData &data) {
	_queueData = &data;
}

// returns true for immediate setup, false if seyup job was scheduled
bool AttachmentHandle::setup(FrameQueue &, Function<void(bool)> &&) {
	return true;
}

void AttachmentHandle::finalize(FrameQueue &, bool successful) {

}

bool AttachmentHandle::isInput() const {
	return (_attachment->getUsage() & AttachmentUsage::Input) != AttachmentUsage::None;
}

bool AttachmentHandle::isOutput() const {
	return (_attachment->getUsage() & AttachmentUsage::Output) != AttachmentUsage::None;
}

uint32_t AttachmentHandle::getDescriptorArraySize(const RenderPassHandle &, const PipelineDescriptor &d, bool isExternal) const {
	return d.count;
}

bool AttachmentHandle::isDescriptorDirty(const RenderPassHandle &, const PipelineDescriptor &, uint32_t, bool isExternal) const {
	return false;
}

}
