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

VectorSprite::VectorSprite() { }

bool VectorSprite::init(Rc<VectorImage> &&img) {
	XL_ASSERT(img, "Image should not be nullptr");

	if (!Sprite::init() || !img) {
		return false;
	}

	_image = img;
	_contentSize = _image->getImageSize();
	return true;
}

bool VectorSprite::init(Size size, StringView data) {
	if (!Sprite::init()) {
		return false;
	}

	_image = Rc<VectorImage>::create(size, data);
	_contentSize = _image->getImageSize();
	return _image != nullptr;
}

bool VectorSprite::init(Size size, VectorPath &&path) {
	if (!Sprite::init()) {
		return false;
	}

	_image = Rc<VectorImage>::create(size, move(path));
	_contentSize = _image->getImageSize();
	return _image != nullptr;
}

bool VectorSprite::init(Size size) {
	if (!Sprite::init()) {
		return false;
	}

	_image = Rc<VectorImage>::create(size);
	_contentSize = _image->getImageSize();
	return _image != nullptr;
}

bool VectorSprite::init(StringView data) {
	if (!Sprite::init()) {
		return false;
	}

	_image = Rc<VectorImage>::create(data);
	_contentSize = _image->getImageSize();
	return _image != nullptr;
}

bool VectorSprite::init(BytesView data) {
	if (!Sprite::init()) {
		return false;
	}

	_image = Rc<VectorImage>::create(data);
	_contentSize = _image->getImageSize();
	return _image != nullptr;
}

bool VectorSprite::init(FilePath path) {
	if (!Sprite::init()) {
		return false;
	}

	_image = Rc<VectorImage>::create(path);
	_contentSize = _image->getImageSize();
	return _image != nullptr;
}

Rc<VectorPathRef> VectorSprite::addPath(StringView id, Mat4 pos) {
	return _image->addPath(id, pos);
}

Rc<VectorPathRef> VectorSprite::addPath(const VectorPath & path, StringView id, Mat4 pos) {
	return _image->addPath(path, id, pos);
}

Rc<VectorPathRef> VectorSprite::addPath(VectorPath && path, StringView id, Mat4 pos) {
	return _image->addPath(move(path), id, pos);
}

Rc<VectorPathRef> VectorSprite::getPath(StringView id) {
	return _image->getPath(id);
}

void VectorSprite::removePath(const Rc<VectorPathRef> &path) {
	_image->removePath(path);
}

void VectorSprite::removePath(StringView id) {
	_image->removePath(id);
}

void VectorSprite::clear() {
	_image->clear();
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

void VectorSprite::setQuality(float val) {
	if (_quality != val) {
		_quality = val;
		_image->setDirty();
	}
}

void VectorSprite::onTransformDirty(const Mat4 &parent) {
	_vertexesDirty = true;
	Sprite::onTransformDirty(parent);
}

void VectorSprite::visit(RenderFrameInfo &frame, NodeFlags parentFlags) {
	if (_image->isDirty()) {
		_vertexesDirty = true;
	}
	Sprite::visit(frame, parentFlags);
}

void VectorSprite::pushCommands(RenderFrameInfo &frame, NodeFlags flags) {
	if (!_result || _result->data.empty()) {
		return;
	}

	auto tmpData = _result->data;
	if (_normalized) {
		auto transform = frame.modelTransformStack.back() * _targetTransform;
		for (auto &it : tmpData) {
			auto modelTransform = transform * it.first;

			Mat4 newMV;
			newMV.m[12] = floorf(modelTransform.m[12]);
			newMV.m[13] = floorf(modelTransform.m[13]);
			newMV.m[14] = floorf(modelTransform.m[14]);

			it.first = newMV;
		}
	} else {
		auto transform = frame.viewProjectionStack.back() * frame.modelTransformStack.back() * _targetTransform;
		for (auto &it : tmpData) {
			it.first = transform * it.first;
		}
	}

	frame.commands->pushVertexArray(tmpData, frame.zPath, _materialId, _realRenderingLevel);
}

void VectorSprite::initVertexes() {
	// prevent to do anything
}

void VectorSprite::updateVertexes() {
	Vec3 viewScale;
	_modelViewTransform.decompose(&viewScale, nullptr, nullptr);

	Size imageSize = _image->getImageSize();
	Size targetViewSpaceSize(_contentSize.width * viewScale.x / _textureRect.size.width,
			_contentSize.height * viewScale.y / _textureRect.size.height);

	float targetScaleX = _textureRect.size.width;
	float targetScaleY = _textureRect.size.height;
	float targetOffsetX = -_textureRect.origin.x * imageSize.width;
	float targetOffsetY = -_textureRect.origin.y * imageSize.height;

	Size texSize(imageSize.width * _textureRect.size.width, imageSize.height * _textureRect.size.height);

	if (_autofit != Autofit::None) {
		float scale = 1.0f;
		switch (_autofit) {
		case Autofit::None: break;
		case Autofit::Width: scale = texSize.width / _contentSize.width; break;
		case Autofit::Height: scale = texSize.height / _contentSize.height; break;
		case Autofit::Contain: scale = std::max(texSize.width / _contentSize.width, texSize.height / _contentSize.height); break;
		case Autofit::Cover: scale = std::min(texSize.width / _contentSize.width, texSize.height / _contentSize.height); break;
		}

		auto texSizeInView = Size(texSize.width / scale, texSize.height / scale);
		targetOffsetX = targetOffsetX + (_contentSize.width - texSizeInView.width) * _autofitPos.x;
		targetOffsetY = targetOffsetY + (_contentSize.height - texSizeInView.height) * _autofitPos.y;

		targetViewSpaceSize = Size(texSizeInView.width * viewScale.x,
				texSizeInView.height * viewScale.y);

		targetScaleX =_textureRect.size.width;
		targetScaleY =_textureRect.size.height;
	}

	Mat4 targetTransform(
		targetScaleX, 0.0f, 0.0f, targetOffsetX,
		0.0f, targetScaleY, 0.0f, targetOffsetY,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	bool isDirty = false;

	if (_targetSize != targetViewSpaceSize) {
		isDirty = true;
		_targetSize = targetViewSpaceSize;
	}

	_targetTransform = targetTransform;
	if (isDirty || _image->isDirty()) {
		_image->clearDirty();
		auto canvas = VectorCanvas::getInstance();
		canvas->setColor(_displayedColor);
		canvas->setQuality(_quality);

		std::cout << targetTransform << " " << targetViewSpaceSize << "\n";

		_result = canvas->draw(_image->popData(), targetViewSpaceSize);
		_vertexColorDirty = false; // color will be already applied
	}
}

void VectorSprite::updateVertexesColor() {
	_result->updateColor(_displayedColor);
}

}
