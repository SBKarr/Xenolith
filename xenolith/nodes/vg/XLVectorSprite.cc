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

bool VectorSprite::init(Size2 size, StringView data) {
	if (!Sprite::init()) {
		return false;
	}

	_image = Rc<VectorImage>::create(size, data);
	_contentSize = _image->getImageSize();
	return _image != nullptr;
}

bool VectorSprite::init(Size2 size, VectorPath &&path) {
	if (!Sprite::init()) {
		return false;
	}

	_image = Rc<VectorImage>::create(size, move(path));
	_contentSize = _image->getImageSize();
	return _image != nullptr;
}

bool VectorSprite::init(Size2 size) {
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

Rc<VectorPathRef> VectorSprite::addPath(StringView id, StringView cache, Mat4 pos) {
	return _image->addPath(id, cache, pos);
}

Rc<VectorPathRef> VectorSprite::addPath(const VectorPath & path, StringView id, StringView cache, Mat4 pos) {
	return _image->addPath(path, id, cache, pos);
}

Rc<VectorPathRef> VectorSprite::addPath(VectorPath && path, StringView id, StringView cache, Mat4 pos) {
	return _image->addPath(move(path), id, cache, pos);
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

bool VectorSprite::visitDraw(RenderFrameInfo &frame, NodeFlags parentFlags) {
	if (_image->isDirty()) {
		_vertexesDirty = true;
	}
	return Sprite::visitDraw(frame, parentFlags);
}

uint32_t VectorSprite::getTrianglesCount() const {
	uint32_t ret = 0;
	if (_result) {
		for (auto &it : _result->data) {
			ret += it.second->indexes.size() / 3;
		}
	}
	return ret;
}

uint32_t VectorSprite::getVertexesCount() const {
	uint32_t ret = 0;
	if (_result) {
		for (auto &it : _result->data) {
			ret += it.second->data.size();
		}
	}
	return ret;
}

void VectorSprite::pushCommands(RenderFrameInfo &frame, NodeFlags flags) {
	if (!_result || _result->data.empty()) {
		return;
	}

	auto &targetData = _result->mut;
	auto reqMemSize = sizeof(Pair<Mat4, Rc<gl::VertexData>>) * targetData.size();

	// pool memory is 16-bytes aligned, no problems with Mat4
	auto tmpData = new (memory::pool::palloc(frame.pool, reqMemSize)) Pair<Mat4, Rc<gl::VertexData>>[targetData.size()];
	auto target = tmpData;
	if (_normalized) {
		auto transform = frame.modelTransformStack.back() * _targetTransform;
		for (auto &it : targetData) {
			auto modelTransform = transform * it.first;

			Mat4 newMV;
			newMV.m[12] = floorf(modelTransform.m[12]);
			newMV.m[13] = floorf(modelTransform.m[13]);
			newMV.m[14] = floorf(modelTransform.m[14]);

			target->first = newMV;
			target->second = it.second;
			++ target;
		}
	} else {
		auto transform = frame.viewProjectionStack.back() * frame.modelTransformStack.back() * _targetTransform;
		for (auto &it : targetData) {
			auto pathTransform = transform * it.first;
			target->first = pathTransform;
			target->second = it.second;
			++ target;
		}
	}

	frame.commands->pushVertexArray(makeSpanView(tmpData, targetData.size()), frame.zPath, _materialId, _realRenderingLevel);
}

void VectorSprite::initVertexes() {
	// prevent to do anything
}

void VectorSprite::updateVertexes() {
	Vec3 viewScale;
	_modelViewTransform.decompose(&viewScale, nullptr, nullptr);

	Size2 imageSize = _image->getImageSize();
	Size2 targetViewSpaceSize(_contentSize.width * viewScale.x / _textureRect.size.width,
			_contentSize.height * viewScale.y / _textureRect.size.height);

	float targetScaleX = _textureRect.size.width;
	float targetScaleY = _textureRect.size.height;
	float targetOffsetX = -_textureRect.origin.x * imageSize.width;
	float targetOffsetY = -_textureRect.origin.y * imageSize.height;

	Size2 texSize(imageSize.width * _textureRect.size.width, imageSize.height * _textureRect.size.height);

	if (_autofit != Autofit::None) {
		float scale = 1.0f;
		switch (_autofit) {
		case Autofit::None: break;
		case Autofit::Width: scale = texSize.width / _contentSize.width; break;
		case Autofit::Height: scale = texSize.height / _contentSize.height; break;
		case Autofit::Contain: scale = std::max(texSize.width / _contentSize.width, texSize.height / _contentSize.height); break;
		case Autofit::Cover: scale = std::min(texSize.width / _contentSize.width, texSize.height / _contentSize.height); break;
		}

		auto texSizeInView = Size2(texSize.width / scale, texSize.height / scale);
		targetOffsetX = targetOffsetX + (_contentSize.width - texSizeInView.width) * _autofitPos.x;
		targetOffsetY = targetOffsetY + (_contentSize.height - texSizeInView.height) * _autofitPos.y;

		targetViewSpaceSize = Size2(texSizeInView.width * viewScale.x,
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

		// std::cout << targetTransform << " " << targetViewSpaceSize << "\n";

		_result = canvas->draw(_image->popData(), targetViewSpaceSize);
		_vertexColorDirty = false; // color will be already applied
	}

	Mat4 scaleTransform;
	scaleTransform.scale(viewScale);
	scaleTransform.inverse();

	_targetTransform *= scaleTransform;

	auto isSolidImage = [&] {
		for (auto &it : _image->getPaths()) {
			if (it.second->isAntialiased()) {
				return false;
			}

			auto s = it.second->getStyle();
			switch (s) {
			case vg::DrawStyle::Fill:
				if (it.second->getFillOpacity() != 255) {
					return false;
				}
				break;
			case vg::DrawStyle::FillAndStroke:
				if (it.second->getFillOpacity() != 255) {
					return false;
				}
				if (it.second->getStrokeOpacity() != 255) {
					return false;
				}
				break;
			case vg::DrawStyle::Stroke:
				if (it.second->getStrokeOpacity() != 255) {
					return false;
				}
				break;
			default:
				break;
			}
		}
		return true;
	};

	auto isSolid = isSolidImage();
	if (isSolid != _imageIsSolid) {
		_materialDirty = true;
		_imageIsSolid = isSolid;
	}
}

void VectorSprite::updateVertexesColor() {
	_result->updateColor(_displayedColor);
}

RenderingLevel VectorSprite::getRealRenderingLevel() const {
	auto level = _renderingLevel;
	if (level == RenderingLevel::Default) {
		if (_displayedColor.a < 1.0f || !_texture || _materialInfo.getLineWidth() != 0.0f) {
			level = RenderingLevel::Transparent;
		} else if (_colorMode.getMode() == ColorMode::Solid) {
			if (_texture->hasAlpha()) {
				level = RenderingLevel::Transparent;
			} else {
				level = _imageIsSolid ? RenderingLevel::Solid : RenderingLevel::Transparent;
			}
		} else {
			auto alphaMapping = _colorMode.getA();
			switch (alphaMapping) {
			case gl::ComponentMapping::Identity:
				if (_texture->hasAlpha()) {
					level = RenderingLevel::Transparent;
				} else {
					level = _imageIsSolid ? RenderingLevel::Solid : RenderingLevel::Transparent;
				}
				break;
			case gl::ComponentMapping::Zero:
				level = RenderingLevel::Transparent;
				break;
			case gl::ComponentMapping::One:
				level = _imageIsSolid ? RenderingLevel::Solid : RenderingLevel::Transparent;
				break;
			default:
				level = RenderingLevel::Transparent;
				break;
			}
		}
	}
	return level;
}

}
