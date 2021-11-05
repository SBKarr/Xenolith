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

#include "XLAppScene.h"

#include "XLAppSceneResource.cc"
#include "XLDirector.h"
#include "XLSprite.h"
#include "XLPlatform.h"

#include "XLGlRenderQueue.h"
#include "XLDefaultShaders.h"

#include "XLVkImageAttachment.h"
#include "XLVkBufferAttachment.h"
#include "XLVkMaterialRenderPass.h"

namespace stappler::xenolith::app {

bool AppScene::init(Extent2 extent) {
	// acquire platform-specific swapchain image format
	// extent will be resized automatically to screen size
	gl::ImageInfo info(extent, gl::ImageUsage::ColorAttachment, platform::graphic::getCommonFormat());

	// build presentation RenderQueue
	gl::RenderQueue::Builder builder("Loader", gl::RenderQueue::Continuous);


	// load shaders by ref - do not copy content into engine
	auto materialFrag = builder.addProgramByRef("Loader_MaterialVert", xenolith::shaders::MaterialVert);
	auto materialVert = builder.addProgramByRef("Loader_MaterialFrag", xenolith::shaders::MaterialFrag);


	// render-to-swapchain RenderPass
	auto pass = Rc<vk::MaterialRenderPass>::create("SwapchainPass", gl::RenderOrderingHighest);
	builder.addRenderPass(pass);


	// pipeline for material-besed rendering
	auto materialPipeline = builder.addPipeline(pass, 0, "Materials", Vector<gl::SpecializationInfo>({
		// no specialization required for vertex shader
		materialVert,
		// specialization for fragment shader - use platform-dependent array sizes
		gl::SpecializationInfo(materialFrag, { gl::PredefinedConstant::SamplersArraySize, gl::PredefinedConstant::TexturesArraySize })
	}));


	// define internal resources (images and buffers)
	gl::Resource::Builder resourceBuilder("LoaderResources");
	auto initImage = resourceBuilder.addImage("Xenolith.png",
			gl::ImageInfo(gl::ImageFormat::R8G8B8A8_UNORM, gl::ImageUsage::Sampled),
			FilePath("resources/xenolith-1.png"));

	builder.setInternalResource(Rc<gl::Resource>::create(move(resourceBuilder)));

	// output attachment - swapchain
	auto out = Rc<vk::SwapchainAttachment>::create("Swapchain", move(info),
			gl::AttachmentLayout::Undefined, gl::AttachmentLayout::PresentSrc);

	// Engine-defined samplers as input attachment
	auto samplers = Rc<gl::SamplersAttachment>::create("Samplers");

	// Material input attachment - per-scene list of materials
	auto materialInput = Rc<vk::MaterialVertexAttachment>::create("MaterialInput",
			gl::BufferInfo(gl::BufferUsage::StorageBuffer),

			// ... with predefined list of materials
			Vector<Rc<gl::Material>>({
				Rc<gl::Material>::create(materialPipeline, initImage)
	}));

	// Vertex input attachment - per-frame vertex list
	auto vertexInput = Rc<vk::VertexMaterialAttachment>::create("VertexInput",
			gl::BufferInfo(gl::BufferUsage::StorageBuffer), materialInput);
	vertexInput->setInputCallback([this] (gl::FrameHandle &frame, const Rc<gl::AttachmentHandle> &a) {
		on2dVertexInput(frame, a);
	});

	// define pass input-output
	builder.addPassInput(pass, 0, samplers); // 0
	builder.addPassInput(pass, 0, vertexInput); // 1
	builder.addPassInput(pass, 0, materialInput); // 2
	builder.addPassOutput(pass, 0, out);

	// define global input-output
	// samplers and materialInput are persistent between frames, only vertexes should be provided before rendering started
	builder.addInput(vertexInput);
	builder.addOutput(out);

	// optional world-to-pass subpass dependency
	/*builder.addSubpassDependency(pass,
			gl::RenderSubpassDependency::External, gl::PipelineStage::ColorAttachmentOutput, gl::AccessType::None,
			0, gl::PipelineStage::ColorAttachmentOutput, gl::AccessType::ColorAttachmentWrite, false);*/

	if (!Scene::init(move(builder))) {
		return false;
	}

	scheduleUpdate();

	return true;
}

void AppScene::update(const UpdateTime &time) {
	Scene::update(time);

	auto t = time.app % 1_usec;

	setRotation(M_PI * 2.0 * (float(t) / 1_usec));
}

void AppScene::onEnter(Scene *scene) {
	Scene::onEnter(scene);
	std::cout << "AppScene::onEnter\n";

	auto sprite = Rc<Sprite>::create("Xenolith.png");
	_sprite = addChild(sprite);
}

void AppScene::onExit() {
	std::cout << "AppScene::onExit\n";
	Scene::onExit();
}

void AppScene::onContentSizeDirty() {
	Scene::onContentSizeDirty();

	_sprite->setPosition(Vec2(_contentSize) / 2.0f);
	_sprite->setAnchorPoint(Anchor::Middle);
	_sprite->setContentSize(_contentSize / 2.0f);
}

}
