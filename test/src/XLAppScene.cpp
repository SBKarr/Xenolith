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
#include "XLVkRenderPass.h"

namespace stappler::xenolith::app {

bool AppScene::init(Extent2 extent) {
	gl::ImageInfo info(extent, gl::ImageUsage::ColorAttachment, platform::graphic::getCommonFormat());

	gl::RenderQueue::Builder builder("Loader", gl::RenderQueue::Continuous);
	auto defaultFrag = builder.addProgramByRef("Loader_DefaultVert", gl::ProgramStage::Vertex, stappler::xenolith::shaders::DefaultVert);
	auto defaultVert = builder.addProgramByRef("Loader_DefaultFrag", gl::ProgramStage::Fragment, stappler::xenolith::shaders::DefaultFrag);
	auto vertexFrag = builder.addProgramByRef("Loader_VertexVert", gl::ProgramStage::Vertex, stappler::xenolith::shaders::VertexVert);
	auto vertexVert = builder.addProgramByRef("Loader_VertexFrag", gl::ProgramStage::Fragment, stappler::xenolith::shaders::VertexFrag);

	auto out = Rc<vk::SwapchainAttachment>::create("Swapchain", move(info),
			gl::AttachmentLayout::Undefined, gl::AttachmentLayout::PresentSrc);

	auto input = Rc<vk::VertexBufferAttachment>::create("VertexInput", gl::BufferInfo(gl::BufferUsage::StorageBuffer, gl::ProgramStage::Vertex));

	auto samplers = Rc<gl::SamplersAttachment>::create("Samplers");

	auto pass = Rc<vk::VertexRenderPass>::create("SwapchainPass", gl::RenderOrderingHighest);
	builder.addRenderPass(pass);
	builder.addPipeline(pass, 0, "Default", Vector<const gl::ProgramData *>({
		defaultVert,
		defaultFrag
	}));
	builder.addPipeline(pass, 0, "Vertexes", Vector<const gl::ProgramData *>({
		vertexVert,
		vertexFrag
	}));

	/*gl::Resource::Builder resourceBuilder("LoaderResources");
	resourceBuilder.addImage("Xenolith", pass,
			gl::ImageInfo(gl::ImageFormat::R8G8B8A8_UNORM, gl::ImageUsage::Sampled, gl::ProgramStage::Fragment),
			FilePath("resources/xenolith-1.png"));

	builder.setInternalResource(Rc<gl::Resource>::create(move(resourceBuilder)));*/

	/*builder.addSubpassDependency(pass,
			gl::RenderSubpassDependency::External, gl::PipelineStage::ColorAttachmentOutput, gl::AccessType::None,
			0, gl::PipelineStage::ColorAttachmentOutput, gl::AccessType::ColorAttachmentWrite, false);*/

	builder.addPassInput(pass, 0, input);
	builder.addPassInput(pass, 0, samplers);
	builder.addPassOutput(pass, 0, out);

	builder.addInput(input);
	builder.addOutput(out);

	if (!Scene::init(move(builder))) {
		return false;
	}

	// addComponent(Rc<AppSceneResource>::create());

	return true;
}

void AppScene::onEnter() {
	Scene::onEnter();
	std::cout << "AppScene::onEnter\n";

	//auto pipeline = _director->getResourceCache()->getPipeline(AppSceneResource::RequestName, AppSceneResource::VertexPipelineName);
	//auto sprite = Rc<Sprite>::create(pipeline);
	//_sprite = addChild(sprite);
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
