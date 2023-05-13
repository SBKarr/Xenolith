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

namespace stappler::xenolith::vk {

bool MaterialShadowPass::makeDefaultRenderQueue(Builder &builder, RenderQueueInfo &info) {
	using namespace renderqueue;

	Rc<MaterialComputeShadowPass> computePass;

	builder.addPass("MaterialComputeShadowPass", PassType::Compute, RenderOrdering(0), [&] (PassBuilder &passBuilder) -> Rc<Pass> {
		computePass = Rc<MaterialComputeShadowPass>::create(builder, passBuilder, info.extent);
		return computePass;
	});

	builder.addPass("MaterialSwapchainPass", PassType::Graphics, RenderOrderingHighest, [&] (PassBuilder &passBuilder) -> Rc<Pass> {
		return Rc<MaterialShadowPass>::create(builder, passBuilder, PassCreateInfo{
			info.target, info.extent, info.flags,
			computePass->getSdf(), computePass->getLights(), computePass->getPrimitives()
		});
	});

	// define internal resources (images and buffers)
	gl::Resource::Builder resourceBuilder("LoaderResources");
	if (info.resourceCallback) {
		info.resourceCallback(resourceBuilder);
	}

	builder.setInternalResource(Rc<gl::Resource>::create(move(resourceBuilder)));

	return true;
}

bool MaterialShadowPass::init(Queue::Builder &queueBuilder, PassBuilder &passBuilder, const PassCreateInfo &info) {
	using namespace renderqueue;

	_output = queueBuilder.addAttachemnt("Output", [&] (AttachmentBuilder &builder) -> Rc<Attachment> {
		// swapchain output
		builder.defineAsOutput();

		return Rc<vk::ImageAttachment>::create(builder,
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
	});

	_shadow = queueBuilder.addAttachemnt("Shadow", [&] (AttachmentBuilder &builder) -> Rc<Attachment> {
		// swapchain output
		return Rc<vk::ImageAttachment>::create(builder,
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
	});

	_depth2d = queueBuilder.addAttachemnt("CommonDepth2d", [&] (AttachmentBuilder &builder) -> Rc<Attachment> {
		// swapchain output
		return Rc<vk::ImageAttachment>::create(builder,
			gl::ImageInfo(
				info.extent,
				gl::ForceImageUsage(gl::ImageUsage::DepthStencilAttachment),
				MaterialVertexPass::select2dDepthFormat(info.target->getSupportedDepthStencilFormat())),
			ImageAttachment::AttachmentInfo{
				.initialLayout = AttachmentLayout::Undefined,
				.finalLayout = AttachmentLayout::DepthStencilAttachmentOptimal,
				.clearOnLoad = true,
				.clearColor = Color4F::WHITE
		});
	});

	_sdf = info.shadowSdfAttachment;

	_materials = queueBuilder.addAttachemnt("MaterialInput2d", [] (AttachmentBuilder &builder) -> Rc<Attachment> {
		return Rc<vk::MaterialAttachment>::create(builder, gl::BufferInfo(gl::BufferUsage::StorageBuffer));
	});

	_vertexes = queueBuilder.addAttachemnt("VertexInput2d", [&] (AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsInput();
		return Rc<vk::VertexMaterialAttachment>::create(builder, gl::BufferInfo(gl::BufferUsage::StorageBuffer), _materials);
	});

	_lightsData = info.lightsAttachment;
	_shadowPrimitives = info.sdfPrimitivesAttachment;

	auto colorAttachment = passBuilder.addAttachment(_output);
	auto shadowAttachment = passBuilder.addAttachment(_shadow);
	auto sdfAttachment = passBuilder.addAttachment(_sdf);
	auto depth2dAttachment = passBuilder.addAttachment(_depth2d);

	auto layout2d = passBuilder.addDescriptorLayout([&] (PipelineLayoutBuilder &layoutBuilder) {
		// Vertex input attachment - per-frame vertex list
		layoutBuilder.addSet([&] (DescriptorSetBuilder &setBuilder) {
			setBuilder.addDescriptor(passBuilder.addAttachment(_vertexes));
			setBuilder.addDescriptor(passBuilder.addAttachment(_materials));
			setBuilder.addDescriptor(passBuilder.addAttachment(_lightsData));
			setBuilder.addDescriptor(passBuilder.addAttachment(_shadowPrimitives));
			setBuilder.addDescriptor(shadowAttachment, DescriptorType::InputAttachment, AttachmentLayout::ShaderReadOnlyOptimal);
			setBuilder.addDescriptor(sdfAttachment, DescriptorType::SampledImage, AttachmentLayout::ShaderReadOnlyOptimal);
		});
	});

	auto subpass2d = passBuilder.addSubpass([&] (SubpassBuilder &subpassBuilder) {
		// load shaders by ref - do not copy data into engine
		auto materialVert = queueBuilder.addProgramByRef("Loader_MaterialVert", xenolith::shaders::MaterialVert);
		auto materialFrag = queueBuilder.addProgramByRef("Loader_MaterialFrag", xenolith::shaders::MaterialFrag);

		auto shaderSpecInfo = Vector<SpecializationInfo>({
			// no specialization required for vertex shader
			SpecializationInfo(materialVert, Vector<PredefinedConstant>{
				PredefinedConstant::BuffersArraySize
			}),
			// specialization for fragment shader - use platform-dependent array sizes
			SpecializationInfo(materialFrag, Vector<PredefinedConstant>{
				PredefinedConstant::SamplersArraySize,
				PredefinedConstant::TexturesArraySize
			})
		});

		// pipelines for material-besed rendering
		auto materialPipeline = subpassBuilder.addGraphicPipeline("Solid", layout2d, shaderSpecInfo, PipelineMaterialInfo({
			BlendInfo(),
			DepthInfo(true, true, gl::CompareOp::Less)
		}));

		auto transparentPipeline = subpassBuilder.addGraphicPipeline("Transparent", layout2d, shaderSpecInfo, PipelineMaterialInfo({
			BlendInfo(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha, gl::BlendOp::Add,
					gl::BlendFactor::Zero, gl::BlendFactor::One, gl::BlendOp::Add),
			DepthInfo(false, true, gl::CompareOp::LessOrEqual)
		}));

		// pipeline for debugging - draw lines instead of triangles
		subpassBuilder.addGraphicPipeline("DebugTriangles", layout2d, shaderSpecInfo, PipelineMaterialInfo(
			BlendInfo(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha, gl::BlendOp::Add,
					gl::BlendFactor::Zero, gl::BlendFactor::One, gl::BlendOp::Add),
			DepthInfo(false, true, gl::CompareOp::Less),
			LineWidth(1.0f)
		));

		auto cache = info.target->getResourceCache();
		((MaterialAttachment *)_materials->attachment.get())->addPredefinedMaterials(Vector<Rc<gl::Material>>({
			Rc<gl::Material>::create(gl::Material::MaterialIdInitial, materialPipeline, cache->getEmptyImage(), ColorMode::IntensityChannel),
			Rc<gl::Material>::create(gl::Material::MaterialIdInitial, materialPipeline, cache->getSolidImage(), ColorMode::IntensityChannel),
			Rc<gl::Material>::create(gl::Material::MaterialIdInitial, transparentPipeline, cache->getEmptyImage(), ColorMode()),
			Rc<gl::Material>::create(gl::Material::MaterialIdInitial, transparentPipeline, cache->getSolidImage(), ColorMode()),
		}));

		subpassBuilder.addColor(colorAttachment, AttachmentDependencyInfo{
			PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,
			PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,
			FrameRenderPassState::Submitted,
		});

		subpassBuilder.addColor(shadowAttachment, AttachmentDependencyInfo{
			PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,
			PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,
			FrameRenderPassState::Submitted,
		});

		subpassBuilder.setDepthStencil(depth2dAttachment, AttachmentDependencyInfo{
			PipelineStage::EarlyFragmentTest, AccessType::DepthStencilAttachmentRead | AccessType::DepthStencilAttachmentWrite,
			PipelineStage::LateFragmentTest, AccessType::DepthStencilAttachmentRead | AccessType::DepthStencilAttachmentWrite,
			FrameRenderPassState::Submitted,
		});
	});

	auto subpassShadows = passBuilder.addSubpass([&] (SubpassBuilder &subpassBuilder) {
		subpassBuilder.addColor(colorAttachment, AttachmentDependencyInfo{
			PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,
			PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,
			FrameRenderPassState::Submitted,
		});

		subpassBuilder.addInput(shadowAttachment, AttachmentDependencyInfo{ // 4
			PipelineStage::FragmentShader, AccessType::ShaderRead,
			PipelineStage::FragmentShader, AccessType::ShaderRead,
			FrameRenderPassState::Submitted,
		});

		auto shadowVert = queueBuilder.addProgramByRef("ShadowMergeVert", xenolith::shaders::SdfShadowsVert);
		auto shadowFrag = queueBuilder.addProgramByRef("ShadowMergeFrag", xenolith::shaders::SdfShadowsFrag);

		subpassBuilder.addGraphicPipeline(MaterialShadowPass::ShadowPipeline, layout2d, Vector<SpecializationInfo>({
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
	});

	passBuilder.addSubpassDependency(subpass2d, PipelineStage::LateFragmentTest, AccessType::DepthStencilAttachmentWrite,
			subpassShadows, PipelineStage::FragmentShader, AccessType::ShaderRead, true);

	if (!MaterialVertexPass::init(passBuilder)) {
		return false;
	}

	_flags = info.flags;
	return true;
}

auto MaterialShadowPass::makeFrameHandle(const FrameQueue &handle) -> Rc<PassHandle> {
	return Rc<MaterialShadowPassHandle>::create(*this, handle);
}

bool MaterialShadowPassHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = (MaterialShadowPass *)_renderPass.get();

	if (auto lightsBuffer = q.getAttachment(pass->getLightsData())) {
		_shadowData = (const ShadowLightDataAttachmentHandle *)lightsBuffer->handle.get();
	}

	if (auto shadowPrimitives = q.getAttachment(pass->getShadowPrimitives())) {
		_shadowPrimitives = (const ShadowPrimitivesAttachmentHandle *)shadowPrimitives->handle.get();
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

	if (_shadowPrimitives->getTriangles()) {
		if (auto b = _shadowPrimitives->getTriangles()->getPendingBarrier()) {
			bufferBarriers.emplace_back(*b);
		}
	}

	if (_shadowPrimitives->getGridSize()) {
		if (auto b = _shadowPrimitives->getGridSize()->getPendingBarrier()) {
			bufferBarriers.emplace_back(*b);
		}
	}

	if (_shadowPrimitives->getGridIndex()) {
		if (auto b = _shadowPrimitives->getGridIndex()->getPendingBarrier()) {
			bufferBarriers.emplace_back(*b);
		}
	}

	if (_shadowPrimitives->getCircles()) {
		if (auto b = _shadowPrimitives->getCircles()->getPendingBarrier()) {
			bufferBarriers.emplace_back(*b);
		}
	}

	if (_shadowPrimitives->getRects()) {
		if (auto b = _shadowPrimitives->getRects()->getPendingBarrier()) {
			bufferBarriers.emplace_back(*b);
		}
	}

	if (_shadowPrimitives->getRoundedRects()) {
		if (auto b = _shadowPrimitives->getRoundedRects()->getPendingBarrier()) {
			bufferBarriers.emplace_back(*b);
		}
	}

	if (_shadowPrimitives->getPolygons()) {
		if (auto b = _shadowPrimitives->getPolygons()->getPendingBarrier()) {
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

	auto &fb = getFramebuffer();
	auto currentExtent = fb->getExtent();

	auto subpassIdx = buf.cmdNextSubpass();

	if (_shadowData->getLightsCount() && _shadowData->getBuffer() && _shadowData->getObjectsCount()) {
		auto pipeline = (GraphicPipeline *)_data->subpasses[subpassIdx]->graphicPipelines
				.get(StringView(MaterialShadowPass::ShadowPipeline))->pipeline.get();

		buf.cmdBindPipeline(pipeline);

		auto viewport = VkViewport{ 0.0f, 0.0f, float(currentExtent.width), float(currentExtent.height), 0.0f, 1.0f };
		buf.cmdSetViewport(0, makeSpanView(&viewport, 1));

		auto scissorRect = VkRect2D{ { 0, 0}, { currentExtent.width, currentExtent.height } };
		buf.cmdSetScissor(0, makeSpanView(&scissorRect, 1));

		uint32_t samplerIndex = 1; // linear filtering
		buf.cmdPushConstants(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0, BytesView((const uint8_t *)&samplerIndex, sizeof(uint32_t)));

		buf.cmdDrawIndexed(
			6, // indexCount
			1, // instanceCount
			0, // firstIndex
			0, // int32_t   vertexOffset
			0  // uint32_t  firstInstance
		);
	}
}

bool MaterialComputeShadowPass::init(Queue::Builder &queueBuilder, PassBuilder &passBuilder, Extent2 defaultExtent) {
	using namespace renderqueue;

	_lights = queueBuilder.addAttachemnt("ShadowLightDataAttachment", [] (AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsInput();
		return Rc<ShadowLightDataAttachment>::create(builder);
	});

	_vertexes = queueBuilder.addAttachemnt("ShadowVertexAttachment", [] (AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsInput();
		return Rc<ShadowVertexAttachment>::create(builder);
	});

	_primitives = queueBuilder.addAttachemnt("ShadowPrimitivesAttachment", [] (AttachmentBuilder &builder) -> Rc<Attachment> {
		return Rc<ShadowPrimitivesAttachment>::create(builder);
	});

	_sdf = queueBuilder.addAttachemnt("ShadowSdfImageAttachment", [&] (AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsInput();
		return Rc<ShadowSdfImageAttachment>::create(builder, defaultExtent);
	});

	auto layout = passBuilder.addDescriptorLayout([&] (PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.addSet([&] (DescriptorSetBuilder &setBuilder) {
			setBuilder.addDescriptor(passBuilder.addAttachment(_lights));
			setBuilder.addDescriptor(passBuilder.addAttachment(_vertexes));
			setBuilder.addDescriptor(passBuilder.addAttachment(_primitives));
			setBuilder.addDescriptor(passBuilder.addAttachment(_sdf), DescriptorType::StorageImage, AttachmentLayout::General);
		});
	});

	passBuilder.addSubpass([&] (SubpassBuilder &subpassBuilder) {
		subpassBuilder.addComputePipeline(MaterialComputeShadowPass::SdfTrianglesComp, layout,
				queueBuilder.addProgramByRef("ShadowPass_SdfTrianglesComp", xenolith::shaders::SdfTrianglesComp));

		subpassBuilder.addComputePipeline(MaterialComputeShadowPass::SdfCirclesComp, layout,
				queueBuilder.addProgramByRef("ShadowPass_SdfCirclesComp", xenolith::shaders::SdfCirclesComp));

		subpassBuilder.addComputePipeline(MaterialComputeShadowPass::SdfRectsComp, layout,
				queueBuilder.addProgramByRef("ShadowPass_SdfRectsComp", xenolith::shaders::SdfRectsComp));

		subpassBuilder.addComputePipeline(MaterialComputeShadowPass::SdfRoundedRectsComp, layout,
				queueBuilder.addProgramByRef("ShadowPass_SdfRoundedRectsComp", xenolith::shaders::SdfRoundedRectsComp));

		subpassBuilder.addComputePipeline(MaterialComputeShadowPass::SdfPolygonsComp, layout,
				queueBuilder.addProgramByRef("ShadowPass_SdfPolygonsComp", xenolith::shaders::SdfPolygonsComp));

		subpassBuilder.addComputePipeline(MaterialComputeShadowPass::SdfImageComp, layout,
				queueBuilder.addProgramByRef("ShadowPass_SdfImageComp", xenolith::shaders::SdfImageComp));
	});

	return QueuePass::init(passBuilder);
}

auto MaterialComputeShadowPass::makeFrameHandle(const FrameQueue &handle) -> Rc<PassHandle> {
	return Rc<MaterialComputeShadowPassHandle>::create(*this, handle);
}

bool MaterialComputeShadowPassHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = (MaterialComputeShadowPass *)_renderPass.get();

	ShadowPrimitivesAttachmentHandle *trianglesHandle = nullptr;
	ShadowLightDataAttachmentHandle *lightsHandle = nullptr;

	if (auto lightsBuffer = q.getAttachment(pass->getLights())) {
		_lightsBuffer = lightsHandle = (ShadowLightDataAttachmentHandle *)lightsBuffer->handle.get();
	}

	if (auto primitivesBuffer = q.getAttachment(pass->getPrimitives())) {
		_primitivesBuffer = trianglesHandle = (ShadowPrimitivesAttachmentHandle *)primitivesBuffer->handle.get();
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

void MaterialComputeShadowPassHandle::writeShadowCommands(RenderPassImpl *pass, CommandBuffer &buf) {
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
	buf.cmdBindDescriptorSets(pass, 0);
	buf.cmdFillBuffer(_primitivesBuffer->getGridSize(), 0);

	BufferMemoryBarrier bufferBarrier(_primitivesBuffer->getGridSize(),
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
	);

	buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(&bufferBarrier, 1));

	if (_vertexBuffer->getTrianglesCount()) {
		pipeline = (ComputePipeline *)_data->subpasses[0]->computePipelines.get(MaterialComputeShadowPass::SdfTrianglesComp)->pipeline.get();
		buf.cmdBindPipeline(pipeline);
		buf.cmdDispatch((_vertexBuffer->getTrianglesCount() - 1) / pipeline->getLocalX() + 1);
	}

	if (_vertexBuffer->getCirclesCount()) {
		pipeline = (ComputePipeline *)_data->subpasses[0]->computePipelines.get(MaterialComputeShadowPass::SdfCirclesComp)->pipeline.get();
		buf.cmdBindPipeline(pipeline);
		buf.cmdDispatch((_vertexBuffer->getCirclesCount() - 1) / pipeline->getLocalX() + 1);
	}

	if (_vertexBuffer->getRectsCount()) {
		pipeline = (ComputePipeline *)_data->subpasses[0]->computePipelines.get(MaterialComputeShadowPass::SdfRectsComp)->pipeline.get();
		buf.cmdBindPipeline(pipeline);
		buf.cmdDispatch((_vertexBuffer->getRectsCount() - 1) / pipeline->getLocalX() + 1);
	}

	if (_vertexBuffer->getRoundedRectsCount()) {
		pipeline = (ComputePipeline *)_data->subpasses[0]->computePipelines.get(MaterialComputeShadowPass::SdfRoundedRectsComp)->pipeline.get();
		buf.cmdBindPipeline(pipeline);
		buf.cmdDispatch((_vertexBuffer->getRoundedRectsCount() - 1) / pipeline->getLocalX() + 1);
	}

	if (_vertexBuffer->getPolygonsCount()) {
		pipeline = (ComputePipeline *)_data->subpasses[0]->computePipelines.get(MaterialComputeShadowPass::SdfPolygonsComp)->pipeline.get();
		buf.cmdBindPipeline(pipeline);
		buf.cmdDispatch((_vertexBuffer->getPolygonsCount() - 1) / pipeline->getLocalX() + 1);
	}

	BufferMemoryBarrier bufferBarriers[] = {
		BufferMemoryBarrier(_vertexBuffer->getVertexes(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		BufferMemoryBarrier(_primitivesBuffer->getTriangles(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		BufferMemoryBarrier(_primitivesBuffer->getGridSize(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		BufferMemoryBarrier(_primitivesBuffer->getGridIndex(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		BufferMemoryBarrier(_primitivesBuffer->getCircles(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		BufferMemoryBarrier(_primitivesBuffer->getRects(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		BufferMemoryBarrier(_primitivesBuffer->getRoundedRects(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		BufferMemoryBarrier(_primitivesBuffer->getPolygons(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
	};

	ImageMemoryBarrier inImageBarriers[] = {
		ImageMemoryBarrier(sdfImage, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL)
	};

	buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
			bufferBarriers, inImageBarriers);

	pipeline = (ComputePipeline *)_data->subpasses[0]->computePipelines.get(MaterialComputeShadowPass::SdfImageComp)->pipeline.get();
	buf.cmdBindPipeline(pipeline);

	buf.cmdDispatch(
			(sdfImage->getInfo().extent.width - 1) / pipeline->getLocalX() + 1,
			(sdfImage->getInfo().extent.height - 1) / pipeline->getLocalY() + 1);

	// transfer image and buffer to transfer queue
	auto gIdx = _device->getQueueFamily(QueueOperations::Graphics)->index;

	if (_pool->getFamilyIdx() != gIdx) {

		BufferMemoryBarrier bufferBarriers[] = {
			BufferMemoryBarrier(_primitivesBuffer->getTriangles(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE),
			BufferMemoryBarrier(_primitivesBuffer->getGridSize(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE),
			BufferMemoryBarrier(_primitivesBuffer->getGridIndex(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE),
			BufferMemoryBarrier(_primitivesBuffer->getCircles(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE),
			BufferMemoryBarrier(_primitivesBuffer->getRects(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE),
			BufferMemoryBarrier(_primitivesBuffer->getRoundedRects(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE),
			BufferMemoryBarrier(_primitivesBuffer->getPolygons(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE),
			BufferMemoryBarrier(_lightsBuffer->getBuffer(), VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE)
		};

		ImageMemoryBarrier transferImageBarrier(sdfImage,
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx});
		sdfImage->setPendingBarrier(transferImageBarrier);

		_primitivesBuffer->getTriangles()->setPendingBarrier(bufferBarriers[0]);
		_primitivesBuffer->getGridSize()->setPendingBarrier(bufferBarriers[1]);
		_primitivesBuffer->getGridIndex()->setPendingBarrier(bufferBarriers[2]);
		_primitivesBuffer->getCircles()->setPendingBarrier(bufferBarriers[3]);
		_primitivesBuffer->getRects()->setPendingBarrier(bufferBarriers[4]);
		_primitivesBuffer->getRoundedRects()->setPendingBarrier(bufferBarriers[5]);
		_primitivesBuffer->getPolygons()->setPendingBarrier(bufferBarriers[6]);
		_lightsBuffer->getBuffer()->setPendingBarrier(bufferBarriers[3]);

		buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
				bufferBarriers, makeSpanView(&transferImageBarrier, 1));
	}
}

Vector<const CommandBuffer *> MaterialComputeShadowPassHandle::doPrepareCommands(FrameHandle &h) {
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
