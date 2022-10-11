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

#include "AppGeneralAutofitTest.h"
#include "XLSprite.h"
#include "XLLabel.h"

namespace stappler::xenolith::app {

class GeneralAutofitTestNode : public Node {
public:
	virtual ~GeneralAutofitTestNode() { }

	virtual bool init() override;

	virtual void onContentSizeDirty() override;

protected:
	Size2 _targetSize;
	Layer *_background = nullptr;
	Layer *_layers[5] = { nullptr };
	Sprite *_sprites[5] = { nullptr };
	Label *_labels[5] = { nullptr };
};

class GeneralAutofitTestResize : public VectorSprite {
public:
	virtual ~GeneralAutofitTestResize() { }

	virtual bool init() override;
};

bool GeneralAutofitTestNode::init() {
	if (!Node::init()) {
		return false;
	}

	_background = addChild(Rc<Layer>::create(Color::Red_50));
	_background->setAnchorPoint(Anchor::Middle);

	for (size_t i = 0; i < 5; ++ i) {
		_layers[i] = addChild(Rc<Layer>::create(Color::Teal_500), 1);
		_layers[i]->setAnchorPoint(Anchor::Middle);

		_sprites[i] = addChild(Rc<Sprite>::create("xenolith-2-480.png"), 2);
		_sprites[i]->setAnchorPoint(Anchor::Middle);
		_sprites[i]->setAutofit(Sprite::Autofit(i));

		_labels[i] = addChild(Rc<Label>::create(), 3);
		_labels[i]->setAnchorPoint(Anchor::MiddleBottom);
		_labels[i]->setColor(Color::Red_500, true);
		_labels[i]->setFontSize(20);
		_labels[i]->setOpacity(0.75);

		switch (Sprite::Autofit(i)) {
		case Sprite::Autofit::None: _labels[i]->setString("Autofit::None"); break;
		case Sprite::Autofit::Width: _labels[i]->setString("Autofit::Width"); break;
		case Sprite::Autofit::Height: _labels[i]->setString("Autofit::Height"); break;
		case Sprite::Autofit::Cover: _labels[i]->setString("Autofit::Cover"); break;
		case Sprite::Autofit::Contain: _labels[i]->setString("Autofit::Contain"); break;
		}
	}

	return true;
}

void GeneralAutofitTestNode::onContentSizeDirty() {
	Node::onContentSizeDirty();

	if (_background) {
		_background->setContentSize(_contentSize);
		_background->setPosition(_contentSize / 2.0f);
	}

	Size2 size(_contentSize * 0.3f);

	Vec2 positions[5] = {
		Vec2(_contentSize.width * 0.2f, _contentSize.height * 0.2f),
		Vec2(_contentSize.width * 0.2f, _contentSize.height * 0.8f),
		Vec2(_contentSize.width * 0.5f, _contentSize.height * 0.5f),
		Vec2(_contentSize.width * 0.8f, _contentSize.height * 0.2f),
		Vec2(_contentSize.width * 0.8f, _contentSize.height * 0.8f),
	};

	for (size_t i = 0; i < 5; ++ i) {
		if (_sprites[i]) {
			_sprites[i]->setContentSize(size);
			_sprites[i]->setPosition(positions[i]);
		}

		if (_layers[i]) {
			_layers[i]->setContentSize(size);
			_layers[i]->setPosition(positions[i]);
		}

		if (_labels[i]) {
			_labels[i]->setPosition(positions[i] + Vec2(0.0f, _contentSize.height * 0.15f + 10));
		}
	}
}


bool GeneralAutofitTestResize::init() {
	auto image = Rc<VectorImage>::create(Size2(24, 24));
	return true;
}

bool GeneralAutofitTest::init() {
	if (!LayoutTest::init(LayoutName::GeneralAutofitTest, "Drag resize node for effect")) {
		return false;
	}

	_nodeAutofit = addChild(Rc<GeneralAutofitTestNode>::create());
	_nodeAutofit->setAnchorPoint(Anchor::Middle);

	return true;
}

void GeneralAutofitTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_nodeAutofit->setPosition(_contentSize / 2.0f);
	_nodeAutofit->setContentSize(_contentSize * 0.85f);
}

}
