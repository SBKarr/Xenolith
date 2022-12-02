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

#include "AppConfigPresentModeSwitcher.h"

#include "../../AppDelegate.h"
#include "XLLabel.h"
#include "XLEventListener.h"
#include "XLInputListener.h"
#include "XLDirector.h"
#include "XLIconNames.h"
#include "XLAction.h"

namespace stappler::xenolith::app {

bool ConfigSwitcher::init(AppDelegate *app, uint32_t selected, Function<void(uint32_t)> &&cb) {
	if (!Node::init()) {
		return false;
	}

	_currentMode = getCurrentValue(app);
	_selectedMode = selected;
	_values = getValueList(app);

	_label = addChild(Rc<Label>::create(), 2);
	_label->setFontSize(20);
	_label->setAnchorPoint(Anchor::Middle);
	_label->setColor(Color::Black);

	_left = addChild(Rc<VectorSprite>::create(Size2(24, 24)), 2);
	_left->setAnchorPoint(Anchor::MiddleLeft);
	_left->setColor(Color::Grey_400);
	getIconData(IconName::Hardware_keyboard_arrow_left_solid, [&] (BytesView bytes) {
		_left->addPath(VectorPath().addPath(bytes).setFillColor(Color::White));
	});
	_left->setContentSize(Size2(40, 40));

	_right = addChild(Rc<VectorSprite>::create(Size2(24, 24)), 2);
	_right->setAnchorPoint(Anchor::MiddleRight);
	_right->setColor(Color::Grey_400);
	getIconData(IconName::Hardware_keyboard_arrow_right_solid, [&] (BytesView bytes) {
		_right->addPath(VectorPath().addPath(bytes).setFillColor(Color::White));
	});
	_right->setContentSize(Size2(40, 40));

	InputListener *l = nullptr;
	_layerLeft = addChild(Rc<Layer>::create(SimpleGradient(Color::Grey_100)), 1);
	_layerLeft->setAnchorPoint(Anchor::MiddleLeft);
	l = _layerLeft->addInputListener(Rc<InputListener>::create());
	l->addMouseOverRecognizer([this] (const GestureData &data) {
		_selectedLeft = data.event == GestureEvent::Began;
		updateState();
		return true;
	});
	l->addPressRecognizer([this] (const GesturePress &press) {
		if (press.event != GestureEvent::Ended) {
			handlePrevMode();
		}
		return true;
	});

	_layerRight = addChild(Rc<Layer>::create(SimpleGradient(Color::Grey_100)), 1);
	_layerRight->setAnchorPoint(Anchor::MiddleRight);
	l = _layerRight->addInputListener(Rc<InputListener>::create());
	l->addMouseOverRecognizer([this] (const GestureData &data) {
		_selectedRight = data.event == GestureEvent::Began;
		updateState();
		return true;
	});
	l->addPressRecognizer([this] (const GesturePress &press) {
		if (press.event != GestureEvent::Ended) {
			handleNextMode();
		}
		return true;
	});

	_callback = move(cb);

	uint32_t idx = 0;
	for (auto &it : _values) {
		if (it == _selectedMode) {
			_presentIndex = idx;
		}
		auto l = addChild(Rc<Layer>::create(it == _selectedMode ? Color::Red_500 : Color::Red_100), 2);
		l->setAnchorPoint(Anchor::MiddleBottom);
		l->setContentSize(Size2(8.0f, 8.0f));
		l->setTag(it);
		_layers.emplace_back(l);
		++ idx;
	}

	updateState();
	return true;
}

void ConfigSwitcher::onContentSizeDirty() {
	Node::onContentSizeDirty();

	_label->setPosition(Vec2(_contentSize / 2.0f) + Vec2(0.0f, 4.0f));

	auto offset = Vec2((_contentSize.width - (_layers.size() * 12.0f) + 4.0f) / 2.0f, 4.0f);
	for (auto &it : _layers) {
		it->setPosition(offset);
		offset.x += 12.0f;
	}

	_left->setPosition(Vec2(2.0f, _contentSize.height / 2.0f));
	_layerLeft->setPosition(Vec2(0.0, _contentSize.height / 2.0f));
	_layerLeft->setContentSize(Size2(_contentSize.width / 2.0f, _contentSize.height));

	_right->setPosition(Vec2(_contentSize.width - 2.0f, _contentSize.height / 2.0f));
	_layerRight->setPosition(Vec2(_contentSize.width, _contentSize.height / 2.0f));
	_layerRight->setContentSize(Size2(_contentSize.width / 2.0f, _contentSize.height));
}

void ConfigSwitcher::updateState() {
	_label->setString(getValueLabel(_selectedMode));
	if (_selectedMode != _currentMode) {
		if (!_selectedLeft || _presentIndex == 0) {
			applyGradient(_layerLeft, SimpleGradient(Color::Red_50));
		} else {
			applyGradient(_layerLeft, SimpleGradient(Color::Grey_300, Color::Red_50, SimpleGradient::Horizontal));
		}
		if (!_selectedRight || _presentIndex + 1 == _layers.size()) {
			applyGradient(_layerRight, SimpleGradient(Color::Red_50));
		} else {
			applyGradient(_layerRight, SimpleGradient(Color::Red_50, Color::Grey_300, SimpleGradient::Horizontal));
		}
	} else {
		if (!_selectedLeft || _presentIndex == 0) {
			applyGradient(_layerLeft, SimpleGradient(Color::Grey_100));
		} else {
			applyGradient(_layerLeft, SimpleGradient(Color::Grey_300, Color::Grey_100, SimpleGradient::Horizontal));
		}
		if (!_selectedRight || _presentIndex + 1 == _layers.size()) {
			applyGradient(_layerRight, SimpleGradient(Color::Grey_100));
		} else {
			applyGradient(_layerRight, SimpleGradient(Color::Grey_100, Color::Grey_300, SimpleGradient::Horizontal));
		}
	}

	for (auto &it : _layers) {
		if (it->getTag() == _selectedMode) {
			if (it->getColor() != Color::Red_500.asColor4F()) {
				it->stopAllActionsByTag(1);
				it->runAction(Rc<TintTo>::create(0.15f, Color::Red_500), 1);
			}
		} else {
			if (it->getColor() != Color::Red_100.asColor4F()) {
				it->stopAllActionsByTag(1);
				it->runAction(Rc<TintTo>::create(0.15f, Color::Red_100), 1);
			}
		}
	}

	_left->setVisible(_presentIndex != 0);
	_right->setVisible(_presentIndex + 1 != _values.size());
}

void ConfigSwitcher::applyGradient(Layer *layer, const SimpleGradient &gradient) {
	layer->stopAllActionsByTag(1);
	if (layer->getGradient() != gradient) {
		layer->runAction(Rc<ActionProgress>::create(0.15f, [layer, start = layer->getGradient(), target = gradient] (float p) {
			layer->setGradient(progress(start, target, p));
		}), 1);
	}
}

void ConfigSwitcher::handlePrevMode() {
	if (_presentIndex > 0) {
		-- _presentIndex;
		if (_layers[_presentIndex]->getTag() != _selectedMode) {
			_selectedMode = _layers[_presentIndex]->getTag();
			_callback(_selectedMode);
		}
		updateState();
	}
}

void ConfigSwitcher::handleNextMode() {
	if (_presentIndex + 1 < _layers.size()) {
		++ _presentIndex;
		if (_layers[_presentIndex]->getTag() != _selectedMode) {
			_selectedMode = _layers[_presentIndex]->getTag();
			_callback(_selectedMode);
		}
		updateState();
	}
}

bool ConfigPresentModeSwitcher::init(AppDelegate *app, uint32_t selected, Function<void(uint32_t)> &&cb) {
	if (!ConfigSwitcher::init(app, selected, move(cb))) {
		return false;
	}

	auto el = Rc<EventListener>::create();
	el->onEvent(AppDelegate::onSwapchainConfig, [this] (const Event &event) {
		updateAppData((AppDelegate *)event.getObject());
		_contentSizeDirty = true;
	});
	addComponent(move(el));

	return true;
}

uint32_t ConfigPresentModeSwitcher::getCurrentValue(AppDelegate *app) const {
	return toInt(app->getSwapchainConfig().presentMode);
}

Vector<uint32_t> ConfigPresentModeSwitcher::getValueList(AppDelegate *app) const {
	Vector<uint32_t> ret;
	for (auto &it : app->getSurfaceinfo().presentModes) {
		ret.emplace_back(toInt(it));
	}
	return ret;
}

String ConfigPresentModeSwitcher::getValueLabel(uint32_t val) const {
	return toString("PresentMode: ", gl::getPresentModeName(gl::PresentMode(val)));
}

void ConfigPresentModeSwitcher::updateAppData(AppDelegate *app) {
	for (auto &it : _layers) {
		it->removeFromParent(true);
	}
	_layers.clear();

	_selectedMode = _currentMode = getCurrentValue(app);
	_values = getValueList(app);
	_presentIndex = maxOf<uint32_t>();

	uint32_t idx = 0;
	for (auto &it : _values) {
		if (it == _selectedMode) {
			_presentIndex = idx;
		}
		auto l = addChild(Rc<Layer>::create(it == _selectedMode ? Color::Red_500 : Color::Red_100), 2);
		l->setAnchorPoint(Anchor::MiddleBottom);
		l->setContentSize(Size2(8.0f, 8.0f));
		l->setTag(it);
		_layers.emplace_back(l);
		++ idx;
	}

	updateState();
}

}
