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
** Author: Eric Veach, July 1994.
*/

#include "XLTessInternal.h"

void tessProjectPolygon(TESStesselator *tess) {
	TESSvertex *v, *vHead = &tess->mesh->vHead;

	v = vHead->next;
	tess->bmin[0] = tess->bmax[0] = v->s;
	tess->bmin[1] = tess->bmax[1] = v->t;
	v = v->next;

	for (; v != vHead; v = v->next) {
		if (v->s < tess->bmin[0]) tess->bmin[0] = v->s;
		if (v->s > tess->bmax[0]) tess->bmax[0] = v->s;
		if (v->t < tess->bmin[1]) tess->bmin[1] = v->t;
		if (v->t > tess->bmax[1]) tess->bmax[1] = v->t;
	}
}

#define AddWinding(eDst,eSrc)	(eDst->winding += eSrc->winding, \
	eDst->Sym->winding += eSrc->Sym->winding)

/* tessMeshTessellateMonoRegion( face ) tessellates a monotone region
* (what else would it do??)  The region must consist of a single
* loop of half-edges (see mesh.h) oriented CCW.  "Monotone" in this
* case means that any vertical line intersects the interior of the
* region in a single interval.
*
* Tessellation consists of adding interior edges (actually pairs of
* half-edges), to split the region into non-overlapping triangles.
*
* The basic idea is explained in Preparata and Shamos (which I don''t
* have handy right now), although their implementation is more
* complicated than this one.  The are two edge chains, an upper chain
* and a lower chain.  We process all vertices from both chains in order,
* from right to left.
*
* The algorithm ensures that the following invariant holds after each
* vertex is processed: the untessellated region consists of two
* chains, where one chain (say the upper) is a single edge, and
* the other chain is concave.  The left vertex of the single edge
* is always to the left of all vertices in the concave chain.
*
* Each step consists of adding the rightmost unprocessed vertex to one
* of the two chains, and forming a fan of triangles from the rightmost
* of two chain endpoints.  Determining whether we can add each triangle
* to the fan is a simple orientation test.  By making the fan as large
* as possible, we restore the invariant (check it yourself).
*/
int tessMeshTessellateMonoRegion( TESSmesh *mesh, TESSface *face )
{
	TESShalfEdge *up, *lo;

	/* All edges are oriented CCW around the boundary of the region.
	* First, find the half-edge whose origin vertex is rightmost.
	* Since the sweep goes from left to right, face->anEdge should
	* be close to the edge we want.
	*/
	up = face->anEdge;
	assert( up->Lnext != up && up->Lnext->Lnext != up );

	for( ; VertLeq( up->Dst, up->Org ); up = up->Lprev )
		;
	for( ; VertLeq( up->Org, up->Dst ); up = up->Lnext )
		;
	lo = up->Lprev;

	while( up->Lnext != lo ) {
		if( VertLeq( up->Dst, lo->Org )) {
			/* up->Dst is on the left.  It is safe to form triangles from lo->Org.
			* The EdgeGoesLeft test guarantees progress even when some triangles
			* are CW, given that the upper and lower chains are truly monotone.
			*/
			while( lo->Lnext != up && (EdgeGoesLeft( lo->Lnext )
				|| EdgeSign( lo->Org, lo->Dst, lo->Lnext->Dst ) <= 0 )) {
					TESShalfEdge *tempHalfEdge= tessMeshConnect( mesh, lo->Lnext, lo );
					if (tempHalfEdge == NULL) return 0;
					lo = tempHalfEdge->Sym;
			}
			lo = lo->Lprev;
		} else {
			/* lo->Org is on the left.  We can make CCW triangles from up->Dst. */
			while( lo->Lnext != up && (EdgeGoesRight( up->Lprev )
				|| EdgeSign( up->Dst, up->Org, up->Lprev->Org ) >= 0 )) {
					TESShalfEdge *tempHalfEdge= tessMeshConnect( mesh, up, up->Lprev );
					if (tempHalfEdge == NULL) return 0;
					up = tempHalfEdge->Sym;
			}
			up = up->Lnext;
		}
	}

	/* Now lo->Org == up->Dst == the leftmost vertex.  The remaining region
	* can be tessellated in a fan from this leftmost vertex.
	*/
	assert( lo->Lnext != up );
	while( lo->Lnext->Lnext != up ) {
		TESShalfEdge *tempHalfEdge= tessMeshConnect( mesh, lo->Lnext, lo );
		if (tempHalfEdge == NULL) return 0;
		lo = tempHalfEdge->Sym;
	}

	return 1;
}

/* tessMeshTessellateInterior( mesh ) tessellates each region of
* the mesh which is marked "inside" the polygon.  Each such region
* must be monotone.
*/
int tessMeshTessellateInterior( TESSmesh *mesh )
{
	TESSface *f, *next;

	/*LINTED*/
	for( f = mesh->fHead.next; f != &mesh->fHead; f = next ) {
		/* Make sure we don''t try to tessellate the new triangles. */
		next = f->next;
		if( f->inside ) {
			if ( !tessMeshTessellateMonoRegion( mesh, f ) ) return 0;
		}
	}
	return 1;
}


typedef struct EdgeStackNode EdgeStackNode;
typedef struct EdgeStack EdgeStack;

struct EdgeStackNode {
	TESShalfEdge *edge;
	EdgeStackNode *next;
};

struct EdgeStack {
	EdgeStackNode *top;
	EdgeStackNode *free;
	TESSalloc *alloc;
};

void stackInit( EdgeStack *stack, TESSalloc *alloc ) {
	stack->top = NULL;
	stack->free = NULL;
	stack->alloc = alloc;
}

int stackEmpty( EdgeStack *stack ) {
	return stack->top == NULL;
}

void stackPush( EdgeStack *stack, TESShalfEdge *e ) {
	EdgeStackNode *node;
	if (stack->free) {
		node = stack->free;
		stack->free = node->next;
	} else {
		node = stack->alloc->memalloc(stack->alloc->userData, sizeof(EdgeStackNode));
	}

	node->edge = e;
	node->next = stack->top;
	stack->top = node;
}

TESShalfEdge *stackPop( EdgeStack *stack )
{
	TESShalfEdge *e = NULL;
	EdgeStackNode *node = stack->top;
	if (node) {
		stack->top = node->next;
		e = node->edge;

		node->next = stack->free;
		stack->free = node;
	}
	return e;
}


//	Starting with a valid triangulation, uses the Edge Flip algorithm to
//	refine the triangulation into a Constrained Delaunay Triangulation.
void tessMeshRefineDelaunay( TESSmesh *mesh, TESSalloc *alloc )
{
	// At this point, we have a valid, but not optimal, triangulation.
	// We refine the triangulation using the Edge Flip algorithm
	//
	//  1) Find all internal edges
	//	2) Mark all dual edges
	//	3) insert all dual edges into a queue

	TESSface *f;
	EdgeStack stack;
	TESShalfEdge *e;
	int maxFaces = 0, maxIter = 0, iter = 0;

	stackInit(&stack, alloc);

	for( f = mesh->fHead.next; f != &mesh->fHead; f = f->next ) {
		if ( f->inside) {
			e = f->anEdge;
			do {
				e->mark = EdgeIsInternal(e); // Mark internal edges
				if (e->mark && !e->Sym->mark) stackPush(&stack, e); // Insert into queue
				e = e->Lnext;
			} while (e != f->anEdge);
			maxFaces++;
		}
	}

	// The algorithm should converge on O(n^2), since the predicate is not robust,
	// we'll save guard against infinite loop.
	maxIter = maxFaces * maxFaces;

	// Pop stack until we find a reversed edge
	// Flip the reversed edge, and insert any of the four opposite edges
	// which are internal and not already in the stack (!marked)
	while (!stackEmpty(&stack) && iter < maxIter) {
		e = stackPop(&stack);
		e->mark = e->Sym->mark = 0;
		if (!tesedgeIsLocallyDelaunay(e)) {
			TESShalfEdge *edges[4];
			int i;
			tessMeshFlipEdge(mesh, e);
			// for each opposite edge
			edges[0] = e->Lnext;
			edges[1] = e->Lprev;
			edges[2] = e->Sym->Lnext;
			edges[3] = e->Sym->Lprev;
			for (i = 0; i < 4; i++) {
				if (!edges[i]->mark && EdgeIsInternal(edges[i])) {
					edges[i]->mark = edges[i]->Sym->mark = 1;
					stackPush(&stack, edges[i]);
				}
			}
		}
		iter++;
	}
}


/* tessMeshDiscardExterior( mesh ) zaps (ie. sets to NULL) all faces
* which are not marked "inside" the polygon.  Since further mesh operations
* on NULL faces are not allowed, the main purpose is to clean up the
* mesh so that exterior loops are not represented in the data structure.
*/
void tessMeshDiscardExterior( TESSmesh *mesh )
{
	TESSface *f, *next;

	/*LINTED*/
	for( f = mesh->fHead.next; f != &mesh->fHead; f = next ) {
		/* Since f will be destroyed, save its next pointer. */
		next = f->next;
		if( ! f->inside ) {
			tessMeshZapFace( mesh, f );
		}
	}
}

/* tessMeshSetWindingNumber( mesh, value, keepOnlyBoundary ) resets the
* winding numbers on all edges so that regions marked "inside" the
* polygon have a winding number of "value", and regions outside
* have a winding number of 0.
*
* If keepOnlyBoundary is TRUE, it also deletes all edges which do not
* separate an interior region from an exterior one.
*/
int tessMeshSetWindingNumber( TESSmesh *mesh, int value,
							 int keepOnlyBoundary )
{
	TESShalfEdge *e, *eNext;

	for( e = mesh->eHead.next; e != &mesh->eHead; e = eNext ) {
		eNext = e->next;
		if( e->Rface->inside != e->Lface->inside ) {

			/* This is a boundary edge (one side is interior, one is exterior). */
			e->winding = (e->Lface->inside) ? value : -value;
		} else {

			/* Both regions are interior, or both are exterior. */
			if( ! keepOnlyBoundary ) {
				e->winding = 0;
			} else {
				if ( !tessMeshDelete( mesh, e ) ) return 0;
			}
		}
	}
	return 1;
}

void* heapAlloc(void* userData, unsigned int size) {
	TESS_NOTUSED( userData );
	return malloc( size );
}

void* heapRealloc(void *userData, void* ptr, unsigned int size) {
	TESS_NOTUSED( userData );
	return realloc( ptr, size );
}

void heapFree( void* userData, void* ptr ) {
	TESS_NOTUSED( userData );
	free( ptr );
}

static TESSalloc defaulAlloc = { heapAlloc, heapFree, 0 };

TESStesselator* tessNewTess(TESSalloc* alloc) {
	TESStesselator* tess;

	if (alloc == NULL)
		alloc = &defaulAlloc;

	/* Only initialize fields which can be changed by the api.  Other fields
	* are initialized where they are used.
	*/

	tess = (TESStesselator *)alloc->memalloc( alloc->userData, sizeof( TESStesselator ));
	if ( tess == NULL ) {
		return 0;          /* out of memory */
	}

	memset(tess, 0, sizeof(TESStesselator));
	tess->alloc = *alloc;
	return tess;
}

void tessDeleteTess(TESStesselator *tess) {
	struct TESSalloc alloc = tess->alloc;
	alloc.memfree( alloc.userData, tess );
}

void tessAddContour( TESStesselator *tess, const TESSVec2* vertices, int numVertices ) {
	TESShalfEdge *e = NULL;

	if ( tess->mesh == NULL )
	  	tess->mesh = tessMeshNewMesh( &tess->alloc );

 	if ( tess->mesh == NULL ) {
		tess->outOfMemory = 1;
		return;
	}

	for (int i = 0; i < numVertices; ++i) {
		if( e == NULL ) {
			/* Make a self-loop (one vertex, one edge). */
			e = tessMeshMakeEdge( tess->mesh );
			if ( e == NULL ) {
				tess->outOfMemory = 1;
				return;
			}
			if ( !tessMeshSplice( tess->mesh, e, e->Sym ) ) {
				tess->outOfMemory = 1;
				return;
			}
		} else {
			/* Create a new vertex and edge which immediately follow e
			* in the ordering around the left face.
			*/
			if ( tessMeshSplitEdge( tess->mesh, e ) == NULL ) {
				tess->outOfMemory = 1;
				return;
			}
			e = e->Lnext;
		}

		/* The new vertex is now e->Org. */
		e->Org->s = vertices->x;
		e->Org->t = vertices->y;
		/* Store the insertion number so that the vertex can be later recognized. */
		e->Org->idx = tess->vertexIndexCounter++;

		/* The winding of an edge says how the winding number changes as we
		* cross from the edge''s right face to its left face.  We add the
		* vertices in such an order that a CCW contour will add +1 to
		* the winding number of the region inside the contour.
		*/
        e->winding = tess->reverseContours ? -1 : 1;
        e->Sym->winding = tess->reverseContours ? 1 : -1;

        ++ vertices;
	}
}
