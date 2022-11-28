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

#ifndef XENOLITH_NODES_XLLABEL_H_
#define XENOLITH_NODES_XLLABEL_H_

#include "XLSprite.h"
#include "XLLabelParameters.h"
#include <future>

namespace stappler::xenolith {

class EventListener;

struct LabelResult : Ref {
	gl::TransformedVertexData data;
	Vector<ColorMask> colorMap;
};

class LabelDeferredResult : public gl::DeferredVertexResult {
public:
	virtual ~LabelDeferredResult();

	bool init(std::future<Rc<LabelResult>> &&);

	virtual SpanView<gl::TransformedVertexData> getData() override;

	virtual void handleReady(Rc<LabelResult> &&);

	void updateColor(const Color4F &);

	Rc<gl::VertexData> getResult() const;

protected:
	mutable Mutex _mutex;
	Rc<LabelResult> _result;
	std::future<Rc<LabelResult>> *_future = nullptr;
};

class Label : public Sprite, public LabelParameters {
public:
	using ColorMapVec = Vector<Vector<bool>>;

	static void writeQuads(VertexArray &vertexes, FormatSpec *format, Vector<ColorMask> &colorMap);
	static Rc<LabelResult> writeResult(FormatSpec *format, const Color4F &);

	virtual ~Label();

	virtual bool init() override;
	virtual bool init(StringView) override;
	virtual bool init(StringView, float w, Alignment = Alignment::Left);
	virtual bool init(font::FontController *, const DescriptionStyle & = DescriptionStyle(),
			StringView = StringView(), float w = 0.0f, Alignment = Alignment::Left);
	virtual bool init(const DescriptionStyle &, StringView = StringView(),
			float w = 0.0f, Alignment = Alignment::Left);

	virtual void tryUpdateLabel();

	virtual void setStyle(const DescriptionStyle &);
	virtual const DescriptionStyle &getStyle() const;

	virtual void onTransformDirty(const Mat4 &) override;
	virtual void onGlobalTransformDirty(const Mat4 &) override;

	/** Standalone labels use its own textures and char-to-texture maps
	 * so, they can be rendered without delays */
	virtual void setStandalone(bool value);
	virtual bool isStandalone() const;

	virtual void setAdjustValue(uint8_t);
	virtual uint8_t getAdjustValue() const;

	virtual bool isOverflow() const;

	virtual size_t getCharsCount() const;
	virtual size_t getLinesCount() const;
	virtual LineSpec getLine(uint32_t num) const;

	virtual uint16_t getFontHeight() const;

	virtual Vec2 getCursorPosition(uint32_t charIndex, bool prefix = true) const;
	virtual Vec2 getCursorOrigin() const;

	// returns character index in FormatSpec for position in label or maxOf<uint32_t>()
	// pair.second - true if index match suffix or false if index match prefix
	// use convertToNodeSpace to get position
	virtual Pair<uint32_t, bool> getCharIndex(const Vec2 &) const;

	float getMaxLineX() const;

	virtual void setDeferred(bool);
	virtual bool isDeferred() const { return _deferred; }

protected:
	using Sprite::init;

	virtual void updateLabel();
	virtual void onFontSourceUpdated();
	virtual void onFontSourceLoaded();
	virtual void onLayoutUpdated();
	virtual void updateColor() override;
	virtual void updateVertexes() override;
	virtual void updateVertexesColor() override;

	virtual void updateQuadsForeground(font::FontController *, FormatSpec *, Vector<ColorMask> &);

	virtual bool checkVertexDirty() const override;

	virtual NodeFlags processParentFlags(RenderFrameInfo &info, NodeFlags parentFlags) override;

	virtual void pushCommands(RenderFrameInfo &, NodeFlags flags);

	void updateLabelScale(const Mat4 &parent);

	EventListener *_listener = nullptr;
	Time _quadRequestTime;
	Rc<font::FontController> _source;
	Rc<FormatSpec> _format;
	Vector<ColorMask> _colorMap;

	bool _standalone = false;
	bool _deferred = true;

	float _density = 1.0f;

	uint8_t _adjustValue = 0;
	size_t _updateCount = 0;

	Rc<LabelDeferredResult> _deferredResult;

	/*Map<String, Vector<char16_t>> _standaloneChars;
	Vector<Rc<cocos2d::Texture2D>> _standaloneTextures;
	FontTextureMap _standaloneMap;*/
};

}

#endif /* XENOLITH_NODES_XLLABEL_H_ */
