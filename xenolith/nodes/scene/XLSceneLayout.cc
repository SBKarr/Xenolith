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

#include "XLSceneLayout.h"
#include "XLSceneContent.h"

namespace stappler::xenolith {

SceneLayout::~SceneLayout() { }

void SceneLayout::setDecorationMask(DecorationMask mask) {
	if (_decorationMask != mask) {
		_decorationMask = mask;
		if (_sceneContent) {
			_sceneContent->updateLayoutNode(this);
		}
	}
}

void SceneLayout::setDecorationPadding(Padding padding) {
	if (_decorationPadding != padding) {
		_decorationPadding = padding;
		_contentSizeDirty = true;
	}
}

bool SceneLayout::onBackButton() {
	if (_backButtonCallback) {
		return _backButtonCallback();
	} else if (_sceneContent) {
		if (_sceneContent->getLayoutsCount() >= 2 && _sceneContent->getTopLayout() == this) {
			_sceneContent->popLayout(this);
			return true;
		}
	}
	return false;
}

void SceneLayout::setBackButtonCallback(const BackButtonCallback &cb) {
	_backButtonCallback = cb;
}
const SceneLayout::BackButtonCallback &SceneLayout::getBackButtonCallback() const {
	return _backButtonCallback;
}

void SceneLayout::onPush(SceneContent *l, bool replace) {
	_sceneContent = l;
	_inTransition = true;
}
void SceneLayout::onPushTransitionEnded(SceneContent *l, bool replace) {
	_sceneContent = l;
	_inTransition = false;
	_contentSizeDirty = true;
}

void SceneLayout::onPopTransitionBegan(SceneContent *l, bool replace) {
	_inTransition = true;
}
void SceneLayout::onPop(SceneContent *l, bool replace) {
	_inTransition = false;
	_contentSizeDirty = true;
	_sceneContent = nullptr;
}

void SceneLayout::onBackground(SceneContent *l, SceneLayout *overlay) {
	_inTransition = true;
}
void SceneLayout::onBackgroundTransitionEnded(SceneContent *l, SceneLayout *overlay) {
	_inTransition = false;
	_contentSizeDirty = true;
}

void SceneLayout::onForegroundTransitionBegan(SceneContent *l, SceneLayout *overlay) {
	_inTransition = true;
}
void SceneLayout::onForeground(SceneContent *l, SceneLayout *overlay) {
	_inTransition = false;
	_contentSizeDirty = true;
}

Rc<SceneLayout::Transition> SceneLayout::makeEnterTransition(SceneContent *) const {
	return nullptr;
}
Rc<SceneLayout::Transition> SceneLayout::makeExitTransition(SceneContent *) const {
	return nullptr;
}

bool SceneLayout::hasBackButtonAction() const {
	return _backButtonCallback != nullptr;
}

}
