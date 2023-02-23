/**
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

#include "XLVkMaterialShadowPass.h"
#include "XLDefaultShaders.h"
#include "XLVkMaterialRenderPass.h"

namespace stappler::xenolith::vk {

bool MaterialShadowPass::makeDefaultRenderQueue(RenderQueueInfo &info) {
	using namespace renderqueue;

	auto &builder = *info.builder;

	// load shaders by ref - do not copy data into engine
	auto materialFrag = builder.addProgramByRef("Loader_MaterialVert", xenolith::shaders::MaterialVert);
	auto materialVert = builder.addProgramByRef("Loader_MaterialFrag", xenolith::shaders::MaterialFrag);

	auto computePass = Rc<vk::MaterialShadowComputePass>::create("ShadowPass", RenderOrdering(0));
	builder.addRenderPass(computePass);

	builder.addComputePipeline(computePass, ShadowPass::SdfTrianglesComp,
			builder.addProgramByRef("ShadowPass_SdfTrianglesComp", xenolith::shaders::SdfTrianglesComp));

	builder.addComputePipeline(computePass, ShadowPass::SdfCirclesComp,
			builder.addProgramByRef("ShadowPass_SdfCirclesComp", xenolith::shaders::SdfCirclesComp));

	builder.addComputePipeline(computePass, ShadowPass::SdfRectsComp,
			builder.addProgramByRef("ShadowPass_SdfRectsComp", xenolith::shaders::SdfRectsComp));

	builder.addComputePipeline(computePass, ShadowPass::SdfRoundedRectsComp,
			builder.addProgramByRef("ShadowPass_SdfRoundedRectsComp", xenolith::shaders::SdfRoundedRectsComp));

	builder.addComputePipeline(computePass, ShadowPass::SdfPolygonsComp,
			builder.addProgramByRef("ShadowPass_SdfPolygonsComp", xenolith::shaders::SdfPolygonsComp));

	builder.addComputePipeline(computePass, ShadowPass::SdfImageComp,
			builder.addProgramByRef("ShadowPass_SdfImageComp", xenolith::shaders::SdfImageComp));

	auto shadowDataInput = Rc<ShadowLightDataAttachment>::create("ShadowDataAttachment");
	auto shadowVertexInput = Rc<ShadowVertexAttachment>::create("ShadowVertexAttachment");
	auto shadowTriangles = Rc<ShadowTrianglesAttachment>::create("ShadowTrianglesAttachment");
	auto shadowImage = Rc<ShadowSdfImageAttachment>::create("ShadowImage", info.extent);

	builder.addPassInput(computePass, 0, shadowDataInput, AttachmentDependencyInfo());
	builder.addPassInput(computePass, 0, shadowVertexInput, AttachmentDependencyInfo());
	builder.addPassOutput(computePass, 0, shadowTriangles, AttachmentDependencyInfo{
			PipelineStage::ComputeShader, AccessType::ShaderWrite,
			PipelineStage::ComputeShader, AccessType::ShaderWrite,
			FrameRenderPassState::Submitted});
	builder.addPassInput(computePass, 0, shadowImage, AttachmentDependencyInfo{
			PipelineStage::ComputeShader, AccessType::ShaderWrite,
			PipelineStage::ComputeShader, AccessType::ShaderWrite,
			FrameRenderPassState::Submitted}, DescriptorType::StorageImage, AttachmentLayout::General);

	// define global input-output
	builder.addInput(shadowDataInput);
	builder.addInput(shadowVertexInput);
	builder.addInput(shadowImage);

	// render-to-swapchain RenderPass
	auto materialPass = Rc<vk::MaterialShadowPass>::create("MaterialSwapchainPass", RenderOrderingHighest, 2);
	builder.addRenderPass(materialPass);
	builder.addSubpassDependency(materialPass, 0, PipelineStage::LateFragmentTest, AccessType::DepthStencilAttachmentWrite,
			1, PipelineStage::FragmentShader, AccessType::ShaderRead, true);

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
	auto materialPipeline = builder.addGraphicPipeline(materialPass, 0, "Solid", shaderSpecInfo, PipelineMaterialInfo({
		BlendInfo(),
		DepthInfo(true, true, gl::CompareOp::Less)
	}));
	auto transparentPipeline = builder.addGraphicPipeline(materialPass, 0, "Transparent", shaderSpecInfo, PipelineMaterialInfo({
		BlendInfo(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha, gl::BlendOp::Add,
				gl::BlendFactor::Zero, gl::BlendFactor::One, gl::BlendOp::Add),
		DepthInfo(false, true, gl::CompareOp::LessOrEqual)
	}));

	// pipeline for debugging - draw lines instead of triangles
	builder.addGraphicPipeline(materialPass, 0, "DebugTriangles", shaderSpecInfo, PipelineMaterialInfo(
		BlendInfo(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha, gl::BlendOp::Add,
				gl::BlendFactor::Zero, gl::BlendFactor::One, gl::BlendOp::Add),
		DepthInfo(false, true, gl::CompareOp::Less),
		LineWidth(1.0f)
	));

	// depth buffer - temporary/transient
	auto depth = Rc<vk::ImageAttachment>::create("CommonDepth",
		gl::ImageInfo(
			info.extent,
			gl::ForceImageUsage(gl::ImageUsage::DepthStencilAttachment),
			MaterialPass::selectDepthFormat(info.app->getGlLoop()->getSupportedDepthStencilFormat())),
		ImageAttachment::AttachmentInfo{
			.initialLayout = AttachmentLayout::Undefined,
			.finalLayout = AttachmentLayout::DepthStencilAttachmentOptimal,
			.clearOnLoad = true,
			.clearColor = Color4F::WHITE
	});

	// swapchain output
	auto out = Rc<vk::ImageAttachment>::create("Output",
		gl::ImageInfo(
			info.extent,
			gl::ForceImageUsage(gl::ImageUsage::ColorAttachment),
			platform::graphic::getCommonFormat()),
		ImageAttachment::AttachmentInfo{
			.initialLayout = AttachmentLayout::Undefined,
			.finalLayout = AttachmentLayout::PresentSrc,
			.clearOnLoad = true,
			.clearColor = Color4F(1.0f, 1.0f, 1.0f, 1.0f) // Color4F::WHITE;
	});

	auto shadow = Rc<vk::ImageAttachment>::create("Shadow",
		gl::ImageInfo(
			info.extent,
			gl::ForceImageUsage(gl::ImageUsage::ColorAttachment | gl::ImageUsage::InputAttachment),
			gl::ImageFormat::R16_SFLOAT),
		ImageAttachment::AttachmentInfo{
			.initialLayout = AttachmentLayout::Undefined,
			.finalLayout = AttachmentLayout::ShaderReadOnlyOptimal,
			.clearOnLoad = true,
			.clearColor = Color4F(0.0f, 0.0f, 0.0f, 0.0f) // Color4F::BLACK;
	});

	// Material input attachment - per-scene list of materials
	auto &cache = info.app->getResourceCache();
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
	builder.addPassInput(materialPass, 0, vertexInput, AttachmentDependencyInfo()); // 0
	builder.addPassInput(materialPass, 0, materialInput, AttachmentDependencyInfo()); // 1
	builder.addPassInput(materialPass, 0, shadowDataInput, AttachmentDependencyInfo()); // 2
	builder.addPassInput(materialPass, 0, shadowTriangles, AttachmentDependencyInfo()); // 3

	builder.addPassOutput(materialPass, 0, out, AttachmentDependencyInfo{
		PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,
		PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,
		FrameRenderPassState::Submitted,
	}, DescriptorType::Attachment, AttachmentLayout::Ignored);

	builder.addPassOutput(materialPass, 0, shadow, AttachmentDependencyInfo{
		PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,
		PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,
		FrameRenderPassState::Submitted,
	}, DescriptorType::Attachment, AttachmentLayout::Ignored);

	builder.addPassDepthStencil(materialPass, 0, depth, AttachmentDependencyInfo{
		PipelineStage::EarlyFragmentTest, AccessType::DepthStencilAttachmentRead | AccessType::DepthStencilAttachmentWrite,
		PipelineStage::LateFragmentTest, AccessType::DepthStencilAttachmentRead | AccessType::DepthStencilAttachmentWrite,
		FrameRenderPassState::Submitted,
	});

	builder.addPassOutput(materialPass, 1, out, AttachmentDependencyInfo{
		PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,
		PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,
		FrameRenderPassState::Submitted,
	}, DescriptorType::Attachment, AttachmentLayout::Ignored);

	builder.addPassInput(materialPass, 1, shadow, AttachmentDependencyInfo{ // 4
		PipelineStage::FragmentShader, AccessType::ShaderRead,
		PipelineStage::FragmentShader, AccessType::ShaderRead,
		FrameRenderPassState::Submitted,
	},  DescriptorType::InputAttachment, AttachmentLayout::ShaderReadOnlyOptimal);

	builder.addPassInput(materialPass, 1, shadowImage, AttachmentDependencyInfo{ // 5
		PipelineStage::FragmentShader, AccessType::ShaderRead,
		PipelineStage::FragmentShader, AccessType::ShaderRead,
	FrameRenderPassState::Submitted}, DescriptorType::SampledImage, AttachmentLayout::ShaderReadOnlyOptimal);

	auto shadowVert = builder.addProgramByRef("ShadowMergeVert", xenolith::shaders::ShadowMergeVert);
	auto shadowFrag = builder.addProgramByRef("ShadowMergeFrag", xenolith::shaders::SdfShadowsFrag);

	builder.addGraphicPipeline(materialPass, 1, MaterialShadowPass::ShadowPipeline, Vector<SpecializationInfo>({
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

	builder.addInput(vertexInput);
	builder.addOutput(out);
	//builder.addOutput(shadowImage);

	// define internal resources (images and buffers)
	gl::Resource::Builder resourceBuilder("LoaderResources");
	if (info.resourceCallback) {
		info.resourceCallback(resourceBuilder);
	}

	info.builder->setInternalResource(Rc<gl::Resource>::create(move(resourceBuilder)));

	return true;
}

bool MaterialShadowPass::init(StringView name, RenderOrdering ord, size_t subpassCount) {
	return QueuePass::init(name, gl::RenderPassType::Graphics, ord, subpassCount);
}

auto MaterialShadowPass::makeFrameHandle(const FrameQueue &handle) -> Rc<PassHandle> {
	return Rc<MaterialShadowPassHandle>::create(*this, handle);
}

void MaterialShadowPass::prepare(gl::Device &dev) {
	MaterialVertexPass::prepare(dev);
	for (auto &it : _data->passDescriptors) {
		if (auto a = dynamic_cast<ShadowLightDataAttachment *>(it->getAttachment())) {
			_shadowData = a;
		} else if (auto a = dynamic_cast<ShadowVertexAttachment *>(it->getAttachment())) {
			_shadowVertexBuffer = a;
		} else if (auto a = dynamic_cast<ShadowTrianglesAttachment *>(it->getAttachment())) {
			_shadowTriangles = a;
		} else if (auto a = dynamic_cast<ShadowSdfImageAttachment *>(it->getAttachment())) {
			_sdf = a;
		}
	}
}

bool MaterialShadowPassHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = (MaterialShadowPass *)_renderPass.get();

	if (auto lightsBuffer = q.getAttachment(pass->getShadowData())) {
		_shadowData = (const ShadowLightDataAttachmentHandle *)lightsBuffer->handle.get();
	}

	if (auto shadowVertexBuffer = q.getAttachment(pass->getShadowVertexBuffer())) {
		_shadowVertexBuffer = (const ShadowVertexAttachmentHandle *)shadowVertexBuffer->handle.get();
	}

	if (auto shadowTriangles = q.getAttachment(pass->getShadowTriangles())) {
		_shadowTriangles = (const ShadowTrianglesAttachmentHandle *)shadowTriangles->handle.get();
	}

	if (auto sdfImage = q.getAttachment(pass->getSdf())) {
		_sdfImage = (const ShadowSdfImageAttachmentHandle *)sdfImage->handle.get();
	}

	return MaterialVertexPassHandle::prepare(q, move(cb));
}

void MaterialShadowPassHandle::prepareRenderPass(CommandBuffer &buf) {
	Vector<BufferMemoryBarrier> bufferBarriers;
	Vector<ImageMemoryBarrier> imageBarriers;
	if (_shadowData->getLightsCount() && _shadowData->getBuffer()) {
		if (auto b = _shadowData->getBuffer()->getPendingBarrier()) {
			bufferBarriers.emplace_back(*b);
		}
	}

	if (_shadowTriangles->getTriangles()) {
		if (auto b = _shadowTriangles->getTriangles()->getPendingBarrier()) {
			bufferBarriers.emplace_back(*b);
		}
	}

	if (_shadowTriangles->getGridSize()) {
		if (auto b = _shadowTriangles->getGridSize()->getPendingBarrier()) {
			bufferBarriers.emplace_back(*b);
		}
	}

	if (_shadowTriangles->getGridIndex()) {
		if (auto b = _shadowTriangles->getGridIndex()->getPendingBarrier()) {
			bufferBarriers.emplace_back(*b);
		}
	}

	if (_shadowTriangles->getCircles()) {
		if (auto b = _shadowTriangles->getCircles()->getPendingBarrier()) {
			bufferBarriers.emplace_back(*b);
		}
	}

	if (_shadowTriangles->getRects()) {
		if (auto b = _shadowTriangles->getRects()->getPendingBarrier()) {
			bufferBarriers.emplace_back(*b);
		}
	}

	if (_shadowTriangles->getRoundedRects()) {
		if (auto b = _shadowTriangles->getRoundedRects()->getPendingBarrier()) {
			bufferBarriers.emplace_back(*b);
		}
	}

	if (_shadowTriangles->getPolygons()) {
		if (auto b = _shadowTriangles->getPolygons()->getPendingBarrier()) {
			bufferBarriers.emplace_back(*b);
		}
	}

	if (auto image = _sdfImage->getImage()) {
		if (auto b = image->getImage().cast<vk::Image>()->getPendingBarrier()) {
			imageBarriers.emplace_back(*b);
		}
	}

	if (!imageBarriers.empty() || !bufferBarriers.empty()) {
		buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, bufferBarriers, imageBarriers);
	} else if (!imageBarriers.empty()) {
		buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, imageBarriers);
	} else if (!bufferBarriers.empty()) {
		buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, bufferBarriers);
	}
}

void MaterialShadowPassHandle::prepareMaterialCommands(gl::MaterialSet * materials, CommandBuffer &buf) {
	MaterialVertexPassHandle::prepareMaterialCommands(materials, buf);

	auto pass = (RenderPassImpl *)_data->impl.get();
	auto &fb = getFramebuffer();
	auto currentExtent = fb->getExtent();

	buf.cmdNextSubpass();

	if (_shadowData->getLightsCount() && _shadowData->getBuffer() && _shadowData->getObjectsCount()) {
		auto pipeline = (GraphicPipeline *)_data->subpasses[1].graphicPipelines.get(StringView(MaterialShadowPass::ShadowPipeline))->pipeline.get();

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
	}
}

bool MaterialShadowComputePass::init(StringView name, RenderOrdering ord) {
	return QueuePass::init(name, gl::RenderPassType::Compute, ord, 1);
}

auto MaterialShadowComputePass::makeFrameHandle(const FrameQueue &handle) -> Rc<PassHandle> {
	return Rc<MaterialShadowComputePassHandle>::create(*this, handle);
}

void MaterialShadowComputePass::prepare(gl::Device &dev) {
	QueuePass::prepare(dev);
	for (auto &it : _data->passDescriptors) {
		if (auto a = dynamic_cast<ShadowVertexAttachment *>(it->getAttachment())) {
			_vertexes = a;
		} else if (auto a = dynamic_cast<ShadowTrianglesAttachment *>(it->getAttachment())) {
			_triangles = a;
		} else if (auto a = dynamic_cast<ShadowLightDataAttachment *>(it->getAttachment())) {
			_lights = a;
		} else if (auto a = dynamic_cast<ShadowSdfImageAttachment *>(it->getAttachment())) {
			_sdf = a;
		}
	}
}

bool MaterialShadowComputePassHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = (MaterialShadowComputePass *)_renderPass.get();

	ShadowTrianglesAttachmentHandle *trianglesHandle = nullptr;
	ShadowLightDataAttachmentHandle *lightsHandle = nullptr;

	if (auto lightsBuffer = q.getAttachment(pass->getLights())) {
		_lightsBuffer = lightsHandle = (ShadowLightDataAttachmentHandle *)lightsBuffer->handle.get();
	}

	if (auto trianglesBuffer = q.getAttachment(pass->getTriangles())) {
		_trianglesBuffer = trianglesHandle = (ShadowTrianglesAttachmentHandle *)trianglesBuffer->handle.get();
	}

	if (auto vertexBuffer = q.getAttachment(pass->getVertexes())) {
		_vertexBuffer = (const ShadowVertexAttachmentHandle *)vertexBuffer->handle.get();
	}

	if (auto sdfImage = q.getAttachment(pass->getSdf())) {
		_sdfImage = (const ShadowSdfImageAttachmentHandle *)sdfImage->handle.get();
	}

	if (lightsHandle && lightsHandle->getLightsCount()) {
		lightsHandle->allocateBuffer(static_cast<DeviceFrameHandle *>(q.getFrame().get()),
				_vertexBuffer, _gridCellSize, q.getExtent());

		if (lightsHandle->getObjectsCount() > 0 && trianglesHandle) {
			trianglesHandle->allocateBuffer(static_cast<DeviceFrameHandle *>(q.getFrame().get()),
					lightsHandle->getObjectsCount(), lightsHandle->getShadowData());
		}

		return QueuePassHandle::prepare(q, move(cb));
	} else {
		cb(true);
		return true;
	}
}

void MaterialShadowComputePassHandle::writeShadowCommands(RenderPassImpl *pass, CommandBuffer &buf) {
	auto sdfImage = (Image *)_sdfImage->getImage()->getImage().get();

	if (!_lightsBuffer || _lightsBuffer->getObjectsCount() == 0) {
		ImageMemoryBarrier inImageBarriers[] = {
			ImageMemoryBarrier(sdfImage, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		};

		buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, inImageBarriers);
		buf.cmdClearColorImage(sdfImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, Color4F(128.0f, 0.0f, 0.0f, 0.0f));

		auto gIdx = _device->getQueueFamily(QueueOperations::Graphics)->index;

		if (_pool->getFamilyIdx() != gIdx) {
			BufferMemoryBarrier transferBufferBarrier(_lightsBuffer->getBuffer(),
				VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
				QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE);

			ImageMemoryBarrier transferImageBarrier(sdfImage,
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx});
			sdfImage->setPendingBarrier(transferImageBarrier);

			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
					makeSpanView(&transferBufferBarrier, 1), makeSpanView(&transferImageBarrier, 1));
		}
		return;
	}

	ComputePipeline *pipeline = nullptr;
	buf.cmdBindDescriptorSets(pass);
	buf.cmdFillBuffer(_trianglesBuffer->getGridSize(), 0);

	BufferMemoryBarrier bufferBarrier(_trianglesBuffer->getGridSize(),
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
	);

	buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(&bufferBarrier, 1));

	if (_vertexBuffer->getTrianglesCount()) {
		pipeline = (ComputePipeline *)_data->subpasses[0].computePipelines.get(StringView(ShadowPass::SdfTrianglesComp))->pipeline.get();
		buf.cmdBindPipeline(pipeline);
		buf.cmdDispatch((_vertexBuffer->getTrianglesCount() - 1) / pipeline->getLocalX() + 1);
	}

	if (_vertexBuffer->getCirclesCount()) {
		pipeline = (ComputePipeline *)_data->subpasses[0].computePipelines.get(StringView(ShadowPass::SdfCirclesComp))->pipeline.get();
		buf.cmdBindPipeline(pipeline);
		buf.cmdDispatch((_vertexBuffer->getCirclesCount() - 1) / pipeline->getLocalX() + 1);
	}

	if (_vertexBuffer->getRectsCount()) {
		pipeline = (ComputePipeline *)_data->subpasses[0].computePipelines.get(StringView(ShadowPass::SdfRectsComp))->pipeline.get();
		buf.cmdBindPipeline(pipeline);
		buf.cmdDispatch((_vertexBuffer->getRectsCount() - 1) / pipeline->getLocalX() + 1);
	}

	if (_vertexBuffer->getRoundedRectsCount()) {
		pipeline = (ComputePipeline *)_data->subpasses[0].computePipelines.get(StringView(ShadowPass::SdfRoundedRectsComp))->pipeline.get();
		buf.cmdBindPipeline(pipeline);
		buf.cmdDispatch((_vertexBuffer->getRoundedRectsCount() - 1) / pipeline->getLocalX() + 1);
	}

	if (_vertexBuffer->getPolygonsCount()) {
		pipeline = (ComputePipeline *)_data->subpasses[0].computePipelines.get(StringView(ShadowPass::SdfPolygonsComp))->pipeline.get();
		buf.cmdBindPipeline(pipeline);
		buf.cmdDispatch((_vertexBuffer->getPolygonsCount() - 1) / pipeline->getLocalX() + 1);
	}

	BufferMemoryBarrier bufferBarriers[] = {
		BufferMemoryBarrier(_vertexBuffer->getVertexes(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		BufferMemoryBarrier(_trianglesBuffer->getTriangles(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		BufferMemoryBarrier(_trianglesBuffer->getGridSize(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		BufferMemoryBarrier(_trianglesBuffer->getGridIndex(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		BufferMemoryBarrier(_trianglesBuffer->getCircles(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		BufferMemoryBarrier(_trianglesBuffer->getRects(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		BufferMemoryBarrier(_trianglesBuffer->getRoundedRects(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		BufferMemoryBarrier(_trianglesBuffer->getPolygons(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
	};

	ImageMemoryBarrier inImageBarriers[] = {
		ImageMemoryBarrier(sdfImage, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL)
	};

	buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
			bufferBarriers, inImageBarriers);

	pipeline = (ComputePipeline *)_data->subpasses[0].computePipelines.get(StringView(ShadowPass::SdfImageComp))->pipeline.get();
	buf.cmdBindPipeline(pipeline);

	buf.cmdDispatch(
			(sdfImage->getInfo().extent.width - 1) / pipeline->getLocalX() + 1,
			(sdfImage->getInfo().extent.height - 1) / pipeline->getLocalY() + 1);

	// transfer image and buffer to transfer queue
	auto gIdx = _device->getQueueFamily(QueueOperations::Graphics)->index;

	if (_pool->getFamilyIdx() != gIdx) {

		BufferMemoryBarrier bufferBarriers[] = {
			BufferMemoryBarrier(_trianglesBuffer->getTriangles(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE),
			BufferMemoryBarrier(_trianglesBuffer->getGridSize(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE),
			BufferMemoryBarrier(_trianglesBuffer->getGridIndex(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE),
			BufferMemoryBarrier(_trianglesBuffer->getCircles(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE),
			BufferMemoryBarrier(_trianglesBuffer->getRects(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE),
			BufferMemoryBarrier(_trianglesBuffer->getRoundedRects(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE),
			BufferMemoryBarrier(_trianglesBuffer->getPolygons(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE),
			BufferMemoryBarrier(_lightsBuffer->getBuffer(), VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE)
		};

		ImageMemoryBarrier transferImageBarrier(sdfImage,
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx});
		sdfImage->setPendingBarrier(transferImageBarrier);

		_trianglesBuffer->getTriangles()->setPendingBarrier(bufferBarriers[0]);
		_trianglesBuffer->getGridSize()->setPendingBarrier(bufferBarriers[1]);
		_trianglesBuffer->getGridIndex()->setPendingBarrier(bufferBarriers[2]);
		_trianglesBuffer->getCircles()->setPendingBarrier(bufferBarriers[3]);
		_trianglesBuffer->getRects()->setPendingBarrier(bufferBarriers[4]);
		_trianglesBuffer->getRoundedRects()->setPendingBarrier(bufferBarriers[5]);
		_trianglesBuffer->getPolygons()->setPendingBarrier(bufferBarriers[6]);
		_lightsBuffer->getBuffer()->setPendingBarrier(bufferBarriers[3]);

		buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
				bufferBarriers, makeSpanView(&transferImageBarrier, 1));
	}
}

Vector<const CommandBuffer *> MaterialShadowComputePassHandle::doPrepareCommands(FrameHandle &h) {
	auto buf = _pool->recordBuffer(*_device, [&] (CommandBuffer &buf) {
		auto pass = (RenderPassImpl *)_data->impl.get();

		pass->perform(*this, buf, [&] {
			writeShadowCommands(pass, buf);
		});
		return true;
	});

	return Vector<const CommandBuffer *>{buf};
}

}
