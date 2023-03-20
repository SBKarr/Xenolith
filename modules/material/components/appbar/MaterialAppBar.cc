/**
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

#include "../appbar/MaterialAppBar.h"

#include "MaterialButton.h"
#include "XLInputListener.h"

namespace stappler::xenolith::material {

bool AppBar::init(AppBarLayout layout, const SurfaceStyle & style) {
	if (!Surface::init(style)) {
		return false;
	}

	_layout = layout;

	_inputListener = addInputListener(Rc<InputListener>::create());
	_inputListener->addTouchRecognizer([this] (const GestureData &) -> bool {
		return isSwallowTouches();
	});
	_inputListener->addPressRecognizer([this] (const GesturePress &press) -> bool {
		if (_barCallback) {
			if (press.event  == GestureEvent::Ended) {
				_barCallback();
			}
			return true;
		}
		return false;
	}, TimeInterval::milliseconds(425), true);
	_inputListener->setSwallowEvents(InputListener::EventMaskTouch);

	_actionMenuSourceListener = addComponent(Rc<DataListener<MenuSource>>::create(std::bind(&AppBar::layoutSubviews, this)));

	_navButton = addChild(Rc<Button>::create(NodeStyle::Text), ZOrder(1));
	_navButton->setTapCallback(std::bind(&AppBar::handleNavTapped, this));
	_navButton->setLeadingIconName(IconName::Navigation_menu_solid);
	_navButton->setIconSize(24.0f);
	_navButton->setSwallowEvents(true);

	_label = addChild(Rc<TypescaleLabel>::create(TypescaleRole::TitleLarge));
	_label->setAnchorPoint(Anchor::MiddleLeft);

	_scissorNode = addChild(Rc<DynamicStateNode>::create());
	_scissorNode->setPosition(Vec2(0, 0));
	_scissorNode->setAnchorPoint(Anchor::BottomLeft);

	_iconsComposer = addChild(Rc<Node>::create(), ZOrder(1));
	_iconsComposer->setPosition(Vec2(0, 0));
	_iconsComposer->setAnchorPoint(Anchor::BottomLeft);
	_iconsComposer->setCascadeOpacityEnabled(true);

	return true;
}

void AppBar::onContentSizeDirty() {
	Surface::onContentSizeDirty();
	layoutSubviews();
}

void AppBar::setLayout(AppBarLayout layout) {
	if (_layout != layout) {
		_layout = layout;
		_contentSizeDirty = true;
	}
}

void AppBar::setTitle(StringView str) {
	_label->setString(str);
}

StringView AppBar::getTitle() const {
	return _label->getString8();
}

void AppBar::setNavButtonIcon(IconName name) {
	_navButton->setLeadingIconName(name);
	_contentSizeDirty = true;
}

IconName AppBar::getNavButtonIcon() const {
	return _navButton->getLeadingIconName();
}

void AppBar::setMaxActionIcons(size_t value) {
	_maxActionIcons = value;
	_contentSizeDirty = true;
}
size_t AppBar::getMaxActionIcons() const {
	return _maxActionIcons;
}

void AppBar::setActionMenuSource(MenuSource *source) {
	if (_actionMenuSourceListener->getSubscription() != source) {
		_actionMenuSourceListener->setSubscription(source);
	}
}

void AppBar::replaceActionMenuSource(MenuSource *source, size_t maxIcons) {
	if (source == _actionMenuSourceListener->getSubscription()) {
		return;
	}

	if (maxIcons == maxOf<size_t>()) {
		maxIcons = source->getHintCount();
	}

	stopAllActionsByTag("replaceActionMenuSource"_tag);
	if (_prevComposer) {
		_prevComposer->removeFromParent();
		_prevComposer = nullptr;
	}

	_actionMenuSourceListener->setSubscription(source);
	_maxActionIcons = maxIcons;

	_prevComposer = _iconsComposer;
	float pos = -_prevComposer->getContentSize().height;
	_iconsComposer = _scissorNode->addChild(Rc<Node>::create(), ZOrder(1));
	_iconsComposer->setPosition(Vec2(0, pos));
	_iconsComposer->setAnchorPoint(Anchor::BottomLeft);
	_iconsComposer->setCascadeOpacityEnabled(true);

	float iconWidth = updateMenu(_iconsComposer, source, _maxActionIcons);
	if (iconWidth > _iconWidth) {
		_iconWidth = iconWidth;
		_contentSizeDirty = true;
	}

	_replaceProgress = 0.0f;
	updateProgress();

	runAction(Rc<ActionProgress>::create(0.15f, [this] (float p) {
		_replaceProgress = p;
		updateProgress();
	}, nullptr, [this] () {
		_replaceProgress = 1.0f;
		updateProgress();
		_contentSizeDirty = true;
	}), "replaceActionMenuSource"_tag);
}

MenuSource * AppBar::getActionMenuSource() const {
	return _actionMenuSourceListener->getSubscription();
}

void AppBar::setBasicHeight(float value) {
	if (_basicHeight != value) {
		_basicHeight = value;
		_contentSizeDirty = true;
	}
}

float AppBar::getBasicHeight() const {
	return _basicHeight;
}

void AppBar::setNavCallback(Function<void()> &&cb) {
	_navCallback = move(cb);
}
const Function<void()> & AppBar::getNavCallback() const {
	return _navCallback;
}

void AppBar::setSwallowTouches(bool value) {
	if (value) {
		_inputListener->setSwallowEvents(InputListener::EventMaskTouch);
	} else {
		_inputListener->clearSwallowEvents(InputListener::EventMaskTouch);
	}
}

bool AppBar::isSwallowTouches() const {
	return _inputListener->isSwallowAllEvents(InputListener::EventMaskTouch);
}

Button *AppBar::getNavNode() const {
	return _navButton;
}

void AppBar::setBarCallback(Function<void()> &&cb) {
	_barCallback = move(cb);
}

const Function<void()> & AppBar::getBarCallback() const {
	return _barCallback;
}

void AppBar::handleNavTapped() {
	if (_navCallback) {
		_navCallback();
	}
}

void AppBar::updateProgress() {
	if (_replaceProgress == 1.0f) {
		if (_prevComposer) {
			_prevComposer->removeFromParent();
			_prevComposer = nullptr;
		}
	}

	if (_iconsComposer) {
		_iconsComposer->setPositionY(progress(_iconsComposer->getContentSize().height, 0.0f, _replaceProgress));
	}
	if (_prevComposer) {
		_prevComposer->setPositionY(progress(0.0f, -_prevComposer->getContentSize().height, _replaceProgress));
	}
}

float AppBar::updateMenu(Node *composer, MenuSource *source, size_t maxIcons) {
	composer->removeAllChildren();
	composer->setContentSize(_contentSize);

	float baseline = getBaseLine();
	size_t iconsCount = 0;
	auto extMenuSource = Rc<MenuSource>::create();
	Vector<Button *> icons;
	bool hasExtMenu = false;

	if (source) {
		auto &menuItems = source->getItems();
		for (auto &item : menuItems) {
			if (item->getType() == MenuSourceItem::Type::Button) {
				auto btnSrc = dynamic_cast<MenuSourceButton *>(item.get());
				if (btnSrc->getNameIcon() != IconName::None) {
					if (iconsCount < maxIcons) {
						auto btn = composer->addChild(Rc<Button>::create(NodeStyle::Text), ZOrder(0), iconsCount);
						btn->setMenuSourceButton(btnSrc);
						btn->setIconSize(24.0f);
						btn->setSwallowEvents(true);
						icons.push_back(btn);
						iconsCount ++;
					} else {
						extMenuSource->addItem(item);
					}
				}
			}
		}
	}

	if (extMenuSource->count() > 0) {
		auto btn = Rc<Button>::create();
		auto source = Rc<MenuSourceButton>::create("more", IconName::Navigation_more_vert_solid, move(extMenuSource));
		btn->setMenuSourceButton(move(source));
		icons.push_back(btn);
		composer->addChild(move(btn), ZOrder(0), iconsCount);
		hasExtMenu = true;
	} else {
		hasExtMenu = false;
	}

	if (icons.size() > 0) {
		if (icons.back()->getLeadingIconName() == IconName::Navigation_more_vert_solid) {
			hasExtMenu = true;
		}
		auto pos = composer->getContentSize().width - 56 * (icons.size() - 1) - (hasExtMenu?8:36);
		for (auto &it : icons) {
			it->setContentSize(Size2(48, std::min(48.0f, _basicHeight)));
			it->setAnchorPoint(Vec2(0.5, 0.5));
			it->setPosition(Vec2(pos, baseline));
			pos += 56;
		}
		if (hasExtMenu) {
			icons.back()->setContentSize(Size2(24, std::min(48.0f, _basicHeight)));
			icons.back()->setPosition(Vec2(composer->getContentSize().width - 24, baseline));
		}
	}

	if (icons.size() > 0) {
		return (56 * (icons.size()) - (hasExtMenu?24:0));
	}
	return 0;
}

void AppBar::layoutSubviews() {
	_scissorNode->setContentSize(_contentSize);

	updateProgress();

	_iconsComposer->setContentSize(_scissorNode->getContentSize());
	auto iconWidth = updateMenu(_iconsComposer, _actionMenuSourceListener->getSubscription(), _maxActionIcons);
	if (_replaceProgress != 1.0f && _iconWidth != 0.0f) {
		_iconWidth = std::max(iconWidth, _iconWidth);
	} else {
		_iconWidth = iconWidth;
	}

	float baseline = getBaseLine();
	if (_navButton->getLeadingIconName() != IconName::Empty && _navButton->getLeadingIconName() != IconName::None) {
		_navButton->setContentSize(Size2(48.0f, 48.0f));
		_navButton->setAnchorPoint(Anchor::Middle);
		_navButton->setPosition(Vec2(32.0f, baseline));
		_navButton->setVisible(true);
	} else {
		_navButton->setVisible(false);
	}

	float labelStart = (getNavButtonIcon() == IconName::None) ? 16.0f : 64.0f;
	float labelEnd = _contentSize.width - _iconWidth - 16.0f;

	_label->setMaxWidth(labelEnd - labelStart);

	switch (_layout) {
	case AppBarLayout::CenterAligned:
		_label->setAnchorPoint(Anchor::Middle);
		_label->setAlignment(Label::Alignment::Center);
		_label->setPosition(Vec2((labelStart + labelEnd) / 2.0f, baseline));
		break;
	case AppBarLayout::Small:
		_label->setAnchorPoint(Anchor::MiddleLeft);
		_label->setAlignment(Label::Alignment::Left);
		_label->setPosition((getNavButtonIcon() == IconName::None) ? Vec2(16.0f, baseline) : Vec2(64.0f, baseline));
		break;
	default:
		break;
	}
}

float AppBar::getBaseLine() const {
	if (_contentSize.height > _basicHeight) {
		return _contentSize.height - _basicHeight / 2;
	} else {
		return _basicHeight / 2;
	}
}

}
