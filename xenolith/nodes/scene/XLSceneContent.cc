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

#include "XLSceneContent.h"
#include "XLSceneLayout.h"

namespace stappler::xenolith {

SceneContent::~SceneContent() { }

bool SceneContent::init() {
	if (!DynamicStateNode::init()) {
		return false;
	}

	_inputListener = addInputListener(Rc<InputListener>::create());
	_inputListener->setPriority(-1);
	_inputListener->addKeyRecognizer([this] (GestureData data) {
		if (data.event == GestureEvent::Ended) {
			return onBackButton();
		}
		return data.event == GestureEvent::Began;
	}, InputListener::makeKeyMask({InputKeyCode::ESCAPE}));

	return true;
}

void SceneContent::onEnter(Scene *scene) {
	DynamicStateNode::onEnter(scene);

	if (_retainBackButton && !_backButtonRetained) {
		_director->getView()->retainBackButton();
		_backButtonRetained = true;
	}
}

void SceneContent::onExit() {
	if (_retainBackButton && _backButtonRetained) {
		_director->getView()->releaseBackButton();
		_backButtonRetained = false;
	}

	DynamicStateNode::onExit();
}

void SceneContent::onContentSizeDirty() {
	Node::onContentSizeDirty();

	for (auto &node : _layouts) {
		updateLayoutNode(node);
	}

	for (auto &overlay : _overlays) {
		updateLayoutNode(overlay);
	}
}

void SceneContent::replaceLayout(SceneLayout *node) {
	if (!node || node->isRunning()) {
		return;
	}

	if (_layouts.empty()) {
		pushLayout(node);
		return;
	}

	updateLayoutNode(node);

	ZOrder zIndex = -(ZOrder)_layouts.size() - ZOrder(2);
	for (auto node : _layouts) {
		node->setLocalZOrder(zIndex);
		zIndex ++;
	}

	_layouts.push_back(node);
	addChild(node, zIndex);

	for (auto &it : _layouts) {
		if (it != node) {
			it->onPopTransitionBegan(this, true);
		} else {
			it->onPush(this, true);
		}
	}

	auto fn = [this, node] {
		for (auto &it : _layouts) {
			if (it != node) {
				it->onPop(this, true);
			} else {
				it->onPushTransitionEnded(this, true);
			}
		}
		replaceNodes();
		updateBackButtonStatus();
	};

	if (auto enter = node->makeEnterTransition(this)) {
		node->runAction(Rc<Sequence>::create(enter, fn));
	} else {
		fn();
	}
}

void SceneContent::pushLayout(SceneLayout *node) {
	if (!node || node->isRunning()) {
		return;
	}

	updateLayoutNode(node);
	pushNodeInternal(node, nullptr);
}

void SceneContent::replaceTopLayout(SceneLayout *node) {
	if (!node || node->isRunning()) {
		return;
	}

	if (!_layouts.empty()) {
		auto back = _layouts.back();
		_layouts.pop_back();
		back->onPopTransitionBegan(this, false);

		// just push node, then silently remove previous

		pushNodeInternal(node, [this, back] {
			eraseLayout(back);
			back->onPop(this, false);
		});
	}
}

void SceneContent::popLayout(SceneLayout *node) {
	auto it = std::find(_layouts.begin(), _layouts.end(), node);
	if (it == _layouts.end()) {
		return;
	}

	auto linkId = node->retain();
	_layouts.erase(it);

	node->onPopTransitionBegan(this, false);
	if (!_layouts.empty()) {
		_layouts.back()->onForegroundTransitionBegan(this, node);
	}

	auto fn = [this, node, linkId] {
		eraseLayout(node);
		node->onPop(this, false);
		if (!_layouts.empty()) {
			_layouts.back()->onForeground(this, node);
		}
		node->release(linkId);
	};

	if (auto exit = node->makeExitTransition(this)) {
		node->runAction(Rc<Sequence>::create(move(exit), fn));
	} else {
		fn();
	}
}

void SceneContent::pushNodeInternal(SceneLayout *node, Function<void()> &&cb) {
	if (!_layouts.empty()) {
		ZOrder zIndex = -(ZOrder)_layouts.size() - ZOrder(2);
		for (auto &node : _layouts) {
			node->setLocalZOrder(zIndex);
			zIndex ++;
		}
	}

	_layouts.push_back(node);

	updateLayoutNode(node);
	addChild(node, ZOrder(-1));

	if (_layouts.size() > 1) {
		_layouts.at(_layouts.size() - 2)->onBackground(this, node);
	}
	node->onPush(this, false);

	auto fn = [this, node, cb = move(cb)] {
		updateNodesVisibility();
		updateBackButtonStatus();
		if (_layouts.size() > 1) {
			_layouts.at(_layouts.size() - 2)->onBackgroundTransitionEnded(this, node);
		}
		node->onPushTransitionEnded(this, false);
		if (cb) {
			cb();
		}
	};

	if (auto enter = node->makeEnterTransition(this)) {
		node->runAction(Rc<Sequence>::create(move(enter), move(fn)));
	} else {
		fn();
	}
}

void SceneContent::eraseLayout(SceneLayout *node) {
	node->removeFromParent();
	if (!_layouts.empty()) {
		ZOrder zIndex = -(ZOrder)_layouts.size() - ZOrder(2);
		for (auto n : _layouts) {
			n->setLocalZOrder(zIndex);
			n->setVisible(false);
			zIndex ++;
		}
		updateNodesVisibility();
	}
	updateBackButtonStatus();
}

void SceneContent::eraseOverlay(SceneLayout *l) {
	l->removeFromParent();
	if (!_overlays.empty()) {
		ZOrder zIndex = ZOrder(1);
		for (auto n : _overlays) {
			n->setLocalZOrder(zIndex);
			zIndex ++;
		}
		updateNodesVisibility();
	}
	updateBackButtonStatus();
}

void SceneContent::replaceNodes() {
	if (!_layouts.empty()) {
		auto vec = _layouts;
		for (auto &node : vec) {
			if (node != _layouts.back()) {
				node->removeFromParent();
			}
		}

		_layouts.erase(_layouts.begin(), _layouts.begin() + _layouts.size() - 1);
	}
}

void SceneContent::updateNodesVisibility() {
	for (size_t i = 0; i < _layouts.size(); i++) {
		if (i == _layouts.size() - 1) {
			_layouts.at(i)->setVisible(true);
		} else {
			_layouts.at(i)->setVisible(false);
		}
	}
}

void SceneContent::updateBackButtonStatus() {
	if (!_overlays.empty() || _layouts.size() > 1 || _layouts.back()->hasBackButtonAction()) {
		if (!_retainBackButton) {
			_retainBackButton = true;
			if (_director && !_backButtonRetained) {
				_director->getView()->retainBackButton();
				_backButtonRetained = true;
			}
		}
	} else {
		if (_retainBackButton) {
			if (_director && _backButtonRetained) {
				_director->getView()->releaseBackButton();
				_backButtonRetained = false;
			}
			_retainBackButton = false;
		}
	}
}

SceneLayout *SceneContent::getTopLayout() {
	if (!_layouts.empty()) {
		return _layouts.back();
	} else {
		return nullptr;
	}
}

SceneLayout *SceneContent::getPrevLayout() {
	if (_layouts.size() > 1) {
		return *(_layouts.end() - 2);
	}
	return nullptr;
}

bool SceneContent::popTopLayout() {
	if (_layouts.size() > 1) {
		popLayout(_layouts.back());
		return true;
	}
	return false;
}

bool SceneContent::isActive() const {
	return !_layouts.empty();
}

bool SceneContent::onBackButton() {
	if (_layouts.empty()) {
		return false;
	} else {
		if (!_layouts.back()->onBackButton()) {
			if (!popTopLayout()) {
				return false;
			}
		}
		return true;
	}
}

size_t SceneContent::getLayoutsCount() const {
	return _layouts.size();
}

const Vector<Rc<SceneLayout>> &SceneContent::getLayouts() const {
	return _layouts;
}

const Vector<Rc<SceneLayout>> &SceneContent::getOverlays() const {
	return _overlays;
}

bool SceneContent::pushOverlay(SceneLayout *l) {
	if (!l || l->isRunning()) {
		return false;
	}

	updateLayoutNode(l);

	ZOrder zIndex = ZOrder(_overlays.size() + 1);

	_overlays.push_back(l);

	addChild(l, zIndex);

	l->onPush(this, false);

	auto fn = [this, l] {
		l->onPushTransitionEnded(this, false);
		updateBackButtonStatus();
	};

	if (auto enter = l->makeEnterTransition(this)) {
		l->runAction(Rc<Sequence>::create(move(enter), move(fn)), "ContentLayer.Transition"_tag);
	} else {
		fn();
	}

	return true;
}

bool SceneContent::popOverlay(SceneLayout *l) {
	auto it = std::find(_overlays.begin(), _overlays.end(), l);
	if (it == _overlays.end()) {
		return false;
	}

	auto linkId = l->retain();
	_overlays.erase(it);
	l->onPopTransitionBegan(this, false);

	auto fn = [this, l, linkId] {
		eraseOverlay(l);
		l->onPop(this, false);
		l->release(linkId);
		updateBackButtonStatus();
	};

	if (auto exit = l->makeExitTransition(this)) {
		l->runAction(Rc<Sequence>::create(move(exit), move(fn)));
	} else {
		fn();
	}

	return true;
}

void SceneContent::setDecorationPadding(Padding padding) {
	if (padding != _decorationPadding) {
		_decorationPadding = padding;
		_contentSizeDirty = true;
	}
}

void SceneContent::updateLayoutNode(SceneLayout *node) {
	auto mask = node->getDecodationMask();

	Vec2 pos = Vec2::ZERO;
	Size2 size = _contentSize;
	Padding effectiveDecorations;

	if ((mask & DecorationMask::Top) != DecorationMask::None) {
		size.height += _decorationPadding.top;
		effectiveDecorations.top = _decorationPadding.top;
	}
	if ((mask & DecorationMask::Right) != DecorationMask::None) {
		size.width += _decorationPadding.right;
		effectiveDecorations.right = _decorationPadding.right;
	}
	if ((mask & DecorationMask::Left) != DecorationMask::None) {
		size.width += _decorationPadding.left;
		pos.x -= _decorationPadding.left;
		effectiveDecorations.left = _decorationPadding.left;
	}
	if ((mask & DecorationMask::Bottom) != DecorationMask::None) {
		size.height += _decorationPadding.bottom;
		pos.y -= _decorationPadding.bottom;
		effectiveDecorations.bottom = _decorationPadding.bottom;
	}

	node->setAnchorPoint(Anchor::BottomLeft);
	node->setDecorationPadding(effectiveDecorations);
	node->setPosition(pos);
	node->setContentSize(size);
}

}
