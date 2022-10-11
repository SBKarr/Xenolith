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

#include "AppVgIconList.h"
#include "AppScene.h"
#include "XLIconNames.h"

namespace stappler::xenolith::app {

class VgIconListNode : public Node {
public:
	virtual ~VgIconListNode() { }

	virtual bool init(IconName, Function<void(IconName)> &&);

	virtual void onContentSizeDirty() override;

protected:
	VectorSprite *_image = nullptr;
	IconName _iconName = IconName::Empty;
	Function<void(IconName)> _callback;
};

bool VgIconListNode::init(IconName iconName, Function<void(IconName)> &&cb) {
	if (!Node::init()) {
		return false;
	}

	auto image = Rc<VectorImage>::create(Size2(24, 24));

	auto path = image->addPath();
	getIconData(iconName, [&] (BytesView bytes) {
		path->getPath()->init(bytes);
	});
	path->setWindingRule(vg::Winding::EvenOdd);
	path->setAntialiased(false);
	auto t = Mat4::IDENTITY;
	t.scale(1, -1, 1);
	t.translate(0, -24, 0);
	path->setTransform(t);

	_image = addChild(Rc<VectorSprite>::create(move(image)));
	_image->setColor(Color::Black);
	_image->setAutofit(Sprite::Autofit::Contain);
	_image->setContentSize(Size2(64.0f, 64.0f));
	_image->setAnchorPoint(Anchor::Middle);
	_iconName = iconName;

	auto l = addInputListener(Rc<InputListener>::create());
	l->addTapRecognizer([this] (GestureEvent ev, const GestureTap &tap) {
		if (ev == GestureEvent::Activated && tap.count == 2 && _callback) {
			_callback(_iconName);
		}
		return true;
	});

	_callback = move(cb);

	setTag(toInt(iconName));

	return true;
}

void VgIconListNode::onContentSizeDirty() {
	Node::onContentSizeDirty();

	_image->setPosition(_contentSize / 2.0f);
}

bool VgIconListPopup::init() {
	if (!Layer::init(Color::Grey_200)) {
		return false;
	}

	_label = addChild(Rc<Label>::create(), 1);
	_label->setAnchorPoint(Anchor::Middle);
	_label->setFontSize(20);
	_label->setOnContentSizeDirtyCallback([this] {
		setContentSize(_label->getContentSize() + Size2(16.0f, 16.0f));
	});

	setVisible(false);

	return true;
}

void VgIconListPopup::onContentSizeDirty() {
	Layer::onContentSizeDirty();

	_label->setPosition(_contentSize / 2.0f);
}

void VgIconListPopup::setString(StringView str) {
	if (!str.empty()) {
		_label->setString(str);
		setVisible(true);
	} else {
		setVisible(false);
	}
}

bool VgIconList::init() {
	if (!LayoutTest::init(LayoutName::VgIconList, "")) {
		return false;
	}

	_scrollView = addChild(Rc<ScrollView>::create(ScrollView::Vertical));
	_scrollView->setAnchorPoint(Anchor::MiddleTop);
	_scrollView->setIndicatorColor(Color::Grey_500);
	auto controller = _scrollView->setController(Rc<ScrollController>::create());

	for (uint32_t i = toInt(IconName::Action_3d_rotation_outline); i < toInt(IconName::Max); ++ i) {
		controller->addItem([this, i] (const ScrollController::Item &) -> Rc<Node> {
			return Rc<VgIconListNode>::create(IconName(i), [this] (IconName name) {
				if (_running) {
					if (auto scene = dynamic_cast<AppScene *>(_scene)) {
						auto l = Rc<VgIconTest>::create();
						l->setDataValue(Value({
							pair("icon", Value(toInt(name)))
						}));
						scene->runLayout(LayoutName::VgIconTest, move(l));
					}
				}
			});
		}, 72.0f, 0, toString("Icon: ", i));
	}

	controller->setRebuildCallback([this] (ScrollController *c) {
		return rebuildScroll(c);
	});

	_popup = addChild(Rc<VgIconListPopup>::create(), 1);
	_popup->setContentSize(Size2(192.0f, 32.0f));

	auto l = addInputListener(Rc<InputListener>::create());
	l->addMoveRecognizer([this] (GestureEvent ev, const InputEvent &input) {
		updatePopupLocation(input.currentLocation);
		return true;
	});

	return true;
}

void VgIconList::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_scrollView->setPosition(Vec2(_contentSize.width / 2.0f, _contentSize.height));
	_scrollView->setContentSize(_contentSize);
	_scrollView->disableScissor();
}

bool VgIconList::rebuildScroll(ScrollController *c) {
	uint32_t nCols = uint32_t(floorf(_contentSize.width / 72.0f));

	uint32_t idx = 0;

	float xOffset = (_contentSize.width - nCols * 72.0f) / 2.0f;
	float xPos = xOffset;
	float yPos = 0.0f;
	for (auto &it : c->getItems()) {
		if (idx >= nCols) {
			idx = 0;
			xPos = xOffset;
			yPos += 72.0f;
		}

		it.size = Size2(72.0f, 72.0f);
		it.pos = Vec2(xPos, yPos);
		xPos += 72.0f;
		++ idx;
	}

	return false;
}

void VgIconList::updatePopupLocation(const Vec2 &pos) {
	uint64_t targetTag = maxOf<uint64_t>();
	for (auto &it : _scrollView->getRoot()->getChildren()) {
		if (it->isTouched(pos)) {
			targetTag = it->getTag();
		}
	}

	auto target = convertToNodeSpace(pos);

	float minX = 8.0f;
	float maxX = _contentSize.width - _popup->getContentSize().width - 8.0f;
	float minY = 8.0f;
	float maxY = _contentSize.height - _popup->getContentSize().height - 8.0f;

	target = Vec2(std::max(target.x, minX), std::max(target.y, minY));
	target = Vec2(std::min(target.x, maxX), std::min(target.y, maxY));

	_popup->setPosition(target);
	_popup->setString(getIconName(IconName(targetTag)));
}

}
