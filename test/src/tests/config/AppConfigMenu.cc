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

#include "AppConfigMenu.h"

#include "../../AppDelegate.h"
#include "AppConfigPresentModeSwitcher.h"
#include "AppSlider.h"
#include "XLEventListener.h"
#include "XLGlView.h"

namespace stappler::xenolith::app {

class ConfigApplyButton : public Layer {
public:
	virtual ~ConfigApplyButton() { }

	bool init(bool enabled, Function<void()> &&);

	virtual void onContentSizeDirty() override;

	void setEnabled(bool);

protected:
	using Layer::init;

	bool _enabled = false;
	Function<void()> _callback;
	Label *_label = nullptr;
};

class ConfigFrameRateSlider : public Layer {
public:
	virtual ~ConfigFrameRateSlider() { }

	bool init(uint64_t value, Function<void(uint64_t)> &&);

	virtual void onContentSizeDirty() override;

	void setValue(uint64_t);

protected:
	using Layer::init;

	uint64_t _currentRate = maxOf<uint64_t>();
	AppSliderWithLabel *_slider = nullptr;
};

bool ConfigApplyButton::init(bool enabled, Function<void()> &&cb) {
	if (!Layer::init(Color::Grey_50)) {
		return false;
	}

	_enabled = enabled;
	_callback = move(cb);

	_label = addChild(Rc<Label>::create());
	_label->setFontSize(20);
	_label->setColor(_enabled ? Color::Black : Color::Grey_500);
	_label->setAnchorPoint(Anchor::Middle);
	_label->setString("Apply");

	auto l = addInputListener(Rc<InputListener>::create());
	l->addMouseOverRecognizer([this] (const GestureData &data) {
		if (_enabled) {
			stopAllActionsByTag(1);
			runAction(Rc<TintTo>::create(0.15f, data.event == GestureEvent::Began ? Color::Grey_200 : Color::Grey_50), 1);
		}
		return true;
	});
	l->addPressRecognizer([this] (const GesturePress &press) {
		if (!_enabled) {
			return false;
		}

		if (press.event == GestureEvent::Ended) {
			_callback();
		}
		return true;
	});

	return true;
}

void ConfigApplyButton::onContentSizeDirty() {
	Layer::onContentSizeDirty();

	_label->setPosition(_contentSize / 2.0f);
}

void ConfigApplyButton::setEnabled(bool val) {
	if (_enabled != val) {
		_label->stopAllActionsByTag(1);
		_enabled = val;
		_label->runAction(Rc<TintTo>::create(0.15f, _enabled ? Color::Black : Color::Grey_500), 1);
		if (!_enabled) {
			stopAllActionsByTag(1);
			runAction(Rc<TintTo>::create(0.15f, Color::Grey_50), 1);
		}
	}
}

bool ConfigFrameRateSlider::init(uint64_t value, Function<void(uint64_t)> &&) {
	if (!Layer::init(Color::Grey_100)) {
		return false;
	}

	if (value == maxOf<uint64_t>()) {
		value = 1'000'000 / 60;
	} else if (value == 0) {
		value = 1'000'000 / 360;
	}

	uint64_t max = 1'000'000 / 10;
	uint64_t min = 1'000'000 / 360;

	auto val = 1.0f - float(value - min) / float(max - min);

	_slider = addChild(Rc<AppSliderWithLabel>::create("60", val, [this] (float value) {
		uint64_t max = 1'000'000 / 10;
		uint64_t min = 1'000'000 / 360;

		value = progress(max, min, value);

		setValue(value);
	}));
	_slider->setAnchorPoint(Anchor::Middle);
	_slider->setPrefix("Frame rate:");
	_slider->setFontSize(20);

	auto el = Rc<EventListener>::create();
	el->onEvent(gl::View::onFrameRate, [this] (const Event &event) {
		if (event.getObject() == _director->getView()) {
			/*_currentRate = uint64_t(event.getIntValue());

			uint64_t max = 1'000'000 / 10;
			uint64_t min = 1'000'000 / 360;

			auto val = 1.0f - float(_currentRate - min) / float(max - min);
			auto v = 1'000'000 / _currentRate;
			_slider->setValue(val);
			_slider->setString(toString(v));*/
		}
	});
	addComponent(move(el));
	auto v = 1'000'000 / value;
	_slider->setString(toString(v));

	return true;
}

void ConfigFrameRateSlider::onContentSizeDirty() {
	Layer::onContentSizeDirty();

	_slider->setContentSize(Size2(_contentSize.width - 216.0f, 24.0f));
	_slider->setPosition(Vec2(_contentSize / 2.0f) + Vec2(56.0f, 0.0f));
}

void ConfigFrameRateSlider::setValue(uint64_t val) {
	auto v = 1'000'000 / val;
	_slider->setString(toString(v));

	_director->getView()->setFrameInterval(val);
}

bool ConfigMenu::init() {
	if (!LayoutTest::init(LayoutName::Config, "")) {
		return false;
	}

	auto el = Rc<EventListener>::create();
	el->onEvent(AppDelegate::onSwapchainConfig, [this] (const Event &event) {
		updateAppData((AppDelegate *)event.getObject());
		updateApplyButton();
		_contentSizeDirty = true;
	});
	el->onEvent(gl::View::onFrameRate, [this] (const Event &event) {
		if (event.getObject() == _director->getView()) {
			_currentRate = uint64_t(event.getIntValue());
		}
	});
	addComponent(move(el));

	_scrollView = addChild(Rc<ScrollView>::create(ScrollView::Vertical));
	_scrollView->setAnchorPoint(Anchor::MiddleTop);
	_scrollView->setIndicatorColor(Color::Grey_500);
	auto controller = _scrollView->setController(Rc<ScrollController>::create());

	makeScrollList(controller);

	return true;
}

void ConfigMenu::onEnter(Scene *scene) {
	LayoutTest::onEnter(scene);

	updateAppData((AppDelegate *)_director->getApplication());
}

void ConfigMenu::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_scrollView->setPosition(Vec2(_contentSize.width / 2.0f, _contentSize.height));
	_scrollView->setContentSize(Size2(std::min(512.0f, _contentSize.width), _contentSize.height));
}

void ConfigMenu::makeScrollList(ScrollController *controller) {
	controller->addPlaceholder(24.0f);

	controller->addItem([this] (const ScrollController::Item &item) -> Rc<Node> {
		return Rc<ConfigApplyButton>::create(!_applyData.empty(), [this] () {
			applyConfig();
		});
	}, 42.0f, ZOrder(0));

	controller->addItem([this] (const ScrollController::Item &item) -> Rc<Node> {
		auto app = (AppDelegate *)_director->getApplication();
		auto m = _currentMode;
		auto modeIt = _applyData.find(ApplyPresentMode);
		if (modeIt != _applyData.end()) {
			m = gl::PresentMode(modeIt->second);
		}
		return Rc<ConfigPresentModeSwitcher>::create(app, toInt(m), [this] (uint32_t mode) {
			updatePresentMode(gl::PresentMode(mode));
		});
	}, 42.0f);
	controller->addItem([this] (const ScrollController::Item &item) -> Rc<Node> {
		return Rc<ConfigFrameRateSlider>::create(_currentRate, [] (uint64_t) { });
	}, 42.0f);
}

void ConfigMenu::updateAppData(AppDelegate *app) {
	_applyData.clear();

	_currentMode = app->getSwapchainConfig().presentMode;
	_currentRate = _director->getView()->getFrameInterval();
}

void ConfigMenu::updatePresentMode(gl::PresentMode mode) {
	if (mode != _currentMode) {
		_applyData.insert_or_assign(ApplyPresentMode, toInt(mode));
	} else {
		_applyData.erase(ApplyPresentMode);
	}
	updateApplyButton();
}

void ConfigMenu::updateApplyButton() {
	if (auto item = _scrollView->getController()->getItem("apply")) {
		if (item->node) {
			((ConfigApplyButton *)item->node)->setEnabled(!_applyData.empty());
		}
	}
}

void ConfigMenu::applyConfig() {
	auto app = (AppDelegate *)_director->getApplication();
	if (!app) {
		return;
	}

	for (auto &it : _applyData) {
		switch (it.first) {
		case ApplyPresentMode: app->setPreferredPresentMode(gl::PresentMode(it.second)); break;
		}
	}

	_director->getView()->deprecateSwapchain();
}

}
