/*
** SGI FREE SOFTWARE LICENSE B (Version 2.0, Sept. 18, 2008)
** Copyright (C) [dates of first publication] Silicon Graphics, Inc.
** All Rights Reserved.
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
** of the Software, and to permit persons to whom the Software is furnished to do so,
** subject to the following conditions:
**
** The above copyright notice including the dates of first publication and either this
** permission notice or a reference to http://oss.sgi.com/projects/FreeB/ shall be
** included in all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
** INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
** PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL SILICON GRAPHICS, INC.
** BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
** OR OTHER DEALINGS IN THE SOFTWARE.
**
** Except as contained in this notice, the name of Silicon Graphics, Inc. shall not
** be used in advertising or otherwise to promote the sale, use or other dealings in
** this Software without prior written authorization from Silicon Graphics, Inc.
*/
/*
** Author: Mikko Mononen, July 2009.
*/

// Modified for Stappler Layout Engine
// Refactored for Xenolith

#ifndef XENOLITH_NODES_VG_SLTESSELATOR_H_
#define XENOLITH_NODES_VG_SLTESSELATOR_H_

#ifdef __cplusplus
extern "C" {
#endif

// See OpenGL Red Book for description of the winding rules
// http://www.glprogramming.com/red/chapter11.html
enum TessWindingRule {
	TESS_WINDING_ODD,
	TESS_WINDING_NONZERO,
	TESS_WINDING_POSITIVE,
	TESS_WINDING_NEGATIVE,
	TESS_WINDING_ABS_GEQ_TWO,
};

enum TessElementType {
	TESS_POLYGONS,
	TESS_CONNECTED_POLYGONS,
	TESS_BOUNDARY_CONTOURS,
};

typedef float TESSreal;
typedef int TESSindex;
typedef uint16_t TESSshort;
typedef struct TESStesselator TESStesselator;
typedef struct TESSalloc TESSalloc;

typedef struct TESSVec2 {
	TESSreal x;
	TESSreal y;
} TESSVec2;

#define TESS_UNDEF (~(TESSshort)0)

#define TESS_NOTUSED(v) do { (void)(1 ? (void)0 : ( (void)(v) ) ); } while(0)

#ifndef __clang__
#define TESS_OPTIMIZE _Pragma( "GCC optimize (\"O3\")" )
//#define TESS_OPTIMIZE
#else
#define TESS_OPTIMIZE
#endif


struct TESSalloc{
	void *(*memalloc)( void *userData, unsigned int size );
	void (*memfree)( void *userData, void *ptr );
	void* userData;				// User data passed to the allocator functions.
};

// tessNewTess() - Creates a new tesselator.
// Use tessDeleteTess() to delete the tesselator.
// Parameters:
//   alloc - pointer to a filled TESSalloc struct or NULL to use default malloc based allocator.
// Returns:
//   new tesselator object.
TESStesselator* tessNewTess( TESSalloc* alloc );

// tessDeleteTess() - Deletes a tesselator.
// Parameters:
//   tess - pointer to tesselator object to be deleted.
void tessDeleteTess( TESStesselator *tess );

// tessAddContour() - Adds a contour to be tesselated.
// The type of the vertex coordinates is assumed to be TESSreal.
// Parameters:
//   tess - pointer to tesselator object.
//   size - number of coordinates per vertex. Must be 2 or 3.
//   pointer - pointer to the first coordinate of the first vertex in the array.
//   stride - defines offset in bytes between consecutive vertices.
//   count - number of vertices in contour.
void tessAddContour( TESStesselator *tess, const TESSVec2* pointer, int count );

void tessLog(const char *);

typedef struct TESSResultInterface {
	void *target;
	int windingRule;
	TESSreal antialiasValue;
	void (*setVertexCount) (void *, int vertexes, int faces);

	// vertexValue used declares desired opacity for hinted vertexes, for contour vertexes it's always 1.0f
	void (*pushVertex) (void *, TESSindex, TESSreal x, TESSreal y, TESSreal vertexValue);
	void (*pushTriangle) (void *, TESSshort, TESSshort, TESSshort);
} TESSResultInterface;

int tessExport(TESStesselator *, TESSResultInterface *);


#ifdef __cplusplus
};
#endif

#endif // XENOLITH_NODES_VG_SLTESSELATOR_H_
