/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>

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

#include "TessScene.h"
#include "TessLayout.h"
#include "TessAppDelegate.h"
#include "XLApplication.h"
#include "XLGlLoop.h"
#include "XLGlDevice.h"
#include "XLDefaultShaders.h"
#include "XLRenderQueueQueue.h"

#include "XLVkAttachment.h"
#include "XLVkMaterialRenderPass.h"
#include "XLDirector.h"

namespace stappler::xenolith::tessapp {

static gl::ImageFormat TessScene_selectDepthFormat(SpanView<gl::ImageFormat> formats) {
	gl::ImageFormat ret = gl::ImageFormat::Undefined;

	uint32_t score = 0;

	auto selectWithScore = [&] (gl::ImageFormat fmt, uint32_t sc) {
		if (score < sc) {
			ret = fmt;
			score = sc;
		}
	};

	for (auto &it : formats) {
		switch (it) {
		case gl::ImageFormat::D16_UNORM: selectWithScore(it, 12); break;
		case gl::ImageFormat::X8_D24_UNORM_PACK32: selectWithScore(it, 7); break;
		case gl::ImageFormat::D32_SFLOAT: selectWithScore(it, 9); break;
		case gl::ImageFormat::S8_UINT: break;
		case gl::ImageFormat::D16_UNORM_S8_UINT: selectWithScore(it, 11); break;
		case gl::ImageFormat::D24_UNORM_S8_UINT: selectWithScore(it, 10); break;
		case gl::ImageFormat::D32_SFLOAT_S8_UINT: selectWithScore(it, 8); break;
		default: break;
		}
	}

	return ret;
}

static void TessScene_makeRenderQueue(Application *app, renderqueue::Queue::Builder &builder, Extent2 extent,
		Function<void(renderqueue::FrameQueue &, const Rc<renderqueue::AttachmentHandle> &, Function<void(bool)> &&)> && cb) {
	using namespace renderqueue;

	auto &cache = app->getResourceCache();

	// load shaders by ref - do not copy content into engine
	auto materialFrag = builder.addProgramByRef("Loader_MaterialVert", xenolith::shaders::MaterialVert);
	auto materialVert = builder.addProgramByRef("Loader_MaterialFrag", xenolith::shaders::MaterialFrag);

	// render-to-swapchain RenderPass
	auto pass = Rc<vk::MaterialPass>::create("SwapchainPass", RenderOrderingHighest);
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
	auto materialPipeline = builder.addPipeline(pass, 0, "Solid", shaderSpecInfo, PipelineMaterialInfo({
		BlendInfo(),
		DepthInfo(true, true, gl::CompareOp::Less)
	}));
	auto transparentPipeline = builder.addPipeline(pass, 0, "Transparent", shaderSpecInfo, PipelineMaterialInfo({
		BlendInfo(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha, gl::BlendOp::Add,
				gl::BlendFactor::Zero, gl::BlendFactor::One, gl::BlendOp::Add),
		DepthInfo(false, true, gl::CompareOp::Less)
	}));

	auto surfacePipeline = builder.addPipeline(pass, 0, "Surface", shaderSpecInfo, PipelineMaterialInfo(
		BlendInfo(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha, gl::BlendOp::Add,
				gl::BlendFactor::Zero, gl::BlendFactor::One, gl::BlendOp::Add),
		DepthInfo(false, true, gl::CompareOp::LessOrEqual)
	));

	builder.addPipeline(pass, 0, "DebugTriangles", shaderSpecInfo, PipelineMaterialInfo(
		BlendInfo(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha, gl::BlendOp::Add,
				gl::BlendFactor::Zero, gl::BlendFactor::One, gl::BlendOp::Add),
		DepthInfo(false, true, gl::CompareOp::Less),
		LineWidth(1.0f)
	));

	// define internal resources (images and buffers)
	/*gl::Resource::Builder resourceBuilder("LoaderResources");
	auto initImage = resourceBuilder.addImage("Xenolith.png",
			gl::ImageInfo(gl::ImageFormat::R8G8B8A8_UNORM, gl::ImageUsage::Sampled, gl::ImageHints::NoAlpha),
			FilePath("resources/xenolith-1.png"));

	builder.setInternalResource(Rc<gl::Resource>::create(move(resourceBuilder)));*/

	gl::ImageInfo depthImageInfo(extent, gl::ForceImageUsage(gl::ImageUsage::DepthStencilAttachment),
			TessScene_selectDepthFormat(app->getGlLoop()->getSupportedDepthStencilFormat()));

	ImageAttachment::AttachmentInfo depthAttachmentInfo;
	depthAttachmentInfo.initialLayout = AttachmentLayout::Undefined;
	depthAttachmentInfo.finalLayout = AttachmentLayout::DepthStencilAttachmentOptimal;
	depthAttachmentInfo.clearOnLoad = true;
	depthAttachmentInfo.clearColor = Color4F::WHITE;
	depthAttachmentInfo.frameSizeCallback = [] (const FrameQueue &frame) {
		return Extent3(frame.getExtent());
	};

	auto depth = Rc<vk::ImageAttachment>::create("CommonDepth", move(depthImageInfo), move(depthAttachmentInfo));

	gl::ImageInfo outImageInfo(extent, gl::ForceImageUsage(gl::ImageUsage::ColorAttachment), platform::graphic::getCommonFormat());

	// output attachment
	ImageAttachment::AttachmentInfo outAttachmentInfo;
	outAttachmentInfo.initialLayout = AttachmentLayout::Undefined;
	outAttachmentInfo.finalLayout = AttachmentLayout::PresentSrc;
	outAttachmentInfo.clearOnLoad = true;
	outAttachmentInfo.clearColor = Color4F(1.0f, 1.0f, 1.0f, 1.0f);
	outAttachmentInfo.frameSizeCallback = [] (const FrameQueue &frame) {
		return Extent3(frame.getExtent());
	};

	auto out = Rc<vk::ImageAttachment>::create("Output", move(outImageInfo), move(outAttachmentInfo));

	// Material input attachment - per-scene list of materials
	auto materialInput = Rc<vk::MaterialVertexAttachment>::create("MaterialInput",
		gl::BufferInfo(gl::BufferUsage::StorageBuffer),

		// ... with predefined list of materials
		Vector<Rc<gl::Material>>({
			// Rc<gl::Material>::create(materialPipeline, initImage),
			// Rc<gl::Material>::create(transparentPipeline, initImage),
			Rc<gl::Material>::create(materialPipeline, cache->getEmptyImage(), ColorMode::IntensityChannel),
			Rc<gl::Material>::create(materialPipeline, cache->getSolidImage(), ColorMode::IntensityChannel),
			Rc<gl::Material>::create(transparentPipeline, cache->getEmptyImage(), ColorMode()),
			Rc<gl::Material>::create(transparentPipeline, cache->getSolidImage(), ColorMode()),
			Rc<gl::Material>::create(surfacePipeline, cache->getEmptyImage(), ColorMode()),
			Rc<gl::Material>::create(surfacePipeline, cache->getSolidImage(), ColorMode())
		})
	);

	// Vertex input attachment - per-frame vertex list
	auto vertexInput = Rc<vk::VertexMaterialAttachment>::create("VertexInput",
			gl::BufferInfo(gl::BufferUsage::StorageBuffer), materialInput);
	vertexInput->setInputCallback(move(cb));

	// define pass input-output
	builder.addPassInput(pass, 0, vertexInput, AttachmentDependencyInfo()); // 0
	builder.addPassInput(pass, 0, materialInput, AttachmentDependencyInfo()); // 1
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
	});

	// define global input-output
	// samplers and materialInput are persistent between frames, only vertexes should be provided before rendering started
	builder.addInput(vertexInput);
	builder.addOutput(out);
}

bool TessScene::init(AppDelegate *app, Extent2 extent) {
	using namespace renderqueue;

	Queue::Builder builder("Initial");

	TessScene_makeRenderQueue(app, builder, extent, [this] (FrameQueue &frame, const Rc<AttachmentHandle> &a,
			Function<void(bool)> &&cb) {
		on2dVertexInput(frame, a, move(cb));
	});

	if (!UtilScene::init(app, move(builder))) {
		return false;
	}

	_layout = addChild(Rc<TessLayout>::create());

	return true;
}

void TessScene::onPresented(Director *dir) {
	UtilScene::onPresented(dir);
}

void TessScene::onFinished(Director *dir) {
	UtilScene::onFinished(dir);
}

void TessScene::update(const UpdateTime &time) {
	UtilScene::update(time);
}

void TessScene::onEnter(Scene *scene) {
	UtilScene::onEnter(scene);
	std::cout << "AppScene::onEnter\n";
}

void TessScene::onExit() {
	std::cout << "AppScene::onExit\n";
	UtilScene::onExit();
}

void TessScene::onContentSizeDirty() {
	UtilScene::onContentSizeDirty();

	if (_layout) {
		_layout->setAnchorPoint(Anchor::Middle);
		_layout->setPosition(_contentSize / 2.0f);
		_layout->setContentSize(_contentSize);
	}
}

}
