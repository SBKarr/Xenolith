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

#include "AppGeneralTemporaryResourceTest.h"
#include "XLDirector.h"
#include "XLResourceCache.h"
#include "XLEventListener.h"

namespace stappler::xenolith::app {

bool GeneralTemporaryResourceTest::init() {
	if (!LayoutTest::init(LayoutName::GeneralTemporaryResourceTest, "Temporary resource test")) {
		return false;
	}

	_label = addChild(Rc<Label>::create(), ZOrder(1));
	_label->setAnchorPoint(Anchor::Middle);
	_label->setFontSize(20);
	_label->setString("Not loaded");
	_label->setColor(Color::Grey_500);
	_label->setFontWeight(Label::FontWeight::Bold);

	_sprite = addChild(Rc<Sprite>::create(), ZOrder(1));
	_sprite->setAutofit(Sprite::Autofit::Contain);
	_sprite->setAnchorPoint(Anchor::Middle);

	_slider = addChild(Rc<AppSliderWithLabel>::create("0.0", 0.0f, [this] (float val) {
		setResourceTimeout(val);
	}), ZOrder(2));
	_slider->setPrefix("Timeout");
	_slider->setAnchorPoint(Anchor::Middle);

	_checkbox = addChild(Rc<AppCheckboxWithLabel>::create("Show/hide", true, [this] (bool val) {
		switchVisibility(val);
	}), ZOrder(2));
	_checkbox->setAnchorPoint(Anchor::Middle);

	auto l = addComponent(Rc<EventListener>::create());
	l->onEvent(TemporaryResource::onLoaded, [this] (const Event &ev) {
		if (ev.getObject() == _resource) {
			if (ev.getBoolValue()) {
				_label->setString("Loaded");
				_label->setColor(Color::Red_600);
			} else {
				_label->setString("Not loaded");
				_label->setColor(Color::Grey_500);
			}
		}
	});

	return true;
}

void GeneralTemporaryResourceTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_label->setPosition(Vec2(_contentSize.width / 2.0f, _contentSize.height - 64.0f));

	if (_sprite) {
		_sprite->setContentSize(_contentSize * 0.75f);
		_sprite->setPosition(_contentSize / 2.0f);
	}

	_slider->setContentSize(Size2(160.0f, 36.0f));
	_slider->setPosition(Vec2(_contentSize.width / 2.0f, 20.0f));

	_checkbox->setContentSize(Size2(36.0f, 36.0f));
	_checkbox->setPosition(Vec2(_contentSize.width / 2.0f - 80.0f, 64.0f));
}

void GeneralTemporaryResourceTest::onEnter(Scene *scene) {
	Node::onEnter(scene);

	auto cache = _director->getResourceCache();

	StringView name("external://resources/xenolith-2-480.png");

	if (auto res = cache->getTemporaryResource(name)) {
		_resource = res;
		_timeoutValue = res->getTimeout().toFloatSeconds();
		_slider->setValue(_timeoutValue / 10.0f);
		_slider->setString(toString(_timeoutValue));
		if (auto tex = res->acquireTexture(name)) {
			_sprite->setTexture(move(tex));
		}
	} else {
		auto tex = cache->addExternalImage(name,
				gl::ImageInfo(gl::ImageFormat::R8G8B8A8_UNORM, gl::ImageUsage::Sampled, gl::ImageHints::Opaque),
				FilePath("resources/xenolith-2-480.png"));
		if (tex) {
			_sprite->setTexture(move(tex));
		}

		if (auto res = cache->getTemporaryResource(name)) {
			_resource = res;
		}
	}
}

void GeneralTemporaryResourceTest::onExit() {
	Node::onExit();
}

void GeneralTemporaryResourceTest::setResourceTimeout(float val) {
	_timeoutValue = val * 10.0f;
	_slider->setString(toString(_timeoutValue));

	if (_resource) {
		_resource->setTimeout(TimeInterval::floatSeconds(_timeoutValue));
	}
}

void GeneralTemporaryResourceTest::switchVisibility(bool val) {
	if (!val) {
		if (_sprite) {
			_sprite->removeFromParent(true);
			_sprite = nullptr;
		}
	} else {
		if (!_sprite) {
			if (_resource) {
				StringView name("external://resources/xenolith-2-480.png");
				if (auto tex = _resource->acquireTexture(name)) {
					_sprite = addChild(Rc<Sprite>::create(move(tex)), ZOrder(1));
					_sprite->setAutofit(Sprite::Autofit::Contain);
					_sprite->setAnchorPoint(Anchor::Middle);
					_sprite->setContentSize(_contentSize * 0.75f);
					_sprite->setPosition(_contentSize / 2.0f);
				}
			}
		}
	}
}

}
