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

#include "AppLayoutMenu.h"
#include "AppLayoutMenuItem.h"
#include "AppScene.h"

namespace stappler::xenolith::app {

bool LayoutMenu::init(LayoutName layout, Vector<LayoutName> &&items) {
	if (!Node::init()) {
		return false;
	}

	_layout = layout;
	_layoutRoot = getRootLayoutForLayout(_layout);

	if (_layoutRoot != _layout) {
		_backButtonCallback = [this] {
			if (_scene) {
				((AppScene *)_scene)->runLayout(_layoutRoot, makeLayoutNode(_layoutRoot));
				return true;
			}
			return false;
		};
	}

	_scrollView = addChild(Rc<ScrollView>::create(ScrollView::Vertical));
	_scrollView->setAnchorPoint(Anchor::MiddleTop);
	_scrollView->setIndicatorColor(Color::Grey_500);
	auto controller = _scrollView->setController(Rc<ScrollController>::create());

	makeScrollList(controller, move(items));

	setName(getLayoutNameId(_layout));

	return true;
}

void LayoutMenu::onContentSizeDirty() {
	Node::onContentSizeDirty();

	_scrollView->setPosition(Vec2(_contentSize.width / 2.0f, _contentSize.height));
	_scrollView->setContentSize(Size2(std::min(512.0f, _contentSize.width), _contentSize.height));
}

void LayoutMenu::onEnter(Scene *scene) {
	Node::onEnter(scene);

	if (auto s = dynamic_cast<AppScene *>(scene)) {
		s->setActiveLayoutId(getName(), Value(getDataValue()));
	}
}

void LayoutMenu::makeScrollList(ScrollController *controller, Vector<LayoutName> &&items) {
	controller->addPlaceholder(24.0f);

	if (_layout != _layoutRoot) {
		controller->addItem([this] (const ScrollController::Item &item) {
			return Rc<LayoutMenuItem>::create("Move back", [this] {
				if (_backButtonCallback) {
					_backButtonCallback();
				}
			});
		}, 48.0f);
	}

	for (auto &it : items) {
		controller->addItem([this, data = it] (const ScrollController::Item &item) {
			return makeItem(item, data);
		}, 48.0f);
	}

	controller->addPlaceholder(24.0f);
}

Rc<Node> LayoutMenu::makeItem(const ScrollController::Item &item, LayoutName name) {
	return Rc<LayoutMenuItem>::create(getLayoutNameTitle(name), [this, name] {
		auto scene = dynamic_cast<AppScene *>(_scene);
		if (scene) {
			scene->runLayout(name, makeLayoutNode(name));
		}
	});
}

}
