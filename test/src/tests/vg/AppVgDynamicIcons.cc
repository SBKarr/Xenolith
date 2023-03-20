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

#include "AppVgDynamicIcons.h"
#include "XLIconNames.h"

namespace stappler::xenolith::app {

bool VgDynamicIcons::init() {
	if (!LayoutTest::init(LayoutName::VgDynamicIcons, "")) {
		return false;
	}

	_iconLoader = addChild(Rc<material::IconSprite>::create(IconName::Dynamic_Loader));
	_iconLoader->setAnchorPoint(Anchor::Middle);
	_iconLoader->setColor(Color::Black);
	_iconLoader->setContentSize(Size2(96.0f, 96.0f));

	_iconNav = addChild(Rc<material::IconSprite>::create(IconName::Dynamic_Nav));
	_iconNav->setAnchorPoint(Anchor::Middle);
	_iconNav->setColor(Color::Black);
	_iconNav->setContentSize(Size2(96.0f, 96.0f));

	_iconProgress = addChild(Rc<material::IconSprite>::create(IconName::Dynamic_DownloadProgress));
	_iconProgress->setAnchorPoint(Anchor::Middle);
	_iconProgress->setColor(Color::Black);
	_iconProgress->setContentSize(Size2(96.0f, 96.0f));

	_listener = addInputListener(Rc<InputListener>::create());
	_listener->addTapRecognizer([this] (const GestureTap &tap) {
		if (_iconLoader->isTouched(tap.input->currentLocation)) {
			_iconLoader->animate((_iconLoader->getProgress() > 0.0f) ? 0.0f : 1.0f, 2.0f);
		}
		if (_iconNav->isTouched(tap.input->currentLocation)) {
			_iconNav->animate((_iconNav->getProgress() > 0.0f) ? 0.0f : 1.0f, 2.0f);
		}
		if (_iconProgress->isTouched(tap.input->currentLocation)) {
			_iconProgress->animate((_iconProgress->getProgress() > 0.0f) ? 0.0f : 1.0f, 2.0f);
		}
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);

	return true;
}

void VgDynamicIcons::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_iconLoader->setPosition(_contentSize / 2.0f);
	_iconNav->setPosition(Vec2(_contentSize / 2.0f) + Vec2(-96.0f, 0.0f));
	_iconProgress->setPosition(Vec2(_contentSize / 2.0f) + Vec2(96.0f, 0.0f));
}

}
