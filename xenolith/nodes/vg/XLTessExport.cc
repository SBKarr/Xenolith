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
		v->pqHandle = 0;
	}

	// Create unique IDs for all vertices and faces.
	for ( TESSface * f = mesh->fHead.next; f != &mesh->fHead; f = f->next ) {
		f->n = TESS_UNDEF;
		if( !f->inside ) continue;

		TESShalfEdge * edge = f->anEdge;
		do {
			if (edge->sub) {
				maxFaceCount += 2;
				if ( edge->sub->n[0] == TESS_UNDEF ) {
					edge->sub->n[0] = *vertexCount + maxVertexCount ++;
				}
				if ( edge->sub->n[1] == TESS_UNDEF ) {
					edge->sub->n[1] = *vertexCount + maxVertexCount ++;
				}
			} else {
				TESSvertex * v = edge->Org;
				if ( v->n == TESS_UNDEF ) {
					v->n = *vertexCount + maxVertexCount ++;
				}
			}
			edge = edge->Lnext;
		} while (edge != f->anEdge);

		f->n = maxFaceCount ++;
	}

	*faceCount += maxFaceCount;
	*vertexCount += maxVertexCount;
}

static inline float RSqrt(float value) {
	return 1.0f / sqrtf(value);
}

static void displaceEdgeAntialias(TESSsubvertex *vertex, TESSvertex * v0, TESSvertex * v1, TESSvertex * v2, TESSreal value) {
	int isCcw = tesvertCCW(v0, v1, v2);

	const float cx = v1->s;
	const float cy = v1->t;

	const float x0 = v0->s - cx;
	const float y0 = v0->t - cy;
	const float x1 = v2->s - cx;
	const float y1 = v2->t - cy;

	const float n0 = RSqrt(x0 * x0 + y0 * y0);
	const float n1 = RSqrt(x0 * x0 + y0 * y0);

	const float tx = x0 * n0 + x1 * n1;
	const float ty = y0 * n0 + y1 * n1;

	const float nt = RSqrt(tx * tx + ty * ty);

	const float ntx = tx * nt * (isCcw ? -1.0f : 1.0f) * value;
	const float nty = ty * nt * (isCcw ? -1.0f : 1.0f) * value;

	vertex->inside[0] = cx - ntx;
	vertex->inside[1] = cy - nty;

	vertex->outside[0] = cx + ntx;
	vertex->outside[1] = cy + nty;
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

	for ( TESShalfEdge * v = mesh->eHead.next; v != &mesh->eHead; v = v->next ) {
		printf("Edge1 %p: %f %f -> %f %f\n", v, v->Org->s, v->Org->t, v->Dst->s, v->Dst->t );
	}

	if (tessComputeInterior( tess ) == 0) {
		return 0; // could've used a label
	}

	mesh = tess->mesh;

	for ( TESShalfEdge * v = mesh->eHead.next; v != &mesh->eHead; v = v->next ) {
		printf("Edge2 %p: %f %f -> %f %f\n", v, v->Org->s, v->Org->t, v->Dst->s, v->Dst->t );
	}

	for ( TESSvertex * v = mesh->vHead.next; v != &mesh->vHead; v = v->next ) {
		TESShalfEdge * tmp = NULL;
		TESShalfEdge * e = v->anEdge;

		if (e->mark == TESS_SENTINEL) {
			continue;
		}

		printf("Vertex %p: %f %f\n", v, v->s, v->t);

		while (e->Rface->inside == e->Lface->inside) {
			e = e->Onext;
		}
		tmp = e;
		e = e->Onext;

		TESShalfEdge * end = e;
		do {
			if (e->Rface->inside != e->Lface->inside) {
				if (tmp->Lface->inside && e->Rface->inside) {
					printf("\tInside: %p %d: %f %f -> %f %f -> %f %f\n",  e, e->Rface->inside != e->Lface->inside,
							tmp->Sym->Org->s, tmp->Sym->Org->t, v->s, v->t, e->Sym->Org->s, e->Sym->Org->t);
				} else {
					printf("\tOutside: %p %d: %f %f -> %f %f -> %f %f\n",  e, e->Rface->inside != e->Lface->inside,
							tmp->Sym->Org->s, tmp->Sym->Org->t, v->s, v->t, e->Sym->Org->s, e->Sym->Org->t);
				}
				tmp = e;
			}
			e = e->Onext;
		} while (e != end);
	}

	if (interface->antialiasValue != 0.0f) {
		for ( TESSface * f = mesh->fHead.next; f != &mesh->fHead; f = f->next ) {
			if ( !f->inside ) continue;

			// Store polygon
			TESShalfEdge * edge = f->anEdge;
			TESShalfEdge * prev = edge->Rnext;
			TESShalfEdge * next;
			do {
				next = edge->Lnext;
				if( edge->Rface->inside != edge->Lface->inside ) {
					if ( !edge->sub ) {
						edge->sub = tess->alloc.memalloc(tess->alloc.userData, sizeof(TESSsubvertex));
						edge->sub->next = NULL;
						edge->sub->prev = NULL;
						edge->sub->n[0] = TESS_UNDEF;
						edge->sub->n[1] = TESS_UNDEF;
						edge->sub->sended = 0;

						if (prev->sub) {
							edge->sub->prev = prev->sub;
							prev->sub->next = edge->sub;
						}

						if (next->sub) {
							edge->sub->next = next->sub;
							next->sub->prev = edge->sub;
						}
					}

					displaceEdgeAntialias(edge->sub, prev->Org, edge->Org, next->Org, interface->antialiasValue);
				}
				prev = edge;
				edge = next;
			} while (edge != f->anEdge);
		}
	}

	if (tessMeshTessellateInterior( mesh ) == 0) {
		return 0;
	}

	tessGetVertexCount(interface, tess->mesh, &maxFaceCount, &maxVertexCount);

	interface->setVertexCount(interface->target, maxVertexCount, maxFaceCount);

	for ( TESSface * f = mesh->fHead.next; f != &mesh->fHead; f = f->next ) {
		if ( !f->inside ) continue;

		TESSshort values[3] = { 0 };

		// Store polygon
		TESShalfEdge * edge = f->anEdge;
		int faceVerts = 0;
		do {
			if (edge->sub) {
				if (!edge->sub->sended) {
					TESSsubvertex *sub = edge->sub;
					interface->pushVertex(interface->target, sub->n[0], sub->inside[0], sub->inside[1], 1.0f);
					interface->pushVertex(interface->target, sub->n[1], sub->outside[0], sub->outside[1], 0.0f);
					sub->sended = 1;

					interface->pushTriangle(interface->target, sub->n[0], sub->next->n[1], sub->n[1]);
					interface->pushTriangle(interface->target, sub->n[0], sub->next->n[0], sub->next->n[1]);
				}
				values[faceVerts++] = edge->sub->n[0];
			} else {
				printf("FaceEdge: %p: %p %f %f\n", f, edge, edge->Org->s, edge->Org->t);
				TESSvertex * v = edge->Org;
				if (!v->pqHandle) {
					interface->pushVertex(interface->target, v->n, v->s, v->t, 1.0f);
					v->pqHandle = 1;
				}
				values[faceVerts++] = v->n;
			}
			edge = edge->Lnext;
		} while (edge != f->anEdge && faceVerts < 3);

		if (faceVerts == 3) {
			interface->pushTriangle(interface->target, values[0], values[1], values[2]);
		}
	}

	return 0;
}
