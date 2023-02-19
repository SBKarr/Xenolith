/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "XLGlCommandList.h"
#include "XLVkMaterialRenderPass.h"
#include "XLVkDevice.h"
#include "XLVkFramebuffer.h"
#include "XLVkRenderPassImpl.h"
#include "XLVkPipeline.h"
#include "XLVkBuffer.h"

#include "XLDefaultShaders.h"

namespace stappler::xenolith::vk {

struct MaterialPass_Data {
	Rc<ShadowLightDataAttachment> lightDataInput;
	Rc<ShadowImageArrayAttachment> lightBufferArray;
};

static void MaterialPass_makeShadowPass(MaterialPass_Data &data, renderqueue::Queue::Builder &builder, Application &app, Extent2 defaultExtent) {
	using namespace renderqueue;

	auto pass = Rc<vk::ShadowPass>::create("ShadowPass", RenderOrdering(0));
	builder.addRenderPass(pass);

	builder.addComputePipeline(pass, ShadowPass::SdfTrianglesComp,
			builder.addProgramByRef("ShadowPass_SdfTrianglesComp", xenolith::shaders::SdfTrianglesComp));
	builder.addComputePipeline(pass, ShadowPass::SdfImageComp,
			builder.addProgramByRef("ShadowPass_SdfImageComp", xenolith::shaders::SdfImageComp));

	data.lightDataInput = Rc<ShadowLightDataAttachment>::create("ShadowLightDataAttachment");
	auto vertexInput = Rc<ShadowVertexAttachment>::create("ShadowVertexAttachment");
	auto triangles = Rc<ShadowTrianglesAttachment>::create("ShadowTrianglesAttachment");

	data.lightBufferArray = Rc<ShadowImageArrayAttachment>::create("LightArray", defaultExtent);

	builder.addPassInput(pass, 0, data.lightDataInput, AttachmentDependencyInfo());
	builder.addPassInput(pass, 0, vertexInput, AttachmentDependencyInfo());
	builder.addPassOutput(pass, 0, triangles, AttachmentDependencyInfo());

	builder.addPassOutput(pass, 0, data.lightBufferArray, AttachmentDependencyInfo{
		PipelineStage::ComputeShader, AccessType::ShaderWrite,
		PipelineStage::ComputeShader, AccessType::ShaderWrite,

		// can be reused after RenderPass is submitted
		FrameRenderPassState::Submitted,
	}, DescriptorType::StorageImage, AttachmentLayout::General);

	// define global input-output
	builder.addInput(data.lightDataInput);
	builder.addInput(vertexInput);

	// requires same input as light data
	builder.addInput(data.lightBufferArray);
}

static void MaterialPass_makePresentPass(MaterialPass_Data &data, renderqueue::Queue::Builder &builder, Application &app, Extent2 defaultExtent) {
	using namespace renderqueue;

	// load shaders by ref - do not copy data into engine
	auto materialFrag = builder.addProgramByRef("Loader_MaterialVert", xenolith::shaders::MaterialVert);
	auto materialVert = builder.addProgramByRef("Loader_MaterialFrag", xenolith::shaders::MaterialFrag);

	// render-to-swapchain RenderPass
	auto pass = Rc<vk::MaterialPass>::create("MaterialSwapchainPass", RenderOrderingHighest);
	builder.addRenderPass(pass);

	auto shaderSpecInfo = Vector<SpecializationInfo>({
		// no specialization required for vertex shader
		materialVert,
		// specialization for fragment shader - use platform-dependent array sizes
		SpecializationInfo(materialFrag, Vector<PredefinedConstant>{
			PredefinedConstant::SamplersArraySize,
			PredefinedConstant::TexturesArraySize
		})
	});

	// pipelines for material-besed rendering
	auto materialPipeline = builder.addGraphicPipeline(pass, 0, "Solid", shaderSpecInfo, PipelineMaterialInfo({
		BlendInfo(),
		DepthInfo(true, true, gl::CompareOp::Less)
	}));
	auto transparentPipeline = builder.addGraphicPipeline(pass, 0, "Transparent", shaderSpecInfo, PipelineMaterialInfo({
		BlendInfo(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha, gl::BlendOp::Add,
				gl::BlendFactor::Zero, gl::BlendFactor::One, gl::BlendOp::Add),
		DepthInfo(false, true, gl::CompareOp::LessOrEqual)
	}));

	// pipeline for debugging - draw lines instead of triangles
	builder.addGraphicPipeline(pass, 0, "DebugTriangles", shaderSpecInfo, PipelineMaterialInfo(
		BlendInfo(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha, gl::BlendOp::Add,
				gl::BlendFactor::Zero, gl::BlendFactor::One, gl::BlendOp::Add),
		DepthInfo(false, true, gl::CompareOp::Less),
		LineWidth(1.0f)
	));

	// depth buffer - temporary/transient
	auto depth = Rc<vk::ImageAttachment>::create("CommonDepth",
		gl::ImageInfo(
			defaultExtent,
			gl::ForceImageUsage(gl::ImageUsage::DepthStencilAttachment),
			MaterialPass::selectDepthFormat(app.getGlLoop()->getSupportedDepthStencilFormat())),
		ImageAttachment::AttachmentInfo{
			.initialLayout = AttachmentLayout::Undefined,
			.finalLayout = AttachmentLayout::DepthStencilAttachmentOptimal,
			.clearOnLoad = true,
			.clearColor = Color4F::WHITE
	});

	// swapchain output
	auto out = Rc<vk::ImageAttachment>::create("Output",
		gl::ImageInfo(
			defaultExtent,
			gl::ForceImageUsage(gl::ImageUsage::ColorAttachment),
			platform::graphic::getCommonFormat()),
		ImageAttachment::AttachmentInfo{
			.initialLayout = AttachmentLayout::Undefined,
			.finalLayout = AttachmentLayout::PresentSrc,
			.clearOnLoad = true,
			.clearColor = Color4F(1.0f, 1.0f, 1.0f, 1.0f) // Color4F::BLACK;
	});

	// Material input attachment - per-scene list of materials
	auto &cache = app.getResourceCache();
	auto materialInput = Rc<vk::MaterialAttachment>::create("MaterialInput",
		gl::BufferInfo(gl::BufferUsage::StorageBuffer),

		// ... with predefined list of materials
		Vector<Rc<gl::Material>>({
			Rc<gl::Material>::create(gl::Material::MaterialIdInitial, materialPipeline, cache->getEmptyImage(), ColorMode::IntensityChannel),
			Rc<gl::Material>::create(gl::Material::MaterialIdInitial, materialPipeline, cache->getSolidImage(), ColorMode::IntensityChannel),
			Rc<gl::Material>::create(gl::Material::MaterialIdInitial, transparentPipeline, cache->getEmptyImage(), ColorMode()),
			Rc<gl::Material>::create(gl::Material::MaterialIdInitial, transparentPipeline, cache->getSolidImage(), ColorMode()),
			//Rc<gl::Material>::create(gl::Material::MaterialIdInitial, surfacePipeline, cache->getEmptyImage(), ColorMode()),
			//Rc<gl::Material>::create(gl::Material::MaterialIdInitial, surfacePipeline, cache->getSolidImage(), ColorMode())
		})
	);

	// Vertex input attachment - per-frame vertex list
	auto vertexInput = Rc<vk::VertexMaterialAttachment>::create("VertexInput",
			gl::BufferInfo(gl::BufferUsage::StorageBuffer), materialInput);

	// define pass input-output
	builder.addPassInput(pass, 0, vertexInput, AttachmentDependencyInfo()); // 0
	builder.addPassInput(pass, 0, materialInput, AttachmentDependencyInfo()); // 1
	if (data.lightDataInput) {
		builder.addPassInput(pass, 0, data.lightDataInput, AttachmentDependencyInfo()); // 2
	}
	if (data.lightBufferArray) {
		auto shadowVert = builder.addProgramByRef("ShadowMergeVert", xenolith::shaders::ShadowMergeVert);
		auto shadowFrag = builder.addProgramByRef("ShadowMergeFrag", xenolith::shaders::ShadowMergeFrag);
		auto shadowNullFrag = builder.addProgramByRef("ShadowMergeNullFrag", xenolith::shaders::ShadowMergeNullFrag);

		builder.addGraphicPipeline(pass, 0, MaterialPass::ShadowPipeline, Vector<SpecializationInfo>({
			// no specialization required for vertex shader
			shadowVert,
			// specialization for fragment shader - use platform-dependent array sizes
			SpecializationInfo(shadowFrag, Vector<PredefinedConstant>{
				PredefinedConstant::SamplersArraySize,
			})
		}), PipelineMaterialInfo({
			BlendInfo(gl::BlendFactor::Zero, gl::BlendFactor::SrcColor, gl::BlendOp::Add,
					gl::BlendFactor::Zero, gl::BlendFactor::One, gl::BlendOp::Add),
			DepthInfo()
		}));
		builder.addGraphicPipeline(pass, 0, MaterialPass::ShadowPipelineNull, Vector<SpecializationInfo>({
			// no specialization required for vertex shader
			shadowVert,
			shadowNullFrag,
		}), PipelineMaterialInfo({
			BlendInfo(gl::BlendFactor::Zero, gl::BlendFactor::SrcColor, gl::BlendOp::Add,
					gl::BlendFactor::Zero, gl::BlendFactor::One, gl::BlendOp::Add),
			DepthInfo()
		}));

		builder.addPassInput(pass, 0, data.lightBufferArray, AttachmentDependencyInfo{ // 3
			PipelineStage::FragmentShader, AccessType::ShaderRead,
			PipelineStage::FragmentShader, AccessType::ShaderRead,

			// can be reused after RenderPass is submitted
			FrameRenderPassState::Submitted,
			FrameRenderPassState::Submission
		}, DescriptorType::SampledImage, AttachmentLayout::ShaderReadOnlyOptimal);
	}

	builder.addPassDepthStencil(pass, 0, depth, AttachmentDependencyInfo{
		PipelineStage::EarlyFragmentTest,
			AccessType::DepthStencilAttachmentRead | AccessType::DepthStencilAttachmentWrite,
		PipelineStage::LateFragmentTest,
			AccessType::DepthStencilAttachmentRead | AccessType::DepthStencilAttachmentWrite,

		// can be reused after RenderPass is submitted
		FrameRenderPassState::Submitted,
	});
	builder.addPassOutput(pass, 0, out, AttachmentDependencyInfo{
		// first used as color attachment to output colors
		PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,

		// last used the same way (the only usage for this attachment)
		PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,

		// can be reused after RenderPass is submitted
		FrameRenderPassState::Submitted,
	}, DescriptorType::Attachment, AttachmentLayout::Ignored);

	builder.addInput(vertexInput);
	builder.addOutput(out);
}

bool MaterialPass::makeDefaultRenderQueue(RenderQueueInfo &info) {
	MaterialPass_Data data;

	if (info.withShadows) {
		MaterialPass_makeShadowPass(data, *info.builder, *info.app, info.extent);
	}
	MaterialPass_makePresentPass(data, *info.builder, *info.app, info.extent);

	// define internal resources (images and buffers)
	gl::Resource::Builder resourceBuilder("LoaderResources");
	if (info.resourceCallback) {
		info.resourceCallback(resourceBuilder);
	}

	info.builder->setInternalResource(Rc<gl::Resource>::create(move(resourceBuilder)));

	return true;
}

bool MaterialPass::init(StringView name, RenderOrdering oridering, size_t subpassCount) {
	return MaterialVertexPass::init(name, oridering, subpassCount);
}

auto MaterialPass::makeFrameHandle(const FrameQueue &handle) -> Rc<PassHandle> {
	return Rc<MaterialPassHandle>::create(*this, handle);
}

void MaterialPass::prepare(gl::Device &dev) {
	MaterialVertexPass::prepare(dev);
	for (auto &it : _data->passDescriptors) {
		if (auto a = dynamic_cast<ShadowLightDataAttachment *>(it->getAttachment())) {
			_lights = a;
		} else if (auto a = dynamic_cast<ShadowImageArrayAttachment *>(it->getAttachment())) {
			_array = a;
		}
	}
}

bool MaterialPassHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = (MaterialPass *)_renderPass.get();

	if (auto lightsBuffer = q.getAttachment(pass->getLights())) {
		_lightsBuffer = (const ShadowLightDataAttachmentHandle *)lightsBuffer->handle.get();
	}

	if (auto arrayImage = q.getAttachment(pass->getArray())) {
		_arrayImage = (const ShadowImageArrayAttachmentHandle *)arrayImage->handle.get();
	}

	return MaterialVertexPassHandle::prepare(q, move(cb));
}

void MaterialPassHandle::prepareRenderPass(CommandBuffer &buf) {
	if (_lightsBuffer->getLightsCount() && _lightsBuffer->getBuffer()) {
		auto cIdx = _device->getQueueFamily(QueueOperations::Compute)->index;
		if (cIdx != _pool->getFamilyIdx()) {
			BufferMemoryBarrier transferBufferBarrier(_lightsBuffer->getBuffer(),
				VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
				QueueFamilyTransfer{cIdx, _pool->getFamilyIdx()}, 0, VK_WHOLE_SIZE);

			auto image = (Image *)_arrayImage->getImage()->getImage().get();

			SpanView<ImageMemoryBarrier> imageBarriers;
			if (auto b = image->getPendingBarrier()) {
				imageBarriers = makeSpanView(b, 1);
			}

			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
					makeSpanView(&transferBufferBarrier, 1), imageBarriers);
		} else {
			// no need for buffer barrier, only switch image layout

			auto image = (Image *)_arrayImage->getImage()->getImage().get();
			ImageMemoryBarrier transferImageBarrier(image,
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
					makeSpanView(&transferImageBarrier, 1));
		}
	}
}

void MaterialPassHandle::prepareMaterialCommands(gl::MaterialSet * materials, CommandBuffer &buf) {
	MaterialVertexPassHandle::prepareMaterialCommands(materials, buf);

	auto pass = (RenderPassImpl *)_data->impl.get();
	auto &fb = getFramebuffer();
	auto currentExtent = fb->getExtent();

	if (_lightsBuffer->getLightsCount() && _lightsBuffer->getBuffer()) {
		auto pipeline = (GraphicPipeline *)_data->subpasses[0].graphicPipelines.get(StringView(MaterialPass::ShadowPipeline))->pipeline.get();

		buf.cmdBindPipeline(pipeline);

		auto viewport = VkViewport{ 0.0f, 0.0f, float(currentExtent.width), float(currentExtent.height), 0.0f, 1.0f };
		buf.cmdSetViewport(0, makeSpanView(&viewport, 1));

		auto scissorRect = VkRect2D{ { 0, 0}, { currentExtent.width, currentExtent.height } };
		buf.cmdSetScissor(0, makeSpanView(&scissorRect, 1));

		uint32_t samplerIndex = 1; // linear filtering
		buf.cmdPushConstants(pass->getPipelineLayout(),
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, BytesView((const uint8_t *)&samplerIndex, sizeof(uint32_t)));

		buf.cmdDrawIndexed(
			6, // indexCount
			1, // instanceCount
			0, // firstIndex
			0, // int32_t   vertexOffset
			0  // uint32_t  firstInstance
		);
	} else {
		auto pipeline = (GraphicPipeline *)_data->subpasses[0].graphicPipelines.get(StringView(MaterialPass::ShadowPipelineNull))->pipeline.get();

		buf.cmdBindPipeline(pipeline);
	}
}

void MaterialPassHandle::doFinalizeTransfer(gl::MaterialSet * materials,
		Vector<ImageMemoryBarrier> &outputImageBarriers, Vector<BufferMemoryBarrier> &outputBufferBarriers) {
	if (!materials) {
		return;
	}

	auto b = (Buffer *)materials->getBuffer().get();
	if (!b) {
		return;
	}

	if (auto barrier = b->getPendingBarrier()) {
		outputBufferBarriers.emplace_back(*barrier);
		b->dropPendingBarrier();
	}

	for (auto &it : materials->getLayouts()) {
		if (it.set) {
			auto &pending = ((TextureSet *)it.set.get())->getPendingBarriers();
			for (auto &barrier : pending) {
				outputImageBarriers.emplace_back(barrier);
			}
			((TextureSet *)it.set.get())->dropPendingBarriers();
		} else {
			log::text("MaterialRenderPassHandle", "No set for material layout");
		}
	}
}

}
