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

#ifndef UTILS_TESS_CANVAS_TESSCANVAS_H_
#define UTILS_TESS_CANVAS_TESSCANVAS_H_

#include "XLLayer.h"
#include "XLLabel.h"
#include "TessPoint.h"

namespace stappler::xenolith::tessapp {

class TessCursor;

class TessCanvas : public Node {
public:
	virtual ~TessCanvas();

	virtual bool init() override;

	virtual void onEnter(Scene *) override;
	virtual void onContentSizeDirty() override;

protected:
	void onTouch(const InputEvent &);
	void onMouseMove(const InputEvent &);

	bool onPointerEnter(bool);

	void onActionTouch(const InputEvent &);

	TessPoint * getTouchedPoint(const Vec2 &) const;

	void updatePoints();

	bool _pointerInWindow = false;
	Vec2 _currentLocation;
	TessCursor *_cursor = nullptr;

	VectorSprite *_test1 = nullptr;
	VectorSprite *_test2 = nullptr;

	Vector<Rc<TessPoint>> _points;

	TessPoint *_capturedPoint = nullptr;
	VectorSprite *_pathFill = nullptr;
	VectorSprite *_pathLines = nullptr;
};

}

#endif /* UTILS_TESS_CANVAS_TESSCANVAS_H_ */
