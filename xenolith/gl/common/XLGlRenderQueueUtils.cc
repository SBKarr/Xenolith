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

namespace stappler::xenolith::gl {

struct RenderQueue::QueueData : NamedMem {
	memory::pool_t *pool = nullptr;
	Mode mode = Mode::RenderOnDemand;
	memory::vector<Attachment *> input;
	memory::vector<Attachment *> output;
	HashTable<Rc<Attachment>> attachments;
	HashTable<RenderPassData *> passes;
	HashTable<ProgramData *> programs;
	HashTable<Rc<Resource>> resources;
	bool compiled = false;

	void clear() {
		for (auto &it : programs) {
			it->program = nullptr;
		}

		for (auto &it : passes) {
			it->framebuffers.clear();
			for (auto &desc : it->descriptors) {
				desc->clear();
			}

			for (auto &pipeline : it->pipelines) {
				pipeline->pipeline = nullptr;
			}
			it->renderPass->invalidate();
			it->renderPass = nullptr;
			it->impl = nullptr;
		}

		for (auto &it : attachments) {
			it->clear();
		}

		resources.clear();
	}
};

static void RenderQueue_buildRenderPaths(RenderQueue &queue, RenderQueue::QueueData *_data) {
	// fill attachment descriptors
	for (auto &attachment : _data->attachments) {
		attachment->sortDescriptors(queue);
	}
}

static void RenderQueue_buildLoadStore(RenderQueue::QueueData *data) {
	for (auto &attachment : data->attachments) {
		if (attachment->getType() == AttachmentType::Buffer) {
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

			if (attachment->getType() == AttachmentType::Buffer) {
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
			if (attachment->getType() == AttachmentType::Buffer) {
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

		if (attachment->getType() == AttachmentType::Buffer) {
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

static void RenderQueue_buildDescriptors(RenderQueue::QueueData *data) {
	for (auto &pass : data->passes) {
		for (auto &attachment : pass->descriptors) {
			auto &desc = attachment->getDescriptor();
			if (desc.type != DescriptorType::Unknown) {
				pass->pipelineLayout.queueDescriptors.emplace_back(&desc);
			}
		}
	}
}

}
