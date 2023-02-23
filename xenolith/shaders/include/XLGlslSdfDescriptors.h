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

#if XL_GLSL

layout (set = 0, binding = 0) uniform ShadowDataBuffer {
	ShadowData shadowData;
};

#define INPUT_BUFFER_LAYOUT_SIZE 7
#define OUTPUT_BUFFER_LAYOUT_SIZE 7

layout (set = 0, binding = 1) readonly buffer TriangleIndexes {
	Triangle2DIndex indexes[];
} indexBuffer[INPUT_BUFFER_LAYOUT_SIZE];

layout (set = 0, binding = 1) buffer Vertices {
	vec4 vertices[];
} vertexBuffer[INPUT_BUFFER_LAYOUT_SIZE];

layout (set = 0, binding = 1) readonly buffer TransformObjects {
	TransformObject transforms[];
} transformObjectBuffer[INPUT_BUFFER_LAYOUT_SIZE];

layout (set = 0, binding = 1) readonly buffer CircleIndexes {
	Circle2DIndex circles[];
} circleIndexBuffer[INPUT_BUFFER_LAYOUT_SIZE];

layout (set = 0, binding = 1) readonly buffer RectIndexes {
	Rect2DIndex rects[];
} rectIndexBuffer[INPUT_BUFFER_LAYOUT_SIZE];

layout (set = 0, binding = 1) readonly buffer RoundedRectIndexes {
	RoundedRect2DIndex rects[];
} roundedRectIndexBuffer[INPUT_BUFFER_LAYOUT_SIZE];

layout (set = 0, binding = 1) readonly buffer PolygonIndexes {
	Polygon2DIndex polygons[];
} polygonIndexBuffer[INPUT_BUFFER_LAYOUT_SIZE];

#define TRIANGLE_INDEX_BUFFER indexBuffer[0].indexes
#define CIRCLE_INDEX_BUFFER circleIndexBuffer[3].circles
#define RECT_INDEX_BUFFER rectIndexBuffer[4].rects
#define ROUNDED_RECT_INDEX_BUFFER roundedRectIndexBuffer[5].rects
#define POLYGON_INDEX_BUFFER polygonIndexBuffer[6].polygons
#define VERTEX_BUFFER vertexBuffer[1].vertices
#define TRANSFORM_BUFFER transformObjectBuffer[2].transforms

layout(set = 0, binding = 2) buffer TrianglesBuffer {
	Triangle2DData triangles[];
} trianglesBuffer[OUTPUT_BUFFER_LAYOUT_SIZE];

layout(set = 0, binding = 2) buffer GridSizeBuffer {
	uint grid[];
} gridSizeBuffer[OUTPUT_BUFFER_LAYOUT_SIZE];

layout(set = 0, binding = 2) buffer GridIndexesBuffer {
	uint index[];
} gridIndexBuffer[OUTPUT_BUFFER_LAYOUT_SIZE];

layout(set = 0, binding = 2) buffer CirclesBuffer {
	Circle2DData circles[];
} circlesBuffer[OUTPUT_BUFFER_LAYOUT_SIZE];

layout(set = 0, binding = 2) buffer RectsBuffer {
	Rect2DData rects[];
} rectsBuffer[OUTPUT_BUFFER_LAYOUT_SIZE];

layout(set = 0, binding = 2) buffer RoundedRectsBuffer {
	RoundedRect2DData rects[];
} roundedRectsBuffer[OUTPUT_BUFFER_LAYOUT_SIZE];

layout(set = 0, binding = 2) buffer PolygonBuffer {
	Polygon2DData polygons[];
} polygonBuffer[OUTPUT_BUFFER_LAYOUT_SIZE];

#define TRIANGLE_DATA_BUFFER trianglesBuffer[0].triangles
#define CIRCLE_DATA_BUFFER circlesBuffer[3].circles
#define RECT_DATA_BUFFER rectsBuffer[4].rects
#define ROUNDED_RECT_DATA_BUFFER roundedRectsBuffer[5].rects
#define POLYGON_DATA_BUFFER polygonBuffer[6].polygons
#define GRID_SIZE_BUFFER gridSizeBuffer[1].grid
#define GRID_INDEX_BUFFER gridIndexBuffer[2].index

#endif
