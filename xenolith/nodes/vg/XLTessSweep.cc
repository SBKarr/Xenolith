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

#ifdef FOR_TRITE_TEST_PROGRAM
extern void DebugEvent( TESStesselator *tess );
#else
#define DebugEvent( tess )
#endif

/*
* Invariants for the Edge Dictionary.
* - each pair of adjacent edges e2=Succ(e1) satisfies EdgeLeq(e1,e2)
*   at any valid location of the sweep event
* - if EdgeLeq(e2,e1) as well (at any valid sweep event), then e1 and e2
*   share a common endpoint
* - for each e, e->Dst has been processed, but not e->Org
* - each edge e satisfies VertLeq(e->Dst,event) && VertLeq(event,e->Org)
*   where "event" is the current sweep line event.
* - no edge e has zero length
*
* Invariants for the Mesh (the processed portion).
* - the portion of the mesh left of the sweep line is a planar graph,
*   ie. there is *some* way to embed it in the plane
* - no processed edge has zero length
* - no two processed vertices have identical coordinates
* - each "inside" region is monotone, ie. can be broken into two chains
*   of monotonically increasing vertices according to VertLeq(v1,v2)
*   - a non-invariant: these chains may intersect (very slightly)
*
* Invariants for the Sweep.
* - if none of the edges incident to the event vertex have an activeRegion
*   (ie. none of these edges are in the edge dictionary), then the vertex
*   has only right-going edges.
* - if an edge is marked "fixUpperEdge" (it is a temporary edge introduced
*   by ConnectRightVertex), then it is the only right-going edge from
*   its associated vertex.  (This says that these edges exist only
*   when it is necessary.)
*/

#define MAX(x,y)	((x) >= (y) ? (x) : (y))
#define MIN(x,y)	((x) <= (y) ? (x) : (y))

/* When we merge two edges into one, we need to compute the combined
* winding of the new edge.
*/
#define AddWinding(eDst,eSrc)	(eDst->winding += eSrc->winding, \
	eDst->Sym->winding += eSrc->Sym->winding)

static int SweepEvent( TESStesselator *tess, TESSvertex *vEvent );
static int WalkDirtyRegions( TESStesselator *tess, ActiveRegion *regUp );
static int CheckForRightSplice( TESStesselator *tess, ActiveRegion *regUp );

static int EdgeLeq( TESStesselator *tess, ActiveRegion *reg1, ActiveRegion *reg2 )
/*
* Both edges must be directed from right to left (this is the canonical
* direction for the upper edge of each region).
*
* The strategy is to evaluate a "t" value for each edge at the
* current sweep line position, given by tess->event.  The calculations
* are designed to be very stable, but of course they are not perfect.
*
* Special case: if both edge destinations are at the sweep event,
* we sort the edges by slope (they would otherwise compare equally).
*/
{
	TESSvertex *event = tess->event;
	TESShalfEdge *e1, *e2;
	TESSreal t1, t2;

	e1 = reg1->eUp;
	e2 = reg2->eUp;

	if( e1->Dst == event ) {
		if( e2->Dst == event ) {
			/* Two edges right of the sweep line which meet at the sweep event.
			* Sort them by slope.
			*/
			if( VertLeq( e1->Org, e2->Org )) {
				return EdgeSign( e2->Dst, e1->Org, e2->Org ) <= 0;
			}
			return EdgeSign( e1->Dst, e2->Org, e1->Org ) >= 0;
		}
		return EdgeSign( e2->Dst, event, e2->Org ) <= 0;
	}
	if( e2->Dst == event ) {
		return EdgeSign( e1->Dst, event, e1->Org ) >= 0;
	}

	/* General case - compute signed distance *from* e1, e2 to event */
	t1 = EdgeEval( e1->Dst, event, e1->Org );
	t2 = EdgeEval( e2->Dst, event, e2->Org );
	return (t1 >= t2);
}


static void DeleteRegion( TESStesselator *tess, ActiveRegion *reg )
{
	if( reg->fixUpperEdge ) {
		/* It was created with zero winding number, so it better be
		* deleted with zero winding number (ie. it better not get merged
		* with a real edge).
		*/
		TESSassert( reg->eUp->winding == 0, "DeleteRegion");
	}
	reg->eUp->activeRegion = NULL;
	dictDelete( tess->dict, reg->nodeUp );
}


static int FixUpperEdge( TESStesselator *tess, ActiveRegion *reg, TESShalfEdge *newEdge )
/*
* Replace an upper edge which needs fixing (see ConnectRightVertex).
*/
{
	TESSassert( reg->fixUpperEdge, "FixUpperEdge");
	if ( !tessMeshDelete( tess->mesh, reg->eUp ) ) return 0;
	reg->fixUpperEdge = FALSE;
	reg->eUp = newEdge;
	newEdge->activeRegion = reg;

	return 1;
}

static ActiveRegion *TopLeftRegion( TESStesselator *tess, ActiveRegion *reg )
{
	TESSvertex *org = reg->eUp->Org;
	TESShalfEdge *e;

	/* Find the region above the uppermost edge with the same origin */
	do {
		reg = RegionAbove( reg );
	} while( reg->eUp->Org == org );

	/* If the edge above was a temporary edge introduced by ConnectRightVertex,
	* now is the time to fix it.
	*/
	if( reg->fixUpperEdge ) {
		e = tessMeshConnect( tess->mesh, RegionBelow(reg)->eUp->Sym, reg->eUp->Lnext );
		if (e == NULL) return NULL;
		if ( !FixUpperEdge( tess, reg, e ) ) return NULL;
		reg = RegionAbove( reg );
	}
	return reg;
}

static ActiveRegion *TopRightRegion( ActiveRegion *reg )
{
	TESSvertex *dst = reg->eUp->Dst;

	/* Find the region above the uppermost edge with the same destination */
	do {
		reg = RegionAbove( reg );
	} while( reg->eUp->Dst == dst );
	return reg;
}

static ActiveRegion *AddRegionBelow( TESStesselator *tess,
									ActiveRegion *regAbove,
									TESShalfEdge *eNewUp )
/*
* Add a new active region to the sweep line, *somewhere* below "regAbove"
* (according to where the new edge belongs in the sweep-line dictionary).
* The upper edge of the new region will be "eNewUp".
* Winding number and "inside" flag are not updated.
*/
{
	ActiveRegion *regNew = (ActiveRegion *)tess->alloc.memalloc(tess->alloc.userData, sizeof(ActiveRegion));
	if (regNew == NULL) return NULL;

	regNew->eUp = eNewUp;
	regNew->nodeUp = dictInsertBefore( tess->dict, regAbove->nodeUp, regNew );
	if (regNew->nodeUp == NULL) return NULL;
	regNew->fixUpperEdge = FALSE;
	regNew->sentinel = FALSE;
	regNew->dirty = FALSE;

	eNewUp->activeRegion = regNew;
	return regNew;
}

static int IsWindingInside( TESStesselator *tess, int n )
{
	switch( tess->windingRule ) {
		case TESS_WINDING_ODD:
			return (n & 1);
		case TESS_WINDING_NONZERO:
			return (n != 0);
		case TESS_WINDING_POSITIVE:
			return (n > 0);
		case TESS_WINDING_NEGATIVE:
			return (n < 0);
		case TESS_WINDING_ABS_GEQ_TWO:
			return (n >= 2) || (n <= -2);
	}
	/*LINTED*/
	TESSassert( FALSE, "IsWindingInside" );
	/*NOTREACHED*/

	return( FALSE );
}


static void ComputeWinding( TESStesselator *tess, ActiveRegion *reg )
{
	reg->windingNumber = RegionAbove(reg)->windingNumber + reg->eUp->winding;
	reg->inside = IsWindingInside( tess, reg->windingNumber );
}


static void FinishRegion( TESStesselator *tess, ActiveRegion *reg )
/*
* Delete a region from the sweep line.  This happens when the upper
* and lower chains of a region meet (at a vertex on the sweep line).
* The "inside" flag is copied to the appropriate mesh face (we could
* not do this before -- since the structure of the mesh is always
* changing, this face may not have even existed until now).
*/
{
	TESShalfEdge *e = reg->eUp;
	TESSface *f = e->Lface;

	f->inside = reg->inside;
	f->anEdge = e;   /* optimization for tessMeshTessellateMonoRegion() */
	DeleteRegion( tess, reg );
}


static TESShalfEdge *FinishLeftRegions( TESStesselator *tess,
									  ActiveRegion *regFirst, ActiveRegion *regLast )
/*
* We are given a vertex with one or more left-going edges.  All affected
* edges should be in the edge dictionary.  Starting at regFirst->eUp,
* we walk down deleting all regions where both edges have the same
* origin vOrg.  At the same time we copy the "inside" flag from the
* active region to the face, since at this point each face will belong
* to at most one region (this was not necessarily true until this point
* in the sweep).  The walk stops at the region above regLast; if regLast
* is NULL we walk as far as possible.  At the same time we relink the
* mesh if necessary, so that the ordering of edges around vOrg is the
* same as in the dictionary.
*/
{
	ActiveRegion *reg, *regPrev;
	TESShalfEdge *e, *ePrev;

	regPrev = regFirst;
	ePrev = regFirst->eUp;
	while( regPrev != regLast ) {
		regPrev->fixUpperEdge = FALSE;	/* placement was OK */
		reg = RegionBelow( regPrev );
		e = reg->eUp;
		if( e->Org != ePrev->Org ) {
			if( ! reg->fixUpperEdge ) {
				/* Remove the last left-going edge.  Even though there are no further
				* edges in the dictionary with this origin, there may be further
				* such edges in the mesh (if we are adding left edges to a vertex
				* that has already been processed).  Thus it is important to call
				* FinishRegion rather than just DeleteRegion.
				*/
				FinishRegion( tess, regPrev );
				break;
			}
			/* If the edge below was a temporary edge introduced by
			* ConnectRightVertex, now is the time to fix it.
			*/
			e = tessMeshConnect( tess->mesh, ePrev->Lprev, e->Sym );
			if (e == NULL) return (void *)TESS_ERROR;
			if ( !FixUpperEdge( tess, reg, e ) ) return (void *)TESS_ERROR;
		}

		/* Relink edges so that ePrev->Onext == e */
		if( ePrev->Onext != e ) {
			if ( !tessMeshSplice( tess->mesh, e->Oprev, e ) ) return (void *)TESS_ERROR;
			if ( !tessMeshSplice( tess->mesh, ePrev, e ) ) return (void *)TESS_ERROR;
		}
		FinishRegion( tess, regPrev );	/* may change reg->eUp */
		ePrev = reg->eUp;
		regPrev = reg;
	}
	return ePrev;
}


static int AddRightEdges( TESStesselator *tess, ActiveRegion *regUp,
						  TESShalfEdge *eFirst, TESShalfEdge *eLast, TESShalfEdge *eTopLeft,
						  int cleanUp )
/*
* Purpose: insert right-going edges into the edge dictionary, and update
* winding numbers and mesh connectivity appropriately.  All right-going
* edges share a common origin vOrg.  Edges are inserted CCW starting at
* eFirst; the last edge inserted is eLast->Oprev.  If vOrg has any
* left-going edges already processed, then eTopLeft must be the edge
* such that an imaginary upward vertical segment from vOrg would be
* contained between eTopLeft->Oprev and eTopLeft; otherwise eTopLeft
* should be NULL.
*/
{
	ActiveRegion *reg, *regPrev;
	TESShalfEdge *e, *ePrev;
	int firstTime = TRUE;

	/* Insert the new right-going edges in the dictionary */
	e = eFirst;
	do {
		// TESSassert( VertLeq( e->Org, e->Dst ), "AddRightEdges");
		if (AddRegionBelow( tess, regUp, e->Sym ) == NULL) {
			return TESS_ERROR;
		}
		e = e->Onext;
	} while ( e != eLast );

	/* Walk *all* right-going edges from e->Org, in the dictionary order,
	* updating the winding numbers of each region, and re-linking the mesh
	* edges to match the dictionary ordering (if necessary).
	*/
	if( eTopLeft == NULL ) {
		eTopLeft = RegionBelow( regUp )->eUp->Rprev;
	}
	regPrev = regUp;
	ePrev = eTopLeft;
	for( ;; ) {
		reg = RegionBelow( regPrev );
		e = reg->eUp->Sym;
		if( e->Org != ePrev->Org ) break;

		if( e->Onext != ePrev ) {
			/* Unlink e from its current position, and relink below ePrev */
			if ( !tessMeshSplice( tess->mesh, e->Oprev, e ) ) return TESS_ERROR;
			if ( !tessMeshSplice( tess->mesh, ePrev->Oprev, e ) ) return TESS_ERROR;
		}
		/* Compute the winding number and "inside" flag for the new regions */
		reg->windingNumber = regPrev->windingNumber - e->winding;
		reg->inside = IsWindingInside( tess, reg->windingNumber );

		/* Check for two outgoing edges with same slope -- process these
		* before any intersection tests (see example in tessComputeInterior).
		*/
		regPrev->dirty = TRUE;
		int res = CheckForRightSplice( tess, regPrev );
		if (res == TESS_ERROR) {
			return TESS_ERROR;
		}
		if( ! firstTime && res) {
			AddWinding( e, ePrev );
			DeleteRegion( tess, regPrev );
			if ( !tessMeshDelete( tess->mesh, ePrev ) ) return TESS_ERROR;
		}
		firstTime = FALSE;
		regPrev = reg;
		ePrev = e;
	}
	regPrev->dirty = TRUE;
	TESSassert( regPrev->windingNumber - e->winding == reg->windingNumber, "AddRightEdges");

	if( cleanUp ) {
		/* Check for intersections between newly adjacent edges. */
		return WalkDirtyRegions( tess, regPrev );
	}

	return TRUE;
}


static int SpliceMergeVertices( TESStesselator *tess, TESShalfEdge *e1,
								TESShalfEdge *e2 )
/*
* Two vertices with idential coordinates are combined into one.
* e1->Org is kept, while e2->Org is discarded.
*/
{
	if ( !tessMeshSplice( tess->mesh, e1, e2 ) ) return TESS_ERROR;
	return TRUE;
}

static void VertexWeights( TESSvertex *isect, TESSvertex *org, TESSvertex *dst,
						  TESSreal *weights )
/*
* Find some weights which describe how the intersection vertex is
* a linear combination of "org" and "dest".  Each of the two edges
* which generated "isect" is allocated 50% of the weight; each edge
* splits the weight between its org and dst according to the
* relative distance to "isect".
*/
{
	TESSreal t1 = VertL1dist( org, isect );
	TESSreal t2 = VertL1dist( dst, isect );

	weights[0] = (TESSreal)0.5 * t2 / (t1 + t2);
	weights[1] = (TESSreal)0.5 * t1 / (t1 + t2);
	isect->s += weights[0]*org->s + weights[1]*dst->s;
	isect->t += weights[0]*org->t + weights[1]*dst->t;
}


static void GetIntersectData( TESStesselator *tess, TESSvertex *isect,
							 TESSvertex *orgUp, TESSvertex *dstUp,
							 TESSvertex *orgLo, TESSvertex *dstLo )
 /*
 * We've computed a new intersection point, now we need a "data" pointer
 * from the user so that we can refer to this new vertex in the
 * rendering callbacks.
 */
{
	TESSreal weights[4];
	TESS_NOTUSED( tess );

	isect->s = isect->t = 0;
	isect->idx = TESS_UNDEF;
	VertexWeights( isect, orgUp, dstUp, &weights[0] );
	VertexWeights( isect, orgLo, dstLo, &weights[2] );
}

static int CheckForRightSplice( TESStesselator *tess, ActiveRegion *regUp )
/*
* Check the upper and lower edge of "regUp", to make sure that the
* eUp->Org is above eLo, or eLo->Org is below eUp (depending on which
* origin is leftmost).
*
* The main purpose is to splice right-going edges with the same
* dest vertex and nearly identical slopes (ie. we can't distinguish
* the slopes numerically).  However the splicing can also help us
* to recover from numerical errors.  For example, suppose at one
* point we checked eUp and eLo, and decided that eUp->Org is barely
* above eLo.  Then later, we split eLo into two edges (eg. from
* a splice operation like this one).  This can change the result of
* our test so that now eUp->Org is incident to eLo, or barely below it.
* We must correct this condition to maintain the dictionary invariants.
*
* One possibility is to check these edges for intersection again
* (ie. CheckForIntersect).  This is what we do if possible.  However
* CheckForIntersect requires that tess->event lies between eUp and eLo,
* so that it has something to fall back on when the intersection
* calculation gives us an unusable answer.  So, for those cases where
* we can't check for intersection, this routine fixes the problem
* by just splicing the offending vertex into the other edge.
* This is a guaranteed solution, no matter how degenerate things get.
* Basically this is a combinatorial solution to a numerical problem.
*/
{
	ActiveRegion *regLo = RegionBelow(regUp);
	TESShalfEdge *eUp = regUp->eUp;
	TESShalfEdge *eLo = regLo->eUp;

	if( VertLeq( eUp->Org, eLo->Org )) {
		if( EdgeSign( eLo->Dst, eUp->Org, eLo->Org ) > 0 ) return FALSE;

		/* eUp->Org appears to be below eLo */
		if( ! VertEq( eUp->Org, eLo->Org )) {
			/* Splice eUp->Org into eLo */
			if ( tessMeshSplitEdge( tess->mesh, eLo->Sym ) == NULL) return TESS_ERROR;
			if ( !tessMeshSplice( tess->mesh, eUp, eLo->Oprev ) ) return TESS_ERROR;
			regUp->dirty = regLo->dirty = TRUE;

		} else if( eUp->Org != eLo->Org ) {
			/* merge the two vertices, discarding eUp->Org */
			pqDelete( tess->pq, eUp->Org->pqHandle );
			return SpliceMergeVertices( tess, eLo->Oprev, eUp );
		}
	} else {
		if( EdgeSign( eUp->Dst, eLo->Org, eUp->Org ) <= 0 ) return FALSE;

		/* eLo->Org appears to be above eUp, so splice eLo->Org into eUp */
		RegionAbove(regUp)->dirty = regUp->dirty = TRUE;
		if (tessMeshSplitEdge( tess->mesh, eUp->Sym ) == NULL) return TESS_ERROR;
		if ( !tessMeshSplice( tess->mesh, eLo->Oprev, eUp ) ) return TESS_ERROR;
	}
	return TRUE;
}

static int CheckForLeftSplice( TESStesselator *tess, ActiveRegion *regUp )
/*
* Check the upper and lower edge of "regUp", to make sure that the
* eUp->Dst is above eLo, or eLo->Dst is below eUp (depending on which
* destination is rightmost).
*
* Theoretically, this should always be true.  However, splitting an edge
* into two pieces can change the results of previous tests.  For example,
* suppose at one point we checked eUp and eLo, and decided that eUp->Dst
* is barely above eLo.  Then later, we split eLo into two edges (eg. from
* a splice operation like this one).  This can change the result of
* the test so that now eUp->Dst is incident to eLo, or barely below it.
* We must correct this condition to maintain the dictionary invariants
* (otherwise new edges might get inserted in the wrong place in the
* dictionary, and bad stuff will happen).
*
* We fix the problem by just splicing the offending vertex into the
* other edge.
*/
{
	ActiveRegion *regLo = RegionBelow(regUp);
	TESShalfEdge *eUp = regUp->eUp;
	TESShalfEdge *eLo = regLo->eUp;
	TESShalfEdge *e;

	TESSassert( ! VertEq( eUp->Dst, eLo->Dst ), "CheckForLeftSplice");

	if( VertLeq( eUp->Dst, eLo->Dst )) {
		if( EdgeSign( eUp->Dst, eLo->Dst, eUp->Org ) < 0 ) return FALSE;

		/* eLo->Dst is above eUp, so splice eLo->Dst into eUp */
		RegionAbove(regUp)->dirty = regUp->dirty = TRUE;
		e = tessMeshSplitEdge( tess->mesh, eUp );
		if (e == NULL) return TESS_ERROR;
		if ( !tessMeshSplice( tess->mesh, eLo->Sym, e ) ) return TESS_ERROR;
		e->Lface->inside = regUp->inside;
	} else {
		if( EdgeSign( eLo->Dst, eUp->Dst, eLo->Org ) > 0 ) return FALSE;

		/* eUp->Dst is below eLo, so splice eUp->Dst into eLo */
		regUp->dirty = regLo->dirty = TRUE;
		e = tessMeshSplitEdge( tess->mesh, eLo );
		if (e == NULL) return TESS_ERROR;
		if ( !tessMeshSplice( tess->mesh, eUp->Lnext, eLo->Sym ) ) return TESS_ERROR;
		e->Rface->inside = regUp->inside;
	}
	return TRUE;
}


static int CheckForIntersect( TESStesselator *tess, ActiveRegion *regUp )
/*
* Check the upper and lower edges of the given region to see if
* they intersect.  If so, create the intersection and add it
* to the data structures.
*
* Returns TRUE if adding the new intersection resulted in a recursive
* call to AddRightEdges(); in this case all "dirty" regions have been
* checked for intersections, and possibly regUp has been deleted.
*/
{
	ActiveRegion *regLo = RegionBelow(regUp);
	TESShalfEdge *eUp = regUp->eUp;
	TESShalfEdge *eLo = regLo->eUp;
	TESSvertex *orgUp = eUp->Org;
	TESSvertex *orgLo = eLo->Org;
	TESSvertex *dstUp = eUp->Dst;
	TESSvertex *dstLo = eLo->Dst;
	TESSreal tMinUp, tMaxLo;
	TESSvertex isect, *orgMin;
	TESShalfEdge *e;

	TESSassert( ! VertEq( dstLo, dstUp ), "CheckForIntersect");
	TESSassert( EdgeSign( dstUp, tess->event, orgUp ) <= 0, "CheckForIntersect");
	TESSassert( EdgeSign( dstLo, tess->event, orgLo ) >= 0, "CheckForIntersect");
	TESSassert( orgUp != tess->event && orgLo != tess->event, "CheckForIntersect");
	TESSassert( ! regUp->fixUpperEdge && ! regLo->fixUpperEdge, "CheckForIntersect");

	if( orgUp == orgLo ) return FALSE;	/* right endpoints are the same */

	tMinUp = MIN( orgUp->t, dstUp->t );
	tMaxLo = MAX( orgLo->t, dstLo->t );
	if( tMinUp > tMaxLo ) return FALSE;	/* t ranges do not overlap */

	if( VertLeq( orgUp, orgLo )) {
		if( EdgeSign( dstLo, orgUp, orgLo ) > 0 ) return FALSE;
	} else {
		if( EdgeSign( dstUp, orgLo, orgUp ) < 0 ) return FALSE;
	}

	/* At this point the edges intersect, at least marginally */
	DebugEvent( tess );

	tesedgeIntersect( dstUp, orgUp, dstLo, orgLo, &isect );
	/* The following properties are guaranteed: */
	TESSassert( MIN( orgUp->t, dstUp->t ) <= isect.t, "CheckForIntersect");
	TESSassert( isect.t <= MAX( orgLo->t, dstLo->t ), "CheckForIntersect");
	TESSassert( MIN( dstLo->s, dstUp->s ) <= isect.s, "CheckForIntersect");
	TESSassert( isect.s <= MAX( orgLo->s, orgUp->s ), "CheckForIntersect");

	if( VertLeq( &isect, tess->event )) {
		/* The intersection point lies slightly to the left of the sweep line,
		* so move it until it''s slightly to the right of the sweep line.
		* (If we had perfect numerical precision, this would never happen
		* in the first place).  The easiest and safest thing to do is
		* replace the intersection by tess->event.
		*/
		isect.s = tess->event->s;
		isect.t = tess->event->t;
	}
	/* Similarly, if the computed intersection lies to the right of the
	* rightmost origin (which should rarely happen), it can cause
	* unbelievable inefficiency on sufficiently degenerate inputs.
	* (If you have the test program, try running test54.d with the
	* "X zoom" option turned on).
	*/
	orgMin = VertLeq( orgUp, orgLo ) ? orgUp : orgLo;
	if( VertLeq( orgMin, &isect )) {
		isect.s = orgMin->s;
		isect.t = orgMin->t;
	}

	if( VertEq( &isect, orgUp ) || VertEq( &isect, orgLo )) {
		/* Easy case -- intersection at one of the right endpoints */
		if (CheckForRightSplice( tess, regUp ) == TESS_ERROR) {
			return TESS_ERROR;
		}
		return FALSE;
	}

	if(    (! VertEq( dstUp, tess->event )
		&& EdgeSign( dstUp, tess->event, &isect ) >= 0)
		|| (! VertEq( dstLo, tess->event )
		&& EdgeSign( dstLo, tess->event, &isect ) <= 0 ))
	{
		/* Very unusual -- the new upper or lower edge would pass on the
		* wrong side of the sweep event, or through it.  This can happen
		* due to very small numerical errors in the intersection calculation.
		*/
		if( dstLo == tess->event ) {
			/* Splice dstLo into eUp, and process the new region(s) */
			if (tessMeshSplitEdge( tess->mesh, eUp->Sym ) == NULL) return TESS_ERROR;
			if ( !tessMeshSplice( tess->mesh, eLo->Sym, eUp ) ) return TESS_ERROR;
			regUp = TopLeftRegion( tess, regUp );
			if (regUp == NULL) return TESS_ERROR;
			eUp = RegionBelow(regUp)->eUp;
			if (FinishLeftRegions( tess, RegionBelow(regUp), regLo ) == (void *)TESS_ERROR) {
				return TESS_ERROR;
			}
			return AddRightEdges( tess, regUp, eUp->Oprev, eUp, eUp, TRUE );
		}
		if( dstUp == tess->event ) {
			/* Splice dstUp into eLo, and process the new region(s) */
			if (tessMeshSplitEdge( tess->mesh, eLo->Sym ) == NULL) return TESS_ERROR;
			if ( !tessMeshSplice( tess->mesh, eUp->Lnext, eLo->Oprev ) ) return TESS_ERROR;
			regLo = regUp;
			regUp = TopRightRegion( regUp );
			e = RegionBelow(regUp)->eUp->Rprev;
			regLo->eUp = eLo->Oprev;
			eLo = FinishLeftRegions( tess, regLo, NULL );
			if (eLo == (void *)TESS_ERROR) {
				return TESS_ERROR;
			}
			return AddRightEdges( tess, regUp, eLo->Onext, eUp->Rprev, e, TRUE );
		}
		/* Special case: called from ConnectRightVertex.  If either
		* edge passes on the wrong side of tess->event, split it
		* (and wait for ConnectRightVertex to splice it appropriately).
		*/
		if( EdgeSign( dstUp, tess->event, &isect ) >= 0 ) {
			RegionAbove(regUp)->dirty = regUp->dirty = TRUE;
			if (tessMeshSplitEdge( tess->mesh, eUp->Sym ) == NULL) return TESS_ERROR;
			eUp->Org->s = tess->event->s;
			eUp->Org->t = tess->event->t;
		}
		if( EdgeSign( dstLo, tess->event, &isect ) <= 0 ) {
			regUp->dirty = regLo->dirty = TRUE;
			if (tessMeshSplitEdge( tess->mesh, eLo->Sym ) == NULL) return TESS_ERROR;
			eLo->Org->s = tess->event->s;
			eLo->Org->t = tess->event->t;
		}
		/* leave the rest for ConnectRightVertex */
		return FALSE;
	}

	/* General case -- split both edges, splice into new vertex.
	* When we do the splice operation, the order of the arguments is
	* arbitrary as far as correctness goes.  However, when the operation
	* creates a new face, the work done is proportional to the size of
	* the new face.  We expect the faces in the processed part of
	* the mesh (ie. eUp->Lface) to be smaller than the faces in the
	* unprocessed original contours (which will be eLo->Oprev->Lface).
	*/
	if (tessMeshSplitEdge( tess->mesh, eUp->Sym ) == NULL) return TESS_ERROR;
	if (tessMeshSplitEdge( tess->mesh, eLo->Sym ) == NULL) return TESS_ERROR;
	if ( !tessMeshSplice( tess->mesh, eLo->Oprev, eUp ) ) return TESS_ERROR;
	eUp->Org->s = isect.s;
	eUp->Org->t = isect.t;
	eUp->Org->pqHandle = pqInsert( &tess->alloc, tess->pq, eUp->Org );
	if (eUp->Org->pqHandle == INV_HANDLE) {
		pqDeletePriorityQ( &tess->alloc, tess->pq );
		tess->pq = NULL;
		return TESS_ERROR;
	}
	GetIntersectData( tess, eUp->Org, orgUp, dstUp, orgLo, dstLo );
	RegionAbove(regUp)->dirty = regUp->dirty = regLo->dirty = TRUE;
	return FALSE;
}

static int WalkDirtyRegions( TESStesselator *tess, ActiveRegion *regUp )
/*
* When the upper or lower edge of any region changes, the region is
* marked "dirty".  This routine walks through all the dirty regions
* and makes sure that the dictionary invariants are satisfied
* (see the comments at the beginning of this file).  Of course
* new dirty regions can be created as we make changes to restore
* the invariants.
*/
{
	ActiveRegion *regLo = RegionBelow(regUp);
	TESShalfEdge *eUp, *eLo;

	for( ;; ) {
		/* Find the lowest dirty region (we walk from the bottom up). */
		while( regLo->dirty ) {
			regUp = regLo;
			regLo = RegionBelow(regLo);
		}
		if( ! regUp->dirty ) {
			regLo = regUp;
			regUp = RegionAbove( regUp );
			if( regUp == NULL || ! regUp->dirty ) {
				/* We've walked all the dirty regions */
				return TRUE;
			}
		}
		regUp->dirty = FALSE;
		eUp = regUp->eUp;
		eLo = regLo->eUp;

		if( eUp->Dst != eLo->Dst ) {
			/* Check that the edge ordering is obeyed at the Dst vertices. */
			int res = CheckForLeftSplice( tess, regUp );
			if( res ) {
				/* If the upper or lower edge was marked fixUpperEdge, then
				* we no longer need it (since these edges are needed only for
				* vertices which otherwise have no right-going edges).
				*/
				if( regLo->fixUpperEdge ) {
					DeleteRegion( tess, regLo );
					if ( !tessMeshDelete( tess->mesh, eLo ) ) return TESS_ERROR;
					regLo = RegionBelow( regUp );
					eLo = regLo->eUp;
				} else if( regUp->fixUpperEdge ) {
					DeleteRegion( tess, regUp );
					if ( !tessMeshDelete( tess->mesh, eUp ) ) return TESS_ERROR;
					regUp = RegionAbove( regLo );
					eUp = regUp->eUp;
				}
			} else if (res == TESS_ERROR) {
				return TESS_ERROR;
			}
		}
		if( eUp->Org != eLo->Org ) {
			if(    eUp->Dst != eLo->Dst
				&& ! regUp->fixUpperEdge && ! regLo->fixUpperEdge
				&& (eUp->Dst == tess->event || eLo->Dst == tess->event) )
			{
				/* When all else fails in CheckForIntersect(), it uses tess->event
				* as the intersection location.  To make this possible, it requires
				* that tess->event lie between the upper and lower edges, and also
				* that neither of these is marked fixUpperEdge (since in the worst
				* case it might splice one of these edges into tess->event, and
				* violate the invariant that fixable edges are the only right-going
				* edge from their associated vertex).
				*/
				int result = CheckForIntersect( tess, regUp );
				if( result == TRUE ) {
					/* WalkDirtyRegions() was called recursively; we're done */
					return TRUE;
				} else if (result == TESS_ERROR) {
					return TESS_ERROR;
				}
			} else {
				/* Even though we can't use CheckForIntersect(), the Org vertices
				* may violate the dictionary edge ordering.  Check and correct this.
				*/
				if (CheckForRightSplice( tess, regUp ) == TESS_ERROR) {
					return TESS_ERROR;
				}
			}
		}
		if( eUp->Org == eLo->Org && eUp->Dst == eLo->Dst ) {
			/* A degenerate loop consisting of only two edges -- delete it. */
			AddWinding( eLo, eUp );
			DeleteRegion( tess, regUp );
			if ( !tessMeshDelete( tess->mesh, eUp ) ) return TESS_ERROR;
			regUp = RegionAbove( regLo );
		}
	}
	return TRUE;
}


static int ConnectRightVertex( TESStesselator *tess, ActiveRegion *regUp,
							   TESShalfEdge *eBottomLeft )
/*
* Purpose: connect a "right" vertex vEvent (one where all edges go left)
* to the unprocessed portion of the mesh.  Since there are no right-going
* edges, two regions (one above vEvent and one below) are being merged
* into one.  "regUp" is the upper of these two regions.
*
* There are two reasons for doing this (adding a right-going edge):
*  - if the two regions being merged are "inside", we must add an edge
*    to keep them separated (the combined region would not be monotone).
*  - in any case, we must leave some record of vEvent in the dictionary,
*    so that we can merge vEvent with features that we have not seen yet.
*    For example, maybe there is a vertical edge which passes just to
*    the right of vEvent; we would like to splice vEvent into this edge.
*
* However, we don't want to connect vEvent to just any vertex.  We don''t
* want the new edge to cross any other edges; otherwise we will create
* intersection vertices even when the input data had no self-intersections.
* (This is a bad thing; if the user's input data has no intersections,
* we don't want to generate any false intersections ourselves.)
*
* Our eventual goal is to connect vEvent to the leftmost unprocessed
* vertex of the combined region (the union of regUp and regLo).
* But because of unseen vertices with all right-going edges, and also
* new vertices which may be created by edge intersections, we don''t
* know where that leftmost unprocessed vertex is.  In the meantime, we
* connect vEvent to the closest vertex of either chain, and mark the region
* as "fixUpperEdge".  This flag says to delete and reconnect this edge
* to the next processed vertex on the boundary of the combined region.
* Quite possibly the vertex we connected to will turn out to be the
* closest one, in which case we won''t need to make any changes.
*/
{
	TESShalfEdge *eNew;
	TESShalfEdge *eTopLeft = eBottomLeft->Onext;
	ActiveRegion *regLo = RegionBelow(regUp);
	TESShalfEdge *eUp = regUp->eUp;
	TESShalfEdge *eLo = regLo->eUp;
	int degenerate = FALSE;

	if( eUp->Dst != eLo->Dst ) {
		if (CheckForIntersect( tess, regUp ) == TESS_ERROR) {
			return TESS_ERROR;
		}
	}

	/* Possible new degeneracies: upper or lower edge of regUp may pass
	* through vEvent, or may coincide with new intersection vertex
	*/
	if( VertEq( eUp->Org, tess->event )) {
		if ( !tessMeshSplice( tess->mesh, eTopLeft->Oprev, eUp ) ) return TESS_ERROR;
		regUp = TopLeftRegion( tess, regUp );
		if (regUp == NULL) return TESS_ERROR;
		eTopLeft = RegionBelow( regUp )->eUp;
		if (FinishLeftRegions( tess, RegionBelow(regUp), regLo ) == (void *)TESS_ERROR) {
			return TESS_ERROR;
		}
		degenerate = TRUE;
	}
	if( VertEq( eLo->Org, tess->event )) {
		if ( !tessMeshSplice( tess->mesh, eBottomLeft, eLo->Oprev ) ) return TESS_ERROR;
		eBottomLeft = FinishLeftRegions( tess, regLo, NULL );
		if (eBottomLeft == (void *)TESS_ERROR) {
			return TESS_ERROR;
		}
		degenerate = TRUE;
	}
	if( degenerate ) {
		return AddRightEdges( tess, regUp, eBottomLeft->Onext, eTopLeft, eTopLeft, TRUE );
	}

	/* Non-degenerate situation -- need to add a temporary, fixable edge.
	* Connect to the closer of eLo->Org, eUp->Org.
	*/
	if( VertLeq( eLo->Org, eUp->Org )) {
		eNew = eLo->Oprev;
	} else {
		eNew = eUp;
	}
	eNew = tessMeshConnect( tess->mesh, eBottomLeft->Lprev, eNew );
	if (eNew == NULL) return TESS_ERROR;

	/* Prevent cleanup, otherwise eNew might disappear before we've even
	* had a chance to mark it as a temporary edge.
	*/
	if (AddRightEdges( tess, regUp, eNew, eNew->Onext, eNew->Onext, FALSE ) == TESS_ERROR) {
		return TESS_ERROR;
	}
	eNew->Sym->activeRegion->fixUpperEdge = TRUE;
	return WalkDirtyRegions( tess, regUp );
}

/* Because vertices at exactly the same location are merged together
* before we process the sweep event, some degenerate cases can't occur.
* However if someone eventually makes the modifications required to
* merge features which are close together, the cases below marked
* TOLERANCE_NONZERO will be useful.  They were debugged before the
* code to merge identical vertices in the main loop was added.
*/
#define TOLERANCE_NONZERO	FALSE

static int ConnectLeftDegenerate( TESStesselator *tess,
								  ActiveRegion *regUp, TESSvertex *vEvent )
/*
* The event vertex lies exacty on an already-processed edge or vertex.
* Adding the new vertex involves splicing it into the already-processed
* part of the mesh.
*/
{
	TESShalfEdge *e, *eTopLeft, *eTopRight, *eLast;
	ActiveRegion *reg;

	e = regUp->eUp;
	if( VertEq( e->Org, vEvent )) {
		/* e->Org is an unprocessed vertex - just combine them, and wait
		* for e->Org to be pulled from the queue
		*/
		TESSassert( TOLERANCE_NONZERO, "ConnectLeftDegenerate");
		return SpliceMergeVertices( tess, e, vEvent->anEdge );
	}

	if( ! VertEq( e->Dst, vEvent )) {
		/* General case -- splice vEvent into edge e which passes through it */
		if (tessMeshSplitEdge( tess->mesh, e->Sym ) == NULL) return TESS_ERROR;
		if( regUp->fixUpperEdge ) {
			/* This edge was fixable -- delete unused portion of original edge */
			if ( !tessMeshDelete( tess->mesh, e->Onext ) ) return TESS_ERROR;
			regUp->fixUpperEdge = FALSE;
		}
		if ( !tessMeshSplice( tess->mesh, vEvent->anEdge, e ) ) return TESS_ERROR;
		return SweepEvent( tess, vEvent );	/* recurse */
	}

	/* vEvent coincides with e->Dst, which has already been processed.
	* Splice in the additional right-going edges.
	*/
	TESSassert( TOLERANCE_NONZERO, "ConnectLeftDegenerate");
	regUp = TopRightRegion( regUp );
	reg = RegionBelow( regUp );
	eTopRight = reg->eUp->Sym;
	eTopLeft = eLast = eTopRight->Onext;
	if( reg->fixUpperEdge ) {
		/* Here e->Dst has only a single fixable edge going right.
		* We can delete it since now we have some real right-going edges.
		*/
		TESSassert( eTopLeft != eTopRight, "ConnectLeftDegenerate");   /* there are some left edges too */
		DeleteRegion( tess, reg );
		if ( !tessMeshDelete( tess->mesh, eTopRight ) ) return TESS_ERROR;
		eTopRight = eTopLeft->Oprev;
	}
	if ( !tessMeshSplice( tess->mesh, vEvent->anEdge, eTopRight ) ) return TESS_ERROR;
	if( ! EdgeGoesLeft( eTopLeft )) {
		/* e->Dst had no left-going edges -- indicate this to AddRightEdges() */
		eTopLeft = NULL;
	}
	return AddRightEdges( tess, regUp, eTopRight->Onext, eLast, eTopLeft, TRUE );
}


static int ConnectLeftVertex( TESStesselator *tess, TESSvertex *vEvent )
/*
* Purpose: connect a "left" vertex (one where both edges go right)
* to the processed portion of the mesh.  Let R be the active region
* containing vEvent, and let U and L be the upper and lower edge
* chains of R.  There are two possibilities:
*
* - the normal case: split R into two regions, by connecting vEvent to
*   the rightmost vertex of U or L lying to the left of the sweep line
*
* - the degenerate case: if vEvent is close enough to U or L, we
*   merge vEvent into that edge chain.  The subcases are:
*	- merging with the rightmost vertex of U or L
*	- merging with the active edge of U or L
*	- merging with an already-processed portion of U or L
*/
{
	ActiveRegion *regUp, *regLo, *reg;
	TESShalfEdge *eUp, *eLo, *eNew;
	ActiveRegion tmp;

	/* assert( vEvent->anEdge->Onext->Onext == vEvent->anEdge ); */

	/* Get a pointer to the active region containing vEvent */
	tmp.eUp = vEvent->anEdge->Sym;
	/* __GL_DICTLISTKEY */ /* tessDictListSearch */
	regUp = (ActiveRegion *)dictKey( dictSearch( tess->dict, &tmp ));
	regLo = RegionBelow( regUp );
	if( !regLo ) {
		// This may happen if the input polygon is coplanar.
		return TRUE;
	}
	eUp = regUp->eUp;
	eLo = regLo->eUp;

	/* Try merging with U or L first */
	if( EdgeSign( eUp->Dst, vEvent, eUp->Org ) == 0 ) {
		return ConnectLeftDegenerate( tess, regUp, vEvent );
	}

	/* Connect vEvent to rightmost processed vertex of either chain.
	* e->Dst is the vertex that we will connect to vEvent.
	*/
	reg = VertLeq( eLo->Dst, eUp->Dst ) ? regUp : regLo;

	if( regUp->inside || reg->fixUpperEdge) {
		if( reg == regUp ) {
			eNew = tessMeshConnect( tess->mesh, vEvent->anEdge->Sym, eUp->Lnext );
			if (eNew == NULL) return TESS_ERROR;
		} else {
			TESShalfEdge *tempHalfEdge= tessMeshConnect( tess->mesh, eLo->Dnext, vEvent->anEdge);
			if (tempHalfEdge == NULL) return TESS_ERROR;

			eNew = tempHalfEdge->Sym;
		}
		if( reg->fixUpperEdge ) {
			if ( !FixUpperEdge( tess, reg, eNew ) ) return TESS_ERROR;
		} else {
			ActiveRegion *region = AddRegionBelow( tess, regUp, eNew );
			if (region == NULL) {
				return TESS_ERROR;
			}
			ComputeWinding( tess, region);
		}
		return SweepEvent( tess, vEvent );
	} else {
		/* The new vertex is in a region which does not belong to the polygon.
		* We don''t need to connect this vertex to the rest of the mesh.
		*/
		return AddRightEdges( tess, regUp, vEvent->anEdge, vEvent->anEdge, NULL, TRUE );
	}
}


static int SweepEvent( TESStesselator *tess, TESSvertex *vEvent )
/*
* Does everything necessary when the sweep line crosses a vertex.
* Updates the mesh and the edge dictionary.
*/
{
	ActiveRegion *regUp, *reg;
	TESShalfEdge *e, *eTopLeft, *eBottomLeft;

	tess->event = vEvent;		/* for access in EdgeLeq() */
	DebugEvent( tess );

	/* Check if this vertex is the right endpoint of an edge that is
	* already in the dictionary.  In this case we don't need to waste
	* time searching for the location to insert new edges.
	*/
	e = vEvent->anEdge;
	while( e->activeRegion == NULL ) {
		e = e->Onext;
		if( e == vEvent->anEdge ) {
			/* All edges go right -- not incident to any processed edges */
			return ConnectLeftVertex( tess, vEvent );;
		}
	}

	/* Processing consists of two phases: first we "finish" all the
	* active regions where both the upper and lower edges terminate
	* at vEvent (ie. vEvent is closing off these regions).
	* We mark these faces "inside" or "outside" the polygon according
	* to their winding number, and delete the edges from the dictionary.
	* This takes care of all the left-going edges from vEvent.
	*/
	regUp = TopLeftRegion( tess, e->activeRegion );
	if (regUp == NULL) return TESS_ERROR;
	reg = RegionBelow( regUp );
	eTopLeft = reg->eUp;
	eBottomLeft = FinishLeftRegions( tess, reg, NULL );
	if (eBottomLeft == (void *)TESS_ERROR) {
		return TESS_ERROR;
	}

	/* Next we process all the right-going edges from vEvent.  This
	* involves adding the edges to the dictionary, and creating the
	* associated "active regions" which record information about the
	* regions between adjacent dictionary edges.
	*/
	if( eBottomLeft->Onext == eTopLeft ) {
		/* No right-going edges -- add a temporary "fixable" edge */
		return ConnectRightVertex( tess, regUp, eBottomLeft );
	} else {
		return AddRightEdges( tess, regUp, eBottomLeft->Onext, eTopLeft, eTopLeft, TRUE );
	}
}


/* Make the sentinel coordinates big enough that they will never be
* merged with real input features.
*/

static int AddSentinel( TESStesselator *tess, TESSreal smin, TESSreal smax, TESSreal t )
/*
* We add two sentinel edges above and below all other edges,
* to avoid special cases at the top and bottom.
*/
{
	TESShalfEdge *e;
	ActiveRegion *reg = (ActiveRegion *)tess->alloc.memalloc(tess->alloc.userData, sizeof(ActiveRegion));
	if (reg == NULL) return TESS_ERROR;

	e = tessMeshMakeEdge( tess->mesh );
	if (e == NULL) return TESS_ERROR;

	e->Org->s = smax;
	e->Org->t = t;
	e->Dst->s = smin;
	e->Dst->t = t;
	tess->event = e->Dst;		/* initialize it */

	reg->eUp = e;
	reg->windingNumber = 0;
	reg->inside = FALSE;
	reg->fixUpperEdge = FALSE;
	reg->sentinel = TRUE;
	reg->dirty = FALSE;
	reg->nodeUp = dictInsert( tess->dict, reg );
	if (reg->nodeUp == NULL) return TESS_ERROR;
	return TRUE;
}


static int InitEdgeDict( TESStesselator *tess )
/*
* We maintain an ordering of edge intersections with the sweep line.
* This order is maintained in a dynamic dictionary.
*/
{
	TESSreal w, h;
	TESSreal smin, smax, tmin, tmax;

	tess->dict = dictNewDict( &tess->alloc, tess, (int (*)(void *, DictKey, DictKey)) EdgeLeq );
	if (tess->dict == NULL) return TESS_ERROR;

	/* If the bbox is empty, ensure that sentinels are not coincident by slightly enlarging it. */
	w = (tess->bmax[0] - tess->bmin[0]) + (TESSreal)0.01;
	h = (tess->bmax[1] - tess->bmin[1]) + (TESSreal)0.01;

	smin = tess->bmin[0] - w;
	smax = tess->bmax[0] + w;
	tmin = tess->bmin[1] - h;
	tmax = tess->bmax[1] + h;

	if (AddSentinel( tess, smin, smax, tmin ) == TESS_ERROR) { return TESS_ERROR; }
	if (AddSentinel( tess, smin, smax, tmax ) == TESS_ERROR) { return TESS_ERROR; }
	return TRUE;
}


static void DoneEdgeDict( TESStesselator *tess )
{
	ActiveRegion *reg;
	int fixedEdges __attribute__((unused)) = 0;

	while( (reg = (ActiveRegion *)dictKey( dictMin( tess->dict ))) != NULL ) {
		/*
		* At the end of all processing, the dictionary should contain
		* only the two sentinel edges, plus at most one "fixable" edge
		* created by ConnectRightVertex().
		*/
		if( ! reg->sentinel ) {
			TESSassert( reg->fixUpperEdge, "DoneEdgeDict");
			TESSassert( ++fixedEdges == 1, "DoneEdgeDict");
		}
		TESSassert( reg->windingNumber == 0, "DoneEdgeDict");
		DeleteRegion( tess, reg );
		/*    tessMeshDelete( reg->eUp );*/
	}
	dictDeleteDict( &tess->alloc, tess->dict );
}


static int RemoveDegenerateEdges( TESStesselator *tess )
/*
* Remove zero-length edges, and contours with fewer than 3 vertices.
*/
{
	TESShalfEdge *e, *eNext, *eLnext;
	TESShalfEdge *eHead = &tess->mesh->eHead;

	/*LINTED*/
	for( e = eHead->next; e != eHead; e = eNext ) {
		eNext = e->next;
		eLnext = e->Lnext;

		if( VertEq( e->Org, e->Dst ) && e->Lnext->Lnext != e ) {
			/* Zero-length edge, contour has at least 3 edges */

			SpliceMergeVertices( tess, eLnext, e );	/* deletes e->Org */
			if ( !tessMeshDelete( tess->mesh, e ) ) return TESS_ERROR; /* e is a self-loop */
			e = eLnext;
			eLnext = e->Lnext;
		}
		if( eLnext->Lnext == e ) {
			/* Degenerate contour (one or two edges) */

			if( eLnext != e ) {
				if( eLnext == eNext || eLnext == eNext->Sym ) { eNext = eNext->next; }
				if ( !tessMeshDelete( tess->mesh, eLnext ) ) return TESS_ERROR;
			}
			if( e == eNext || e == eNext->Sym ) { eNext = eNext->next; }
			if ( !tessMeshDelete( tess->mesh, e ) ) return TESS_ERROR;
		}
	}
	return TRUE;
}

static int InitPriorityQ( TESStesselator *tess )
/*
* Insert all vertices into the priority queue which determines the
* order in which vertices cross the sweep line.
*/
{
	PriorityQ *pq;
	TESSvertex *v, *vHead;
	int vertexCount = 0;

	vHead = &tess->mesh->vHead;
	for( v = vHead->next; v != vHead; v = v->next ) {
		vertexCount++;
	}
	/* Make sure there is enough space for sentinels. */
	vertexCount += 16;

	pq = tess->pq = pqNewPriorityQ( &tess->alloc, vertexCount, (int (*)(PQkey, PQkey)) tesvertLeq );
	if (pq == NULL) return 0;

	vHead = &tess->mesh->vHead;
	for( v = vHead->next; v != vHead; v = v->next ) {
		v->pqHandle = pqInsert( &tess->alloc, pq, v );
		if (v->pqHandle == INV_HANDLE)
			break;
	}
	if (v != vHead || !pqInit( &tess->alloc, pq ) ) {
		pqDeletePriorityQ( &tess->alloc, tess->pq );
		tess->pq = NULL;
		return 0;
	}

	return 1;
}


static void DonePriorityQ( TESStesselator *tess )
{
	pqDeletePriorityQ( &tess->alloc, tess->pq );
}


static int RemoveDegenerateFaces( TESStesselator *tess, TESSmesh *mesh )
/*
* Delete any degenerate faces with only two edges.  WalkDirtyRegions()
* will catch almost all of these, but it won't catch degenerate faces
* produced by splice operations on already-processed edges.
* The two places this can happen are in FinishLeftRegions(), when
* we splice in a "temporary" edge produced by ConnectRightVertex(),
* and in CheckForLeftSplice(), where we splice already-processed
* edges to ensure that our dictionary invariants are not violated
* by numerical errors.
*
* In both these cases it is *very* dangerous to delete the offending
* edge at the time, since one of the routines further up the stack
* will sometimes be keeping a pointer to that edge.
*/
{
	TESSface *f, *fNext;
	TESShalfEdge *e;

	/*LINTED*/
	for( f = mesh->fHead.next; f != &mesh->fHead; f = fNext ) {
		fNext = f->next;
		e = f->anEdge;
		TESSassert( e->Lnext != e, "RemoveDegenerateFaces");

		if( e->Lnext->Lnext == e ) {
			/* A face with only two edges */
			AddWinding( e->Onext, e );
			if ( !tessMeshDelete( tess->mesh, e ) ) return 0;
		}
	}
	return 1;
}

int tessComputeInterior( TESStesselator *tess )
/*
* tessComputeInterior( tess ) computes the planar arrangement specified
* by the given contours, and further subdivides this arrangement
* into regions.  Each region is marked "inside" if it belongs
* to the polygon, according to the rule given by tess->windingRule.
* Each interior region is guaranteed be monotone.
*/
{
	TESSvertex *v, *vNext;

	/* Each vertex defines an event for our sweep line.  Start by inserting
	* all the vertices in a priority queue.  Events are processed in
	* lexicographic order, ie.
	*
	*	e1 < e2  iff  e1.x < e2.x || (e1.x == e2.x && e1.y < e2.y)
	*/
	if (RemoveDegenerateEdges( tess ) == TESS_ERROR) {
		return TESS_ERROR;
	}

	if ( !InitPriorityQ( tess ) ) return 0; /* if error */
	if (InitEdgeDict( tess ) == TESS_ERROR) {
		return TESS_ERROR;
	}

	while( (v = (TESSvertex *)pqExtractMin( tess->pq )) != NULL ) {
		for( ;; ) {
			vNext = (TESSvertex *)pqMinimum( tess->pq );
			if( vNext == NULL || ! VertEq( vNext, v )) break;

			/* Merge together all vertices at exactly the same location.
			* This is more efficient than processing them one at a time,
			* simplifies the code (see ConnectLeftDegenerate), and is also
			* important for correct handling of certain degenerate cases.
			* For example, suppose there are two identical edges A and B
			* that belong to different contours (so without this code they would
			* be processed by separate sweep events).  Suppose another edge C
			* crosses A and B from above.  When A is processed, we split it
			* at its intersection point with C.  However this also splits C,
			* so when we insert B we may compute a slightly different
			* intersection point.  This might leave two edges with a small
			* gap between them.  This kind of error is especially obvious
			* when using boundary extraction (TESS_BOUNDARY_ONLY).
			*/
			vNext = (TESSvertex *)pqExtractMin( tess->pq );
			SpliceMergeVertices( tess, v->anEdge, vNext->anEdge );
		}
		if (SweepEvent( tess, v ) == TESS_ERROR) {
			return TESS_ERROR;
		}
	}

	/* Set tess->event for debugging purposes */
	tess->event = ((ActiveRegion *) dictKey( dictMin( tess->dict )))->eUp->Org;
	DebugEvent( tess );
	DoneEdgeDict( tess );
	DonePriorityQ( tess );

	if ( !RemoveDegenerateFaces( tess, tess->mesh ) ) return 0;
	tessMeshCheckMesh( tess->mesh );

	return 1;
}
