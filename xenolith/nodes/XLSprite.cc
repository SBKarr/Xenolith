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
#include "XLApplication.h"

#include "XLGlCommandList.h"

namespace stappler::xenolith {

Sprite::Sprite() {
	_materialInfo.blend = BlendInfo(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha, gl::BlendOp::Add,
			gl::BlendFactor::One, gl::BlendFactor::Zero, gl::BlendOp::Add);
	_materialInfo.depth = DepthInfo(false, true, gl::CompareOp::Less);
}

bool Sprite::init() {
	return init(SolidTextureName);
}

bool Sprite::init(StringView textureName) {
	if (!Node::init()) {
		return false;
	}

	_textureName = textureName.str();
	_vertexes.init(4, 6);
	updateVertexes();
	return true;
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
	if (!_running) {
		_textureName = textureName.str();
	} else {
		if (textureName.empty()) {
			if (_texture) {
				_texture = nullptr;
				_materialDirty = true;
			}
		} else if (!_texture || _texture->getName() != textureName) {
			if (auto &cache = _director->getApplication()->getResourceCache()) {
				_texture = cache->acquireTexture(textureName);
				_materialDirty = true;
			}
		}
	}
}

void Sprite::setTexture(Rc<Texture> &&tex) {
	if (_texture) {
		if (!tex) {
			_texture = nullptr;
			_textureName.clear();
			_materialDirty = true;
		} else if (_texture->getName() != tex->getName()) {
			_texture = move(tex);
			_textureName = _texture->getName().str();
			_materialDirty = true;
		}
	} else {
		if (tex) {
			_texture = move(tex);
			_textureName = _texture->getName().str();
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
			updateBlendAndDepth();

			auto info = getMaterialInfo();
			_materialId = frame.scene->getMaterial(info);
			if (_materialId == 0) {
				_materialId = frame.scene->acquireMaterial(info, getMaterialImages());
				if (_materialId == 0) {
					log::vtext("Sprite", "Material for sprite with texture '", _texture->getName(), "' not found");
				}
			}
			_materialDirty = false;
		}

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
}

void Sprite::onEnter(Scene *scene) {
	Node::onEnter(scene);

	if (!_textureName.empty()) {
		if (!_texture || _texture->getName() != _textureName) {
			if (auto &cache = _director->getApplication()->getResourceCache()) {
				_texture = cache->acquireTexture(_textureName);
				_materialDirty = true;
			}
		}
		_textureName.clear();
	}
}

void Sprite::setColorMode(const ColorMode &mode) {
	if (_colorMode != mode) {
		_colorMode = mode;
		_materialDirty = true;
	}
}

void Sprite::setBlendInfo(const BlendInfo &info) {
	if (_materialInfo.blend != info) {
		_materialInfo.blend = info;
		_materialDirty = true;
	}
}

void Sprite::setForceSolid(bool val) {
	if (_forceSolid != val) {
		_forceSolid = val;
		updateBlendAndDepth();
	}
}

void Sprite::setSurface(bool value) {
	if (_isSurface != value) {
		_isSurface = value;
		updateBlendAndDepth();
	}
}

void Sprite::setNormalized(bool value) {
	if (_normalized != value) {
		_normalized = value;
	}
}

MaterialInfo Sprite::getMaterialInfo() const {
	MaterialInfo ret;
	ret.type = gl::MaterialType::Basic2D;
	ret.images[0] = _texture->getIndex();
	ret.colorModes[0] = _colorMode;
	ret.pipeline = _materialInfo.normalize();
	return ret;
}

Vector<gl::MaterialImage> Sprite::getMaterialImages() const {
	Vector<gl::MaterialImage> ret;
	ret.emplace_back(_texture->getMaterialImage());
	return ret;
}

void Sprite::updateColor() {
	if (_tmpColor != _displayedColor) {
		updateVertexesColor();
		if (_tmpColor.a != _displayedColor.a) {
			if (_displayedColor.a == 1.0f || _tmpColor.a == 1.0f) {
				updateBlendAndDepth();
			}
		}
		_tmpColor = _displayedColor;
	}
}

void Sprite::updateVertexesColor() {
	_vertexes.updateColor(_displayedColor);
}

void Sprite::updateVertexes() {
	_vertexes.clear();
	_vertexes.addQuad()
		.setGeometry(Vec4::ZERO, _contentSize)
		.setTextureRect(_textureRect, 1.0f, 1.0f, _flippedX, _flippedY, _rotated)
		.setColor(_displayedColor);
}

void Sprite::updateBlendAndDepth() {
	bool shouldBlendColors = false;
	bool shouldWriteDepth = false;

	auto checkBlendColors = [&]  {
		if (_forceSolid) {
			shouldWriteDepth = true;
			shouldBlendColors = false;
			return;
		}
		if (_displayedColor.a < 1.0f || !_texture || _isSurface) {
			shouldBlendColors = true;
			shouldWriteDepth = false;
			return;
		}
		if (_colorMode.getMode() == ColorMode::Solid) {
			if (_texture->hasAlpha()) {
				shouldBlendColors = true;
				shouldWriteDepth = false;
			} else {
				shouldBlendColors = false;
				shouldWriteDepth = true;
			}
		} else {
			auto alphaMapping = _colorMode.getA();
			switch (alphaMapping) {
			case gl::ComponentMapping::Identity:
				if (_texture->hasAlpha()) {
					shouldBlendColors = true;
					shouldWriteDepth = false;
				}
				break;
			case gl::ComponentMapping::Zero:
				shouldBlendColors = true;
				shouldWriteDepth = false;
				break;
			case gl::ComponentMapping::One:
				shouldBlendColors = false;
				shouldWriteDepth = true;
				break;
			default:
				shouldBlendColors = true;
				shouldWriteDepth = false;
				break;
			}
		}
	};

	checkBlendColors();

	if (shouldBlendColors) {
		if (!_materialInfo.blend.enabled) {
			_materialInfo.blend.enabled = 1;
			_materialDirty = true;
		}
	} else {
		if (_materialInfo.blend.enabled) {
			_materialInfo.blend.enabled = 0;
			_materialDirty = true;
		}
	}
	if (shouldWriteDepth) {
		if (!_materialInfo.depth.writeEnabled) {
			_materialInfo.depth.writeEnabled = 1;
			_materialDirty = true;
		}
	} else {
		if (_materialInfo.depth.writeEnabled) {
			_materialInfo.depth.writeEnabled = 0;
			_materialDirty = true;
		}
	}
	if (_isSurface) {
		if (_materialInfo.depth.compare != toInt(gl::CompareOp::LessOrEqual)) {
			_materialInfo.depth.compare = toInt(gl::CompareOp::LessOrEqual);
			_materialDirty = true;
		}
	} else {
		if (_materialInfo.depth.compare != toInt(gl::CompareOp::Less)) {
			_materialInfo.depth.compare = toInt(gl::CompareOp::Less);
			_materialDirty = true;
		}
	}
}

}
