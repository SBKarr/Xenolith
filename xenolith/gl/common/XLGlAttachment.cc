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

void Attachment::sortDescriptors(RenderQueue &queue) {
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
		it->sortRefs(queue);
	}

	if (_type == gl::AttachmentType::SwapchainImage) {
		_descriptors.back()->getRenderPass()->isPresentable = true;
	}
}

Rc<AttachmentHandle> Attachment::makeFrameHandle(const FrameHandle &) {
	return nullptr;
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

	return true;
}

void AttachmentDescriptor::clear() {

}

void AttachmentDescriptor::reset() {

}

AttachmentRef *AttachmentDescriptor::addRef(uint32_t idx, AttachmentUsage usage) {
	for (auto &it : _refs) {
		if (it->getSubpass() == idx) {
			if ((it->getUsage() & usage) != AttachmentUsage::None) {
				return nullptr;
			} else {
				it->addUsage(usage);
				return it;
			}
		}
	}

	if (auto ref = makeRef(idx, usage)) {
		return _refs.emplace_back(ref);
	}

	return nullptr;
}

void AttachmentDescriptor::sortRefs(RenderQueue &queue) {
	std::sort(_refs.begin(), _refs.end(), [&] (const Rc<AttachmentRef> &l, const Rc<AttachmentRef> &r) {
		return l->getSubpass() < r->getSubpass();
	});

	for (auto &it : _refs) {
		it->updateLayout();
	}

	ProgramStage globalStages;
	for (auto &pipeline : _renderPass->pipelines) {
		for (auto &it : pipeline->shaders) {
			if (auto sh = queue.getProgram(it)) {
				globalStages |= sh->stage;
			}
		}
	}

	auto findUsageStages = [&] (Attachment *attachment) {
		return globalStages;
	};

	if (_descriptor.type != DescriptorType::Unknown) {
		return;
	}

	if (_descriptor.attachment->getType() == AttachmentType::Buffer) {
		auto buffer = (BufferAttachment *)_descriptor.attachment;
		if (_descriptor.attachment->getDescriptorType() != DescriptorType::Unknown) {
			_descriptor.type = _descriptor.attachment->getDescriptorType();
			if (buffer->getInfo().stages != ProgramStage::None) {
				_descriptor.stages = buffer->getInfo().stages;
			} else {
				_descriptor.stages = findUsageStages(_descriptor.attachment);
			}
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
				if (buffer->getInfo().stages != ProgramStage::None) {
					_descriptor.stages = buffer->getInfo().stages;
				} else {
					_descriptor.stages = findUsageStages(_descriptor.attachment);
				}
			}
		}
	} else {
		bool isInputAttachment = false;
		for (auto &usage : _refs) {
			if ((usage->getUsage() & AttachmentUsage::Input) != AttachmentUsage::None) {
				isInputAttachment = true;
				break;
			}
		}
		if (isInputAttachment) {
			auto image = (ImageAttachment *)_descriptor.attachment;
			_descriptor.type = DescriptorType::InputAttachment;
			if (image->getInfo().stages != ProgramStage::None) {
				_descriptor.stages = image->getInfo().stages;
			} else {
				_descriptor.stages = findUsageStages(_descriptor.attachment);
			}
		}
	}
}

bool AttachmentRef::init(AttachmentDescriptor *desc, uint32_t idx, AttachmentUsage usage) {
	_descriptor = desc;
	_subpass = idx;
	_usage = usage;
	return true;
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

BufferAttachmentRef *BufferAttachmentDescriptor::addBufferRef(uint32_t idx, AttachmentUsage usage) {
	return (BufferAttachmentRef *)addRef(idx, usage);
}

Rc<AttachmentRef> BufferAttachmentDescriptor::makeRef(uint32_t idx, AttachmentUsage usage) {
	return Rc<BufferAttachmentRef>::create(this, idx, usage);
}

bool ImageAttachment::init(StringView str, const ImageInfo &info, AttachmentLayout init,
		AttachmentLayout fin, bool clear) {
	if (Attachment::init(str, AttachmentType::Image)) {
		_info = info;
		_info.key = _name;
		_initialLayout = init;
		_finalLayout = fin;
		_clearOnLoad = clear;
		return true;
	}
	return false;
}

void ImageAttachment::onSwapchainUpdate(const ImageInfo &info) {
	if (_type == AttachmentType::SwapchainImage) {
		_info = info;
	}
}

ImageAttachmentDescriptor *ImageAttachment::addImageDescriptor(RenderPassData *data) {
	return (ImageAttachmentDescriptor *)addDescriptor(data);
}

bool ImageAttachment::isCompatible(const ImageInfo &image) const {
	return _info.isCompatible(image);
}

Rc<AttachmentDescriptor> ImageAttachment::makeDescriptor(RenderPassData *pass) {
	return Rc<ImageAttachmentDescriptor>::create(pass, this);
}

bool ImageAttachmentDescriptor::init(RenderPassData *pass, ImageAttachment *attachment) {
	if (AttachmentDescriptor::init(pass, attachment)) {
		return true;
	}
	return false;
}

ImageAttachmentRef *ImageAttachmentDescriptor::addImageRef(uint32_t idx, AttachmentUsage usage, AttachmentLayout layout) {
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
				return (ImageAttachmentRef *)it.get();
			}
		}
	}

	if (auto ref = makeImageRef(idx, usage, layout)) {
		_refs.emplace_back(ref);
		return ref;
	}

	return nullptr;
}

Rc<ImageAttachmentRef> ImageAttachmentDescriptor::makeImageRef(uint32_t idx, AttachmentUsage usage, AttachmentLayout layout) {
	return Rc<ImageAttachmentRef>::create(this, idx, usage, layout);
}

bool ImageAttachmentRef::init(ImageAttachmentDescriptor *desc, uint32_t subpass, AttachmentUsage usage, AttachmentLayout layout) {
	if (AttachmentRef::init(desc, subpass, usage)) {
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
			} else if (hasDepth && hasStencil) {
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
			if (hasDepth && hasStencil) {
				_layout = AttachmentLayout::DepthAttachmentOptimal;
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

SwapchainAttachment::~SwapchainAttachment() { }

bool SwapchainAttachment::init(StringView str, const ImageInfo &info, AttachmentLayout init,
		AttachmentLayout fin, bool clear) {
	if (Attachment::init(str, AttachmentType::SwapchainImage)) {
		_info = info;
		_info.key = _name;
		_initialLayout = init;
		_finalLayout = fin;
		_clearOnLoad = clear;
		return true;
	}
	return false;
}

bool SwapchainAttachment::acquireForFrame(gl::FrameHandle &frame) {
	if (_owner) {
		if (_next) {
			_next->invalidate();
		}
		_next = &frame;
		return false;
	} else {
		_owner = &frame;
		return true;
	}
}

bool SwapchainAttachment::releaseForFrame(gl::FrameHandle &frame) {
	if (_owner.get() == &frame) {
		if (_next) {
			_owner = move(_next);
			_next = nullptr;
			_owner->getLoop()->pushEvent(gl::Loop::Event::FrameUpdate, _owner);
		} else {
			_owner = nullptr;
		}
		return true;
	} else if (_next.get() == &frame) {
		_next = nullptr;
		return true;
	}
	return false;
}

Rc<AttachmentDescriptor> SwapchainAttachment::makeDescriptor(RenderPassData *pass) {
	return Rc<SwapchainAttachmentDescriptor>::create(pass, this);
}

bool AttachmentHandle::init(const Rc<Attachment> &attachment, const FrameHandle &frame) {
	_attachment = attachment;
	return true;
}

// returns true for immediate setup, false if seyup job was scheduled
bool AttachmentHandle::setup(FrameHandle &) {
	return true;
}

bool AttachmentHandle::isInput() const {
	return (_attachment->getUsage() & AttachmentUsage::Input) != AttachmentUsage::None;
}

uint32_t AttachmentHandle::getDescriptorArraySize(const RenderPassHandle &, const PipelineDescriptor &d, bool isExternal) const {
	return d.count;
}

bool AttachmentHandle::isDescriptorDirty(const RenderPassHandle &, const PipelineDescriptor &, uint32_t, bool isExternal) const {
	return false;
}

void AttachmentHandle::setReady(bool value) {
	_ready = value;
}

}
