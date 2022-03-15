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

#include "AppScene.h"

#include "XLVkAttachment.h"
#include "XLAppSceneResource.cc"
#include "XLDirector.h"
#include "XLSprite.h"
#include "XLPlatform.h"
#include "XLApplication.h"

#include "XLGlRenderQueue.h"
#include "XLDefaultShaders.h"

#include "XLVkAttachment.h"
#include "XLVkMaterialRenderPass.h"

#include "AppRootLayout.h"
#include "XLFontLibrary.h"

namespace stappler::xenolith::app {

static gl::ImageFormat AppScene_selectDepthFormat(SpanView<gl::ImageFormat> formats) {
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

static void AppScene_makeRenderQueue(Application *app, gl::RenderQueue::Builder &builder, Extent2 extent,
		Function<void(gl::FrameQueue &, const Rc<gl::AttachmentHandle> &, Function<void(bool)> &&)> && cb) {
	auto &dev = app->getGlLoop()->getDevice();
	auto &cache = app->getResourceCache();

	// load shaders by ref - do not copy content into engine
	auto materialFrag = builder.addProgramByRef("Loader_MaterialVert", xenolith::shaders::MaterialVert);
	auto materialVert = builder.addProgramByRef("Loader_MaterialFrag", xenolith::shaders::MaterialFrag);

	// render-to-swapchain RenderPass
	auto pass = Rc<vk::MaterialRenderPass>::create("SwapchainPass", gl::RenderOrderingHighest);
	builder.addRenderPass(pass);

	auto shaderSpecInfo = Vector<gl::SpecializationInfo>({
		// no specialization required for vertex shader
		materialVert,
		// specialization for fragment shader - use platform-dependent array sizes
		gl::SpecializationInfo(materialFrag, { gl::PredefinedConstant::SamplersArraySize, gl::PredefinedConstant::TexturesArraySize })
	});

	// pipelines for material-besed rendering
	auto materialPipeline = builder.addPipeline(pass, 0, "Solid", shaderSpecInfo, PipelineMaterialInfo({
		BlendInfo(),
		DepthInfo(true, true, gl::CompareOp::Less)
	}));
	auto transparentPipeline = builder.addPipeline(pass, 0, "Transparent", shaderSpecInfo, PipelineMaterialInfo({
		BlendInfo(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha, gl::BlendOp::Add,
				gl::BlendFactor::One, gl::BlendFactor::Zero, gl::BlendOp::Add),
		DepthInfo(false, true, gl::CompareOp::Less)
	}));

	// define internal resources (images and buffers)
	gl::Resource::Builder resourceBuilder("LoaderResources");
	auto initImage = resourceBuilder.addImage("Xenolith.png",
			gl::ImageInfo(gl::ImageFormat::R8G8B8A8_UNORM, gl::ImageUsage::Sampled),
			FilePath("resources/xenolith-1.png"));

	builder.setInternalResource(Rc<gl::Resource>::create(move(resourceBuilder)));

	gl::ImageInfo depthImageInfo(extent, gl::ForceImageUsage(gl::ImageUsage::DepthStencilAttachment),
			AppScene_selectDepthFormat(dev->getSupportedDepthStencilFormat()));

	gl::ImageAttachment::AttachmentInfo depthAttachmentInfo;
	depthAttachmentInfo.initialLayout = gl::AttachmentLayout::Undefined;
	depthAttachmentInfo.finalLayout = gl::AttachmentLayout::DepthStencilAttachmentOptimal;
	depthAttachmentInfo.clearOnLoad = true;
	depthAttachmentInfo.clearColor = Color4F::WHITE;
	depthAttachmentInfo.frameSizeCallback = [] (const gl::FrameQueue &frame) {
		return Extent3(frame.getExtent());
	};

	auto depth = Rc<vk::ImageAttachment>::create("CommonDepth", move(depthImageInfo), move(depthAttachmentInfo));

	gl::ImageInfo outImageInfo(extent, gl::ForceImageUsage(gl::ImageUsage::ColorAttachment), platform::graphic::getCommonFormat());

	// output attachment
	gl::ImageAttachment::AttachmentInfo outAttachmentInfo;
	outAttachmentInfo.initialLayout = gl::AttachmentLayout::Undefined;
	outAttachmentInfo.finalLayout = gl::AttachmentLayout::PresentSrc;
	outAttachmentInfo.clearOnLoad = true;
	outAttachmentInfo.clearColor = Color4F(0.0f, 0.0f, 0.0f, 0.0f); // Color4F::BLACK;
	outAttachmentInfo.frameSizeCallback = [] (const gl::FrameQueue &frame) {
		return Extent3(frame.getExtent());
	};

	auto out = Rc<vk::ImageAttachment>::create("Output", move(outImageInfo), move(outAttachmentInfo));

	// Engine-defined samplers as input attachment
	auto samplers = Rc<gl::SamplersAttachment>::create("Samplers");

	// Material input attachment - per-scene list of materials
	auto materialInput = Rc<vk::MaterialVertexAttachment>::create("MaterialInput",
		gl::BufferInfo(gl::BufferUsage::StorageBuffer),

		// ... with predefined list of materials
		Vector<Rc<gl::Material>>({
			/*Rc<gl::Material>::create(materialPipeline, initImage),
			Rc<gl::Material>::create(materialPipeline, cache->getEmptyImage(), ColorMode::IntensityChannel),
			Rc<gl::Material>::create(materialPipeline, cache->getSolidImage(), ColorMode::IntensityChannel),
			Rc<gl::Material>::create(transparentPipeline, initImage),
			Rc<gl::Material>::create(transparentPipeline, cache->getEmptyImage(), ColorMode::IntensityChannel),
			Rc<gl::Material>::create(transparentPipeline, cache->getSolidImage(), ColorMode::IntensityChannel)*/
		})
	);

	// Vertex input attachment - per-frame vertex list
	auto vertexInput = Rc<vk::VertexMaterialAttachment>::create("VertexInput",
			gl::BufferInfo(gl::BufferUsage::StorageBuffer), materialInput);
	vertexInput->setInputCallback(move(cb));

	// define pass input-output
	builder.addPassInput(pass, 0, samplers, gl::AttachmentDependencyInfo()); // 0
	builder.addPassInput(pass, 0, vertexInput, gl::AttachmentDependencyInfo()); // 1
	builder.addPassInput(pass, 0, materialInput, gl::AttachmentDependencyInfo()); // 2
	builder.addPassDepthStencil(pass, 0, depth, gl::AttachmentDependencyInfo{
		gl::PipelineStage::EarlyFragmentTest,
			gl::AccessType::DepthStencilAttachmentRead | gl::AccessType::DepthStencilAttachmentWrite,
		gl::PipelineStage::LateFragmentTest,
			gl::AccessType::DepthStencilAttachmentRead | gl::AccessType::DepthStencilAttachmentWrite,

		// can be reused after RenderPass is submitted
		gl::FrameRenderPassState::Submitted,
	});
	builder.addPassOutput(pass, 0, out, gl::AttachmentDependencyInfo{
		// first used as color attachment to output colors
		gl::PipelineStage::ColorAttachmentOutput, gl::AccessType::ColorAttachmentWrite,

		// last used the same way (the only usage for this attachment)
		gl::PipelineStage::ColorAttachmentOutput, gl::AccessType::ColorAttachmentWrite,

		// can be reused after RenderPass is submitted
		gl::FrameRenderPassState::Submitted,
	});

	// define global input-output
	// samplers and materialInput are persistent between frames, only vertexes should be provided before rendering started
	builder.addInput(vertexInput);
	builder.addOutput(out);

	// optional world-to-pass subpass dependency
	/*builder.addSubpassDependency(pass,
			gl::RenderSubpassDependency::External, gl::PipelineStage::ColorAttachmentOutput, gl::AccessType::None,
			0, gl::PipelineStage::ColorAttachmentOutput, gl::AccessType::ColorAttachmentWrite, false);*/
}

bool AppScene::init(Application *app, Extent2 extent) {
	// build presentation RenderQueue
	gl::RenderQueue::Builder builder("Loader", gl::RenderQueue::Continuous);

	AppScene_makeRenderQueue(app, builder, extent, [this] (gl::FrameQueue &frame, const Rc<gl::AttachmentHandle> &a,
			Function<void(bool)> &&cb) {
		on2dVertexInput(frame, a, move(cb));
	});

	if (!Scene::init(app, move(builder))) {
		return false;
	}

	_layout = addChild(Rc<RootLayout>::create());

	// _sprite = addChild(Rc<Sprite>::create("Xenolith.png"));

	/*_node1 = addChild(Rc<Sprite>::create());
	_node1->setColor(Color::Teal_400);*/

	//_node2 = addChild(Rc<test::NetworkTestSprite>::create());

	scheduleUpdate();

	return true;
}

void AppScene::onPresented(Director *dir) {
	Scene::onPresented(dir);
}

void AppScene::onFinished(Director *dir) {
	Scene::onFinished(dir);
}

void AppScene::update(const UpdateTime &time) {
	Scene::update(time);

	/*auto t = time.app % 5_usec;

	if (_sprite) {
		_sprite->setRotation(M_PI * 2.0 * (float(t) / 5_usec));
	}*/
}

void AppScene::onEnter(Scene *scene) {
	Scene::onEnter(scene);
	std::cout << "AppScene::onEnter\n";
}

void AppScene::onExit() {
	std::cout << "AppScene::onExit\n";
	Scene::onExit();
}

void AppScene::onContentSizeDirty() {
	Scene::onContentSizeDirty();

	_layout->setAnchorPoint(Anchor::Middle);
	_layout->setPosition(_contentSize / 2.0f);
	_layout->setContentSize(_contentSize);

	/*if (_sprite) {
		_sprite->setPosition(Vec2(_contentSize) / 2.0f);
		_sprite->setAnchorPoint(Anchor::Middle);
		_sprite->setContentSize(_contentSize / 2.0f);
	}

	if (_node1) {
		_node1->setContentSize(Size(_contentSize.width / 2.0f, _contentSize.height / 4.0f));
		_node1->setPosition(Vec2(_contentSize) / 2.0f - Vec2(0, _contentSize.height / 4.0f));
		_node1->setAnchorPoint(Anchor::Middle);
	}

	if (_node2) {
		_node2->setContentSize(Size(_contentSize.width / 2.0f, _contentSize.height / 4.0f));
		_node2->setPosition(Vec2(_contentSize) / 2.0f + Vec2(0, _contentSize.height / 4.0f));
		_node2->setAnchorPoint(Anchor::Middle);
	}*/
}

void AppScene::addFontController(const Rc<font::FontController> &c) {
	/*_sprite = addChild(Rc<Sprite>::create(Rc<Texture>(c->getTexture())));

	_sprite->setPosition(Vec2(_contentSize) / 2.0f);
	_sprite->setAnchorPoint(Anchor::Middle);
	_sprite->setContentSize(_contentSize / 2.0f);*/
}

}
