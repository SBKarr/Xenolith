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
	gl::ImageInfo info(extent, gl::ImageUsage::ColorAttachment, platform::graphic::getCommonFormat());
	gl::RenderQueue::Builder builder("Loader", gl::RenderQueue::Continuous);
	auto materialFrag = builder.addProgramByRef("Loader_MaterialVert", xenolith::shaders::MaterialVert);
	auto materialVert = builder.addProgramByRef("Loader_MaterialFrag", xenolith::shaders::MaterialFrag);
	// auto vertexFrag = builder.addProgramByRef("Loader_VertexVert", gl::ProgramStage::Vertex, xenolith::shaders::VertexVert);
	// auto vertexVert = builder.addProgramByRef("Loader_VertexFrag", gl::ProgramStage::Fragment, xenolith::shaders::VertexFrag);

	auto pass = Rc<vk::MaterialRenderPass>::create("SwapchainPass", gl::RenderOrderingHighest);
	builder.addRenderPass(pass);

	/*builder.addPipeline(pass, 0, "Vertexes", Vector<gl::SpecializationInfo>({
		vertexVert,
		vertexFrag
	}));*/
	auto materialPipeline = builder.addPipeline(pass, 0, "Materials", Vector<gl::SpecializationInfo>({
		materialVert,
		gl::SpecializationInfo(materialFrag, { gl::PredefinedConstant::SamplersArraySize, gl::PredefinedConstant::TexturesArraySize })
	}));

	gl::Resource::Builder resourceBuilder("LoaderResources");
	auto initImage = resourceBuilder.addImage("Xenolith", pass,
			gl::ImageInfo(gl::ImageFormat::R8G8B8A8_UNORM, gl::ImageUsage::Sampled),
			FilePath("resources/xenolith-1.png"));

	builder.setInternalResource(Rc<gl::Resource>::create(move(resourceBuilder)));

	auto out = Rc<vk::SwapchainAttachment>::create("Swapchain", move(info),
			gl::AttachmentLayout::Undefined, gl::AttachmentLayout::PresentSrc);

	auto samplers = Rc<gl::SamplersAttachment>::create("Samplers");

	auto vertexInput = Rc<vk::VertexMaterialAttachment>::create("VertexInput",
			gl::BufferInfo(gl::BufferUsage::StorageBuffer));

	auto materialInput = Rc<vk::MaterialVertexAttachment>::create("MaterialInput",
			gl::BufferInfo(gl::BufferUsage::StorageBuffer),
			Vector<Rc<gl::Material>>({
				Rc<gl::Material>::create(0, materialPipeline, initImage)
	}));

	builder.addPassInput(pass, 0, samplers); // 0
	builder.addPassInput(pass, 0, vertexInput); // 1
	builder.addPassInput(pass, 0, materialInput); // 2
	builder.addPassOutput(pass, 0, out);

	builder.addInput(vertexInput);
	builder.addOutput(out);

	/*builder.addSubpassDependency(pass,
			gl::RenderSubpassDependency::External, gl::PipelineStage::ColorAttachmentOutput, gl::AccessType::None,
			0, gl::PipelineStage::ColorAttachmentOutput, gl::AccessType::ColorAttachmentWrite, false);*/
	if (!Scene::init(move(builder))) {
		return false;
	}

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
