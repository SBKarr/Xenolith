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

#include "MaterialIconSprite.h"
#include "XLRenderFrameInfo.h"

namespace stappler::xenolith::material {

bool IconSprite::init(IconName icon) {
	if (!VectorSprite::init(Size2(24.0f, 24.0f))) {
		return false;
	}

	setContentSize(Size2(24.0f, 24.0f));

	_iconName = icon;

	return true;
}

void IconSprite::setIconName(IconName name) {
	if (_iconName != name) {
		_iconName = name;
		updateIcon();
	}
}

bool IconSprite::visitDraw(RenderFrameInfo &frame, NodeFlags parentFlags) {
	if (!_visible) {
		return false;
	}

	auto style = frame.getComponent<SurfaceInterior>(SurfaceInterior::ComponentFrameTag);
	if (style) {
		auto &s = style->getStyle();
		auto color = s.colorOn.asColor4F();
		if (color != getColor()) {
			setColor(color, true);
		}
	}

	return VectorSprite::visitDraw(frame, parentFlags);
}

void IconSprite::animate() {
	// TODO
}

void IconSprite::animate(float targetProgress, float duration) {
	// TODO
}

void IconSprite::updateIcon() {
	_image->clear();
	switch (_iconName) {
	case IconName::None:
	case IconName::Empty:
		break;
	default:
		getIconData(_iconName, [&] (BytesView bytes) {
			auto t = Mat4::IDENTITY;
			t.scale(1, -1, 1);
			t.translate(0, -24, 0);
			// adding icon to tesselator cache with name org.stappler.xenolith.material.icon.*
			auto path = _image->addPath("", toString("org.stappler.xenolith.material.icon.", xenolith::getIconName(_iconName)))
					->getPath();
			path->init(bytes);
			path->setTransform(t);
		});
	}
}

}
