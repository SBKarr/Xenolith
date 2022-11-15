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

#include "XLDefine.h"
#include "AppScene.h"

#include "XLVkAttachment.h"
#include "XLDirector.h"
#include "XLSprite.h"
#include "XLPlatform.h"
#include "XLApplication.h"

#include "XLVkAttachment.h"
#include "XLVkMaterialRenderPass.h"
#include "XLFontLibrary.h"

#include "AppRootLayout.h"

namespace stappler::xenolith::app {

bool AppScene::init(Application *app, Extent2 extent, float density) {
	// build presentation RenderQueue
	renderqueue::Queue::Builder builder("Loader");

	vk::MaterialPass::RenderQueueInfo info{
		app, &builder, extent,
		[this] (renderqueue::FrameQueue &frame, const Rc<renderqueue::AttachmentHandle> &a,	Function<void(bool)> &&cb) {
			on2dVertexInput(frame, a, move(cb));
		},
		[&] (renderqueue::Resource::Builder &resourceBuilder) {
			resourceBuilder.addImage("xenolith-1-480.png",
					gl::ImageInfo(gl::ImageFormat::R8G8B8A8_UNORM, gl::ImageUsage::Sampled, gl::ImageHints::Opaque),
					FilePath("resources/xenolith-1-480.png"));
			resourceBuilder.addImage("xenolith-2-480.png",
					gl::ImageInfo(gl::ImageFormat::R8G8B8A8_UNORM, gl::ImageUsage::Sampled, gl::ImageHints::Opaque),
					FilePath("resources/xenolith-2-480.png"));
		}
	};

	vk::MaterialPass::makeDefaultRenderQueue(info);

	if (!UtilScene::init(app, move(builder), extent, density)) {
		return false;
	}

	filesystem::mkdir(filesystem::cachesPath<Interface>());

	auto dataPath = filesystem::cachesPath<Interface>("org.stappler.xenolith.test.AppScene.cbor");
	if (auto d = data::readFile<Interface>(dataPath)) {
		auto layoutName = getLayoutNameById(d.getString("id"));

		_layout = addChild(makeLayoutNode(layoutName));
		if (!d.getValue("data").empty()) {
			_layout->setDataValue(move(d.getValue("data")));
		}
	}

	if (!_layout) {
		_layout = addChild(Rc<RootLayout>::create());
	}

	scheduleUpdate();

	return true;
}

void AppScene::onPresented(Director *dir) {
	UtilScene::onPresented(dir);
}

void AppScene::onFinished(Director *dir) {
	UtilScene::onFinished(dir);
}

void AppScene::update(const UpdateTime &time) {
	UtilScene::update(time);
}

void AppScene::onEnter(Scene *scene) {
	UtilScene::onEnter(scene);
	std::cout << "AppScene::onEnter\n";
}

void AppScene::onExit() {
	std::cout << "AppScene::onExit\n";
	UtilScene::onExit();
}

void AppScene::onContentSizeDirty() {
	UtilScene::onContentSizeDirty();

	if (_layout) {
		_layout->setAnchorPoint(Anchor::Middle);
		_layout->setPosition(_contentSize / 2.0f);
		_layout->setContentSize(_contentSize);
	}
}

void AppScene::runLayout(LayoutName l, Rc<Node> &&node) {
	if (_layout) {
		_layout->removeFromParent(true);
		_layout = nullptr;
	}

	_layout = addChild(move(node));
	_contentSizeDirty = true;
}

void AppScene::setActiveLayoutId(StringView name, Value &&data) {
	Value sceneData({
		pair("id", Value(name)),
		pair("data", Value(move(data)))
	});

	auto path = filesystem::cachesPath<Interface>("org.stappler.xenolith.test.AppScene.cbor");
	data::save(sceneData, path, data::EncodeFormat::CborCompressed);
}

}
