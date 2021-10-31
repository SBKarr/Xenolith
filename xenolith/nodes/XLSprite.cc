/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>

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

#include "XLSprite.h"

#include "XLGlCommandList.h"

namespace stappler::xenolith {

bool Sprite::init() {
	return init(Rc<Texture>(nullptr));
}

bool Sprite::init(StringView textureName) {
	if (auto cache = ResourceCache::getInstance()) {
		return init(cache->acquireTexture(textureName));
	}
	return false;
}

bool Sprite::init(Rc<Texture> &&texture) {
	if (!Node::init()) {
		return false;
	}

	if (texture) {
		_texture = move(texture);
	}
	_vertexes.init(4, 6);
	updateVertexes();
	return true;
}

void Sprite::setTexture(StringView textureName) {
	if (textureName.empty()) {
		if (_texture) {
			_texture = nullptr;
			_materialDirty = true;
		}
	} else if (!_texture || _texture->getName() != textureName) {
		if (auto cache = ResourceCache::getInstance()) {
			_texture = cache->acquireTexture(textureName);
			_materialDirty = true;
		}
	}
}

void Sprite::setTexture(Rc<Texture> &&tex) {
	if (_texture) {
		if (!tex) {
			_texture = nullptr;
			_materialDirty = true;
		} else if (_texture->getName() != tex->getName()) {
			_texture = move(tex);
			_materialDirty = true;
		}
	} else {
		if (tex) {
			_texture = move(tex);
			_materialDirty = true;
		}
	}
}

void Sprite::visit(RenderFrameInfo &info, NodeFlags parentFlags) {
	if (_contentSizeDirty) {
		updateVertexes();
	}
	Node::visit(info, parentFlags);
}

void Sprite::draw(RenderFrameInfo &frame, NodeFlags flags) {
	if (_texture) {
		if (_materialDirty) {
			_materialId = frame.scene->getMaterial(getMaterialInfo());
			_materialDirty = false;
		}
		frame.commands->pushVertexArray(_vertexes.pop(), frame.transformStack.back(), frame.zPath, _materialId);
	}
}

MaterialInfo Sprite::getMaterialInfo() const {
	MaterialInfo ret;
	ret.type = gl::MaterialType::Basic2D;
	ret.images[0] = _texture->getIndex();
	return ret;
}

void Sprite::updateColor() {
	if (_tmpColor != _displayedColor) {
		_vertexes.updateColor(_displayedColor);
		if (_tmpColor.a != _displayedColor.a) {
			if (_displayedColor.a == 1.0f || _tmpColor.a == 0.0f) {
				// opaque/transparent material switch
				_materialDirty = true;
			}
		}
		_tmpColor = _displayedColor;
	}
}

void Sprite::updateVertexes() {
	_vertexes.clear();
	_vertexes.addQuad()
		.setGeometry(Vec4::ZERO, _contentSize)
		.setTextureRect(_textureRect, 1.0f, 1.0f, _flippedX, _flippedY, _rotated)
		.setColor(_displayedColor);
}

}
