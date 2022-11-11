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

#ifndef XENOLITH_NODES_XLSPRITE_H_
#define XENOLITH_NODES_XLSPRITE_H_

#include "XLDynamicStateNode.h"
#include "XLFontStyle.h"
#include "XLResourceCache.h"
#include "XLVertexArray.h"

namespace stappler::xenolith {

class Sprite : public DynamicStateNode {
public:
	using Autofit = font::Autofit;

	static constexpr uint16_t SamplerIndexDefaultFilterNearest = 0;
	static constexpr uint16_t SamplerIndexDefaultFilterLinear = 1;

	Sprite();
	virtual ~Sprite() { }

	virtual bool init() override;
	virtual bool init(StringView);
	virtual bool init(Rc<Texture> &&);

	virtual void setTexture(StringView);
	virtual void setTexture(Rc<Texture> &&);

	// texture rect should be normalized
	virtual void setTextureRect(const Rect &);
	virtual const Rect &getTextureRect() const { return _textureRect; }

	virtual void draw(RenderFrameInfo &, NodeFlags flags) override;

	virtual void onEnter(Scene *) override;
	virtual void onExit() override;
	virtual void onContentSizeDirty() override;

	virtual void setColorMode(const ColorMode &);
	virtual const ColorMode &getColorMode() const { return _colorMode; }

	virtual void setBlendInfo(const BlendInfo &);
	virtual const BlendInfo &getBlendInfo() const { return _materialInfo.getBlendInfo(); }

	// used for debug purposes only, follow rules from PipelineMaterialInfo.lineWidth:
	// 0.0f - draw triangles, < 0.0f - points,  > 0.0f - lines with width
	// corresponding pipeline should be precompiled
	// points and lines are always RenderingLevel::Transparent, when Default level resolves
	virtual void setLineWidth(float);
	virtual float getLineWidth() const { return _materialInfo.getLineWidth(); }

	virtual void setRenderingLevel(RenderingLevel);
	virtual RenderingLevel getRenderingLevel() const { return _renderingLevel; }

	virtual void setNormalized(bool);
	virtual bool isNormalized() const { return _normalized; }

	virtual void setAutofit(Autofit);
	virtual Autofit getAutofit() const { return _autofit; }

	virtual void setAutofitPosition(const Vec2 &);
	virtual const Vec2 &getAutofitPosition() const { return _autofitPos; }

	/** Семплеры определяются во время старта цикла графики (gl::Loop) и неизменны в последствии
	 * По умолчанию, семплер с индексом 0 использует фильтр nearest, 1 - linear.
	 * Разработчики приложений могут определять свою схему для семплеров,
	 * но рекомендуем следовать соглашению в отношении 0 и 1
	 *
	 * Если семплер с указанным индексом не определён в движке - поведение не определено
	 */
	virtual void setSamplerIndex(uint16_t);
	virtual uint16_t getSamplerIndex() const { return _samplerIdx; }

	virtual void setCommandFlags(gl::CommandFlags flags) { _commandFlags = flags; }
	virtual void addCommandFlags(gl::CommandFlags flags) { _commandFlags |= flags; }
	virtual void removeCommandFlags(gl::CommandFlags flags) { _commandFlags &= ~flags; }
	virtual gl::CommandFlags getCommandFlags() const { return _commandFlags; }

protected:
	using DynamicStateNode::init;

	virtual void pushCommands(RenderFrameInfo &, NodeFlags flags);

	virtual MaterialInfo getMaterialInfo() const;
	virtual Vector<gl::MaterialImage> getMaterialImages() const;
	virtual bool isMaterialRevokable() const;
	virtual void updateColor() override;
	virtual void updateVertexesColor();
	virtual void initVertexes();
	virtual void updateVertexes();

	virtual void updateBlendAndDepth();

	virtual RenderingLevel getRealRenderingLevel() const;

	static bool getAutofitParams(Autofit autofit, const Vec2 &autofitValue, const Size2 &contentSize, const Size2 &texSize,
			Rect &contentRect, Rect &textureRect);

	virtual bool checkVertexDirty() const;

	String _textureName;
	Rc<Texture> _texture;
	VertexArray _vertexes;

	uint16_t _samplerIdx = 0;
	bool _flippedX = false;
	bool _flippedY = false;
	bool _rotated = false;
	Rect _textureRect = Rect(0.0f, 0.0f, 1.0f, 1.0f); // normalized

	Autofit _autofit = Autofit::None;
	Vec2 _autofitPos = Vec2(0.5f, 0.5f);

	Vec2 _textureOrigin;
	Size2 _textureSize;
	Extent3 _targetTextureSize;

	RenderingLevel _renderingLevel = RenderingLevel::Default;
	RenderingLevel _realRenderingLevel = RenderingLevel::Default;
	uint64_t _materialId = 0;
	gl::CommandFlags _commandFlags = gl::CommandFlags::None;

	bool _materialDirty = true;
	bool _normalized = false;
	bool _vertexesDirty = true;
	bool _vertexColorDirty = true;

	Color4F _tmpColor;
	ColorMode _colorMode;
	BlendInfo _blendInfo;
	PipelineMaterialInfo _materialInfo;

	Vector<Rc<renderqueue::DependencyEvent>> _pendingDependencies;

};

}

#endif /* XENOLITH_NODES_XLSPRITE_H_ */
