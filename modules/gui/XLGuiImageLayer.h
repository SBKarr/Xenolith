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

#ifndef MODULES_GUI_XLGUIIMAGELAYER_H_
#define MODULES_GUI_XLGUIIMAGELAYER_H_

#include "XLSprite.h"

namespace stappler::xenolith {

class ImageLayer : public Node {
public:
	static constexpr float GetMaxScaleFactor() { return 1.0f; }

	virtual ~ImageLayer();

	virtual bool init() override;
	virtual void onContentSizeDirty() override;
    virtual void onTransformDirty(const Mat4 &) override;

	virtual void setTexture(Rc<Texture> &&);
	virtual const Rc<Texture> &getTexture() const;

	virtual void setActionCallback(Function<void()> &&);

	virtual Vec2 getTexturePosition() const;

	virtual void setScaleDisabled(bool);

	virtual bool handleTap(Vec2 point, int count);

	virtual bool handleSwipeBegin(Vec2 point);
	virtual bool handleSwipe(Vec2 delta);
	virtual bool handleSwipeEnd(Vec2 velocity);

	virtual bool handlePinch(Vec2 point, float scale, float velocity, bool isEnded);

protected:
	Rect getCorrectRect(Size2 containerSize);
	Vec2 getCorrectPosition(Size2 containerSize, Vec2 point);

	Size2 getContainerSize() const;
	Size2 getContainerSizeForScale(float value) const;

	InputListener *_gestureListener = nullptr;

	Node *_root = nullptr;
	Sprite *_image = nullptr;

	Size2 _prevContentSize;
    Vec2 _globalScale; // values to scale input gestures

	float _minScale = 0.0f;
	float _maxScale = 0.0f;

	float _scaleSource;
	bool _scaleDisabled = false;
	bool _hasPinch = false;
	bool _textureDirty = false;

	Function<void()> _actionCallback;
};

}
#endif /* MODULES_GUI_XLGUIIMAGELAYER_H_ */
