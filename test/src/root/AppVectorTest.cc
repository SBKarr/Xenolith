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

#include "AppVectorTest.h"
#include "XLVectorImage.h"

namespace stappler::xenolith::app {

bool VectorTest::init() {
	if (!Node::init()) {
		return false;
	}

	Mat4 imageScale;
	Mat4::createScale(2.0f, 2.0f, 1.0f, &imageScale);

	auto app = (AppDelegate *)Application::getInstance();
	auto fontController = app->getFontController();

	for (size_t i = 0; i < 5; ++ i) {
		auto image = Rc<VectorImage>::create(Size(200, 200));
		auto path = image->addPath();
		path->setFillColor(Color::Red_500);
		path->addOval(Rect(0, 0, 100, 100));
		path->setTransform(imageScale);
		path->setAntialiased(false);

		_sprite[i] = addChild(Rc<VectorSprite>::create(move(image)));
		_sprite[i]->setAnchorPoint(Anchor::Middle);
		_sprite[i]->setAutofit(Sprite::Autofit(i));
		_sprite[i]->setQuality(0.25f + 0.5f * i);

		_labels[i] = addChild(Rc<Label>::create(fontController), 3);
		_labels[i]->setFontFamily("Roboto");
		_labels[i]->setAnchorPoint(Anchor::MiddleBottom);
		_labels[i]->setColor(Color::Red_500, true);
		_labels[i]->setFontSize(24);
		_labels[i]->setOpacity(0.75);

		switch (Sprite::Autofit(i)) {
		case Sprite::Autofit::None: _labels[i]->setString(toString("Autofit::None; Q: ", 0.25f + 0.5f * i)); break;
		case Sprite::Autofit::Width: _labels[i]->setString(toString("Autofit::Width; Q: ", 0.25f + 0.5f * i)); break;
		case Sprite::Autofit::Height: _labels[i]->setString(toString("Autofit::Height; Q: ", 0.25f + 0.5f * i)); break;
		case Sprite::Autofit::Cover: _labels[i]->setString(toString("Autofit::Cover; Q: ", 0.25f + 0.5f * i)); break;
		case Sprite::Autofit::Contain: _labels[i]->setString(toString("Autofit::Contain; Q: ", 0.25f + 0.5f * i)); break;
		}
	}

	return true;
}

void VectorTest::onContentSizeDirty() {
	Node::onContentSizeDirty();

	Size size(_contentSize * 0.3f);

	Vec2 positions[5] = {
		Vec2(_contentSize.width * 0.2f, _contentSize.height * 0.2f),
		Vec2(_contentSize.width * 0.2f, _contentSize.height * 0.8f),
		Vec2(_contentSize.width * 0.5f, _contentSize.height * 0.5f),
		Vec2(_contentSize.width * 0.8f, _contentSize.height * 0.2f),
		Vec2(_contentSize.width * 0.8f, _contentSize.height * 0.8f),
	};

	for (size_t i = 0; i < 5; ++ i) {
		if (_sprite[i]) {
			_sprite[i]->setContentSize(size);
			_sprite[i]->setPosition(positions[i]);
		}

		if (_labels[i]) {
			_labels[i]->setPosition(positions[i] + Vec2(0.0f, _contentSize.height * 0.15f + 10));
		}
	}
}

}
