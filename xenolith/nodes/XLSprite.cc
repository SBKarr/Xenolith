/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLSprite.h"
#include "XLApplication.h"

#include "XLGlCommandList.h"

namespace stappler::xenolith {

Sprite::Sprite() {
	_blendInfo = BlendInfo(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha, gl::BlendOp::Add,
			gl::BlendFactor::Zero, gl::BlendFactor::One, gl::BlendOp::Add);
	_materialInfo.setBlendInfo(_blendInfo);
	_materialInfo.setDepthInfo(DepthInfo(false, true, gl::CompareOp::Less));
	_applyMode = DoNotApply;
}

bool Sprite::init() {
	return Sprite::init(SolidTextureName);
}

bool Sprite::init(StringView textureName) {
	if (!DynamicStateNode::init()) {
		return false;
	}

	_textureName = textureName.str<Interface>();
	initVertexes();
	return true;
}

bool Sprite::init(Rc<Texture> &&texture) {
	if (!DynamicStateNode::init()) {
		return false;
	}

	if (texture) {
		_texture = move(texture);
		_isTextureLoaded = _texture->isLoaded();
	}

	initVertexes();
	return true;
}

void Sprite::setTexture(StringView textureName) {
	if (!_running) {
		if (_texture) {
			_texture = nullptr;
			_materialDirty = true;
		}
		_textureName = textureName.str<Interface>();
	} else {
		if (textureName.empty()) {
			if (_texture) {
				if (_running) {
					_texture->onExit(_scene);
				}
				_texture = nullptr;
				_materialDirty = true;
			}
		} else if (!_texture || _texture->getName() != textureName) {
			if (auto &cache = _director->getApplication()->getResourceCache()) {
				if (auto tex = cache->acquireTexture(textureName)) {
					setTexture(move(tex));
				}
			}
		}
	}
}

void Sprite::setTexture(Rc<Texture> &&tex) {
	if (_texture) {
		if (!tex) {
			if (_running) {
				_texture->onExit(_scene);
			}
			_texture = nullptr;
			_textureName.clear();
			_materialDirty = true;
			_isTextureLoaded = false;
		} else if (_texture->getName() != tex->getName()) {
			if (_running) {
				_texture->onExit(_scene);
			}
			_texture = move(tex);
			if (_running) {
				_texture->onEnter(_scene);
			}
			_isTextureLoaded = _texture->isLoaded();
			if (_isTextureLoaded && _textureLoadedCallback) {
				_textureLoadedCallback();
			}
			_textureName = _texture->getName().str<Interface>();
			_materialDirty = true;
		}
	} else {
		if (tex) {
			_texture = move(tex);
			if (_running) {
				_texture->onEnter(_scene);
			}
			_isTextureLoaded = _texture->isLoaded();
			if (_isTextureLoaded && _textureLoadedCallback) {
				_textureLoadedCallback();
			}
			_textureName = _texture->getName().str<Interface>();
			_materialDirty = true;
		}
	}
}

const Rc<Texture> &Sprite::getTexture() const {
	return _texture;
}

void Sprite::setTextureRect(const Rect &rect) {
	if (!_textureRect.equals(rect)) {
		_textureRect = rect;
		_vertexesDirty = true;
	}
}

bool Sprite::visitDraw(RenderFrameInfo &frame, NodeFlags parentFlags) {
	if (_texture) {
		auto loaded = _texture->isLoaded();
		if (loaded != _isTextureLoaded && loaded) {
			onTextureLoaded();
			_isTextureLoaded = loaded;
		}
	}
	return DynamicStateNode::visitDraw(frame, parentFlags);
}

void Sprite::draw(RenderFrameInfo &frame, NodeFlags flags) {
	if (!_texture) {
		return;
	}

	if (_texture->isLoaded()) {
		if (_autofit != Autofit::None) {
			auto size = _texture->getExtent();
			if (_targetTextureSize != size) {
				_targetTextureSize = size;
				_vertexesDirty = true;
			}
		}

		if (checkVertexDirty()) {
			updateVertexes();
			_vertexesDirty = false;
		}

		if (_vertexColorDirty) {
			updateVertexesColor();
			_vertexColorDirty = false;
		}

		if (_materialDirty) {
			updateBlendAndDepth();

			auto info = getMaterialInfo();
			_materialId = frame.scene->getMaterial(info);
			if (_materialId == 0) {
				_materialId = frame.scene->acquireMaterial(info, getMaterialImages(), isMaterialRevokable());
				if (_materialId == 0) {
					log::vtext("Sprite", "Material for sprite with texture '", _texture->getName(), "' not found");
				}
			}
			_materialDirty = false;
		}

		for (auto &it : _pendingDependencies) {
			emplace_ordered(frame.commands->waitDependencies, move(it));
		}

		pushCommands(frame, flags);
		_pendingDependencies.clear();
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
	}

	if (_texture) {
		_texture->onEnter(_scene);
	}
}

void Sprite::onExit() {
	if (_texture) {
		_texture->onExit(_scene);
	}
	Node::onExit();
}

void Sprite::onContentSizeDirty() {
	_vertexesDirty = true;
	Node::onContentSizeDirty();
}

void Sprite::onTextureLoaded() {
	if (_textureLoadedCallback) {
		_textureLoadedCallback();
	}
}

void Sprite::setColorMode(const ColorMode &mode) {
	if (_colorMode != mode) {
		_colorMode = mode;
		_materialDirty = true;
	}
}

void Sprite::setBlendInfo(const BlendInfo &info) {
	if (_blendInfo != info) {
		_blendInfo = info;
		_materialInfo.setBlendInfo(info);
		_materialDirty = true;
	}
}

void Sprite::setLineWidth(float value) {
	if (_materialInfo.getLineWidth() != value) {
		_materialInfo.setLineWidth(value);
		_materialDirty = true;
	}
}

void Sprite::setRenderingLevel(RenderingLevel level) {
	if (_renderingLevel != level) {
		_renderingLevel = level;
		if (_running) {
			updateBlendAndDepth();
		}
	}
}

void Sprite::setNormalized(bool value) {
	if (_normalized != value) {
		_normalized = value;
	}
}

void Sprite::setAutofit(Autofit autofit) {
	if (_autofit != autofit) {
		_autofit = autofit;
		_vertexesDirty = true;
	}
}

void Sprite::setAutofitPosition(const Vec2 &vec) {
	if (!_autofitPos.equals(vec)) {
		_autofitPos = vec;
		if (_autofit != Autofit::None) {
			_vertexesDirty = true;
		}
	}
}

void Sprite::setSamplerIndex(uint16_t idx) {
	if (_samplerIdx != idx) {
		_samplerIdx = idx;
		_materialDirty = true;
	}
}

void Sprite::setTextureLoadedCallback(Function<void()> &&cb) {
	_textureLoadedCallback = move(cb);
}

void Sprite::pushShadowCommands(RenderFrameInfo &frame, NodeFlags flags, const Mat4 &t, SpanView<gl::TransformedVertexData> data) {
	frame.shadows->pushShadowArray(data, frame.shadowStack.back());
}

void Sprite::pushCommands(RenderFrameInfo &frame, NodeFlags flags) {
	auto data = _vertexes.pop();
	Mat4 newMV;
	if (_normalized) {
		auto &modelTransform = frame.modelTransformStack.back();
		newMV.m[12] = floorf(modelTransform.m[12]);
		newMV.m[13] = floorf(modelTransform.m[13]);
		newMV.m[14] = floorf(modelTransform.m[14]);
	} else {
		newMV = frame.modelTransformStack.back();
	}

	if (_shadowIndex > 0.0f) {
		gl::TransformedVertexData transformData{newMV, data};
		pushShadowCommands(frame, flags, newMV, makeSpanView(&transformData, 1));
	}
	frame.commands->pushVertexArray(move(data), frame.viewProjectionStack.back() * newMV,
			frame.zPath, _materialId, _realRenderingLevel, frame.shadowStack.back(), _commandFlags);
}

MaterialInfo Sprite::getMaterialInfo() const {
	MaterialInfo ret;
	ret.type = gl::MaterialType::Basic2D;
	ret.images[0] = _texture->getIndex();
	ret.samplers[0] = _samplerIdx;
	ret.colorModes[0] = _colorMode;
	ret.pipeline = _materialInfo;
	return ret;
}

Vector<gl::MaterialImage> Sprite::getMaterialImages() const {
	Vector<gl::MaterialImage> ret;
	ret.emplace_back(_texture->getMaterialImage());
	return ret;
}

bool Sprite::isMaterialRevokable() const {
	return _texture && _texture->getTemporary();
}

void Sprite::updateColor() {
	if (_tmpColor != _displayedColor) {
		_vertexColorDirty = true;
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

void Sprite::initVertexes() {
	_vertexes.init(4, 6);
	_vertexesDirty = true;
}

void Sprite::updateVertexes() {
	_vertexes.clear();

	auto texExtent = _texture->getExtent();
	auto texSize = Size2(texExtent.width, texExtent.height);

	texSize = Size2(texSize.width * _textureRect.size.width, texSize.height * _textureRect.size.height);

	Rect contentRect;
	Rect textureRect;

	if (!getAutofitParams(_autofit, _autofitPos, _contentSize, Size2(texSize.width, texSize.height), contentRect, textureRect)) {
		texSize = Size2(1.0f, 1.0f);
		contentRect = Rect(0.0f, 0.0f, _contentSize.width, _contentSize.height);
		textureRect = _textureRect;
	} else {
		textureRect = Rect(
			_textureRect.origin.x + textureRect.origin.x / texSize.width,
			_textureRect.origin.y + textureRect.origin.y / texSize.height,
			textureRect.size.width / texSize.width,
			textureRect.size.height / texSize.height);
	}

	_vertexes.addQuad()
		.setGeometry(Vec4(contentRect.origin.x, contentRect.origin.y, 0.0f, 1.0f), contentRect.size)
		.setTextureRect(textureRect, 1.0f, 1.0f, _flippedX, _flippedY, _rotated)
		.setColor(_displayedColor);

	_vertexColorDirty = false;
}

void Sprite::updateBlendAndDepth() {
	bool shouldBlendColors = false;
	bool shouldWriteDepth = false;

	_realRenderingLevel = getRealRenderingLevel();
	switch (_realRenderingLevel) {
	case RenderingLevel::Default:
		break;
	case RenderingLevel::Solid:
		shouldWriteDepth = true;
		shouldBlendColors = false;
		break;
	case RenderingLevel::Surface:
		shouldBlendColors = true;
		shouldWriteDepth = false;
		break;
	case RenderingLevel::Transparent:
		shouldBlendColors = true;
		shouldWriteDepth = false;
		break;
	}

	if (shouldBlendColors) {
		if (!_blendInfo.enabled) {
			_blendInfo.enabled = 1;
			_materialDirty = true;
		}
	} else {
		if (_blendInfo.enabled) {
			_blendInfo.enabled = 0;
			_materialDirty = true;
		}
	}

	_materialInfo.setBlendInfo(_blendInfo);

	auto depth = _materialInfo.getDepthInfo();
	if (shouldWriteDepth) {
		if (!depth.writeEnabled) {
			depth.writeEnabled = 1;
			_materialDirty = true;
		}
	} else {
		if (depth.writeEnabled) {
			depth.writeEnabled = 0;
			_materialDirty = true;
		}
	}
	if (_realRenderingLevel == RenderingLevel::Surface || _realRenderingLevel == RenderingLevel::Transparent) {
		if (depth.compare != toInt(gl::CompareOp::LessOrEqual)) {
			depth.compare = toInt(gl::CompareOp::LessOrEqual);
			_materialDirty = true;
		}
	} else {
		if (depth.compare != toInt(gl::CompareOp::Less)) {
			depth.compare = toInt(gl::CompareOp::Less);
			_materialDirty = true;
		}
	}
	_materialInfo.setDepthInfo(depth);
}

RenderingLevel Sprite::getRealRenderingLevel() const {
	auto level = _renderingLevel;
	if (level == RenderingLevel::Default) {
		if (_displayedColor.a < 1.0f || !_texture || _materialInfo.getLineWidth() != 0.0f) {
			level = RenderingLevel::Transparent;
		} else if (_colorMode.getMode() == ColorMode::Solid) {
			if (_texture->hasAlpha()) {
				level = RenderingLevel::Transparent;
			} else {
				level = RenderingLevel::Solid;
			}
		} else {
			auto alphaMapping = _colorMode.getA();
			switch (alphaMapping) {
			case gl::ComponentMapping::Identity:
				if (_texture->hasAlpha()) {
					level = RenderingLevel::Transparent;
				} else {
					level = RenderingLevel::Solid;
				}
				break;
			case gl::ComponentMapping::Zero:
				level = RenderingLevel::Transparent;
				break;
			case gl::ComponentMapping::One:
				level = RenderingLevel::Solid;
				break;
			default:
				level = RenderingLevel::Transparent;
				break;
			}
		}
	}
	return level;
}

bool Sprite::checkVertexDirty() const {
	return _vertexesDirty;
}

bool Sprite::getAutofitParams(Autofit autofit, const Vec2 &autofitPos, const Size2 &contentSize, const Size2 &texSize,
		Rect &contentRect, Rect &textureRect) {

	contentRect = Rect(Vec2::ZERO, contentSize);

	float scale = 1.0f;
	switch (autofit) {
	case Autofit::None:
		return false;
		break;
	case Autofit::Width: scale = texSize.width / contentSize.width; break;
	case Autofit::Height: scale = texSize.height / contentSize.height; break;
	case Autofit::Contain: scale = std::max(texSize.width / contentSize.width, texSize.height / contentSize.height); break;
	case Autofit::Cover: scale = std::min(texSize.width / contentSize.width, texSize.height / contentSize.height); break;
	}

	contentRect = Rect(Vec2::ZERO, contentSize);
	textureRect = Rect(0, 0, texSize.width, texSize.height);

	auto texSizeInView = Size2(texSize.width / scale, texSize.height / scale);
	if (texSizeInView.width < contentSize.width) {
		contentRect.size.width -= (contentSize.width - texSizeInView.width);
		contentRect.origin.x = (contentSize.width - texSizeInView.width) * autofitPos.x;
	} else if (texSizeInView.width > contentSize.width) {
		textureRect.origin.x = (textureRect.size.width - contentSize.width * scale) * autofitPos.x;
		textureRect.size.width = contentSize.width * scale;
	}

	if (texSizeInView.height < contentSize.height) {
		contentRect.size.height -= (contentSize.height - texSizeInView.height);
		contentRect.origin.y = (contentSize.height - texSizeInView.height) * autofitPos.y;
	} else if (texSizeInView.height > contentSize.height) {
		textureRect.origin.y = (textureRect.size.height - contentSize.height * scale) * autofitPos.y;
		textureRect.size.height = contentSize.height * scale;
	}

	return true;
}

}
