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

#include "XLGlRenderQueue.h"
#include "XLGlFrameEmitter.h"

namespace stappler::xenolith::gl {

struct RenderQueue::QueueData : NamedMem {
	memory::pool_t *pool = nullptr;
	Mode mode = Mode::RenderOnDemand;
	memory::vector<Attachment *> input;
	memory::vector<Attachment *> output;
	HashTable<Rc<Attachment>> attachments;
	HashTable<RenderPassData *> passes;
	HashTable<ProgramData *> programs;
	HashTable<PipelineData *> pipelines;
	HashTable<Rc<Resource>> linked;
	Function<void(gl::FrameRequest &)> beginCallback;
	Function<void(gl::FrameRequest &)> endCallback;
	Function<void(const Swapchain *)> enableCallback;
	Function<void()> disableCallback;
	Rc<Resource> resource;
	bool compiled = false;
	uint64_t order = 0;

	Set<FrameCacheStorage *> frameCache;

	void clear() {
		for (auto &it : programs) {
			it->program = nullptr;
		}

		for (auto &it : passes) {
			for (auto &desc : it->descriptors) {
				desc->clear();
			}

			for (auto &subpass : it->subpasses) {
				for (auto &pipeline : subpass.pipelines) {
					pipeline->pipeline = nullptr;
				}
			}

			it->renderPass->invalidate();
			it->renderPass = nullptr;
			it->impl = nullptr;
		}

		for (auto &it : attachments) {
			it->clear();
		}

		auto tmpCache = move(frameCache);
		frameCache.clear();
		for (auto &it : tmpCache) {
			it->invalidate();
		}

		if (resource) {
			resource->clear();
			resource = nullptr;
		}
		linked.clear();
		compiled = false;
	}
};

static void RenderQueue_buildLoadStore(RenderQueue::QueueData *data) {
	for (auto &attachment : data->attachments) {
		if (!isImageAttachmentType(attachment->getType())) {
			continue;
		}

		auto img = (ImageAttachment *)attachment.get();

		bool hasColor = false;
		bool hasStencil = false;
		switch (img->getInfo().format) {
		case ImageFormat::S8_UINT:
			hasStencil = true;
			break;
		case ImageFormat::D16_UNORM_S8_UINT:
		case ImageFormat::D24_UNORM_S8_UINT:
		case ImageFormat::D32_SFLOAT_S8_UINT:
			hasColor = true;
			hasStencil = true;
			break;
		default:
			hasColor = true;
			break;
		}

		for (auto &descriptor : img->getDescriptors()) {
			if (descriptor->getOps() != AttachmentOps::Undefined) {
				// operations was hinted, no heuristics required
				continue;
			}

			AttachmentOps ops = AttachmentOps::Undefined;
			for (auto &it : descriptor->getRefs()) {
				if (it->getOps() != AttachmentOps::Undefined) {
					ops |= it->getOps();
					continue;
				}

				AttachmentOps refOps = AttachmentOps::Undefined;
				auto imgRef = (ImageAttachmentRef *)it.get();
				bool hasWriters = false;
				bool hasReaders = false;
				bool colorReadOnly = true;
				bool stencilReadOnly = true;

				if ((it->getUsage() & AttachmentUsage::Output) != AttachmentUsage::None
						|| (it->getUsage() & AttachmentUsage::Resolve) != AttachmentUsage::None
						|| (it->getUsage() & AttachmentUsage::DepthStencil) != AttachmentUsage::None) {
					hasWriters = true;
				}
				if ((it->getUsage() & AttachmentUsage::Input) != AttachmentUsage::None
						|| (it->getUsage() & AttachmentUsage::DepthStencil) != AttachmentUsage::None) {
					hasReaders = true;
				}
				if ((it->getUsage() & AttachmentUsage::DepthStencil) != AttachmentUsage::None) {
					switch (imgRef->getLayout()) {
					case AttachmentLayout::DepthStencilAttachmentOptimal:
					case AttachmentLayout::DepthReadOnlyStencilAttachmentOptimal:
					case AttachmentLayout::DepthAttachmentStencilReadOnlyOptimal:
					case AttachmentLayout::DepthAttachmentOptimal:
					case AttachmentLayout::StencilAttachmentOptimal:
					case AttachmentLayout::General:
						hasWriters = true;
						break;
					default:
						break;
					}
				}

				switch (imgRef->getLayout()) {
				case AttachmentLayout::General:
				case AttachmentLayout::DepthStencilAttachmentOptimal:
					stencilReadOnly = false;
					colorReadOnly = false;
					break;
				case AttachmentLayout::ColorAttachmentOptimal:
				case AttachmentLayout::DepthAttachmentStencilReadOnlyOptimal:
				case AttachmentLayout::DepthAttachmentOptimal:
					colorReadOnly = false;
					break;
				case AttachmentLayout::DepthReadOnlyStencilAttachmentOptimal:
				case AttachmentLayout::StencilAttachmentOptimal:
					stencilReadOnly = false;
					break;
				default:
					break;
				}

				if (hasWriters) {
					if (hasColor && !colorReadOnly) {
						refOps |= AttachmentOps::WritesColor;
					}
					if (hasStencil && !stencilReadOnly) {
						refOps |= AttachmentOps::WritesStencil;
					}
				}

				if (hasReaders) {
					if (hasColor) {
						refOps |= AttachmentOps::ReadColor;
					}
					if (hasStencil) {
						refOps |= AttachmentOps::ReadStencil;
					}
				}

				it->setOps(refOps);
				ops |= refOps;
			}
			descriptor->setOps(ops);
		}
	};

	auto dataWasWritten = [] (Attachment *data, uint32_t idx) -> Pair<bool, bool> {
		if ((data->getUsage() & AttachmentUsage::Input) != AttachmentUsage::None) {
			if ((data->getOps() & (AttachmentOps::WritesColor | AttachmentOps::WritesStencil)) != AttachmentOps::Undefined) {
				return pair(true, true);
			}
		}

		bool colorWasWritten = (data->getOps() & AttachmentOps::WritesColor) != AttachmentOps::Undefined;
		bool stencilWasWritten = (data->getOps() & AttachmentOps::WritesStencil) != AttachmentOps::Undefined;

		auto &descriptors = data->getDescriptors();
		for (size_t i = 0; i < idx; ++ i) {
			auto desc = descriptors[i].get();
			if ((desc->getOps() & AttachmentOps::WritesColor) != AttachmentOps::Undefined) {
				colorWasWritten = true;
			}
			if ((desc->getOps() & AttachmentOps::WritesStencil) != AttachmentOps::Undefined) {
				stencilWasWritten = true;
			}
		}

		return pair(colorWasWritten, stencilWasWritten);
	};

	auto dataWillBeRead = [] (Attachment *data, uint32_t idx) -> Pair<bool, bool> {
		if ((data->getUsage() & AttachmentUsage::Output) != AttachmentUsage::None) {
			if ((data->getOps() & (AttachmentOps::ReadColor | AttachmentOps::ReadStencil)) != AttachmentOps::Undefined) {
				return pair(true, true);
			}
		}

		bool colorWillBeRead = (data->getOps() & AttachmentOps::ReadColor) != AttachmentOps::Undefined;
		bool stencilWillBeRead = (data->getOps() & AttachmentOps::ReadStencil) != AttachmentOps::Undefined;

		auto &descriptors = data->getDescriptors();
		for (size_t i = idx + 1; i < descriptors.size(); ++ i) {
			auto desc = descriptors[i].get();
			if ((desc->getOps() & AttachmentOps::ReadColor) != AttachmentOps::Undefined) {
				colorWillBeRead = true;
			}
			if ((desc->getOps() & AttachmentOps::ReadStencil) != AttachmentOps::Undefined) {
				stencilWillBeRead = true;
			}
		}

		return pair(colorWillBeRead, stencilWillBeRead);
	};

	// fill layout chain
	for (auto &attachment : data->attachments) {
		if (attachment->getDescriptors().empty()) {
			continue;
		}

		if (attachment->getDescriptors().size() == 1 && attachment->getUsage() == AttachmentUsage::None) {
			attachment->setTransient(true);

			if (!isImageAttachmentType(attachment->getType())) {
				continue;
			}

			for (auto &desc : attachment->getDescriptors()) {
				auto img = (ImageAttachmentDescriptor *)desc.get();

				img->setLoadOp(AttachmentLoadOp::DontCare);
				img->setStencilLoadOp(AttachmentLoadOp::DontCare);
				img->setStoreOp(AttachmentStoreOp::DontCare);
				img->setStencilStoreOp(AttachmentStoreOp::DontCare);
			}
		} else {
			if (!isImageAttachmentType(attachment->getType())) {
				continue;
			}

			size_t descIndex = 0;
			for (auto &desc : attachment->getDescriptors()) {
				auto imgDesc = (ImageAttachmentDescriptor *)desc.get();
				auto wasWritten = dataWasWritten(attachment, descIndex); // Color, Stencil
				auto willBeRead = dataWillBeRead(attachment, descIndex);

				if (wasWritten.first) {
					if ((desc->getOps() & AttachmentOps::ReadColor) != AttachmentOps::Undefined) {
						imgDesc->setLoadOp(AttachmentLoadOp::Load);
					} else {
						imgDesc->setLoadOp(AttachmentLoadOp::DontCare);
					}
				} else {
					bool isRead = ((desc->getOps() & AttachmentOps::ReadColor) != AttachmentOps::Undefined);
					bool isWrite = ((desc->getOps() & AttachmentOps::WritesColor) != AttachmentOps::Undefined);
					if (isRead && !isWrite) {
						log::vtext("Gl-Error", "Attachment's color component '", attachment->getName(), "' is read in renderpass ",
								desc->getRenderPass()->key, " before written");
					}

					auto img = (ImageAttachment *)attachment.get();
					imgDesc->setLoadOp(img->shouldClearOnLoad() ? AttachmentLoadOp::Clear : AttachmentLoadOp::DontCare);
				}

				if (wasWritten.second) {
					if ((desc->getOps() & AttachmentOps::ReadStencil) != AttachmentOps::Undefined) {
						imgDesc->setStencilLoadOp(AttachmentLoadOp::Load);
					} else {
						imgDesc->setStencilLoadOp(AttachmentLoadOp::DontCare);
					}
				} else {
					bool isRead = ((desc->getOps() & AttachmentOps::ReadStencil) != AttachmentOps::Undefined);
					bool isWrite = ((desc->getOps() & AttachmentOps::WritesStencil) != AttachmentOps::Undefined);
					if (isRead && !isWrite) {
						log::vtext("Gl-Error", "Attachment's stencil component '", attachment->getName(), "' is read in renderpass ",
								desc->getRenderPass()->key, " before written");
					}
					auto img = (ImageAttachment *)attachment.get();
					imgDesc->setStencilLoadOp(img->shouldClearOnLoad() ? AttachmentLoadOp::Clear : AttachmentLoadOp::DontCare);
				}

				if (willBeRead.first) {
					if ((desc->getOps() & AttachmentOps::WritesColor) != AttachmentOps::Undefined) {
						imgDesc->setStoreOp(AttachmentStoreOp::Store);
					} else {
						imgDesc->setStoreOp(AttachmentStoreOp::DontCare);
					}
				} else {
					bool isRead = ((desc->getOps() & AttachmentOps::ReadColor) != AttachmentOps::Undefined);
					bool isWrite = ((desc->getOps() & AttachmentOps::WritesColor) != AttachmentOps::Undefined);
					if (!isRead && isWrite) {
						log::vtext("Gl-Error", "Attachment's color component '", attachment->getName(), "' is writeen in renderpass ",
								desc->getRenderPass()->key, " but never read");
					}
					imgDesc->setStoreOp(AttachmentStoreOp::DontCare);
				}

				if (willBeRead.second) {
					if ((desc->getOps() & AttachmentOps::WritesStencil) != AttachmentOps::Undefined) {
						imgDesc->setStencilStoreOp(AttachmentStoreOp::Store);
					} else {
						imgDesc->setStencilStoreOp(AttachmentStoreOp::DontCare);
					}
				} else {
					bool isRead = ((desc->getOps() & AttachmentOps::ReadStencil) != AttachmentOps::Undefined);
					bool isWrite = ((desc->getOps() & AttachmentOps::WritesStencil) != AttachmentOps::Undefined);
					if (!isRead && isWrite) {
						log::vtext("Gl-Error", "Attachment's stencil component '", attachment->getName(), "' is writeen in renderpass ",
								desc->getRenderPass()->key, " but never read");
					}
					imgDesc->setStencilStoreOp(AttachmentStoreOp::DontCare);
				}
			}
			++ descIndex;
		}

		if (!isImageAttachmentType(attachment->getType())) {
			continue;
		}

		auto img = (ImageAttachment *)attachment.get();
		AttachmentLayout layout = img->getInitialLayout();
		for (auto &desc : attachment->getDescriptors()) {
			auto imgDesc = (ImageAttachmentDescriptor *)desc.get();
			if (layout == AttachmentLayout::Ignored) {
				imgDesc->setInitialLayout(((ImageAttachmentRef *)desc->getRefs().front().get())->getLayout());
			} else {
				imgDesc->setInitialLayout(layout);
			}
			layout = ((ImageAttachmentRef *)desc->getRefs().back().get())->getLayout();
			imgDesc->setFinalLayout(layout);
		}
		if (img->getFinalLayout() != AttachmentLayout::Ignored) {
			((ImageAttachmentDescriptor *)attachment->getDescriptors().back().get())->setFinalLayout(img->getFinalLayout());
		}
	}
}

static void RenderQueue_buildDescriptors(RenderQueue::QueueData *data, Device &dev) {
	for (auto &pass : data->passes) {
		for (auto &subpass : pass->subpasses) {
			for (auto a : subpass.outputImages) {
				if (isImageAttachmentType(a->getAttachment()->getType())) {
					((ImageAttachment *)a->getAttachment())->addImageUsage(ImageUsage::ColorAttachment);
				}
			}
			for (auto a : subpass.resolveImages) {
				if (isImageAttachmentType(a->getAttachment()->getType())) {
					((ImageAttachment *)a->getAttachment())->addImageUsage(ImageUsage::ColorAttachment);
				}
			}
			for (auto a : subpass.inputImages) {
				if (isImageAttachmentType(a->getAttachment()->getType())) {
					((ImageAttachment *)a->getAttachment())->addImageUsage(ImageUsage::InputAttachment);
				}
			}
			if (subpass.depthStencil) {
				if (isImageAttachmentType(subpass.depthStencil->getAttachment()->getType())) {
					((ImageAttachment *)subpass.depthStencil->getAttachment())->addImageUsage(ImageUsage::DepthStencilAttachment);
				}
			}
		}

		for (auto &attachment : pass->descriptors) {
			auto &desc = attachment->getDescriptor();
			if (desc.type != DescriptorType::Unknown) {
				if (dev.supportsUpdateAfterBind(desc.type)) {
					const_cast<PipelineDescriptor &>(desc).updateAfterBind = true;
					pass->hasUpdateAfterBind = true;
				}
				pass->queueDescriptors.emplace_back(&desc);
				if (desc.type == DescriptorType::Sampler) {
					pass->usesSamplers = true;
				}
			}

			if (isImageAttachmentType(attachment->getAttachment()->getType())) {
				auto desc = (ImageAttachmentDescriptor *)attachment;
				switch (desc->getFinalLayout()) {
				case AttachmentLayout::Undefined:
				case AttachmentLayout::General:
				case AttachmentLayout::ShaderReadOnlyOptimal:
				case AttachmentLayout::Preinitialized:
				case AttachmentLayout::PresentSrc:
				case AttachmentLayout::Ignored:
					break;
				case AttachmentLayout::ColorAttachmentOptimal:
					desc->getImageAttachment()->addImageUsage(gl::ImageUsage::ColorAttachment);
					break;
				case AttachmentLayout::TransferSrcOptimal:
					desc->getImageAttachment()->addImageUsage(gl::ImageUsage::TransferSrc);
					break;
				case AttachmentLayout::TransferDstOptimal:
					desc->getImageAttachment()->addImageUsage(gl::ImageUsage::TransferDst);
					break;
				case AttachmentLayout::DepthStencilAttachmentOptimal:
				case AttachmentLayout::DepthStencilReadOnlyOptimal:
				case AttachmentLayout::DepthReadOnlyStencilAttachmentOptimal:
				case AttachmentLayout::DepthAttachmentStencilReadOnlyOptimal:
				case AttachmentLayout::DepthAttachmentOptimal:
				case AttachmentLayout::DepthReadOnlyOptimal:
				case AttachmentLayout::StencilAttachmentOptimal:
				case AttachmentLayout::StencilReadOnlyOptimal:
					desc->getImageAttachment()->addImageUsage(gl::ImageUsage::DepthStencilAttachment);
					break;
				}
			}
		}
	}
}

RenderQueue::RenderQueue() { }

RenderQueue::~RenderQueue() {
	if (_data) {
		_data->clear();
		auto p = _data->pool;
		_data->~QueueData();
		memory::pool::destroy(p);
		_data = nullptr;
	}
}

bool RenderQueue::init(Builder && buf) {
	if (buf._data) {
		_data = buf._data;
		buf._data = nullptr;

		for (auto &it : _data->passes) {
			it->renderPass->_data = it;
		}

		if (_data->resource) {
			_data->resource->setOwner(this);
		}

		return true;
	}
	return false;
}

bool RenderQueue::isCompiled() const {
	return _data->compiled;
}

void RenderQueue::setCompiled(bool value) {
	_data->compiled = value;
}

bool RenderQueue::isCompatible(const ImageInfo &info) const {
	if (_data && _data->output.size() == 1) {
		auto out = _data->output.front();
		if (isImageAttachmentType(out->getType())) {
			return out->isCompatible(info);
		}
	}
	return false;
}

StringView RenderQueue::getName() const {
	return _data->key;
}

const HashTable<ProgramData *> &RenderQueue::getPrograms() const {
	return _data->programs;
}

const HashTable<RenderPassData *> &RenderQueue::getPasses() const {
	return _data->passes;
}

const HashTable<PipelineData *> &RenderQueue::getPipelines() const {
	return _data->pipelines;
}

const HashTable<Rc<Attachment>> &RenderQueue::getAttachments() const {
	return _data->attachments;
}

const HashTable<Rc<Resource>> &RenderQueue::getLinkedResources() const {
	return _data->linked;
}

Rc<Resource> RenderQueue::getInternalResource() const {
	return _data->resource;
}

const memory::vector<Attachment *> &RenderQueue::getInputAttachments() const {
	return _data->input;
}

const memory::vector<Attachment *> &RenderQueue::getOutputAttachments() const {
	return _data->output;
}

const RenderPassData *RenderQueue::getPass(StringView key) const {
	return _data->passes.get(key);
}

const ProgramData *RenderQueue::getProgram(StringView key) const {
	return _data->programs.get(key);
}

const PipelineData *RenderQueue::getPipeline(StringView key) const {
	return _data->pipelines.get(key);
}

const Attachment *RenderQueue::getAttachment(StringView key) const {
	return _data->attachments.get(key);
}

Vector<Rc<Attachment>> RenderQueue::getOutput() const {
	Vector<Rc<Attachment>> ret; ret.reserve(_data->output.size());
	for (auto &it : _data->output) {
		ret.emplace_back(it);
	}
	return ret;
}

Vector<Rc<Attachment>> RenderQueue::getOutput(AttachmentType t) const {
	Vector<Rc<Attachment>> ret;
	for (auto &it : _data->output) {
		if (it->getType() == t) {
			ret.emplace_back(it);
		}
	}
	return ret;
}

uint64_t RenderQueue::incrementOrder() {
	auto ret = _data->order;
	++ _data->order;
	return ret;
}

bool RenderQueue::prepare(Device &dev) {
	memory::pool::context ctx(_data->pool);

	Vector<MaterialType> materialTypes;

	// fill attachment descriptors
	for (auto &attachment : _data->attachments) {
		attachment->sortDescriptors(*this, dev);

		if (auto a = dynamic_cast<gl::MaterialAttachment *>(attachment.get())) {
			auto t = a->getType();
			auto lb = std::lower_bound(materialTypes.begin(), materialTypes.end(), t);
			if (lb != materialTypes.end() && *lb == t) {
				log::vtext("RenderQueue", "Duplicate MaterialType in queue from attachment: ", attachment->getName());
			} else {
				materialTypes.emplace(lb, t);
			}
		}
	}


	RenderQueue_buildLoadStore(_data);
	RenderQueue_buildDescriptors(_data, dev);

	for (auto &it : _data->passes) {
		it->renderPass->prepare(dev);
	}

	return true;
}

void RenderQueue::beginFrame(gl::FrameRequest &frame) {
	if (_data->beginCallback) {
		_data->beginCallback(frame);
	}
}

void RenderQueue::endFrame(gl::FrameRequest &frame) {
	if (_data->endCallback) {
		_data->endCallback(frame);
	}
}

void RenderQueue::enable(const Swapchain *swapchain) {
	if (_data->enableCallback) {
		_data->enableCallback(swapchain);
	}
}

void RenderQueue::disable() {
	if (_data->disableCallback) {
		_data->disableCallback();
	}
}

bool RenderQueue::usesSamplers() const {
	for (auto &it : _data->passes) {
		if (it->usesSamplers) {
			return true;
		}
	}
	return false;
}

void RenderQueue::addCacheStorage(FrameCacheStorage *storage) const {
	_data->frameCache.emplace(storage);
}

void RenderQueue::removeCacheStorage(const FrameCacheStorage *storage) const {
	auto it = _data->frameCache.find(storage);
	if (it != _data->frameCache.end()) {
		_data->frameCache.erase(it);
	}
}

RenderQueue::Builder::Builder(StringView name, Mode mode) {
	auto p = memory::pool::create((memory::pool_t *)nullptr);
	memory::pool::push(p);
	_data = new (p) QueueData;
	_data->pool = p;
	_data->mode = mode;
	_data->key = name.pdup(p);
	memory::pool::pop();
}

RenderQueue::Builder::~Builder() {
	if (_data) {
		auto p = _data->pool;
		memory::pool::destroy(p);
		_data = nullptr;
	}
}

void RenderQueue::Builder::setMode(Mode mode) {
	_data->mode = mode;
}

RenderPassData * RenderQueue::Builder::addRenderPass(const Rc<RenderPass> &renderPass) {
	auto it = _data->passes.find(renderPass->getName());
	if (renderPass->getData() == nullptr && it == _data->passes.end()) {
		memory::pool::push(_data->pool);
		auto ret = new (_data->pool) RenderPassData();
		ret->key = StringView(renderPass->getName()).pdup(_data->pool);
		ret->subpasses.reserve(renderPass->getSubpassCount());
		for (size_t i = 0; i < renderPass->getSubpassCount(); ++ i) {
			auto &it = ret->subpasses.emplace_back();
			it.index = i;
			it.renderPass = ret;
		}
		ret->ordering = renderPass->getOrdering();
		ret->renderPass = renderPass;
		_data->passes.emplace(ret);
		memory::pool::pop();
		return ret;
	} else {
		log::vtext("Gl-Error", "RenderPass for name already defined: ", renderPass->getName());
	}
	return nullptr;
}

static bool subpass_attachment_exists(memory::vector<ImageAttachmentRef *> &vec, ImageAttachmentDescriptor *ref) {
	for (auto &it : vec) {
		if (it->getDescriptor() == ref) {
			return true;
		}
	}
	return false;
}

template <typename T>
inline T * emplaceAttachment(RenderPassData *pass, T *val) {
	T *ret = nullptr;

	auto lb = std::find(pass->descriptors.begin(), pass->descriptors.end(), val);
	if (lb == pass->descriptors.end()) {
		ret = (T *)pass->descriptors.emplace_back(std::move(val));
	} else {
		ret = (T *)(*lb);
	}

	return ret;
}

AttachmentRef *RenderQueue::Builder::addPassInput(const Rc<RenderPass> &p, uint32_t subpassIdx,
		const Rc<BufferAttachment> &attachment, AttachmentDependencyInfo info) {
	auto pass = getPassData(p);
	if (!pass) {
		log::vtext("Gl-Error", "RenderPass '", p->getName(),"' was not added to render queue '", _data->key, "'");
		return nullptr;
	}

	memory::pool::context ctx(_data->pool);
	if (subpassIdx >= pass->subpasses.size()) {
		log::vtext("Gl-Error", "Invalid subpass index: '", subpassIdx, "' for pass '", pass->key ,"'");
		return nullptr;
	}

	auto emplaced = _data->attachments.emplace(attachment).second;
	if (emplaced) {
		attachment->setIndex(_data->attachments.size() - 1);
	}

	auto desc = emplaceAttachment(pass, attachment->addBufferDescriptor(pass));
	if (auto ref = desc->addBufferRef(subpassIdx, AttachmentUsage::Input, info)) {
		pass->subpasses[subpassIdx].inputBuffers.emplace_back(ref);
		return ref;
	}

	log::vtext("Gl-Error", "Attachment '", attachment->getName(), "' is already added to subpass '", pass->key ,"' input");
	return nullptr;
}

AttachmentRef *RenderQueue::Builder::addPassOutput(const Rc<RenderPass> &p, uint32_t subpassIdx,
		const Rc<BufferAttachment> &attachment, AttachmentDependencyInfo info) {
	auto pass = getPassData(p);
	if (!pass) {
		log::vtext("Gl-Error", "RenderPass '", p->getName(),"' was not added to render queue '", _data->key, "'");
		return nullptr;
	}

	memory::pool::context ctx(_data->pool);
	if (subpassIdx >= pass->subpasses.size()) {
		log::vtext("Gl-Error", "Invalid subpass index: '", subpassIdx, "' for pass '", pass->key ,"'");
		return nullptr;
	}

	auto emplaced = _data->attachments.emplace(attachment).second;
	if (emplaced) {
		attachment->setIndex(_data->attachments.size() - 1);
	}

	auto desc = emplaceAttachment(pass, attachment->addBufferDescriptor(pass));
	if (auto ref = desc->addBufferRef(subpassIdx, AttachmentUsage::Output, info)) {
		pass->subpasses[subpassIdx].outputBuffers.emplace_back(ref);
		return ref;
	}

	log::vtext("Gl-Error", "Attachment '", attachment->getName(), "' is already added to subpass '", pass->key ,"' output");
	return nullptr;
}

AttachmentRef *RenderQueue::Builder::addPassInput(const Rc<RenderPass> &p, uint32_t subpassIdx,
		const Rc<GenericAttachment> &attachment, AttachmentDependencyInfo info) {
	auto pass = getPassData(p);
	if (!pass) {
		log::vtext("Gl-Error", "RenderPass '", p->getName(),"' was not added to render queue '", _data->key, "'");
		return nullptr;
	}

	memory::pool::context ctx(_data->pool);
	if (subpassIdx >= pass->subpasses.size()) {
		log::vtext("Gl-Error", "Invalid subpass index: '", subpassIdx, "' for pass '", pass->key ,"'");
		return nullptr;
	}

	auto emplaced = _data->attachments.emplace(attachment).second;
	if (emplaced) {
		attachment->setIndex(_data->attachments.size() - 1);
	}

	auto desc = emplaceAttachment(pass, attachment->addDescriptor(pass));
	if (auto ref = desc->addRef(subpassIdx, AttachmentUsage::Input, info)) {
		pass->subpasses[subpassIdx].inputGenerics.emplace_back(ref);
		return ref;
	}

	log::vtext("Gl-Error", "Attachment '", attachment->getName(), "' is already added to subpass '", pass->key ,"' input");
	return nullptr;
}

AttachmentRef *RenderQueue::Builder::addPassOutput(const Rc<RenderPass> &p, uint32_t subpassIdx,
		const Rc<GenericAttachment> &attachment, AttachmentDependencyInfo info) {
	auto pass = getPassData(p);
	if (!pass) {
		log::vtext("Gl-Error", "RenderPass '", p->getName(),"' was not added to render queue '", _data->key, "'");
		return nullptr;
	}

	memory::pool::context ctx(_data->pool);
	if (subpassIdx >= pass->subpasses.size()) {
		log::vtext("Gl-Error", "Invalid subpass index: '", subpassIdx, "' for pass '", pass->key ,"'");
		return nullptr;
	}

	auto emplaced = _data->attachments.emplace(attachment).second;
	if (emplaced) {
		attachment->setIndex(_data->attachments.size() - 1);
	}

	auto desc = emplaceAttachment(pass, attachment->addDescriptor(pass));
	if (auto ref = desc->addRef(subpassIdx, AttachmentUsage::Output, info)) {
		pass->subpasses[subpassIdx].outputGenerics.emplace_back(ref);
		return ref;
	}

	log::vtext("Gl-Error", "Attachment '", attachment->getName(), "' is already added to subpass '", pass->key ,"' output");
	return nullptr;
}

ImageAttachmentRef *RenderQueue::Builder::addPassInput(const Rc<RenderPass> &p, uint32_t subpassIdx,
		const Rc<ImageAttachment> &attachment, AttachmentDependencyInfo info) {
	auto pass = getPassData(p);
	if (!pass) {
		log::vtext("Gl-Error", "RenderPass '", p->getName(),"' was not added to render queue '", _data->key, "'");
		return nullptr;
	}

	memory::pool::context ctx(_data->pool);
	if (subpassIdx >= pass->subpasses.size()) {
		log::vtext("Gl-Error", "Invalid subpass index: '", subpassIdx, "' for pass '", pass->key ,"'");
		return nullptr;
	}

	auto emplaced = _data->attachments.emplace(attachment).second;
	if (emplaced) {
		attachment->setIndex(_data->attachments.size() - 1);
	}
	auto desc = emplaceAttachment(pass, attachment->addImageDescriptor(pass));
	if (auto ref = desc->addImageRef(subpassIdx, AttachmentUsage::Input, AttachmentLayout::Ignored, info)) {
		pass->subpasses[subpassIdx].inputImages.emplace_back(ref);
		return ref;
	}

	log::vtext("Gl-Error", "Attachment '", attachment->getName(), "' is already added to subpass '", pass->key ,"' input");
	return nullptr;
}

ImageAttachmentRef *RenderQueue::Builder::addPassOutput(const Rc<RenderPass> &p, uint32_t subpassIdx,
		const Rc<ImageAttachment> &attachment, AttachmentDependencyInfo info) {
	auto pass = getPassData(p);
	if (!pass) {
		log::vtext("Gl-Error", "RenderPass '", p->getName(),"' was not added to render queue '", _data->key, "'");
		return nullptr;
	}

	memory::pool::context ctx(_data->pool);
	if (subpassIdx >= pass->subpasses.size()) {
		log::vtext("Gl-Error", "Invalid subpass index: '", subpassIdx, "' for pass '", pass->key ,"'");
		return nullptr;
	}

	auto emplaced = _data->attachments.emplace(attachment).second;
	if (emplaced) {
		attachment->setIndex(_data->attachments.size() - 1);
	}
	auto desc = emplaceAttachment(pass, attachment->addImageDescriptor(pass));
	if (auto ref = desc->addImageRef(subpassIdx, AttachmentUsage::Output, AttachmentLayout::Ignored, info)) {
		pass->subpasses[subpassIdx].outputImages.emplace_back(ref);
		return ref;
	}

	log::vtext("Gl-Error", "Attachment '", attachment->getName(), "' is already added to subpass '", pass->key ,"' output");
	return nullptr;
}

Pair<ImageAttachmentRef *, ImageAttachmentRef *> RenderQueue::Builder::addPassResolve(const Rc<RenderPass> &p,
		uint32_t subpassIdx, const Rc<ImageAttachment> &color, const Rc<ImageAttachment> &resolve,
		AttachmentDependencyInfo colorDep, AttachmentDependencyInfo resolveDep) {
	auto pass = getPassData(p);
	if (!pass) {
		log::vtext("Gl-Error", "RenderPass '", p->getName(),"' was not added to render queue '", _data->key, "'");
		return pair(nullptr, nullptr);
	}

	memory::pool::context ctx(_data->pool);
	if (subpassIdx >= pass->subpasses.size()) {
		log::vtext("Gl-Error", "Invalid subpass index: '", subpassIdx, "' for pass '", pass->key ,"'");
		return pair(nullptr, nullptr);
	}

	switch (color->getType()) {
	case AttachmentType::Buffer:
	case AttachmentType::Generic:
		log::vtext("Gl-Error", "Attachment '", color->getName(), "' can not be resolved output attachment for pass '", pass->key ,"'");
		return pair(nullptr, nullptr);
		break;
	case AttachmentType::Image:
		break;
	}

	if (resolve->getType() != AttachmentType::Image) {
		log::vtext("Gl-Error", "Buffer attachment '", resolve->getName(), "' can not be resolve attachment for pass '", pass->key ,"'");
		return pair(nullptr, nullptr);
	}

	auto emplacedColor = _data->attachments.emplace(color).second;
	if (emplacedColor) {
		color->setIndex(_data->attachments.size() - 1);
	}
	auto emplacedResolve = _data->attachments.emplace(resolve).second;
	if (emplacedResolve) {
		resolve->setIndex(_data->attachments.size() - 1);
	}

	auto colorDesc = emplaceAttachment(pass, color->addImageDescriptor(pass));
	auto resolveDesc = emplaceAttachment(pass, resolve->addImageDescriptor(pass));

	if (subpass_attachment_exists(pass->subpasses[subpassIdx].outputImages, colorDesc)) {
		log::vtext("Gl-Error", "Attachment '", color->getName(), "' is already added to subpass '", pass->key ,"' output");
		return pair(nullptr, nullptr);
	}

	if (subpass_attachment_exists(pass->subpasses[subpassIdx].resolveImages, resolveDesc)) {
		log::vtext("Gl-Error", "Attachment '", resolve->getName(), "' is already added to subpass '", pass->key ,"' resolves");
		return pair(nullptr, nullptr);
	}

	auto colorRef = colorDesc->addImageRef(subpassIdx, AttachmentUsage::Output, AttachmentLayout::Ignored, colorDep);
	if (!colorRef) {
		log::vtext("Gl-Error", "Fail to add attachment '", color->getName(), "' into subpass '", pass->key ,"' output");
	}

	auto resolveRef = colorDesc->addImageRef(subpassIdx, AttachmentUsage::Resolve, AttachmentLayout::Ignored, resolveDep);
	if (!resolveRef) {
		log::vtext("Gl-Error", "Fail to add attachment '", resolve->getName(), "' into subpass '", pass->key ,"' resolves");
	}

	pass->subpasses[subpassIdx].outputImages.emplace_back(colorRef);
	pass->subpasses[subpassIdx].resolveImages.resize(pass->subpasses[subpassIdx].outputImages.size() - 1, nullptr);
	pass->subpasses[subpassIdx].resolveImages.emplace_back(resolveRef);
	return pair(colorRef, resolveRef);
}

ImageAttachmentRef * RenderQueue::Builder::addPassDepthStencil(const Rc<RenderPass> &p,
		uint32_t subpassIdx, const Rc<ImageAttachment> &attachment, AttachmentDependencyInfo info) {
	auto pass = getPassData(p);
	if (!pass) {
		log::vtext("Gl-Error", "RenderPass '", p->getName(),"' was not added to render queue '", _data->key, "'");
		return nullptr;
	}

	memory::pool::context ctx(_data->pool);
	if (subpassIdx >= pass->subpasses.size()) {
		log::vtext("Gl-Error", "Invalid subpass index: '", subpassIdx, "' for pass '", pass->key ,"'");
		return nullptr;
	}

	switch (attachment->getType()) {
	case AttachmentType::Buffer:
	case AttachmentType::Generic:
		log::vtext("Gl-Error", "Attachment '", attachment->getName(), "' can not be depth/stencil attachment for pass '", pass->key ,"'");
		return nullptr;
		break;
	case AttachmentType::Image:
		break;
	}

	if (pass->subpasses[subpassIdx].depthStencil) {
		log::vtext("Gl-Error", "Depth/stencil attachment for subpass '", pass->key, "' already defined");
		return nullptr;
	}

	auto emplaced = _data->attachments.emplace(attachment).second;
	if (emplaced) {
		attachment->setIndex(_data->attachments.size() - 1);
	}

	auto desc = emplaceAttachment(pass, attachment->addImageDescriptor(pass));
	if (auto ref = desc->addImageRef(subpassIdx, AttachmentUsage::DepthStencil, AttachmentLayout::Ignored, info)) {
		pass->subpasses[subpassIdx].depthStencil = ref;
		return ref;
	}

	return nullptr;
}

bool RenderQueue::Builder::addSubpassDependency(const Rc<RenderPass> &p,
		uint32_t srcSubpass, PipelineStage srcStage, AccessType srcAccess,
		uint32_t dstSubpass, PipelineStage dstStage, AccessType dstAccess, bool byRegion) {
	auto pass = getPassData(p);
	if (!pass) {
		log::vtext("Gl-Error", "RenderPass '", p->getName(),"' was not added to render queue '", _data->key, "'");
		return false;
	}

	memory::pool::context ctx(_data->pool);
	auto dep = RenderSubpassDependency({srcSubpass, srcStage, srcAccess, dstSubpass, dstStage, dstAccess, byRegion});

	auto it = std::find(pass->dependencies.begin(), pass->dependencies.end(), dep);
	if (it != pass->dependencies.end()) {
		log::vtext("Gl-Error", "Dependency for '", pass->key ,"': ", srcSubpass, " -> ", dstSubpass, " already defined");
		return false;
	}

	pass->dependencies.emplace_back(dep);
	return true;
}

bool RenderQueue::Builder::addInput(const Rc<Attachment> &data, AttachmentOps ops) {
	memory::pool::context ctx(_data->pool);
	auto lb = std::lower_bound(_data->input.begin(), _data->input.end(), data);
	if (lb == _data->input.end()) {
		_data->input.emplace_back(data);
		data->addUsage(AttachmentUsage::Input, ops);
		return true;
	} else if (*lb != data) {
		_data->input.emplace(lb, data);
		data->addUsage(AttachmentUsage::Input, ops);
		return true;
	}

	log::vtext("Gl-Error", "Attachment '", data->getName(), "' is already added to input");
	return false;
}

bool RenderQueue::Builder::addOutput(const Rc<Attachment> &data, AttachmentOps ops) {
	memory::pool::context ctx(_data->pool);
	auto lb = std::lower_bound(_data->output.begin(), _data->output.end(), data);
	if (lb == _data->output.end()) {
		_data->output.emplace_back(data);
		data->addUsage(AttachmentUsage::Output, ops);
		return true;
	} else if (*lb != data) {
		_data->output.emplace(lb, data);
		data->addUsage(AttachmentUsage::Output, ops);
		return true;
	}

	log::vtext("Gl-Error", "Attachment '", data->getName(), "' is already added to output");
	return false;
}

const ProgramData * RenderQueue::Builder::addProgram(StringView key, SpanView<uint32_t> data, const ProgramInfo *info) {
	if (!_data) {
		log::vtext("Resource", "Fail to add shader: ", key, ", not initialized");
		return nullptr;
	}

	if (auto r = Resource_conditionalInsert<ProgramData>(_data->programs, key, [&] () -> ProgramData * {
		auto program = new (_data->pool) ProgramData;
		program->key = key.pdup(_data->pool);
		program->data = data.pdup(_data->pool);
		if (info) {
			program->stage = info->stage;
			program->bindings = info->bindings;
			program->constants = info->constants;
		} else {
			program->inspect(data);
		}
		return program;
	}, _data->pool)) {
		return r;
	}

	log::vtext("Resource", _data->key, ": Shader already added: ", key);
	return nullptr;
}

const ProgramData * RenderQueue::Builder::addProgramByRef(StringView key, SpanView<uint32_t> data, const ProgramInfo *info) {
	if (!_data) {
		log::vtext("Resource", "Fail tom add shader: ", key, ", not initialized");
		return nullptr;
	}

	if (auto r = Resource_conditionalInsert<ProgramData>(_data->programs, key, [&] () -> ProgramData * {
		auto program = new (_data->pool) ProgramData;
		program->key = key.pdup(_data->pool);
		program->data = data;
		if (info) {
			program->stage = info->stage;
			program->bindings = info->bindings;
			program->constants = info->constants;
		} else {
			program->inspect(data);
		}
		return program;
	}, _data->pool)) {
		return r;
	}

	log::vtext("Resource", _data->key, ": Shader already added: ", key);
	return nullptr;
}

const ProgramData * RenderQueue::Builder::addProgram(StringView key, const memory::function<void(const ProgramData::DataCallback &)> &cb,
		const ProgramInfo *info) {
	if (!_data) {
		log::vtext("Resource", "Fail to add shader: ", key, ", not initialized");
		return nullptr;
	}

	if (auto r = Resource_conditionalInsert<ProgramData>(_data->programs, key, [&] () -> ProgramData * {
		auto program = new (_data->pool) ProgramData;
		program->key = key.pdup(_data->pool);
		program->callback = cb;
		if (info) {
			program->stage = info->stage;
			program->bindings = info->bindings;
			program->constants = info->constants;
		} else {
			cb([&] (SpanView<uint32_t> data) {
				program->inspect(data);
			});
		}
		return program;
	}, _data->pool)) {
		return r;
	}

	log::vtext("Resource", _data->key, ": Shader already added: ", key);
	return nullptr;
}

void RenderQueue::Builder::setInternalResource(Rc<Resource> &&res) {
	if (!_data) {
		log::vtext("Resource", "Fail to set internal resource: ", res->getName(), ", not initialized");
		return;
	}
	if (_data->resource) {
		log::vtext("Resource", "Fail to set internal resource: resource already defined");
		return;
	}
	if (res->getOwner() != nullptr) {
		log::vtext("Resource", "Fail to set internal resource: ", res->getName(), ", already owned by ", res->getOwner()->getName());
		return;
	}
	_data->resource = move(res);
}

void RenderQueue::Builder::addLinkedResource(const Rc<Resource> &res) {
	if (!_data) {
		log::vtext("Resource", "Fail to add linked resource: ", res->getName(), ", not initialized");
		return;
	}
	if (res->getOwner() != nullptr) {
		log::vtext("Resource", "Fail to add linked resource: ", res->getName(), ", it's owned by ", res->getOwner()->getName());
		return;
	}
	if (!res->isCompiled()) {
		log::vtext("Resource", "Fail to add linked resource: ", res->getName(), ", resource is not compiled");
		return;
	}
	_data->linked.emplace(res);
}

void RenderQueue::Builder::setBeginCallback(Function<void(gl::FrameRequest &)> &&cb) {
	_data->beginCallback = move(cb);
}

void RenderQueue::Builder::setEndCallback(Function<void(gl::FrameRequest &)> &&cb) {
	_data->endCallback = move(cb);
}

void RenderQueue::Builder::setEnableCallback(Function<void(const Swapchain *)> &&cb) {
	_data->enableCallback = move(cb);
}

void RenderQueue::Builder::setDisableCallback(Function<void()> &&cb) {
	_data->disableCallback = move(cb);
}

PipelineData *RenderQueue::Builder::emplacePipeline(const Rc<RenderPass> &d, uint32_t subpass, StringView key) {
	auto pass = getSubpassData(d, subpass);
	if (!pass) {
		log::vtext("Gl-Error", "RenderPass '", d->getName(),"' was not added to render queue '", _data->key, "'");
		return nullptr;
	}

	if (!_data) {
		log::vtext("Resource", "Fail tom add pipeline: ", key, ", not initialized");
		return nullptr;
	}

	auto it = _data->pipelines.find(key);
	if (it != _data->pipelines.end()) {
		log::vtext("Resource", _data->key, ": Pipeline '", key, "' already added");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<PipelineData>(pass->pipelines, key, [&] () -> PipelineData * {
		auto pipeline = new (_data->pool) PipelineData;
		pipeline->key = key.pdup(_data->pool);
		pipeline->renderPass = d.get();
		pipeline->subpass = subpass;
		return pipeline;
	}, _data->pool);
	if (!p) {
		log::vtext("Resource", _data->key, ": Pipeline '", key, "' already added to pass '", d->getName(), "'");
		return nullptr;
	}
	if (p) {
		_data->pipelines.emplace(p);
	}
	return p;
}

void RenderQueue::Builder::erasePipeline(const Rc<RenderPass> &p, uint32_t subpass, PipelineData *data) {
	auto pass = getSubpassData(p, subpass);
	if (!pass) {
		log::vtext("Gl-Error", "RenderPass '", p->getName(),"' was not added to render queue '", _data->key, "'");
		return;
	}

	_data->pipelines.erase(data->key);
	pass->pipelines.erase(data->key);
}

bool RenderQueue::Builder::setPipelineOption(PipelineData &f, DynamicState state) {
	f.dynamicState = state;
	return true;
}

bool RenderQueue::Builder::setPipelineOption(PipelineData &f, const Vector<SpecializationInfo> &programs) {
	for (auto &it : programs) {
		auto p = _data->programs.get(it.data->key);
		if (!p) {
			log::vtext("PipelineRequest", _data->key, ": Shader not found in request: ", it.data->key);
			return false;
		}
	}

	f.shaders.reserve(programs.size());
	for (auto &it : programs) {
		f.shaders.emplace_back(move(it));
	}
	return true;
}

bool RenderQueue::Builder::setPipelineOption(PipelineData &f, const PipelineMaterialInfo &info) {
	f.material = info.normalize();
	return true;
}

memory::pool_t *RenderQueue::Builder::getPool() const {
	return _data->pool;
}

RenderPassData *RenderQueue::Builder::getPassData(const Rc<RenderPass> &pass) const {
	auto it = _data->passes.find(pass->getName());
	if (it != _data->passes.end()) {
		if ((*it)->renderPass == pass) {
			return *it;
		}
	}
	return nullptr;
}

RenderSubpassData *RenderQueue::Builder::getSubpassData(const Rc<RenderPass> &pass, uint32_t subpass) const {
	if (auto p = getPassData(pass)) {
		if (subpass < p->subpasses.size()) {
			return &p->subpasses[subpass];
		}
	}
	return nullptr;
}

}
