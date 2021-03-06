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

#include "XLDefine.h"
#include "XLDefaultShaders.h"

namespace stappler::xenolith::shaders {

#include "compiled/shader_default.frag"
#include "compiled/shader_default.vert"
#include "compiled/shader_vertex.frag"
#include "compiled/shader_vertex.vert"
#include "compiled/shader_material.frag"
#include "compiled/shader_material.vert"

SpanView<uint32_t> DefaultFrag(default_frag, sizeof(default_frag) / sizeof(uint32_t));
SpanView<uint32_t> DefaultVert(default_vert, sizeof(default_vert) / sizeof(uint32_t));

SpanView<uint32_t> VertexFrag(vertex_frag, sizeof(vertex_frag) / sizeof(uint32_t));
SpanView<uint32_t> VertexVert(vertex_vert, sizeof(vertex_vert) / sizeof(uint32_t));

SpanView<uint32_t> MaterialFrag(material_frag, sizeof(material_frag) / sizeof(uint32_t));
SpanView<uint32_t> MaterialVert(material_vert, sizeof(material_vert) / sizeof(uint32_t));

}
