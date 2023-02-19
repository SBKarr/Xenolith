/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLUtilScene.h"
#include "XLLabel.h"
#include "XLLayer.h"
#include "XLDirector.h"
#include "XLApplication.h"
#include "XLFontLibrary.h"

namespace stappler::xenolith {

class UtilScene::FpsDisplay : public Layer {
public:
	enum DisplayMode {
		Fps,
		Vertexes,
		Cache,
		Full,
		Disabled,
	};

	virtual ~FpsDisplay() { }

	virtual bool init(font::FontController *fontController);
	virtual void update(const UpdateTime &) override;

	void incrementMode();

protected:
	using Layer::init;

	uint32_t _frames = 0;
	Label *_label = nullptr;
	DisplayMode _mode = Fps;
};

bool UtilScene::FpsDisplay::init(font::FontController *fontController) {
	if (!Layer::init(Color::White)) {
		return false;
	}

	if (fontController) {
		_label = addChild(Rc<Label>::create(fontController), Node::ZOrderMax);
		_label->setString("0.0\n0.0\n0.0\n0 0 0 0");
		_label->setFontFamily("monospace");
		_label->setAnchorPoint(Anchor::BottomLeft);
		_label->setColor(Color::Black, true);
		_label->setFontSize(16);
		_label->setOnContentSizeDirtyCallback([this] {
			setContentSize(_label->getContentSize());
		});
		_label->setPersistentLayout(true);
		_label->addCommandFlags(gl::CommandFlags::DoNotCount);
	}

	addCommandFlags(gl::CommandFlags::DoNotCount);
	scheduleUpdate();

	return true;
}

void UtilScene::FpsDisplay::update(const UpdateTime &) {
	if (_director) {
		auto fps = _director->getAvgFps();
		auto spf = _director->getSpf();
		auto local = _director->getLocalFrameTime();
		auto stat = _director->getDrawStat();

		if (_label) {
			String str;
			switch (_mode) {
			case Fps:
				str = toString(std::setprecision(3),
					"FPS: ", fps, "\nSPF: ", spf, "\nGPU: ", local,
					"\nF12 to switch");
				break;
			case Vertexes:
				str = toString(std::setprecision(3),
					"V:", stat.vertexes, " T:", stat.triangles, "\nZ:", stat.zPaths, " C:", stat.drawCalls, " M: ", stat.materials, "\n",
					stat.solidCmds, "/", stat.surfaceCmds, "/", stat.transparentCmds,
					"\nF12 to switch");
				break;
			case Cache:
				str = toString(std::setprecision(3),
					"Cache:", stat.cachedFramebuffers, "/", stat.cachedImages, "/", stat.cachedImageViews,
					"\nF12 to switch");
				break;
			case Full:
				str = toString(std::setprecision(3),
					"FPS: ", fps, "\nSPF: ", spf, "\nGPU: ", local, "\n",
					"V:", stat.vertexes, " T:", stat.triangles, "\nZ:", stat.zPaths, " C:", stat.drawCalls, " M: ", stat.materials, "\n",
					stat.solidCmds, "/", stat.surfaceCmds, "/", stat.transparentCmds, "\n",
					"Cache:", stat.cachedFramebuffers, "/", stat.cachedImages, "/", stat.cachedImageViews,
					"\nF12 to switch");
				break;
			default:
				break;
			}
			_label->setString(str);
		}
		++ _frames;
	}
}

void UtilScene::FpsDisplay::incrementMode() {
	_mode = DisplayMode(toInt(_mode) + 1);
	if (_mode > Disabled) {
		_mode = Fps;
	}

	setVisible(_mode != Disabled);
}

bool UtilScene::init(Application *app, RenderQueue::Builder &&builder, const gl::FrameContraints &constraints) {
	if (!Scene::init(app, move(builder), constraints)) {
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
	_fps = addChild(Rc<FpsDisplay>::create(app->getFontController()), Node::ZOrderMax);

	auto l = addInputListener(Rc<InputListener>::create());
	l->addKeyRecognizer([this] (const GestureData &ev) {
		if (ev.event == GestureEvent::Ended) {
			_fps->incrementMode();
		}
		return true;
	}, InputListener::makeKeyMask({InputKeyCode::F12}));
}

}
