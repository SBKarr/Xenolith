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

#include "XLUtilScene.h"
#include "XLLabel.h"
#include "XLDirector.h"
#include "XLApplication.h"
#include "XLFontLibrary.h"

namespace stappler::xenolith {

class FpsDisplay : public Node {
public:
	virtual ~FpsDisplay() { }

	virtual bool init(font::FontController *fontController);
	virtual void update(const UpdateTime &) override;

protected:
	uint32_t _frames = 0;
	Label *_label = nullptr;
};

bool FpsDisplay::init(font::FontController *fontController) {
	if (!Node::init()) {
		return false;
	}

	if (fontController) {
		_label = addChild(Rc<Label>::create(fontController), maxOf<int16_t>());
		_label->setString("0.0\n0.0\n0.0\n0 0 0 0");
		_label->setFontFamily("monospace");
		_label->setAnchorPoint(Anchor::BottomLeft);
		_label->setColor(Color::Black, true);
		_label->setFontSize(16);
		_label->setOnContentSizeDirtyCallback([this] {
			setContentSize(_label->getContentSize());
		});
	}

	scheduleUpdate();
	setVisible(false);

	 return true;
}

void FpsDisplay::update(const UpdateTime &) {
	if (_director) {
		auto fps = _director->getAvgFps();
		auto spf = _director->getSpf();
		auto local = _director->getLocalFrameTime();
		auto stat = _director->getDrawStat();

		if (_label) {
			auto str = toString(std::setprecision(3), fps, "\n", spf, "\n", local, "\n",
					stat.vertexes, " ", stat.triangles, " ", stat.zPaths, " ", stat.drawCalls);
			_label->setString(str);
		}
		++ _frames;
	}
}

bool UtilScene::init(Application *app, RenderQueue::Builder &&builder) {
	if (!Scene::init(app, move(builder))) {
		return false;
	}

	initialize(app);

	scheduleUpdate();

	return true;
}

bool UtilScene::init(Application *app, RenderQueue::Builder &&builder, Size2 size) {
	if (!Scene::init(app, move(builder), size)) {
		return false;
	}

	initialize(app);

	return true;
}

void UtilScene::update(const UpdateTime &time) {
	Scene::update(time);
}

void UtilScene::onContentSizeDirty() {
	Scene::onContentSizeDirty();

	if (_fps) {
		_fps->setPosition(Vec2(6.0f, 6.0f));
	}
}

void UtilScene::setFpsVisible(bool value) {
	_fps->setVisible(value);
}

bool UtilScene::isFpsVisible() const {
	return _fps->isVisible();
}

void UtilScene::initialize(Application *app) {
	_fps = addChild(Rc<FpsDisplay>::create(app->getFontController()), maxOf<int16_t>());
}

}
