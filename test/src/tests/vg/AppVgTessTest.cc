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

#include "AppVgTessTest.h"
#include "AppButton.h"

namespace stappler::xenolith::app {

class VgWindingSwitcher : public Button {
public:
	virtual ~VgWindingSwitcher() { }

	virtual bool init(vg::Winding, Function<void(vg::Winding)> &&cb);

	virtual void onContentSizeDirty() override;

protected:
	void updateWinding();

	vg::Winding _winding = vg::Winding::EvenOdd;
	Function<void(vg::Winding)> _windingCallback;
	Label *_label = nullptr;
};

class VgDrawStyleSwitcher : public Button {
public:
	virtual ~VgDrawStyleSwitcher() { }

	virtual bool init(vg::DrawStyle, Function<void(vg::DrawStyle)> &&cb);

	virtual void onContentSizeDirty() override;

protected:
	void updateStyle();

	vg::DrawStyle _style = vg::DrawStyle::Fill;
	Function<void(vg::DrawStyle)> _windingCallback;
	Label *_label = nullptr;
};

class VgContourSwitcherButton : public Button {
public:
	virtual ~VgContourSwitcherButton() { }

	virtual bool init(uint32_t idx, Function<void()> &&cb);

	virtual void onContentSizeDirty() override;

protected:
	uint32_t _index = 0;
	Label *_label = nullptr;
	VectorSprite *_indicator = nullptr;
};

class VgContourSwitcherAdd : public Button {
public:
	virtual ~VgContourSwitcherAdd() { }

	virtual bool init(Function<void()> &&cb);

	virtual void onContentSizeDirty() override;

protected:
	Label *_label = nullptr;
};

class VgContourSwitcher : public Node {
public:
	virtual ~VgContourSwitcher() { }

	virtual bool init(uint32_t count, uint32_t selected);

	virtual void onContentSizeDirty() override;

	void setContours(uint32_t count, uint32_t selected);
	void setAddCallback(Function<void()> &&cb);
	void setSelectedCallback(Function<void(uint32_t)> &&cb);

protected:
	using Node::init;

	VgContourSwitcherAdd *_add = nullptr;
	Vector<VgContourSwitcherButton *> _buttons;
	Function<void(uint32_t)> _selectedCallback;
	uint32_t _selected = 0;
};

bool VgWindingSwitcher::init(vg::Winding w, Function<void(vg::Winding)> &&cb) {
	if (!Button::init([this] {
		auto w = toInt(_winding);
		++ w;
		w = w % 5;
		_winding = vg::Winding(w);
		updateWinding();
		_windingCallback(_winding);
	})) {
		return false;
	}

	_winding = w;
	_windingCallback = move(cb);

	_label = addChild(Rc<Label>::create());
	_label->setFontSize(20);
	_label->setOnContentSizeDirtyCallback([this] {
		setContentSize(_label->getContentSize() + Size2(24, 12));
	});
	_label->setAnchorPoint(Anchor::Middle);

	updateWinding();

	return true;
}

void VgWindingSwitcher::onContentSizeDirty() {
	Button::onContentSizeDirty();
	_label->setPosition(_contentSize / 2.0f);
}

void VgWindingSwitcher::updateWinding() {
	switch (_winding) {
	case vg::Winding::EvenOdd: _label->setString("Winding: EvenOdd"); break;
	case vg::Winding::NonZero: _label->setString("Winding: NonZero"); break;
	case vg::Winding::Positive: _label->setString("Winding: Positive"); break;
	case vg::Winding::Negative: _label->setString("Winding: Negative"); break;
	case vg::Winding::AbsGeqTwo: _label->setString("Winding: AbsGeqTwo"); break;
	}
}

bool VgDrawStyleSwitcher::init(vg::DrawStyle style, Function<void(vg::DrawStyle)> &&cb) {
	if (!Button::init([this] {
		auto w = toInt(_style);
		++ w;
		if (w > 3) {
			w = 1;
		}
		_style = vg::DrawStyle(w);
		updateStyle();
		_windingCallback(_style);
	})) {
		return false;
	}

	_style = style;
	_windingCallback = move(cb);

	_label = addChild(Rc<Label>::create());
	_label->setFontSize(20);
	_label->setOnContentSizeDirtyCallback([this] {
		setContentSize(_label->getContentSize() + Size2(24, 12));
	});
	_label->setAnchorPoint(Anchor::Middle);

	updateStyle();

	return true;
}

void VgDrawStyleSwitcher::onContentSizeDirty() {
	Button::onContentSizeDirty();
	_label->setPosition(_contentSize / 2.0f);
}

void VgDrawStyleSwitcher::updateStyle() {
	switch (_style) {
	case vg::DrawStyle::Fill: _label->setString("DrawStyle: Fill"); break;
	case vg::DrawStyle::Stroke: _label->setString("DrawStyle: Stroke"); break;
	case vg::DrawStyle::FillAndStroke: _label->setString("DrawStyle: FillAndStroke"); break;
	case vg::DrawStyle::None: break;
	}
}

bool VgContourSwitcherButton::init(uint32_t idx, Function<void()> &&cb) {
	if (!Button::init(move(cb))) {
		return false;
	}

	_index = idx;

	_label = addChild(Rc<Label>::create());
	_label->setFontSize(16);
	_label->setString(toString("Contour ", idx));
	_label->setOnContentSizeDirtyCallback([this] {
		setContentSize(_label->getContentSize() + Size2(50, 12));
	});
	_label->setAnchorPoint(Anchor::MiddleLeft);

	auto image = Rc<VectorImage>::create(Size2(10, 10));
	image->addPath("", "org.stappler.xenolith.tess.TessPoint")
		->setFillColor(Color::White)
		.addOval(Rect(0, 0, 10, 10))
		.setAntialiased(false);

	_indicator = addChild(Rc<VectorSprite>::create(move(image)), 1);
	_indicator->setColor(VgTessCanvas::getColorForIndex(_index));
	_indicator->setAnchorPoint(Anchor::MiddleRight);
	_indicator->setContentSize(Size2(16.0f, 16.0f));

	return true;
}

void VgContourSwitcherButton::onContentSizeDirty() {
	Button::onContentSizeDirty();
	_label->setPosition(Vec2(12.0f, _contentSize.height / 2.0f));
	_indicator->setPosition(Vec2(_contentSize.width - 12.0f, _contentSize.height / 2.0f));
}

bool VgContourSwitcherAdd::init(Function<void()> &&cb) {
	if (!Button::init(move(cb))) {
		return false;
	}

	_label = addChild(Rc<Label>::create());
	_label->setFontSize(16);
	_label->setString("Add contour");
	_label->setOnContentSizeDirtyCallback([this] {
		setContentSize(_label->getContentSize() + Size2(24, 12));
	});
	_label->setAnchorPoint(Anchor::Middle);

	return true;
}

void VgContourSwitcherAdd::onContentSizeDirty() {
	Button::onContentSizeDirty();
	_label->setPosition(_contentSize / 2.0f);
}

bool VgContourSwitcher::init(uint32_t count, uint32_t selected) {
	if (!Node::init()) {
		return false;
	}

	_selected = selected;

	for (uint32_t i = 0; i < count; ++ i) {
		auto v = addChild(Rc<VgContourSwitcherButton>::create(i, [this, i] {
			if (_selectedCallback) {
				_selectedCallback(i);
			}
		}));
		v->setAnchorPoint(Anchor::TopRight);
		if (i == selected) {
			v->setEnabled(true);
		}
		_buttons.emplace_back(v);
	}

	_add = addChild(Rc<VgContourSwitcherAdd>::create([] { }));
	_add->setAnchorPoint(Anchor::TopRight);

	return true;
}

void VgContourSwitcher::onContentSizeDirty() {
	Node::onContentSizeDirty();

	auto pt = Vec2(_contentSize);

	for (auto &it : _buttons) {
		it->setPosition(pt);
		pt -= Vec2(0.0f, 32.0f);
	}

	_add->setPosition(pt);
}

void VgContourSwitcher::setContours(uint32_t count, uint32_t selected) {
	if (_buttons.size() == count) {
		uint32_t i = 0;
		for (auto &it : _buttons) {
			it->setEnabled(i == selected);
			++ i;
		}
	} else {
		for (auto &it : _buttons) {
			it->removeFromParent();
		}

		_buttons.clear();

		for (uint32_t i = 0; i < count; ++ i) {
			auto v = addChild(Rc<VgContourSwitcherButton>::create(i, [this, i] {
				if (_selectedCallback) {
					_selectedCallback(i);
				}
			}));
			v->setAnchorPoint(Anchor::TopRight);
			if (i == selected) {
				v->setEnabled(true);
			}
			_buttons.emplace_back(v);
		}

		_contentSizeDirty = true;
	}
}

void VgContourSwitcher::setAddCallback(Function<void()> &&cb) {
	_add->setCallback(move(cb));
}

void VgContourSwitcher::setSelectedCallback(Function<void(uint32_t)> &&cb) {
	_selectedCallback = move(cb);
}

bool VgTessTest::init() {
	if (!LayoutTest::init(LayoutName::VgTessTest, "Click to add point, ctrl+click to remove")) {
		return false;
	}

	_canvas = addChild(Rc<VgTessCanvas>::create([this] {
		handleContoursUpdated();
	}));
	_canvas->setAnchorPoint(Anchor::Middle);

	_windingSwitcher = addChild(Rc<VgWindingSwitcher>::create(_canvas->getWinding(), [this] (vg::Winding w) {
		_canvas->setWinding(w);
	}));
	_windingSwitcher->setAnchorPoint(Anchor::TopLeft);

	_drawStyleSwitcher = addChild(Rc<VgDrawStyleSwitcher>::create(_canvas->getDrawStyle(), [this] (vg::DrawStyle w) {
		_canvas->setDrawStyle(w);
	}));
	_drawStyleSwitcher->setAnchorPoint(Anchor::TopLeft);

	_contourSwitcher = addChild(Rc<VgContourSwitcher>::create(_canvas->getContoursCount(), _canvas->getSelectedContour()));
	_contourSwitcher->setAnchorPoint(Anchor::TopRight);
	_contourSwitcher->setAddCallback([this] {
		_canvas->addContour();
	});
	_contourSwitcher->setSelectedCallback([this] (uint32_t n) {
		_canvas->setSelectedContour(n);
	});

	return true;
}

void VgTessTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	Vec2 center = _contentSize / 2.0f;

	if (_canvas) {
		_canvas->setPosition(center);
		_canvas->setContentSize(_contentSize);
	}

	if (_windingSwitcher) {
		_windingSwitcher->setPosition(Vec2(0.0f, _contentSize.height - 0.0f));
	}

	if (_drawStyleSwitcher) {
		_drawStyleSwitcher->setPosition(Vec2(0.0f, _contentSize.height - 40.0f));
	}

	if (_contourSwitcher) {
		_contourSwitcher->setPosition(Vec2(_contentSize) - Vec2(0.0f, 42.0f));
	}
}

void VgTessTest::handleContoursUpdated() {
	if (_contourSwitcher) {
		_contourSwitcher->setContours(_canvas->getContoursCount(), _canvas->getSelectedContour());
	}
}

}
