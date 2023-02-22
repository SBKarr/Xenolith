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

#include "embedded/material.frag"
#include "embedded/material.vert"
#include "embedded/sdf_triangles.comp"
#include "embedded/sdf_circles.comp"
#include "embedded/sdf_rects.comp"
#include "embedded/sdf_rounded_rects.comp"
#include "embedded/sdf_shadows.frag"
#include "embedded/sdf_image.comp"
#include "embedded/shadow_merge.frag"
#include "embedded/shadow_merge_null.frag"
#include "embedded/shadow_merge.vert"

SpanView<uint32_t> MaterialFrag(material_frag, sizeof(material_frag) / sizeof(uint32_t));
SpanView<uint32_t> MaterialVert(material_vert, sizeof(material_vert) / sizeof(uint32_t));
SpanView<uint32_t> SdfTrianglesComp(sdf_triangles_comp, sizeof(sdf_triangles_comp) / sizeof(uint32_t));
SpanView<uint32_t> SdfCirclesComp(sdf_circles_comp, sizeof(sdf_circles_comp) / sizeof(uint32_t));
SpanView<uint32_t> SdfRectsComp(sdf_rects_comp, sizeof(sdf_rects_comp) / sizeof(uint32_t));
SpanView<uint32_t> SdfRoundedRectsComp(sdf_rounded_rects_comp, sizeof(sdf_rounded_rects_comp) / sizeof(uint32_t));
SpanView<uint32_t> SdfShadowsFrag(sdf_shadows_frag, sizeof(sdf_shadows_frag) / sizeof(uint32_t));
SpanView<uint32_t> SdfImageComp(sdf_image_comp, sizeof(sdf_image_comp) / sizeof(uint32_t));
SpanView<uint32_t> ShadowMergeFrag(shadow_merge_frag, sizeof(shadow_merge_frag) / sizeof(uint32_t));
SpanView<uint32_t> ShadowMergeNullFrag(shadow_merge_null_frag, sizeof(shadow_merge_null_frag) / sizeof(uint32_t));
SpanView<uint32_t> ShadowMergeVert(shadow_merge_vert, sizeof(shadow_merge_vert) / sizeof(uint32_t));



}
