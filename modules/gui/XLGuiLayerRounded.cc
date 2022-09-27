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

#include "XLGuiLayerRounded.h"

namespace stappler::xenolith {

bool LayerRounded::init(const Color4F &color, float borderRadius) {
	if (!VectorSprite::init(Size2(8.0f, 8.0f))) {
		return false;
	}

	setColor(color, true);
	_borderRadius = borderRadius;
	return true;
}

void LayerRounded::onContentSizeDirty() {
	auto radius = std::min(std::min(_contentSize.width / 2.0f, _contentSize.height / 2.0f), _borderRadius);

	if (radius != _realBorderRadius || _contentSize != _image->getImageSize()) {
		if (radius <= 0.0f) {
			if (_realBorderRadius != 0.0f) {
				setImage(Rc<VectorImage>::create(_contentSize));
				return;
			}

			_realBorderRadius = 0.0f;
		}

		auto img = Rc<VectorImage>::create(_contentSize);
		auto path = img->addPath();
		path->moveTo(0.0f, radius)
				.arcTo(radius, radius, 0.0f, false, true, radius, 0.0f)
				.lineTo(_contentSize.width - radius, 0.0f)
				.arcTo(radius, radius, 0.0f, false, true, _contentSize.width, radius)
				.lineTo(_contentSize.width, _contentSize.height - radius)
				.arcTo(radius, radius, 0.0f, false, true, _contentSize.width - radius, _contentSize.height)
				.lineTo(radius, _contentSize.height)
				.arcTo(radius, radius, 0.0f, false, true, 0.0f, _contentSize.height - radius)
				.closePath()
				.setAntialiased(false)
				.setFillColor(Color4B::WHITE)
				.setStyle(vg::DrawStyle::Fill);

		setImage(move(img));

		_realBorderRadius = radius;
	}

	VectorSprite::onContentSizeDirty();
}

void LayerRounded::setBorderRadius(float radius) {
	if (_borderRadius != radius) {
		_borderRadius = radius;
		_contentSizeDirty = true;
	}
}

}
