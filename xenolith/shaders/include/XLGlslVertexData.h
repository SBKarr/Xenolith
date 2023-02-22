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

#include "XLGlslCompatibility.h"

#ifndef XL_GLSL
namespace stappler::xenolith::gl::glsl {
#endif

struct Vertex {
	vec4 pos;
	vec4 color;
	vec2 tex;
	uint material;
	uint object;
};

struct Material {
	uint samplerIdx;
	uint imageIdx;
	uint setIdx;
	uint padding0;
};

struct TransformObject {
	mat4 transform;
	vec4 mask;
	vec4 offset;
	vec4 shadow;
	vec4 padding;

#ifndef XL_GLSL
	TransformObject() :
	transform(mat4::IDENTITY),
	mask(vec4(1.0f, 1.0f, 0.0f, 0.0f)),
	offset(vec4(0.0f, 0.0f, 0.0f, 1.0f)),
	shadow(vec4::ZERO)
	{ }

	TransformObject(const mat4 &m) :
	transform(m),
	mask(vec4(1.0f, 1.0f, 0.0f, 0.0f)),
	offset(vec4(0.0f, 0.0f, 0.0f, 1.0f)),
	shadow(vec4::ZERO)
	{ }
#endif
};

#ifndef XL_GLSL
}
#endif
