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

#include "XLGlRenderQueue.h"
#include "XLGlRenderQueueUtils.cc"

namespace stappler::xenolith::gl {

RenderQueue::RenderQueue() { }

RenderQueue::~RenderQueue() {
	if (_data) {
		_data->clear();
		auto p = _data->pool;
		memory::pool::destroy(p);
		_data = nullptr;
	}
}

bool RenderQueue::init(Builder && buf) {
	if (buf._data) {
		_data = buf._data;
		buf._data = nullptr;
		return true;
	}
	return false;
}

void RenderQueue::setCompiled(bool value) {
	_data->compiled = value;
}

bool RenderQueue::updateSwapchainInfo(const ImageInfo &info) {
	if (_data && _data->output.size() == 1) {
		auto out = _data->output.front();
		if (out->getType() == AttachmentType::SwapchainImage || out->getType() == AttachmentType::Image) {
			if (out->isCompatible(info)) {
				for (auto &it : _data->attachments) {
					it->onSwapchainUpdate(info);
				}
				return true;
			}
		}
	}
	return false;
}

bool RenderQueue::isPresentable() const {
	return _data && _data->output.size() == 1 && _data->output.front()->getType() == AttachmentType::SwapchainImage;
}

bool RenderQueue::isCompatible(const ImageInfo &info) const {
	if (_data && _data->output.size() == 1) {
		auto out = _data->output.front();
		if (out->getType() == AttachmentType::SwapchainImage || out->getType() == AttachmentType::Image) {
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

const HashTable<Rc<Resource>> &RenderQueue::getResources() const {
	return _data->resources;
}

const HashTable<RenderPassData *> &RenderQueue::getPasses() const {
	return _data->passes;
}

const HashTable<Rc<Attachment>> &RenderQueue::getAttachments() const {
	return _data->attachments;
}

const ProgramData *RenderQueue::getProgram(StringView key) const {
	return _data->programs.get(key);
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

bool RenderQueue::prepare() {
	memory::pool::context ctx(_data->pool);

	RenderQueue_buildRenderPaths(*this, _data);
	RenderQueue_buildLoadStore(_data);
	RenderQueue_buildDescriptors(_data);

	return true;
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

void RenderQueue::Builder::addRenderPass(const Rc<RenderPass> &renderPass) {
	auto it = _data->passes.find(renderPass->getName());
	if (it == _data->passes.end()) {
		memory::pool::push(_data->pool);
		auto ret = new (_data->pool) RenderPassData();
		ret->key = StringView(renderPass->getName()).pdup(_data->pool);
		ret->subpasses.resize(renderPass->getSubpassCount());
		ret->ordering = renderPass->getOrdering();
		ret->renderPass = renderPass;
		_data->passes.emplace(ret);
		memory::pool::pop();
	} else {
		log::vtext("Gl-Error", "RenderPass for name already defined: ", renderPass->getName());
	}
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
	auto lb = std::lower_bound(pass->descriptors.begin(), pass->descriptors.end(), val);
	if (lb == pass->descriptors.end()) {
		pass->descriptors.emplace_back(std::move(val));
		ret = val;
	} else if (*lb != val) {
		(*pass->descriptors.emplace(lb, std::move(val)));
		ret = val;
	} else {
		ret = (T *)(*lb);
	}

	return ret;
}

AttachmentRef *RenderQueue::Builder::addPassInput(const Rc<RenderPass> &p, uint32_t subpassIdx,
		const Rc<BufferAttachment> &attachment) {
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

	_data->attachments.emplace(attachment);
	auto desc = emplaceAttachment(pass, attachment->addBufferDescriptor(pass));
	if (auto ref = desc->addBufferRef(subpassIdx, AttachmentUsage::Input)) {
		pass->subpasses[subpassIdx].inputBuffers.emplace_back(ref);
		return ref;
	}

	log::vtext("Gl-Error", "Attachment '", attachment->getName(), "' is already added to subpass '", pass->key ,"' input");
	return nullptr;
}

AttachmentRef *RenderQueue::Builder::addPassOutput(const Rc<RenderPass> &p, uint32_t subpassIdx,
		const Rc<BufferAttachment> &attachment) {
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

	_data->attachments.emplace(attachment);
	auto desc = emplaceAttachment(pass, attachment->addBufferDescriptor(pass));
	if (auto ref = desc->addBufferRef(subpassIdx, AttachmentUsage::Output)) {
		pass->subpasses[subpassIdx].outputBuffers.emplace_back(ref);
		return ref;
	}

	log::vtext("Gl-Error", "Attachment '", attachment->getName(), "' is already added to subpass '", pass->key ,"' output");
	return nullptr;
}

ImageAttachmentRef *RenderQueue::Builder::addPassInput(const Rc<RenderPass> &p, uint32_t subpassIdx,
		const Rc<ImageAttachment> &attachment) {
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

	_data->attachments.emplace(attachment);
	auto desc = emplaceAttachment(pass, attachment->addImageDescriptor(pass));
	if (auto ref = desc->addImageRef(subpassIdx, AttachmentUsage::Input, AttachmentLayout::Ignored)) {
		pass->subpasses[subpassIdx].inputImages.emplace_back(ref);
		return ref;
	}

	log::vtext("Gl-Error", "Attachment '", attachment->getName(), "' is already added to subpass '", pass->key ,"' input");
	return nullptr;
}

ImageAttachmentRef *RenderQueue::Builder::addPassOutput(const Rc<RenderPass> &p, uint32_t subpassIdx,
		const Rc<ImageAttachment> &attachment) {
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

	_data->attachments.emplace(attachment);
	auto desc = emplaceAttachment(pass, attachment->addImageDescriptor(pass));
	if (auto ref = desc->addImageRef(subpassIdx, AttachmentUsage::Output, AttachmentLayout::Ignored)) {
		pass->subpasses[subpassIdx].outputImages.emplace_back(ref);
		return ref;
	}

	log::vtext("Gl-Error", "Attachment '", attachment->getName(), "' is already added to subpass '", pass->key ,"' output");
	return nullptr;
}

Pair<ImageAttachmentRef *, ImageAttachmentRef *> RenderQueue::Builder::addPassResolve(const Rc<RenderPass> &p,
		uint32_t subpassIdx, const Rc<ImageAttachment> &color, const Rc<ImageAttachment> &resolve) {
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
	case AttachmentType::SwapchainImage:
	case AttachmentType::Buffer:
		log::vtext("Gl-Error", "Attachment '", color->getName(), "' can not be resolved output attachment for pass '", pass->key ,"'");
		return pair(nullptr, nullptr);
		break;
	default:
		break;
	}

	if (resolve->getType() == AttachmentType::Buffer) {
		log::vtext("Gl-Error", "Buffer attachment '", resolve->getName(), "' can not be resolve attachment for pass '", pass->key ,"'");
		return pair(nullptr, nullptr);
	}

	_data->attachments.emplace(color);
	_data->attachments.emplace(resolve);

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

	auto colorRef = colorDesc->addImageRef(subpassIdx, AttachmentUsage::Output, AttachmentLayout::Ignored);
	if (!colorRef) {
		log::vtext("Gl-Error", "Fail to add attachment '", color->getName(), "' into subpass '", pass->key ,"' output");
	}

	auto resolveRef = colorDesc->addImageRef(subpassIdx, AttachmentUsage::Resolve, AttachmentLayout::Ignored);
	if (!resolveRef) {
		log::vtext("Gl-Error", "Fail to add attachment '", resolve->getName(), "' into subpass '", pass->key ,"' resolves");
	}

	pass->subpasses[subpassIdx].outputImages.emplace_back(colorRef);
	pass->subpasses[subpassIdx].resolveImages.resize(pass->subpasses[subpassIdx].outputImages.size() - 1, nullptr);
	pass->subpasses[subpassIdx].resolveImages.emplace_back(resolveRef);
	return pair(colorRef, resolveRef);
}

ImageAttachmentRef * RenderQueue::Builder::addPassDepthStencil(const Rc<RenderPass> &p,
		uint32_t subpassIdx, const Rc<ImageAttachment> &attachment) {
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
	case AttachmentType::SwapchainImage:
	case AttachmentType::Buffer:
		log::vtext("Gl-Error", "Attachment '", attachment->getName(), "' can not be depth/stencil attachment for pass '", pass->key ,"'");
		return nullptr;
		break;
	default:
		break;
	}

	if (pass->subpasses[subpassIdx].depthStencil) {
		log::vtext("Gl-Error", "Depth/stencil attachment for subpass '", pass->key, "' already defined");
		return nullptr;
	}

	_data->attachments.emplace(attachment);

	auto desc = emplaceAttachment(pass, attachment->addImageDescriptor(pass));
	if (auto ref = desc->addImageRef(subpassIdx, AttachmentUsage::DepthStencil, AttachmentLayout::Ignored)) {
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

bool RenderQueue::Builder::addResource(const Rc<Resource> &resource) {
	auto it = _data->resources.find(resource->getName());
	if (it == _data->resources.end()) {
		memory::pool::push(_data->pool);
		_data->resources.emplace(resource);
		memory::pool::pop();
		return true;
	} else {
		log::vtext("Gl-Error", "Resource for name already defined: ", resource->getName());
	}
	return false;
}

const ProgramData * RenderQueue::Builder::addProgram(StringView key, ProgramStage stage, SpanView<uint32_t> data) {
	if (!_data) {
		log::vtext("Resource", "Fail to add shader: ", key, ", not initialized");
		return nullptr;
	}

	if (auto r = Resource_conditionalInsert<ProgramData>(_data->programs, key, [&] () -> ProgramData * {
		auto program = new (_data->pool) ProgramData;
		program->key = key.pdup(_data->pool);
		program->data = data.pdup(_data->pool);
		program->stage = stage;
		return program;
	}, _data->pool)) {
		return r;
	}

	log::vtext("Resource", _data->key, ": Shader already added: ", key);
	return nullptr;
}

const ProgramData * RenderQueue::Builder::addProgramByRef(StringView key, ProgramStage stage, SpanView<uint32_t> data) {
	if (!_data) {
		log::vtext("Resource", "Fail tom add shader: ", key, ", not initialized");
		return nullptr;
	}

	if (auto r = Resource_conditionalInsert<ProgramData>(_data->programs, key, [&] () -> ProgramData * {
		auto program = new (_data->pool) ProgramData;
		program->key = key.pdup(_data->pool);
		program->data = data;
		program->stage = stage;
		return program;
	}, _data->pool)) {
		return r;
	}

	log::vtext("Resource", _data->key, ": Shader already added: ", key);
	return nullptr;
}

const ProgramData * RenderQueue::Builder::addProgram(StringView key, ProgramStage stage, const memory::function<void(const ProgramData::DataCallback &)> &cb) {
	if (!_data) {
		log::vtext("Resource", "Fail to add shader: ", key, ", not initialized");
		return nullptr;
	}

	if (auto r = Resource_conditionalInsert<ProgramData>(_data->programs, key, [&] () -> ProgramData * {
		auto program = new (_data->pool) ProgramData;
		program->key = key.pdup(_data->pool);
		program->callback = cb;
		program->stage = stage;
		return program;
	}, _data->pool)) {
		return r;
	}

	log::vtext("Resource", _data->key, ": Shader already added: ", key);
	return nullptr;
}

PipelineData *RenderQueue::Builder::emplacePipeline(const Rc<RenderPass> &d, StringView key) {
	auto pass = getPassData(d);
	if (!pass) {
		log::vtext("Gl-Error", "RenderPass '", d->getName(),"' was not added to render queue '", _data->key, "'");
		return nullptr;
	}

	if (!_data) {
		log::vtext("Resource", "Fail tom add pipeline: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<PipelineData>(pass->pipelines, key, [&] () -> PipelineData * {
		auto pipeline = new (_data->pool) PipelineData;
		pipeline->key = key.pdup(_data->pool);
		return pipeline;
	}, _data->pool);
	if (!p) {
		log::vtext("Resource", _data->key, ": Pipeline '", key, "' already added to pass '", pass->key, "'");
		return nullptr;
	}
	return p;
}

void RenderQueue::Builder::erasePipeline(const Rc<RenderPass> &p, PipelineData *data) {
	auto pass = getPassData(p);
	if (!pass) {
		log::vtext("Gl-Error", "RenderPass '", p->getName(),"' was not added to render queue '", _data->key, "'");
		return;
	}

	pass->pipelines.erase(data->key);
}

bool RenderQueue::Builder::setPipelineOption(PipelineData &f, DynamicState state) {
	f.dynamicState = state;
	return true;
}

bool RenderQueue::Builder::setPipelineOption(PipelineData &f, const Vector<const ProgramData *> &programs) {
	for (auto &it : programs) {
		auto p = _data->programs.get(it->key);
		if (!p) {
			log::vtext("PipelineRequest", _data->key, ": Shader not found in request: ", it->key);
			return false;
		}
	}

	f.shaders.reserve(programs.size());
	for (auto &it : programs) {
		f.shaders.emplace_back(it->key.pdup(_data->pool));
	}
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

}
