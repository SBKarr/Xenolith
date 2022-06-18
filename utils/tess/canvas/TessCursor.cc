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

#include "TessCursor.h"

namespace stappler::xenolith::tessapp {

bool TessCursor::init() {
	auto image = Rc<VectorImage>::create(Size2(64, 64));
	updateState(*image, _state);

	if (!VectorSprite::init(move(image))) {
		return false;
	}

	setAutofit(Sprite::Autofit::Contain);
	setAnchorPoint(Anchor::Middle);

	return true;
}

void TessCursor::setState(State state) {
	if (_state != state) {
		_state = state;
		updateState(*_image, _state);
	}
}

void TessCursor::updateState(VectorImage &image, State state) {
	switch (state) {
	case Point:
		image.clear();
		image.addPath()
			->setFillColor(Color::White)
			.addOval(Rect(16, 16, 32, 32))
			.setAntialiased(false);
		break;
	case Capture:
		image.clear();
		image.addPath()
			->setFillColor(Color::White)
			.moveTo(0, 24) .lineTo(4, 24) .lineTo(4, 4) .lineTo(24, 4) .lineTo(24, 0) .lineTo(0, 0)
			.moveTo(0, 40) .lineTo(0, 64) .lineTo(24, 64) .lineTo(24, 60) .lineTo(4, 60) .lineTo(4, 40)
			.moveTo(40, 64) .lineTo(64, 64) .lineTo(64, 40) .lineTo(60, 40) .lineTo(60, 60) .lineTo(40, 60)
			.moveTo(40, 0) .lineTo(64, 0) .lineTo(64, 24) .lineTo(60, 24) .lineTo(60, 4) .lineTo(40, 4)
			.setAntialiased(false);
		break;
	case Target:
		image.clear();
		image.addPath()
			->setFillColor(Color::White)
			.moveTo(0.0f, 30.0f)
			.lineTo(0.0f, 34.0f)
			.lineTo(30.0f, 34.0f)
			.lineTo(30.0f, 64.0f)
			.lineTo(34.0f, 64.0f)
			.lineTo(34.0f, 34.0f)
			.lineTo(64.0f, 34.0f)
			.lineTo(64.0f, 30.0f)
			.lineTo(34.0f, 30.0f)
			.lineTo(34.0f, 0.0f)
			.lineTo(30.0f, 0.0f)
			.lineTo(30.0f, 30.0f)
			.setAntialiased(false);
		break;
	}
}

}
