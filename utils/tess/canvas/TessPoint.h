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

#ifndef UTILS_TESS_CANVAS_TESSPOINT_H_
#define UTILS_TESS_CANVAS_TESSPOINT_H_

#include "XLVectorSprite.h"

namespace stappler::xenolith::tessapp {

class TessPoint : public VectorSprite {
public:
	bool init(const Vec2 &p, uint32_t index);

	void setPoint(const Vec2 &);
	const Vec2 &getPoint() const { return _point; }

	void setIndex(uint32_t index);
	uint32_t getIndex() const { return _index; }

protected:
	uint32_t _index = 0;
	Vec2 _point;
	Label *_label = nullptr;
};

}

#endif /* UTILS_TESS_CANVAS_TESSPOINT_H_ */
