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

#include "AppRootLayout.h"
#include "XLInputListener.h"
#include "XLAction.h"

#include "XLGuiLayerRounded.h"

namespace stappler::xenolith::app {

class RootLayoutButton : public Node {
public:
	virtual ~RootLayoutButton() { }

	virtual bool init(StringView);

	virtual void onContentSizeDirty() override;

protected:
	void handleFocusEnter();
	void handleFocusLeave();

	Layer *_layer = nullptr;
	Label *_label = nullptr;
	bool _focus = false;
};

class RootLayoutSlider : public Node {
public:
protected:
	Layer *_layer = nullptr;
};

bool RootLayoutButton::init(StringView str) {
	if (!Node::init()) {
		return false;
	}

	_label = addChild(Rc<Label>::create(str), 2);
	_label->setAnchorPoint(Anchor::Middle);
	_label->setFontSize(48);
	_label->setOnContentSizeDirtyCallback([this] {
		setContentSize(_label->getContentSize() + Size2(20.0f, 20.0f));
	});

	_layer = addChild(Rc<Layer>::create(Color::Grey_200));
	_layer->setAnchorPoint(Anchor::BottomLeft);

	auto l = addInputListener(Rc<InputListener>::create());
	l->setTouchFilter([] (const InputEvent &event, const InputListener::DefaultEventFilter &) {
		return true;
	});
	l->addMoveRecognizer([this] (GestureEvent event, const InputEvent &ev) {
		bool touched = isTouched(ev.currentLocation);
		if (touched != _focus) {
			_focus = touched;
			if (_focus) {
				std::cout << "handleFocusEnter\n";
				handleFocusEnter();
			} else {
				std::cout << "handleFocusLeave\n";
				handleFocusLeave();
			}
		}
		return true;
	});

	return true;
}

void RootLayoutButton::onContentSizeDirty() {
	Node::onContentSizeDirty();

	_layer->setContentSize(_contentSize);
	_label->setPosition(_contentSize / 2.0f);
}

void RootLayoutButton::handleFocusEnter() {
	_layer->stopAllActions();
	_layer->runAction(Rc<Sequence>::create(
			Rc<Spawn>::create([] {
				log::text("Test", "test0.1");
			},[] {
				log::text("Test", "test0.2");
			},
			Rc<TintTo>::create(0.5f, Color::Red_200)),
			[] {
				log::text("Test", "test1");
			},
			1.5f,
			Rc<Spawn>::create([] {
				log::text("Test", "test2.1");
			},[] {
				log::text("Test", "test2.2");
			}),
			Rc<TintTo>::create(0.5f, Color::Blue_200),
			[] {
				log::text("Test", "test3");
			}
	));
}

void RootLayoutButton::handleFocusLeave() {
	_layer->stopAllActions();
	_layer->runAction(Rc<Sequence>::create(Rc<TintTo>::create(0.5f, Color::Grey_200)));
}

bool IconLayout::init() {
	if (!Node::init()) {
		return false;
	}

	float initialQuality = 2.0f;
	float initialScale = 0.5f;

	auto btn = addChild(Rc<RootLayoutButton>::create("Test button"));
	btn->setAnchorPoint(Anchor::BottomLeft);
	btn->setPosition(Vec2(20.0f, 20.0f));

	do {
		auto image = Rc<VectorImage>::create(Size2(24, 24));

		_sprite = addChild(Rc<VectorSprite>::create(move(image)), 0);
		_sprite->setContentSize(Size2(256, 256));
		_sprite->setAnchorPoint(Anchor::Middle);
		_sprite->setColor(Color::Black);
		_sprite->setOpacity(0.5f);
		_sprite->setQuality(initialQuality);
		_sprite->setScale(initialScale);
	} while (0);

	do {
		auto image = Rc<VectorImage>::create(Size2(24, 24));

		_triangles = addChild(Rc<VectorSprite>::create(move(image)), 1);
		_triangles->setContentSize(Size2(256, 256));
		_triangles->setAnchorPoint(Anchor::Middle);
		_triangles->setColor(Color::Green_500);
		_triangles->setOpacity(0.5f);
		_triangles->setLineWidth(1.0f);
		_triangles->setQuality(initialQuality);
		_triangles->setVisible(false);
		_triangles->setScale(initialScale);
	} while (0);

	_spriteLayer = addChild(Rc<LayerRounded>::create(Color::Grey_200, 20.0f), -1);
	_spriteLayer->setContentSize(Size2(256, 256));
	_spriteLayer->setAnchorPoint(Anchor::Middle);

	_label = addChild(Rc<Label>::create());
	_label->setFontSize(32);
	_label->setString(getIconName(_currentName));
	_label->setAnchorPoint(Anchor::MiddleTop);

	_info = addChild(Rc<Label>::create());
	_info->setFontSize(24);
	_info->setString("Test");
	_info->setAnchorPoint(Anchor::MiddleTop);

	_qualityLabel = addChild(Rc<Label>::create(toString("Quality: ", initialQuality)));
	_qualityLabel->setFontSize(24);
	_qualityLabel->setAnchorPoint(Anchor::MiddleLeft);

	_qualitySlider = addChild(Rc<AppSlider>::create((initialQuality - 0.1f) / 4.9f, [this] (float val) {
		updateQualityValue(val);
	}));
	_qualitySlider->setAnchorPoint(Anchor::TopLeft);
	_qualitySlider->setContentSize(Size2(128.0f, 32.0f));

	_scaleLabel = addChild(Rc<Label>::create(toString("Scale: ", initialScale)));
	_scaleLabel->setFontSize(24);
	_scaleLabel->setAnchorPoint(Anchor::MiddleLeft);

	_scaleSlider = addChild(Rc<AppSlider>::create((initialScale - 0.1f) / 2.9f, [this] (float val) {
		updateScaleValue(val);
	}));
	_scaleSlider->setAnchorPoint(Anchor::TopLeft);
	_scaleSlider->setContentSize(Size2(128.0f, 32.0f));

	_visibleLabel = addChild(Rc<Label>::create("Triangles"));
	_visibleLabel->setFontSize(24);
	_visibleLabel->setAnchorPoint(Anchor::MiddleLeft);

	_visibleCheckbox = addChild(Rc<AppCheckbox>::create(false, [this] (bool value) {
		_triangles->setVisible(value);
	}));
	_visibleCheckbox->setAnchorPoint(Anchor::TopLeft);
	_visibleCheckbox->setContentSize(Size2(32.0f, 32.0f));

	_antialiasLabel = addChild(Rc<Label>::create("Antialias"));
	_antialiasLabel->setFontSize(24);
	_antialiasLabel->setAnchorPoint(Anchor::MiddleLeft);

	_antialiasCheckbox = addChild(Rc<AppCheckbox>::create(_antialias, [this] (bool value) {
		updateAntialiasValue(value);
	}));
	_antialiasCheckbox->setAnchorPoint(Anchor::TopLeft);
	_antialiasCheckbox->setContentSize(Size2(32.0f, 32.0f));

	auto l = _sprite->addInputListener(Rc<InputListener>::create());
	l->addTouchRecognizer([this] (GestureEvent ev, const InputEvent &data) -> bool {
		if (ev == GestureEvent::Ended) {
			if (data.data.button == InputMouseButton::Mouse8) {
				if (_currentName == IconName::Action_3d_rotation_outline) {
					updateIcon(IconName::Toggle_toggle_on_solid);
				} else {
					updateIcon(IconName(toInt(_currentName) - 1));
				}
			} else if (data.data.button == InputMouseButton::Mouse9) {
				if (_currentName == IconName::Toggle_toggle_on_solid) {
					updateIcon(IconName::Action_3d_rotation_outline);
				} else {
					updateIcon(IconName(toInt(_currentName) + 1));
				}
			}
		}
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::MouseScrollLeft, InputMouseButton::MouseScrollRight, InputMouseButton::Mouse8, InputMouseButton::Mouse9}));

	l->addKeyRecognizer([this] (GestureEvent event, const InputEvent &ev) {
		if (event == GestureEvent::Ended) {
			if (ev.data.key.keycode == InputKeyCode::LEFT) {
				if (_currentName == IconName::Action_3d_rotation_outline) {
					updateIcon(IconName::Toggle_toggle_on_solid);
				} else {
					updateIcon(IconName(toInt(_currentName) - 1));
				}
			} else if (ev.data.key.keycode == InputKeyCode::RIGHT) {
				if (_currentName == IconName::Toggle_toggle_on_solid) {
					updateIcon(IconName::Action_3d_rotation_outline);
				} else {
					updateIcon(IconName(toInt(_currentName) + 1));
				}
			}
		}
		return true;
	}, InputListener::makeKeyMask({InputKeyCode::LEFT, InputKeyCode::RIGHT}));

	l->addPressRecognizer([this] (GestureEvent event, const GesturePress &ev) {
		std::cout << "Press: " << event << ": " << ev.pos << ": " << ev.time.toMillis() << ": " << ev.count << "\n";
		return true;
	}, TimeInterval::milliseconds(350), true);

	l->addSwipeRecognizer([this] (GestureEvent event, const GestureSwipe &ev) {
		std::cout << "Swipe: " << event << ": " << ev.midpoint << "\n";
		return true;
	});

	scheduleUpdate();
	updateIcon(_currentName);

	return true;
}

void IconLayout::onContentSizeDirty() {
	Node::onContentSizeDirty();

	_sprite->setPosition(Size2(_contentSize.width / 2.0f, _contentSize.height / 2.0f));
	_triangles->setPosition(Size2(_contentSize.width / 2.0f, _contentSize.height / 2.0f));
	_spriteLayer->setPosition(Size2(_contentSize.width / 2.0f, _contentSize.height / 2.0f));

	_label->setPosition(Vec2(_contentSize / 2.0f) - Vec2(0.0f, 128.0f));
	_info->setPosition(Vec2(_contentSize / 2.0f) - Vec2(0.0f, 180.0f));

	_qualitySlider->setPosition(Vec2(16.0f, _contentSize.height - 16.0f));
	_qualityLabel->setPosition(Vec2(156.0f, _contentSize.height - 32.0f));

	_scaleSlider->setPosition(Vec2(16.0f, _contentSize.height - 16.0f - 48.0f));
	_scaleLabel->setPosition(Vec2(156.0f, _contentSize.height - 32.0f - 48.0f));

	_visibleCheckbox->setPosition(Vec2(16.0f, _contentSize.height - 16.0f - 96.0f));
	_visibleLabel->setPosition(Vec2(64.0f, _contentSize.height - 32.0f - 96.0f));

	_antialiasCheckbox->setPosition(Vec2(16.0f, _contentSize.height - 16.0f - 144.0f));
	_antialiasLabel->setPosition(Vec2(64.0f, _contentSize.height - 32.0f - 144.0f));
}

}
