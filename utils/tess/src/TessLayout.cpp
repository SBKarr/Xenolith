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

#include "XLDefine.h"
#include "XLInputListener.h"
#include "XLAction.h"
#include "TessLayout.h"
#include "TessAppDelegate.h"
#include "TessCanvas.h"
#include "TessButton.h"

namespace stappler::xenolith::tessapp {

class WindingSwitcher : public Button {
public:
	virtual ~WindingSwitcher() { }

	bool init(Function<void(vg::Winding)> &&cb) {
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

	virtual void onContentSizeDirty() override {
		Button::onContentSizeDirty();
		_label->setPosition(_contentSize / 2.0f);
	}

protected:
	void updateWinding() {
		switch (_winding) {
		case vg::Winding::EvenOdd: _label->setString("Winding: EvenOdd"); break;
		case vg::Winding::NonZero: _label->setString("Winding: NonZero"); break;
		case vg::Winding::Positive: _label->setString("Winding: Positive"); break;
		case vg::Winding::Negative: _label->setString("Winding: Negative"); break;
		case vg::Winding::AbsGeqTwo: _label->setString("Winding: AbsGeqTwo"); break;
		}
	}

	vg::Winding _winding = vg::Winding::EvenOdd;
	Function<void(vg::Winding)> _windingCallback;
	Label *_label = nullptr;
};

class DrawStyleSwitcher : public Button {
public:
	virtual ~DrawStyleSwitcher() { }

	bool init(Function<void(vg::DrawStyle)> &&cb) {
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

	virtual void onContentSizeDirty() override {
		Button::onContentSizeDirty();
		_label->setPosition(_contentSize / 2.0f);
	}

protected:
	void updateStyle() {
		switch (_style) {
		case vg::DrawStyle::Fill: _label->setString("DrawStyle: Fill"); break;
		case vg::DrawStyle::Stroke: _label->setString("DrawStyle: Stroke"); break;
		case vg::DrawStyle::FillAndStroke: _label->setString("DrawStyle: FillAndStroke"); break;
		case vg::DrawStyle::None: break;
		}
	}

	vg::DrawStyle _style = vg::DrawStyle::Fill;
	Function<void(vg::DrawStyle)> _windingCallback;
	Label *_label = nullptr;
};

class ContourSwitcherButton : public Button {
public:
	virtual ~ContourSwitcherButton() { }

	bool init(uint32_t idx, Function<void()> &&cb) {
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
		_indicator->setColor(TessCanvas::getColorForIndex(_index));
		_indicator->setAnchorPoint(Anchor::MiddleRight);
		_indicator->setContentSize(Size2(16.0f, 16.0f));

		return true;
	}

	virtual void onContentSizeDirty() override {
		Button::onContentSizeDirty();
		_label->setPosition(Vec2(12.0f, _contentSize.height / 2.0f));
		_indicator->setPosition(Vec2(_contentSize.width - 12.0f, _contentSize.height / 2.0f));
	}

protected:
	uint32_t _index = 0;
	Label *_label = nullptr;
	VectorSprite *_indicator = nullptr;
};

class ContourSwitcherAdd : public Button {
public:
	virtual ~ContourSwitcherAdd() { }

	bool init(Function<void()> &&cb) {
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

	virtual void onContentSizeDirty() override {
		Button::onContentSizeDirty();
		_label->setPosition(_contentSize / 2.0f);
	}

protected:
	Label *_label = nullptr;
};

class ContourSwitcher : public Node {
public:
	virtual ~ContourSwitcher() { }

	bool init(uint32_t count, uint32_t selected) {
		if (!Node::init()) {
			return false;
		}

		_selected = selected;

		for (uint32_t i = 0; i < count; ++ i) {
			auto v = addChild(Rc<ContourSwitcherButton>::create(i, [this, i] {
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

		_add = addChild(Rc<ContourSwitcherAdd>::create([] { }));
		_add->setAnchorPoint(Anchor::TopRight);

		return true;
	}

	virtual void onContentSizeDirty() override {
		Node::onContentSizeDirty();

		auto pt = Vec2(_contentSize);

		for (auto &it : _buttons) {
			it->setPosition(pt);
			pt -= Vec2(0.0f, 32.0f);
		}

		_add->setPosition(pt);
	}

	void setContours(uint32_t count, uint32_t selected) {
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
				auto v = addChild(Rc<ContourSwitcherButton>::create(i, [this, i] {
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

	void setAddCallback(Function<void()> &&cb) {
		_add->setCallback(move(cb));
	}

	void setSelectedCallback(Function<void(uint32_t)> &&cb) {
		_selectedCallback = move(cb);
	}

protected:
	ContourSwitcherAdd *_add = nullptr;
	Vector<ContourSwitcherButton *> _buttons;
	Function<void(uint32_t)> _selectedCallback;
	uint32_t _selected = 0;
};

bool TessLayout::init() {
	if (!Node::init()) {
		return false;
	}

	_background = addChild(Rc<Layer>::create(Color::White), -1);
	_background->setColorMode(ColorMode::IntensityChannel);
	_background->setAnchorPoint(Anchor::Middle);

	_canvas = addChild(Rc<TessCanvas>::create([this] {
		handleContoursUpdated();
	}));
	_canvas->setAnchorPoint(Anchor::Middle);

	_windingSwitcher = addChild(Rc<WindingSwitcher>::create([this] (vg::Winding w) {
		_canvas->setWinding(w);
	}));
	_windingSwitcher->setAnchorPoint(Anchor::TopLeft);

	_drawStyleSwitcher = addChild(Rc<DrawStyleSwitcher>::create([this] (vg::DrawStyle w) {
		_canvas->setDrawStyle(w);
	}));
	_drawStyleSwitcher->setAnchorPoint(Anchor::TopLeft);

	_contourSwitcher = addChild(Rc<ContourSwitcher>::create(_canvas->getContoursCount(), _canvas->getSelectedContour()));
	_contourSwitcher->setAnchorPoint(Anchor::TopRight);
	_contourSwitcher->setAddCallback([this] {
		_canvas->addContour();
	});
	_contourSwitcher->setSelectedCallback([this] (uint32_t n) {
		_canvas->setSelectedContour(n);
	});

	return true;
}

void TessLayout::onContentSizeDirty() {
	Node::onContentSizeDirty();

	Vec2 center = _contentSize / 2.0f;

	if (_background) {
		_background->setPosition(center);
		_background->setContentSize(_contentSize);
	}

	if (_canvas) {
		_canvas->setPosition(center);
		_canvas->setContentSize(_contentSize);
	}

	if (_windingSwitcher) {
		_windingSwitcher->setPosition(Vec2(12.0f, _contentSize.height - 12.0f));
	}

	if (_drawStyleSwitcher) {
		_drawStyleSwitcher->setPosition(Vec2(12.0f, _contentSize.height - 64.0f));
	}

	if (_contourSwitcher) {
		_contourSwitcher->setPosition(Vec2(_contentSize) - Vec2(12.0f, 12.0f));
	}
}

void TessLayout::handleContoursUpdated() {
	if (_contourSwitcher) {
		_contourSwitcher->setContours(_canvas->getContoursCount(), _canvas->getSelectedContour());
	}
}

}
