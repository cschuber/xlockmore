/* -*- Mode: C; tab-width: 4 -*- */
/* voters --- Dewdney's Voting Simulation */

#if 0
static const char sccsid[] = "@(#)voters.c	5.24 2007/01/18 xlockmore";

#endif

/*-
 * Copyright (c) 1997 by David Bagley.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * Revision History:
 * 18-Jan-2007: Added vertical option.
 * 01-Nov-2000: Allocation checks
 * 10-Jun-1997: Coded from A.K. Dewdney's "The Armchair Universe, Computer
 *              Recreations from the Pages of Scientific American Magazine"
 *              W.H. Freedman and Company, New York, 1988  (Apr 1985)
 *              Used wator.c and demon.c as a guide.
 */

#ifdef STANDALONE
#define MODE_voters
#define DEFAULTS "*delay: 1000 \n" \
	"*cycles: 327670 \n" \
	"*size: 0 \n" \
	"*ncolors: 64 \n" \

# define free_voters 0
# define reshape_voters 0
# define voters_handle_event 0
#define UNIFORM_COLORS
#define BRIGHT_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */
#include "automata.h"

#ifdef MODE_voters

/*-
 * neighbors of 0 randomizes it between 3, 4, 6, 8, 9, and 12.
 */
#define DEF_NEIGHBORS  "0"      /* choose random value */
#define DEF_VERTICAL "False"

static int  neighbors;
static Bool vertical;

static XrmOptionDescRec opts[] =
{
        {(char *) "-neighbors", (char *) ".voters.neighbors", XrmoptionSepArg, (caddr_t) NULL},
	{(char *) "-vertical", (char *) ".voters.vertical", XrmoptionNoArg, (caddr_t) "on"},
        {(char *) "+vertical", (char *) ".voters.vertical", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] =
{
        {(void *) & neighbors, (char *) "neighbors", (char *) "Neighbors", (char *) DEF_NEIGHBORS, t_Int},
	{(void *) & vertical, (char *) "vertical", (char *) "Vertical", (char *) DEF_VERTICAL, t_Bool}
};
static OptionStruct desc[] =
{
        {(char *) "-neighbors num", (char *) "squares 4 or 8, hexagons 6, triangles 3, 9 or 12"},
	{(char *) "-/+vertical", (char *) "change orientation for hexagons and triangles"}
};

ENTRYPOINT ModeSpecOpt voters_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};


#ifdef USE_MODULES
ModStruct   voters_description =
{"voters", "init_voters", "draw_voters", "release_voters",
 "refresh_voters", "init_voters", (char *) NULL, &voters_opts,
 1000, 0, 327670, 0, 64, 1.0, "",
 "Shows Dewdney's Voters", 0, NULL};

#endif

/*-
 * From far left to right, at least in the currently in the US.  By the way, I
 * consider myself to be a proud bleeding heart liberal democrat, in
 * case anyone wants to know....  Please, no fascist "improvements".  :)
 */

#include "bitmaps/elephant.xbm"
#ifdef COMMIE
#include "bitmaps/sickle.xbm"
#else
#include "bitmaps/green.xbm"
#endif
#include "bitmaps/donkey.xbm"

#define MINPARTIES 2
#define BITMAPS 3
#define MINGRIDSIZE 10
#define MINSIZE 4
#define FACTOR 10
#define NEIGHBORKINDS 6

static XImage logo[BITMAPS] =
{
    {0, 0, 0, XYBitmap, (char *) elephant_bits, LSBFirst, 8, LSBFirst, 8, 1},
    {0, 0, 0, XYBitmap, (char *) green_bits, LSBFirst, 8, LSBFirst, 8, 1},
    {0, 0, 0, XYBitmap, (char *) donkey_bits, LSBFirst, 8, LSBFirst, 8, 1},
};

/* Voter data */
typedef struct {
	char        kind;
	int         age;
	int         col, row;
} cellstruct;

/* Doubly linked list */
typedef struct _CellList {
	cellstruct  info;
	struct _CellList *previous, *next;
} CellList;

typedef struct {
	Bool        painted, vertical;
	int         party;	/* Currently working on donkey, elephant, or green? */
	int         xs, ys;	/* Size of party icon */
	int         xb, yb;	/* Bitmap offset for party icon */
	int         nparties;	/* 2 parties or 3 */
	int         number_in_party[BITMAPS];	/* Good to know when one party rules */
	int         pixelmode;
	int         generation;
	int         ncols, nrows;
	int         npositions;
	int         width, height;
	CellList   *last, *first;
	char       *arr;
	int         neighbors, polygon;
	int         busyLoop;
	union {
		XPoint      hexagon[6];
		XPoint      triangle[2][3];
		XPoint      pentagon[4][5];
	} shape;
} voterstruct;

static char plots[NEIGHBORKINDS] =
{
	3, 4, 6, 8, 9, 12	/* Neighborhoods */
};

static voterstruct *voters = (voterstruct *) NULL;
static int  icon_width, icon_height;

static void
drawCell(ModeInfo * mi, int col, int row, unsigned long color, int bitmap,
		Bool firstChange)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	voterstruct *vp = &voters[MI_SCREEN(mi)];
	unsigned long colour = (MI_NPIXELS(mi) > 2) ?
		MI_PIXEL(mi, color) : MI_WHITE_PIXEL(mi);

	XSetForeground(display, gc, colour);
	if (vp->neighbors == 6) {
		int ccol = 2 * col + !(row & 1), crow = 2 * row;

		if (vp->vertical) {
			vp->shape.hexagon[0].x = vp->xb + ccol * vp->xs;
			vp->shape.hexagon[0].y = vp->yb + crow * vp->ys;
		} else {
			vp->shape.hexagon[0].y = vp->xb + ccol * vp->xs;
			vp->shape.hexagon[0].x = vp->yb + crow * vp->ys;
		}
		if (vp->xs == 1 && vp->ys == 1)
			XDrawPoint(display, window, gc,
				vp->shape.hexagon[0].x,
				vp->shape.hexagon[0].y);
		else if (bitmap == BITMAPS - 1)
			XFillPolygon(display, window, gc,
				vp->shape.hexagon, 6,
				Convex, CoordModePrevious);
		else {
			int ix = 0, iy = 0, sx, sy;

			if (firstChange) {
				XSetForeground(display, gc,
					MI_BLACK_PIXEL(mi));
				XFillPolygon(display, window, gc,
					vp->shape.hexagon, 6,
					Convex, CoordModePrevious);
				XSetForeground(display, gc, colour);
			}
			if (vp->vertical) {
				vp->shape.hexagon[0].x -= vp->xs;
				vp->shape.hexagon[0].y += vp->ys / 4;
				sx = 2 * vp->xs - 6;
				sy = 2 * vp->ys - 2;
				if (vp->xs <= 6 || vp->ys <= 2) {
					ix = 3;
					iy = 1;
				} else
					ix = 5;
			} else {
				vp->shape.hexagon[0].y -= vp->xs;
				vp->shape.hexagon[0].x += vp->ys / 4;
				sy = 2 * vp->xs - 6;
				sx = 2 * vp->ys - 2;
				if (vp->xs <= 6 || vp->ys <= 2) {
					iy = 3;
					ix = 1;
				} else
					iy = 5;
			}
			if (vp->xs <= 6 || vp->ys <= 2)
				XFillRectangle(display, window, gc,
					vp->shape.hexagon[0].x + ix, 
					vp->shape.hexagon[0].y + iy,
					vp->xs, vp->ys);
			else
				XFillArc(display, window, gc,
					vp->shape.hexagon[0].x + ix,
					vp->shape.hexagon[0].y + iy,
					sx, sy,
					0, 23040);
		}
	} else if (vp->neighbors == 4 || vp->neighbors == 8) {
		if (vp->pixelmode) {
			if (bitmap == BITMAPS - 1 || (vp->xs <= 2 || vp->ys <= 2))
				XFillRectangle(display, window, gc,
					vp->xb + vp->xs * col,
					vp->yb + vp->ys * row,
					vp->xs - (vp->xs > 3),
					vp->ys - (vp->ys > 3));
			else {
				if (firstChange) {
					XSetForeground(display, gc,
						MI_BLACK_PIXEL(mi));
					XFillRectangle(display, window, gc,
						vp->xb + vp->xs * col,
						vp->yb + vp->ys * row,
						vp->xs, vp->ys);
					XSetForeground(display, gc, colour);
				}
				XFillArc(display, window, gc,
					vp->xb + vp->xs * col,
					vp->yb + vp->ys * row,
					vp->xs - 1, vp->ys - 1,
					0, 23040);
			}
		} else
			(void) XPutImage(display, window, gc,
					 &logo[bitmap], 0, 0,
				vp->xb + vp->xs * col, vp->yb + vp->ys * row,
					 icon_width, icon_height);
	} else {		/* TRI */
		int orient = (col + row) % 2;	/* O left 1 right */
		Bool small = (vp->xs <= 3 || vp->ys <= 3);
		int ix = 0, iy = 0;

		if (vp->vertical) {
			vp->shape.triangle[orient][0].x = vp->xb + col * vp->xs;
			vp->shape.triangle[orient][0].y = vp->yb + row * vp->ys;
			if (small)
				vp->shape.triangle[orient][0].x +=
					((orient) ? -1 : 1);
			else
				vp->shape.triangle[orient][0].x +=
					(vp->xs / 2  - 1) * ((orient) ? 1 : -1);
		} else {
			vp->shape.triangle[orient][0].y = vp->xb + col * vp->xs;
			vp->shape.triangle[orient][0].x = vp->yb + row * vp->ys;
			if (small)
				vp->shape.triangle[orient][0].y +=
					((orient) ? -1 : 1);
			else
				vp->shape.triangle[orient][0].y +=
					(vp->xs / 2  - 1) * ((orient) ? 1 : -1);
		}
		if (small)
			XDrawPoint(display, window, gc,
				vp->shape.triangle[orient][0].x,
				vp->shape.triangle[orient][0].y);
		else {
			if (bitmap == BITMAPS - 1)
				XFillPolygon(display, window, gc,
					vp->shape.triangle[orient], 3,
					Convex, CoordModePrevious);
			else {
				if (firstChange) {
					XSetForeground(display, gc,
						MI_BLACK_PIXEL(mi));
					XFillPolygon(display, window, gc,
						vp->shape.triangle[orient], 3,
						Convex, CoordModePrevious);
					XSetForeground(display, gc, colour);
				}
				if (vp->vertical) {
					vp->shape.triangle[orient][0].x += -4 * vp->xs / 5 +
						((orient) ? vp->xs / 3 : 3 * vp->xs / 5);
					vp->shape.triangle[orient][0].y += -vp->ys / 2 + 1;
					ix = ((orient) ? -vp->xs / 2 : vp->xs / 2);
				} else {
					vp->shape.triangle[orient][0].y += -4 * vp->xs / 5 +
						((orient) ? vp->xs / 3 : 3 * vp->xs / 5);
					vp->shape.triangle[orient][0].x += -vp->ys / 2 + 1;
					iy = ((orient) ? -vp->xs / 2 : vp->xs / 2);
				}
				XFillArc(display, window, gc,
					vp->shape.triangle[orient][0].x + ix,
					vp->shape.triangle[orient][0].y + iy,
					vp->ys - 3, vp->ys - 3,
					0, 23040);
			}
		}
	}
}

static Bool
init_list(voterstruct * vp)
{
	/* Waste some space at the beginning and end of list
	   so we do not have to complicated checks against falling off the ends. */
	if (((vp->last = (CellList *) malloc(sizeof (CellList))) == NULL) ||
	    ((vp->first = (CellList *) malloc(sizeof (CellList))) == NULL)) {
		return False;
	}
	vp->first->previous = vp->last->next = (struct _CellList *) NULL;
	vp->first->next = vp->last->previous = (struct _CellList *) NULL;
	vp->first->next = vp->last;
	vp->last->previous = vp->first;
	return True;
}

static Bool
addto_list(voterstruct * vp, cellstruct info)
{
	CellList   *curr;

	if ((curr = (CellList *) malloc(sizeof (CellList))) == NULL) {
		return False;
	}
	vp->last->previous->next = curr;
	curr->previous = vp->last->previous;
	curr->next = vp->last;
	vp->last->previous = curr;
	curr->info = info;
	return True;
}

static void
removefrom_list(CellList * ptr)
{
	ptr->previous->next = ptr->next;
	ptr->next->previous = ptr->previous;
	free(ptr);
}

static void
flush_list(voterstruct * vp)
{
	CellList   *curr;

	while (vp->last->previous != vp->first) {
		curr = vp->last->previous;
		curr->previous->next = vp->last;
		vp->last->previous = curr->previous;
		free(curr);
	}
}


static int
positionOfNeighbor(voterstruct * vp, int n, int col, int row)
{
	int dir = n * (360 / vp->neighbors);

	if (vp->polygon == 4 || vp->polygon == 6) {
		switch (dir) {
		case 0:
			col = (col + 1 == vp->ncols) ? 0 : col + 1;
			break;
		case 45:
			col = (col + 1 == vp->ncols) ? 0 : col + 1;
			row = (!row) ? vp->nrows - 1 : row - 1;
			break;
		case 60:
			if (!(row & 1))
				col = (col + 1 == vp->ncols) ? 0 : col + 1;
			row = (!row) ? vp->nrows - 1 : row - 1;
			break;
		case 90:
			row = (!row) ? vp->nrows - 1 : row - 1;
			break;
		case 120:
			if (row & 1)
				col = (!col) ? vp->ncols - 1 : col - 1;
			row = (!row) ? vp->nrows - 1 : row - 1;
			break;
		case 135:
			col = (!col) ? vp->ncols - 1 : col - 1;
			row = (!row) ? vp->nrows - 1 : row - 1;
			break;
		case 180:
			col = (!col) ? vp->ncols - 1 : col - 1;
			break;
		case 225:
			col = (!col) ? vp->ncols - 1 : col - 1;
			row = (row + 1 == vp->nrows) ? 0 : row + 1;
			break;
		case 240:
			if (row & 1)
				col = (!col) ? vp->ncols - 1 : col - 1;
			row = (row + 1 == vp->nrows) ? 0 : row + 1;
			break;
		case 270:
			row = (row + 1 == vp->nrows) ? 0 : row + 1;
			break;
		case 300:
			if (!(row & 1))
				col = (col + 1 == vp->ncols) ? 0 : col + 1;
			row = (row + 1 == vp->nrows) ? 0 : row + 1;
			break;
		case 315:
			col = (col + 1 == vp->ncols) ? 0 : col + 1;
			row = (row + 1 == vp->nrows) ? 0 : row + 1;
			break;
		default:
			(void) fprintf(stderr, "wrong direction %d\n", dir);
		}
	} else if (vp->polygon == 3) {
		if ((col + row) & 1) {	/* right */
			switch (dir) {
			case 0:
				col = (!col) ? vp->ncols - 1 : col - 1;
				break;
			case 30:
			case 40:
				col = (!col) ? vp->ncols - 1 : col - 1;
				row = (row + 1 == vp->nrows) ? 0 : row + 1;
				break;
			case 60:
				col = (!col) ? vp->ncols - 1 : col - 1;
				if (row + 1 == vp->nrows)
					row = 1;
				else if (row + 2 == vp->nrows)
					row = 0;
				else
					row = row + 2;
				break;
			case 80:
			case 90:
				if (row + 1 == vp->nrows)
					row = 1;
				else if (row + 2 == vp->nrows)
					row = 0;
				else
					row = row + 2;
				break;
			case 120:
				row = (row + 1 == vp->nrows) ? 0 : row + 1;
				break;
			case 150:
			case 160:
				col = (col + 1 == vp->ncols) ? 0 : col + 1;
				row = (row + 1 == vp->nrows) ? 0 : row + 1;
				break;
			case 180:
				col = (col + 1 == vp->ncols) ? 0 : col + 1;
				break;
			case 200:
			case 210:
				col = (col + 1 == vp->ncols) ? 0 : col + 1;
				row = (!row) ? vp->nrows - 1 : row - 1;
				break;
			case 240:
				row = (!row) ? vp->nrows - 1 : row - 1;
				break;
			case 270:
			case 280:
				if (!row)
					row = vp->nrows - 2;
				else if (!(row - 1))
					row = vp->nrows - 1;
				else
					row = row - 2;
				break;
			case 300:
				col = (!col) ? vp->ncols - 1 : col - 1;
				if (!row)
					row = vp->nrows - 2;
				else if (!(row - 1))
					row = vp->nrows - 1;
				else
					row = row - 2;
				break;
			case 320:
			case 330:
				col = (!col) ? vp->ncols - 1 : col - 1;
				row = (!row) ? vp->nrows - 1 : row - 1;
				break;
			default:
				(void) fprintf(stderr, "wrong direction %d\n",
					dir);
			}
		} else {	/* left */
			switch (dir) {
			case 0:
				col = (col + 1 == vp->ncols) ? 0 : col + 1;
				break;
			case 30:
			case 40:
				col = (col + 1 == vp->ncols) ? 0 : col + 1;
				row = (!row) ? vp->nrows - 1 : row - 1;
				break;
			case 60:
				col = (col + 1 == vp->ncols) ? 0 : col + 1;
				if (!row)
					row = vp->nrows - 2;
				else if (row == 1)
					row = vp->nrows - 1;
				else
					row = row - 2;
				break;
			case 80:
			case 90:
				if (!row)
					row = vp->nrows - 2;
				else if (row == 1)
					row = vp->nrows - 1;
				else
					row = row - 2;
				break;
			case 120:
				row = (!row) ? vp->nrows - 1 : row - 1;
				break;
			case 150:
			case 160:
				col = (!col) ? vp->ncols - 1 : col - 1;
				row = (!row) ? vp->nrows - 1 : row - 1;
				break;
			case 180:
				col = (!col) ? vp->ncols - 1 : col - 1;
				break;
			case 200:
			case 210:
				col = (!col) ? vp->ncols - 1 : col - 1;
				row = (row + 1 == vp->nrows) ? 0 : row + 1;
				break;
			case 240:
				row = (row + 1 == vp->nrows) ? 0 : row + 1;
				break;
			case 270:
			case 280:
				if (row + 1 == vp->nrows)
					row = 1;
				else if (row + 2 == vp->nrows)
					row = 0;
				else
					row = row + 2;
				break;
			case 300:
				col = (col + 1 == vp->ncols) ? 0 : col + 1;
				if (row + 1 == vp->nrows)
					row = 1;
				else if (row + 2 == vp->nrows)
					row = 0;
				else
					row = row + 2;
				break;
			case 320:
			case 330:
				col = (col + 1 == vp->ncols) ? 0 : col + 1;
				row = (row + 1 == vp->nrows) ? 0 : row + 1;
				break;
			default:
				(void) fprintf(stderr, "wrong direction %d\n",
					dir);
			}
		}
#if 0
	} else {
		int orient = ((row & 1) * 2 + col) % 4;
		switch (orient) { /* up, down, left, right */
		case 0:
			switch (dir) {
			case 0:
				col++;
				break;
			case 51: /* 7 */
			case 72: /* 5 */
				col = (col + 2 >= vp->ncols) ? 0 : col + 2;
				break;
			case 102: /* 7 corner */
				col = (col + 3 >= vp->ncols) ? 1 : col + 3;
				row = (row == 0) ? vp->nrows - 1 : row - 1;
				break;
			case 144: /* 5 */
			case 153: /* 7 */
				col++;
				row = (row == 0) ? vp->nrows - 1 : row - 1;
				break;
			case 204: /* 7 */
			case 216: /* 5 */
				row = (row == 0) ? vp->nrows - 1 : row - 1;
				break;
			case 255: /* 7 */
				col = (col == 0) ? vp->ncols - 1 : col - 1;
				row = (row == 0) ? vp->nrows - 1 : row - 1;
				break;
			case 288: /* 5 */
			case 306: /* 7 */
				col = (col == 0) ? vp->ncols - 1 : col - 1;
				break;
			default:
				(void) fprintf(stderr, "wrong direction %d\n",
					dir);
			}
			break;
		case 1:
			switch (dir) {
			case 0:
				col--;
				break;
			case 51: /* 7 */
			case 72: /* 5 */
				col = (col == 1) ? vp->ncols - 1 : col - 2;
				break;
			case 102: /* 7 */
				col = (col == 1) ? vp->ncols - 2 : col - 3;
				row = (row + 1 == vp->nrows) ? 0 : row + 1;
				break;
			case 144: /* 5 */
			case 153: /* 7 */
				col--;
				row = (row + 1 == vp->nrows) ? 0 : row + 1;
				break;
			case 204: /* 7 */
			case 216: /* 5 */
				row = (row + 1 == vp->nrows) ? 0 : row + 1;
				break;
			case 255: /* 7 */
				col = (col + 1 >= vp->ncols) ? 0 : col + 1;
				row = (row + 1 == vp->nrows) ? 0 : row + 1;
				break;
			case 288: /* 5 */
			case 306: /* 7 */
				col = (col + 1 >= vp->ncols) ? 0 : col + 1;
				break;
			default:
				(void) fprintf(stderr, "wrong direction %d\n",
					dir);
			}
			break;
		case 2:
			switch (dir) {
			case 0:
				col = (col + 1 >= vp->ncols) ? 0 : col + 1;
				break;
			case 51: /* 7 */
			case 72: /* 5 */
				row = (row == 0) ? vp->nrows - 1 : row - 1;
				col++;
				break;
			case 102: /* 7 */
				col = (col == 0) ? vp->ncols - 1 : col - 1;
				row = (row == 0) ? vp->nrows - 1 : row - 1;
				break;
			case 144: /* 5 */
			case 153: /* 7 */
				col = (col == 0) ? vp->ncols - 2 : col - 2;
				break;
			case 204: /* 7 */
			case 216: /* 5 */
				col = (col == 0) ? vp->ncols - 1 : col - 1;
				break;
			case 255: /* 7 */
				row = (row + 1 == vp->nrows) ? 0 : row + 1;
				col = (col == 0) ? vp->ncols - 1 : col - 1;
				break;
			case 288: /* 5 */
			case 306: /* 7 */
				row = (row + 1 == vp->nrows) ? 0 : row + 1;
				break;
			default:
				(void) fprintf(stderr, "wrong direction %d\n",
					dir);
			}
			break;
		case 3:
			switch (dir) {
			case 0:
				col--;
				break;
			case 51: /* 7 */
			case 72: /* 5 */
				col = (col == 0) ? vp->ncols - 1 : col - 1;
				row = (row + 1 == vp->nrows) ? 0 : row + 1;
				break;
			case 102: /* 7 */
				col = (col + 1 >= vp->ncols) ? 0 : col + 1;
				row = (row + 1 == vp->nrows) ? 0 : row + 1;
				break;
			case 144: /* 5 */
			case 153: /* 7 */
				col = (col + 2 >= vp->ncols) ? 1 : col + 2;
				break;
			case 204: /* 7 */
			case 216: /* 5 */
				col = (col + 1 >= vp->ncols) ? 0 : col + 1;
				break;
			case 255: /* 7 */
				col = (col + 1 >= vp->ncols) ? 0 : col + 1;
				row = (row == 0) ? vp->nrows - 1 : row - 1;
				break;
			case 288: /* 5 */
			case 306: /* 7 */
				row = (row == 0) ? vp->nrows - 1 : row - 1;
				break;
			default:
				(void) fprintf(stderr, "wrong direction %d\n",
					dir);
			}
			break;
		default:
			(void) fprintf(stderr, "wrong orient %d\n",
				orient);
		}
#endif
	}
	return (row * vp->ncols + col);
}

static void
advanceColors(ModeInfo * mi, int col, int row)
{
	voterstruct *vp = &voters[MI_SCREEN(mi)];
	CellList   *curr;

	curr = vp->first->next;
	while (curr != vp->last) {
		if (curr->info.col == col && curr->info.row == row) {
			curr = curr->next;
			removefrom_list(curr->previous);
		} else {
			if (curr->info.age > 0)
				curr->info.age--;
			else if (curr->info.age < 0)
				curr->info.age++;
			drawCell(mi, curr->info.col, curr->info.row,
				 (MI_NPIXELS(mi) + curr->info.age / FACTOR +
				  (MI_NPIXELS(mi) * curr->info.kind / BITMAPS)) % MI_NPIXELS(mi),
				 curr->info.kind, False);
			if (curr->info.age == 0) {
				curr = curr->next;
				removefrom_list(curr->previous);
			} else
				curr = curr->next;
		}
	}
}

static void
free_voters_screen(voterstruct *vp)
{
	if (vp == NULL) {
		return;
	}
	if (vp->first != NULL) {
		flush_list(vp);
		free(vp->first);
		vp->first = (CellList *) NULL;
	}
	if (vp->last != NULL) {
		free(vp->last);
		vp->last = (CellList *) NULL;
	}
	if (vp->arr != NULL) {
		free(vp->arr);
		vp->arr = (char *) NULL;
	}
	vp = NULL;
}

ENTRYPOINT void
init_voters(ModeInfo * mi)
{
	int         size = MI_SIZE(mi);
	int         i, col, row, colrow;
	voterstruct *vp;

	MI_INIT(mi, voters);
	vp = &voters[MI_SCREEN(mi)];

	vp->generation = 0;
	if (!vp->first) {	/* Genesis of democracy */
		icon_width = donkey_width;
		icon_height = donkey_height;
		if (!init_list(vp)) {
			free_voters_screen(vp);
			return;
		}
		for (i = 0; i < BITMAPS; i++) {
			logo[i].width = icon_width;
			logo[i].height = icon_height;
			logo[i].bytes_per_line = (icon_width + 7) / 8;
		}
	} else			/* Exterminate all free thinking individuals */
		flush_list(vp);
	if (MI_IS_FULLRANDOM(mi)) {
		vp->vertical = (Bool) (LRAND() & 1);
	} else {
		vp->vertical = vertical;
	}
	vp->width = MI_WIDTH(mi);
	vp->height = MI_HEIGHT(mi);

	for (i = 0; i < NEIGHBORKINDS; i++) {
		if (neighbors == plots[i]) {
			vp->neighbors = neighbors;
			break;
		}
		if (i == NEIGHBORKINDS - 1) {
#if 0
			vp->neighbors = plots[NRAND(NEIGHBORKINDS)];
			vp->neighbors = (LRAND() & 1) ? 4 : 8;
#else
			vp->neighbors = 8;
#endif
			break;
		}
	}

	if (vp->neighbors == 6) {
		int nccols, ncrows, sides;

		vp->polygon = 6;
		if (!vp->vertical) {
			vp->height = MI_WIDTH(mi);
			vp->width = MI_HEIGHT(mi);
		}
		if (vp->width < 8)
			vp->width = 8;
		if (vp->height < 8)
			vp->height = 8;
		if (size < -MINSIZE)
			vp->ys = NRAND(MIN(-size, MAX(MINSIZE, MIN(vp->width, vp->height) /
				      MINGRIDSIZE)) - MINSIZE + 1) + MINSIZE;
		else if (size < MINSIZE) {
			if (!size)
				vp->ys = MAX(MINSIZE, MIN(vp->width, vp->height) / MINGRIDSIZE);
			else
				vp->ys = MINSIZE;
		} else
			vp->ys = MIN(size, MAX(MINSIZE, MIN(vp->width, vp->height) /
					       MINGRIDSIZE));
		vp->xs = vp->ys;
		vp->pixelmode = True;
		nccols = MAX(vp->width / vp->xs - 2, 2);
		ncrows = MAX(vp->height / vp->ys - 1, 4);
		vp->ncols = nccols / 2;
		vp->nrows = 2 * (ncrows / 4);
		vp->xb = (vp->width - vp->xs * nccols) / 2 + vp->xs / 2;
		vp->yb = (vp->height - vp->ys * (ncrows / 2) * 2) / 2 +
			vp->ys - 2;
		for (sides = 0; sides < 6; sides++) {
			if (vp->vertical) {
				vp->shape.hexagon[sides].x =
					(vp->xs - 1) * hexagonUnit[sides].x;
				vp->shape.hexagon[sides].y =
					((vp->ys - 1) * hexagonUnit[sides].y /
					2) * 4 / 3;
			} else {
				vp->shape.hexagon[sides].y =
					(vp->xs - 1) * hexagonUnit[sides].x;
				vp->shape.hexagon[sides].x =
					((vp->ys - 1) * hexagonUnit[sides].y /
					2) * 4 / 3;
			}
		}
	} else if (vp->neighbors == 4 || vp->neighbors == 8) {
		vp->polygon = 4;
		if (vp->width < 2)
			vp->width = 2;
		if (vp->height < 2)
			vp->height = 2;
		if (size == 0 ||
		    MINGRIDSIZE * size > vp->width || MINGRIDSIZE * size > vp->height) {
			if (vp->width > MINGRIDSIZE * icon_width &&
			    vp->height > MINGRIDSIZE * icon_height) {
				vp->pixelmode = False;
				vp->xs = icon_width;
				vp->ys = icon_height;
			} else {
				vp->pixelmode = True;
				vp->xs = vp->ys = MAX(MINSIZE, MIN(vp->width, vp->height) /
						      MINGRIDSIZE);
			}
		} else {
			vp->pixelmode = True;
			if (size < -MINSIZE)
				vp->ys = NRAND(MIN(-size, MAX(MINSIZE, MIN(vp->width, vp->height) /
				      MINGRIDSIZE)) - MINSIZE + 1) + MINSIZE;
			else if (size < MINSIZE)
				vp->ys = MINSIZE;
			else
				vp->ys = MIN(size, MAX(MINSIZE, MIN(vp->width, vp->height) /
						       MINGRIDSIZE));
			vp->xs = vp->ys;
		}
		vp->ncols = MAX(vp->width / vp->xs, 2);
		vp->nrows = MAX(vp->height / vp->ys, 2);
		vp->xb = (vp->width - vp->xs * vp->ncols) / 2;
		vp->yb = (vp->height - vp->ys * vp->nrows) / 2;
	} else {		/* TRI */
		int orient, sides;

		vp->polygon = 3;
		if (!vp->vertical) {
			vp->height = MI_WIDTH(mi);
			vp->width = MI_HEIGHT(mi);
		}
		if (vp->width < 2)
			vp->width = 2;
		if (vp->height < 2)
			vp->height = 2;
		if (size < -MINSIZE)
			vp->ys = NRAND(MIN(-size, MAX(MINSIZE, MIN(vp->width, vp->height) /
				      MINGRIDSIZE)) - MINSIZE + 1) + MINSIZE;
		else if (size < MINSIZE) {
			if (!size)
				vp->ys = MAX(MINSIZE, MIN(vp->width, vp->height) / MINGRIDSIZE);
			else
				vp->ys = MINSIZE;
		} else
			vp->ys = MIN(size, MAX(MINSIZE, MIN(vp->width, vp->height) /
					       MINGRIDSIZE));
		vp->xs = (int) (1.52 * vp->ys);
		vp->pixelmode = True;
		vp->ncols = (MAX(vp->width / vp->xs - 1, 2) / 2) * 2;
		vp->nrows = (MAX(vp->height / vp->ys - 1, 2) / 2) * 2;
		vp->xb = (vp->width - vp->xs * vp->ncols) / 2 + vp->xs / 2;
		vp->yb = (vp->height - vp->ys * vp->nrows) / 2 + vp->ys / 2;
		for (orient = 0; orient < 2; orient++) {
			for (sides = 0; sides < 3; sides++) {
				if (vp->vertical) {
					vp->shape.triangle[orient][sides].x =
						(vp->xs - 2) * triangleUnit[orient][sides].x;
					vp->shape.triangle[orient][sides].y =
						(vp->ys - 2) * triangleUnit[orient][sides].y;
				} else {
					vp->shape.triangle[orient][sides].y =
						(vp->xs - 2) * triangleUnit[orient][sides].x;
					vp->shape.triangle[orient][sides].x =
						(vp->ys - 2) * triangleUnit[orient][sides].y;
				}
			}
		}
	}

	vp->npositions = vp->ncols * vp->nrows;
	if (vp->arr != NULL)
		free(vp->arr);
	if ((vp->arr = (char *) calloc(vp->npositions, sizeof (char))) == NULL) {
		free_voters_screen(vp);
		return;
	}

	/* Play G-d with these numbers */
	vp->nparties = MI_COUNT(mi);
	if (vp->nparties < MINPARTIES || vp->nparties > BITMAPS)
		vp->nparties = NRAND(BITMAPS - MINPARTIES + 1) + MINPARTIES;
	if (vp->pixelmode)
		vp->nparties = 2;

	vp->busyLoop = 0;
	MI_CLEARWINDOW(mi);
	vp->painted = False;

	for (i = 0; i < BITMAPS; i++)
		vp->number_in_party[i] = 0;

	for (row = 0; row < vp->nrows; row++)
		for (col = 0; col < vp->ncols; col++) {
			colrow = col + row * vp->ncols;
			if (vp->nparties == 2)
				i = (NRAND(vp->nparties) + 2) % BITMAPS;
			else
				i = NRAND(vp->nparties);
			vp->arr[colrow] = (char) i;
			drawCell(mi, col, row, (unsigned long) (MI_NPIXELS(mi) * i / BITMAPS),
				 i, False);
			vp->number_in_party[i]++;
		}
}

ENTRYPOINT void
refresh_voters(ModeInfo * mi)
{
	int         col, row, colrow;
	voterstruct *vp;

	if (voters == NULL)
		return;
	vp = &voters[MI_SCREEN(mi)];
	if (vp->first == NULL)
		return;

	if (vp->painted) {
		MI_CLEARWINDOW(mi);
		vp->painted = False;
		for (row = 0; row < vp->nrows; row++)
			for (col = 0; col < vp->ncols; col++) {
				colrow = col + row * vp->ncols;
				/* Draw all old, will get corrected soon if wrong... */
				drawCell(mi, col, row,
					 (unsigned long) (MI_NPIXELS(mi) * vp->arr[colrow] / BITMAPS),
					 vp->arr[colrow], False);
			}
	}
}

ENTRYPOINT void
draw_voters(ModeInfo * mi)
{
	int         i, spineless_dude, neighbor_direction;
	int         spineless_col, spineless_row;
	int         new_opinion, old_opinion;
	cellstruct  info;
	voterstruct *vp;

	if (voters == NULL)
		return;
	vp = &voters[MI_SCREEN(mi)];
	if (vp->first == NULL)
		return;

	MI_IS_DRAWN(mi) = True;
	vp->painted = True;
	if (vp->busyLoop) {
		if (vp->busyLoop >= 5000)
			vp->busyLoop = 0;
		else
			vp->busyLoop++;
		return;
	}
	for (i = 0; i < BITMAPS; i++)
		if (vp->number_in_party[i] == vp->npositions) {		/* The End of the WORLD */
			init_voters(mi);	/* Create a more interesting planet */
		}
	spineless_dude = NRAND(vp->npositions);
	neighbor_direction = NRAND(vp->neighbors);
	spineless_col = spineless_dude % vp->ncols;
	spineless_row = spineless_dude / vp->ncols;
	old_opinion = vp->arr[spineless_dude];
	/* neighbors opinion */
	new_opinion = vp->arr[positionOfNeighbor(vp, neighbor_direction, spineless_col, spineless_row)];
	if (old_opinion != new_opinion) {
		vp->number_in_party[old_opinion]--;
		vp->number_in_party[new_opinion]++;
		vp->arr[spineless_dude] = new_opinion;
		info.kind = new_opinion;
		info.age = (old_opinion - new_opinion);
		if (info.age == 2)
			info.age = -1;
		if (info.age == -2)
			info.age = 1;
		info.age *= (FACTOR * MI_NPIXELS(mi)) / 3;
		info.col = spineless_col;
		info.row = spineless_row;
		if (MI_NPIXELS(mi) > 2) {
			advanceColors(mi, spineless_col, spineless_row);
			if (!addto_list(vp, info)) {
				free_voters_screen(vp);
				return;
			}
		}
		drawCell(mi, spineless_col, spineless_row,
			 (MI_NPIXELS(mi) + info.age / FACTOR +
		  (MI_NPIXELS(mi) * new_opinion / BITMAPS)) % MI_NPIXELS(mi),
			 new_opinion, True);
	} else if (MI_NPIXELS(mi) > 2)
		advanceColors(mi, -1, -1);
	vp->generation++;
	for (i = 0; i < BITMAPS; i++)
		if (vp->number_in_party[i] == vp->npositions) {		/* The End of the WORLD */
			vp->busyLoop = 1;
			refresh_voters(mi);
		}
}

ENTRYPOINT void
release_voters(ModeInfo * mi)
{
	if (voters != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_voters_screen(&voters[screen]);
		free(voters);
		voters = (voterstruct *) NULL;
	}
}

XSCREENSAVER_MODULE ("Voters", voters)

#endif /* MODE_voters */
