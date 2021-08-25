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
#include "XLGlDrawScheme.h"

namespace stappler::xenolith {

/*bool Sprite::init(const Rc<Pipeline> &p) {
	if (!Node::init()) {
		return false;
	}

	_vertexes.init(4, 6);
	updateVertexes();

	_pipeline = p;
	return true;
}

void Sprite::draw(RenderFrameInfo &frame, NodeFlags flags) {
	if (_pipeline && _pipeline->getPipeline()) {
		frame.scheme->pushVertexArrayCmd(_pipeline->getPipeline(), _vertexes.pop(), frame.transformStack.back(), frame.zPath);
	}
}

void Sprite::setPipeline(const Rc<Pipeline> &pipeline) {
	_pipeline = pipeline;
}*/

void Sprite::updateColor() {
	_vertexes.updateColor(_displayedColor);
}

void Sprite::updateVertexes() {
	_vertexes.clear();
	_vertexes.addQuad()
		.setGeometry(Vec4::ZERO, _contentSize)
		.setTextureRect(_textureRect, 1.0f, 1.0f, _flippedX, _flippedY, _rotated)
		.setColor(_displayedColor);
}

}
