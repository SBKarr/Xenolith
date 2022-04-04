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

#include "XLTessInternal.h"

static void tessGetVertexCount(TESSResultInterface *interface, TESSmesh *mesh, int *faceCount, int *vertexCount ) {
	int maxFaceCount = 0;
	int maxVertexCount = 0;

	// Mark unused
	for ( TESSvertex * v = mesh->vHead.next; v != &mesh->vHead; v = v->next ) {
		v->n = TESS_UNDEF;
	}

	// Create unique IDs for all vertices and faces.
	for ( TESSface * f = mesh->fHead.next; f != &mesh->fHead; f = f->next ) {
		f->n = TESS_UNDEF;
		if( !f->inside ) continue;

		TESShalfEdge * edge = f->anEdge;
		do {
			TESSvertex * v = edge->Org;
			if ( v->n == TESS_UNDEF ) {
				v->n = *vertexCount + maxVertexCount ++;
			}
			edge = edge->Lnext;
		} while (edge != f->anEdge);

		f->n = maxFaceCount ++;
	}

	*faceCount += maxFaceCount;
	*vertexCount += maxVertexCount;
}

int tessExport(TESStesselator *tess, TESSResultInterface *interface) {
	if (!tess->mesh) {
		return 0;
	}

	int maxFaceCount = 0;
	int maxVertexCount = 0;
	TESSmesh *mesh = tess->mesh;

	tess->vertexIndexCounter = 0;
	tess->windingRule = interface->windingRule;

	tessProjectPolygon( tess );

	if (tessComputeInterior( tess ) == 0) {
		return 0; // could've used a label
	}

	mesh = tess->mesh;

	if (tessMeshTessellateInterior( mesh ) == 0) {
		return 0;
	}

	for ( TESSface * f = mesh->fHead.next; f != &mesh->fHead; f = f->next ) {
		if ( !f->inside ) continue;

		//TESSshort values[3] = { 0 };

		// Store polygon
		TESShalfEdge * edge = f->anEdge;
		int faceVerts = 0;
		do {
			//TESSvertex * v = edge->Org;
			//values[faceVerts++] = v->n;
			faceVerts++;
			edge = edge->Lnext;
		} while (edge != f->anEdge);

		printf("%d\n", faceVerts);
	}

	tessGetVertexCount(interface, tess->mesh, &maxFaceCount, &maxVertexCount);


	if (interface->antialiasValue != 0.0f) {
		interface->setVertexCount(interface->target, maxVertexCount * 2, maxVertexCount * 2);

	} else {
		interface->setVertexCount(interface->target, maxVertexCount, maxFaceCount);

		// Output vertices.
		for ( TESSvertex * v = mesh->vHead.next; v != &mesh->vHead; v = v->next ) {
			if ( v->n != TESS_UNDEF ) {
				interface->pushVertex(interface->target, v->n, v->s, v->t, 1.0f);
			}
		}

		// Output indices.
		for ( TESSface * f = mesh->fHead.next; f != &mesh->fHead; f = f->next ) {
			if ( !f->inside ) continue;

			TESSshort values[3] = { 0 };

			// Store polygon
			TESShalfEdge * edge = f->anEdge;
			int faceVerts = 0;
			do {
				TESSvertex * v = edge->Org;
				values[faceVerts++] = v->n;
				edge = edge->Lnext;
			} while (edge != f->anEdge && faceVerts < 3);

			if (faceVerts == 3) {
				interface->pushTriangle(interface->target, values[0], values[1], values[2]);
			}
		}
	}

	return 0;
}
