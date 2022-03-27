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

#include "XLVectorSprite.h"
#include "XLVectorCanvas.h"

namespace stappler::xenolith {

VectorSprite::VectorSprite() {
	_style.backgroundSizeHeight.metric = layout::style::Metric::Contain;
	_style.backgroundSizeWidth.metric = layout::style::Metric::Contain;
}

bool VectorSprite::init(Rc<VectorImage> &&img) {
	XL_ASSERT(img, "Image should not be nullptr");

	if (!Sprite::init() || !_image) {
		return false;
	}

	_image = img;
	return true;
}

bool VectorSprite::init(Size size, StringView data) {
	if (!Sprite::init()) {
		return false;
	}

	_image = Rc<VectorImage>::create(size, data);
	return _image != nullptr;
}

bool VectorSprite::init(Size size, layout::Path &&path) {
	if (!Sprite::init()) {
		return false;
	}

	_image = Rc<VectorImage>::create(size, move(path));
	return _image != nullptr;
}

bool VectorSprite::init(Size size) {
	if (!Sprite::init()) {
		return false;
	}

	_image = Rc<VectorImage>::create(size);
	return _image != nullptr;
}

bool VectorSprite::init(StringView data) {
	if (!Sprite::init()) {
		return false;
	}

	_image = Rc<VectorImage>::create(data);
	return _image != nullptr;
}

bool VectorSprite::init(BytesView data) {
	if (!Sprite::init()) {
		return false;
	}

	_image = Rc<VectorImage>::create(data);
	return _image != nullptr;
}

bool VectorSprite::init(FilePath path) {
	if (!Sprite::init()) {
		return false;
	}

	_image = Rc<VectorImage>::create(path);
	return _image != nullptr;
}

Rc<VectorPath> VectorSprite::addPath(StringView id, Mat4 pos) {
	return _image->addPath(id, pos);
}

Rc<VectorPath> VectorSprite::addPath(const layout::Path & path, StringView id, Mat4 pos) {
	return _image->addPath(path, id, pos);
}

Rc<VectorPath> VectorSprite::addPath(layout::Path && path, StringView id, Mat4 pos) {
	return _image->addPath(move(path), id, pos);
}

Rc<VectorPath> VectorSprite::getPath(StringView id) {
	return _image->getPath(id);
}

void VectorSprite::removePath(const Rc<VectorPath> &path) {
	_image->removePath(path);
}

void VectorSprite::removePath(StringView id) {
	_image->removePath(id);
}

void VectorSprite::clear() {
	_image->clear();
}

void VectorSprite::setAntialiased(bool value) {
	_image->setAntialiased(value);
}

bool VectorSprite::isAntialiased() const {
	return _image->isAntialiased();
}

void VectorSprite::setImage(Rc<VectorImage> &&img) {
	XL_ASSERT(img, "Image should not be nullptr");

	if (_image != img) {
		_image = move(img);
		_image->setDirty();
	}
}

const Rc<VectorImage> &VectorSprite::getImage() const {
	return _image;
}

void VectorSprite::onTransformDirty(const Mat4 &parent) {
	bool isDirty = false;
	Vec3 scale;
	parent.decompose(&scale, nullptr, nullptr);

	if (_scale.x != 1.f) { scale.x *= _scale.x; }
	if (_scale.y != 1.f) { scale.y *= _scale.y; }
	if (_scale.z != 1.f) { scale.z *= _scale.z; }

	auto boxRect = layout::calculateImageBoxRect(
			Rect(Vec2(0.0f, 0.0f), Size(_contentSize.width * scale.x, _contentSize.height * scale.y)),
			_image->getImageSize(), _style);

	//if (boxRect != _boxRect) {
		isDirty = true;
		_boxRect = boxRect;
	//}

	if (isDirty || _image->isDirty()) {
		_image->clearDirty();
		_vertexesDirty = true;
	}

	Sprite::onTransformDirty(parent);
}

void VectorSprite::visit(RenderFrameInfo &frame, NodeFlags parentFlags) {
	if (_image->isDirty()) {
		_transformDirty = true;
	}
	Sprite::visit(frame, parentFlags);
}

void VectorSprite::pushCommands(RenderFrameInfo &frame, NodeFlags flags) {
	auto &modelTransform = frame.modelTransformStack.back();
	if (_normalized) {
		Mat4 newMV;
		newMV.m[12] = floorf(modelTransform.m[12]);
		newMV.m[13] = floorf(modelTransform.m[13]);
		newMV.m[14] = floorf(modelTransform.m[14]);

		frame.commands->pushVertexArray(_vertexes.pop(),
				frame.viewProjectionStack.back() * newMV, frame.zPath, _materialId, _isSurface);
	} else {
		frame.commands->pushVertexArray(_vertexes.pop(),
				frame.viewProjectionStack.back() * modelTransform, frame.zPath, _materialId, _isSurface);
	}
}

void VectorSprite::initVertexes() {
	// prevent to do anything
}

void VectorSprite::updateVertexes() {
	auto canvas = VectorCanvas::getInstance();
	canvas->setColor(_displayedColor);
	canvas->setQuality(_quality);

	_result = canvas->draw(_image->popData(), _boxRect, _style);

	_vertexColorDirty = false; // color will be already applied
}

void VectorSprite::updateVertexesColor() {
	_result->updateColor(_displayedColor);
}

}
